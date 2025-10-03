#pragma once

#include "HAEntity.h"

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

/**
 * @brief Home Assistant Button entity
 * 
 * Trigger-only action (restart, calibrate, etc.)
 */
class HAButton : public HAEntity {
public:
    HAButton(const String& id, const String& name,
             std::function<void()> pressCallback = nullptr,
             const String& icon = "")
        : HAEntity(id, name, "button"), pressCallback(pressCallback) {
        this->icon = icon;
    }
    
    String payloadPress = "PRESS";
    std::function<void()> pressCallback;
    
    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation (without state_topic for buttons)
        doc["name"] = name;
        doc["unique_id"] = getUniqueId(nodeId);
        
        if (!icon.isEmpty()) {
            doc["icon"] = icon;
        }
        
        if (!deviceClass.isEmpty()) {
            doc["device_class"] = deviceClass;
        }
        
        doc["device"] = device;
        
        if (!availabilityTopic.isEmpty()) {
            doc["availability_topic"] = availabilityTopic;
            doc["payload_available"] = "online";
            doc["payload_not_available"] = "offline";
        }
        
        // Add button-specific fields
        doc["command_topic"] = getCommandTopic(nodeId, discoveryPrefix);
        doc["payload_press"] = payloadPress;
    }
    
    /**
     * @brief Handle button press from Home Assistant
     * @param payload Command payload
     */
    void handleCommand(const String& payload) {
        if (pressCallback && payload == payloadPress) {
            pressCallback();
        }
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
