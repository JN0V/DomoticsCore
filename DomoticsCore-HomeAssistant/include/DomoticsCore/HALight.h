#pragma once

#include "HAEntity.h"

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

/**
 * @brief Home Assistant Light entity
 * 
 * Controllable light with optional brightness support
 */
class HALight : public HAEntity {
public:
    HALight(const String& id, const String& name)
        : HAEntity(id, name, "light") {}

    bool supportsBrightness = true;
    bool optimistic = false;
    bool state = false;        // Current light state (updated by handleCommand)
    uint8_t brightness = 0;    // Current brightness (updated by handleCommand)
    
    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);
        
        // Add light-specific fields
        char buf[HA_TOPIC_BUF_SIZE];
        getCommandTopic(buf, sizeof(buf), nodeId.c_str(), discoveryPrefix.c_str());
        doc["cmd_t"] = buf;
        doc["stat_val_tpl"] = "{{ value_json.state }}";

        // brightness_scale defaults to 255; "brightness" belongs to the JSON schema.
        if (supportsBrightness) {
            getStateTopic(buf, sizeof(buf), nodeId.c_str(), discoveryPrefix.c_str());
            doc["bri_stat_t"] = buf;
            // brightness_command_topic is the same as command_topic
            doc["bri_cmd_t"] = doc["cmd_t"];
            doc["bri_val_tpl"] = "{{ value_json.brightness }}";
            doc["on_cmd_type"] = "brightness";
        }
        
        if (optimistic) {
            doc["opt"] = true;
        }
    }
    
    /**
     * @brief Handle command from Home Assistant
     * @param payload JSON command payload or simple ON/OFF
     * @return true if command was valid and processed, false for invalid/garbage payloads
     */
    bool handleCommand(const String& payload) override {
        // The default schema sends a brightness as a bare number on the command topic.
        if (isDigits(payload)) {
            if (payload.length() > 3 || payload.toInt() > 255) return false;
            brightness = (uint8_t)payload.toInt();
            state = brightness > 0;
            return true;
        }

        // Parse JSON command
        JsonDocument cmdDoc;
        DeserializationError error = deserializeJson(cmdDoc, payload);

        if (error || !cmdDoc.is<JsonObject>()) {
            // Try simple ON/OFF
            if (payload == "ON" || payload == "OFF") {
                state = (payload == "ON");
                brightness = state ? 255 : 0;
                return true;
            }
            // Invalid payload (not JSON, not ON/OFF)
            DLOG_W(LOG_HA, "Invalid light command payload: %s", payload.c_str());
            return false;
        }

        // Extract state and brightness from JSON; a brightness off the scale is refused
        JsonVariant bri = cmdDoc["brightness"];
        if (!bri.isNull() && (!bri.is<int>() || bri.as<int>() < 0 || bri.as<int>() > 255)) return false;
        String stateStr = cmdDoc["state"] | String("ON");
        brightness = bri.isNull() ? 255 : (uint8_t)bri.as<int>();
        state = (stateStr == "ON");
        return true;
    }

private:
    static bool isDigits(const String& s) {
        if (s.isEmpty()) return false;
        for (size_t i = 0; i < s.length(); i++) if (s[i] < '0' || s[i] > '9') return false;
        return true;
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
