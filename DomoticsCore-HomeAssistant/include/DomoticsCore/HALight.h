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
        doc["command_topic"] = buf;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["state_value_template"] = "{{ value_json.state }}";

        if (supportsBrightness) {
            doc["brightness"] = true;
            doc["brightness_scale"] = 255;
            getStateTopic(buf, sizeof(buf), nodeId.c_str(), discoveryPrefix.c_str());
            doc["brightness_state_topic"] = buf;
            // brightness_command_topic is the same as command_topic
            doc["brightness_command_topic"] = doc["command_topic"];
            doc["brightness_value_template"] = "{{ value_json.brightness }}";
            doc["on_command_type"] = "brightness";
        }
        
        if (optimistic) {
            doc["optimistic"] = true;
        }
    }
    
    /**
     * @brief Handle command from Home Assistant
     * @param payload JSON command payload or simple ON/OFF
     * @return true if command was valid and processed, false for invalid/garbage payloads
     */
    bool handleCommand(const String& payload) override {
        // Parse JSON command
        JsonDocument cmdDoc;
        DeserializationError error = deserializeJson(cmdDoc, payload);

        if (error) {
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

        // Extract state and brightness from JSON
        String stateStr = cmdDoc["state"] | String("ON");
        brightness = cmdDoc["brightness"] | 255;
        state = (stateStr == "ON");
        return true;
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
