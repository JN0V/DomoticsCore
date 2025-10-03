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
    HALight(const String& id, const String& name,
            std::function<void(bool, uint8_t)> commandCallback = nullptr)
        : HAEntity(id, name, "light"), commandCallback(commandCallback) {}
    
    bool supportsBrightness = true;
    bool optimistic = false;
    std::function<void(bool, uint8_t)> commandCallback;  // state, brightness
    
    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);
        
        // Add light-specific fields
        doc["command_topic"] = getCommandTopic(nodeId, discoveryPrefix);
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["state_value_template"] = "{{ value_json.state }}";
        
        if (supportsBrightness) {
            doc["brightness"] = true;
            doc["brightness_scale"] = 255;
            doc["brightness_state_topic"] = getStateTopic(nodeId, discoveryPrefix);
            doc["brightness_command_topic"] = getCommandTopic(nodeId, discoveryPrefix);
            doc["brightness_value_template"] = "{{ value_json.brightness }}";
            doc["on_command_type"] = "brightness";
        }
        
        if (optimistic) {
            doc["optimistic"] = true;
        }
    }
    
    /**
     * @brief Handle command from Home Assistant
     * @param payload JSON command payload
     */
    void handleCommand(const String& payload) {
        if (!commandCallback) return;
        
        // Parse JSON command
        JsonDocument cmdDoc;
        DeserializationError error = deserializeJson(cmdDoc, payload);
        
        if (error) {
            // Try simple ON/OFF
            bool state = (payload == "ON");
            commandCallback(state, state ? 255 : 0);
            return;
        }
        
        // Extract state and brightness
        String state = cmdDoc["state"] | String("ON");
        uint8_t brightness = cmdDoc["brightness"] | 255;
        
        bool isOn = (state == "ON");
        commandCallback(isOn, brightness);
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
