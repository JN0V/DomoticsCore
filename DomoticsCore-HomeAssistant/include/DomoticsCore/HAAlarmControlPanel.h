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
inline constexpr AlarmFeature operator|(AlarmFeature a, AlarmFeature b) {
    return static_cast<AlarmFeature>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline AlarmFeature& operator|=(AlarmFeature& a, AlarmFeature b) {
    return a = a | b;
}
inline constexpr bool operator&(AlarmFeature a, AlarmFeature b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
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
                        const String& icon = "mdi:shield-home")
        : HAEntity(id, name, "alarm_control_panel") {
        this->icon = icon;
    }

    String code;
    AlarmFeature supportedFeatures = AlarmFeature::ArmAway;
    bool codeArmRequired = false;
    bool codeDisarmRequired = false;
    bool codeTriggerRequired = false;
    char lastCommand[64] = {};  // Parsed command from last handleCommand (e.g., "ARM_AWAY")
    char lastCode[32] = {};     // Parsed code from last handleCommand (e.g., "1234")

    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);

        // Add command topic
        char buf[HA_TOPIC_BUF_SIZE];
        getCommandTopic(buf, sizeof(buf), nodeId.c_str(), discoveryPrefix.c_str());
        doc["cmd_t"] = buf;

        // Home Assistant reads an absent code_*_required as true where this
        // component defaults to false, so each key is written where its value can
        // act: arming and disarming always, triggering where the panel offers it.
        doc["cod_arm_req"] = codeArmRequired;
        doc["cod_dis_req"] = codeDisarmRequired;
        if (supportedFeatures & AlarmFeature::Trigger) doc["cod_trig_req"] = codeTriggerRequired;

        // The code itself, and the template that carries it, only matter when a
        // code travels — configured here, or asked of the user by Home Assistant.
        if (!code.isEmpty()) doc["code"] = code;
        const bool triggerCodeApplies = codeTriggerRequired && (supportedFeatures & AlarmFeature::Trigger);
        if (!code.isEmpty() || codeArmRequired || codeDisarmRequired || triggerCodeApplies) {
            doc["cmd_tpl"] = "{{ action }}{% if code %} {{ code }}{% endif %}";
        }

        // No pl_* key: each would carry the payload Home Assistant assumes for an
        // absent key, spending the discovery field to state what it already knows.
        // The constants above are the agreement, and a native test holds them to it.

        // Supported features array (built from bitmask)
        JsonArray features = doc["sup_feat"].to<JsonArray>();
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
     * Parses "COMMAND" or "COMMAND CODE" format and stores in lastCommand/lastCode.
     * @param payload Raw MQTT payload
     * @return true (alarm commands are always valid — parsing always succeeds)
     */
    bool handleCommand(const String& payload) override {
        // Zero-initialize to prevent stale data leakage
        memset(lastCommand, 0, sizeof(lastCommand));
        memset(lastCode, 0, sizeof(lastCode));

        // Trim boundaries without copying the full payload
        int start = 0;
        int len = payload.length();
        while (start < len && payload.charAt(start) == ' ') start++;
        int end = len;
        while (end > start && payload.charAt(end - 1) == ' ') end--;

        if (start >= end) {
            DLOG_W(LOG_HA, "Empty alarm command payload");
            return true;
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

        strncpy(lastCommand, command.c_str(), sizeof(lastCommand) - 1);
        strncpy(lastCode, codeValue.c_str(), sizeof(lastCode) - 1);
        return true;
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
