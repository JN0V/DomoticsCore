#pragma once

#include "HAEntity.h"

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

/**
 * @brief Alarm panel feature flags (uint8_t bitmask)
 */
enum class AlarmFeature : uint8_t {
    ArmHome         = 0x01,
    ArmAway         = 0x02,
    ArmNight        = 0x04,
    ArmVacation     = 0x08,
    ArmCustomBypass = 0x10,
    Trigger         = 0x20
};

// Bitwise operators for AlarmFeature bitmask usage
inline constexpr uint8_t operator|(AlarmFeature a, AlarmFeature b) {
    return static_cast<uint8_t>(a) | static_cast<uint8_t>(b);
}
inline constexpr uint8_t operator|(uint8_t a, AlarmFeature b) {
    return a | static_cast<uint8_t>(b);
}
inline constexpr uint8_t operator&(uint8_t a, AlarmFeature b) {
    return a & static_cast<uint8_t>(b);
}

/**
 * @brief HA alarm_control_panel state constants (zero per-instance heap)
 */
namespace AlarmPanelState {
    constexpr const char* Disarmed          = "disarmed";
    constexpr const char* Arming            = "arming";
    constexpr const char* ArmedHome         = "armed_home";
    constexpr const char* ArmedAway         = "armed_away";
    constexpr const char* ArmedNight        = "armed_night";
    constexpr const char* ArmedVacation     = "armed_vacation";
    constexpr const char* ArmedCustomBypass = "armed_custom_bypass";
    constexpr const char* Pending           = "pending";
    constexpr const char* Triggered         = "triggered";
    // Consumer convenience constant only. No HA command triggers this state.
    // Consumers may publish it if their alarm system supports a disarming delay.
    constexpr const char* Disarming         = "disarming";
}

/**
 * @brief HA alarm_control_panel command constants (zero per-instance heap)
 */
namespace AlarmPanelCommand {
    constexpr const char* ARM_HOME          = "ARM_HOME";
    constexpr const char* ARM_AWAY          = "ARM_AWAY";
    constexpr const char* ARM_NIGHT         = "ARM_NIGHT";
    constexpr const char* ARM_VACATION      = "ARM_VACATION";
    constexpr const char* ARM_CUSTOM_BYPASS = "ARM_CUSTOM_BYPASS";
    constexpr const char* DISARM            = "DISARM";
    constexpr const char* TRIGGER           = "TRIGGER";
}

/**
 * @brief Home Assistant Alarm Control Panel entity
 *
 * Native HA alarm panel with multiple arm modes, intermediate states,
 * and optional PIN code passthrough. Renders as the native alarm-panel
 * Lovelace card with keypad and color-coded status.
 *
 * Consumer is responsible for all state transitions — no auto-publish.
 */
class HAAlarmControlPanel : public HAEntity {
public:
    HAAlarmControlPanel(const String& id, const String& name,
                        const std::function<void(const String&, const String&)>& onCommand,
                        const String& icon = "mdi:shield-home")
        : HAEntity(id, name, "alarm_control_panel"), commandCallback(onCommand) {
        this->icon = icon;
    }

    String code;
    uint8_t supportedFeatures = static_cast<uint8_t>(AlarmFeature::ArmAway);
    bool codeArmRequired = false;
    bool codeDisarmRequired = false;
    bool codeTriggerRequired = false;
    std::function<void(const String&, const String&)> commandCallback;

    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);

        // Add command topic
        char buf[HA_TOPIC_BUF_SIZE];
        getCommandTopic(buf, sizeof(buf), nodeId.c_str(), discoveryPrefix.c_str());
        doc["command_topic"] = buf;

        // Code configuration (only when code support is active)
        bool hasCodeConfig = !code.isEmpty() || codeArmRequired || codeDisarmRequired || codeTriggerRequired;
        if (hasCodeConfig) {
            if (!code.isEmpty()) {
                doc["code"] = code;
            }
            doc["code_arm_required"] = codeArmRequired;
            doc["code_disarm_required"] = codeDisarmRequired;
            doc["code_trigger_required"] = codeTriggerRequired;
            doc["command_template"] = "{{ action }}{% if code %} {{ code }}{% endif %}";
        }

        // Command payload constants (only for supported features + always disarm)
        if (supportedFeatures & AlarmFeature::ArmHome)         doc["payload_arm_home"] = AlarmPanelCommand::ARM_HOME;
        if (supportedFeatures & AlarmFeature::ArmAway)         doc["payload_arm_away"] = AlarmPanelCommand::ARM_AWAY;
        if (supportedFeatures & AlarmFeature::ArmNight)        doc["payload_arm_night"] = AlarmPanelCommand::ARM_NIGHT;
        if (supportedFeatures & AlarmFeature::ArmVacation)     doc["payload_arm_vacation"] = AlarmPanelCommand::ARM_VACATION;
        if (supportedFeatures & AlarmFeature::ArmCustomBypass) doc["payload_arm_custom_bypass"] = AlarmPanelCommand::ARM_CUSTOM_BYPASS;
        doc["payload_disarm"] = AlarmPanelCommand::DISARM;  // Always available
        if (supportedFeatures & AlarmFeature::Trigger)         doc["payload_trigger"] = AlarmPanelCommand::TRIGGER;

        // Supported features array (built from bitmask)
        JsonArray features = doc["supported_features"].to<JsonArray>();
        if (supportedFeatures & AlarmFeature::ArmHome)         features.add("arm_home");
        if (supportedFeatures & AlarmFeature::ArmAway)         features.add("arm_away");
        if (supportedFeatures & AlarmFeature::ArmNight)        features.add("arm_night");
        if (supportedFeatures & AlarmFeature::ArmVacation)     features.add("arm_vacation");
        if (supportedFeatures & AlarmFeature::ArmCustomBypass) features.add("arm_custom_bypass");
        if (supportedFeatures & AlarmFeature::Trigger)         features.add("trigger");
    }

    /**
     * @brief Handle command from Home Assistant
     *
     * Parses "COMMAND" or "COMMAND CODE" format and calls commandCallback.
     * Consumer callback handles all validation.
     */
    void handleCommand(const String& payload) override {
        if (!commandCallback) return;

        // Trim boundaries without copying the full payload
        int start = 0;
        int len = payload.length();
        while (start < len && payload.charAt(start) == ' ') start++;
        int end = len;
        while (end > start && payload.charAt(end - 1) == ' ') end--;

        if (start >= end) {
            DLOG_W(LOG_HA, "Empty alarm command payload");
            return;
        }

        int spaceIdx = payload.indexOf(' ', start);
        if (spaceIdx < start || spaceIdx >= end) spaceIdx = -1;

        String command = (spaceIdx > start)
            ? payload.substring(start, spaceIdx)
            : payload.substring(start, end);

        String codeValue;
        if (spaceIdx > start) {
            int codeStart = spaceIdx + 1;
            while (codeStart < end && payload.charAt(codeStart) == ' ') codeStart++;
            if (codeStart < end) codeValue = payload.substring(codeStart, end);
        }

        commandCallback(command, codeValue);
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
