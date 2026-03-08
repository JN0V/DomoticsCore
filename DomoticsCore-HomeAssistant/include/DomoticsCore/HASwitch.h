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
             const String& icon = "")
        : HAEntity(id, name, "switch") {
        this->icon = icon;
    }

    String payloadOn = "ON";
    String payloadOff = "OFF";
    bool optimistic = false;    // If true, HA assumes state changes immediately
    bool autoPublishState = true;  // If true, auto-publishes state back to HA after command
    bool state = false;  // Current switch state (updated by handleCommand)
    
    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);
        
        // Add switch-specific fields
        char buf[HA_TOPIC_BUF_SIZE];
        getCommandTopic(buf, sizeof(buf), nodeId.c_str(), discoveryPrefix.c_str());
        doc["command_topic"] = buf;
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
     * @return true (switch commands are always valid)
     */
    bool handleCommand(const String& payload) override {
        state = (payload == payloadOn);
        return true;
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
