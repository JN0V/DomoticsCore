#pragma once

#include "HAEntity.h"

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

/**
 * @brief Home Assistant Switch entity
 * 
 * Controllable on/off device (relay, socket, etc.)
 */
class HASwitch : public HAEntity {
public:
    HASwitch(const String& id, const String& name,
             std::function<void(bool)> commandCallback = nullptr,
             const String& icon = "")
        : HAEntity(id, name, "switch"), commandCallback(commandCallback) {
        this->icon = icon;
    }
    
    String payloadOn = "ON";
    String payloadOff = "OFF";
    bool optimistic = false;    // If true, HA assumes state changes immediately
    std::function<void(bool)> commandCallback;
    
    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);
        
        // Add switch-specific fields
        doc["command_topic"] = getCommandTopic(nodeId, discoveryPrefix);
        doc["payload_on"] = payloadOn;
        doc["payload_off"] = payloadOff;
        doc["state_on"] = payloadOn;
        doc["state_off"] = payloadOff;
        
        if (optimistic) {
            doc["optimistic"] = true;
        }
    }
    
    /**
     * @brief Handle command from Home Assistant
     * @param payload Command payload (ON/OFF)
     */
    void handleCommand(const String& payload) {
        if (commandCallback) {
            bool state = (payload == payloadOn);
            commandCallback(state);
        }
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
