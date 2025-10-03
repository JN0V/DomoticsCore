#pragma once

#include "HAEntity.h"

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

/**
 * @brief Home Assistant Binary Sensor entity
 * 
 * Read-only on/off states (motion, door, window, etc.)
 */
class HABinarySensor : public HAEntity {
public:
    HABinarySensor(const String& id, const String& name,
                   const String& deviceClass = "", const String& icon = "")
        : HAEntity(id, name, "binary_sensor") {
        this->deviceClass = deviceClass;
        this->icon = icon;
    }
    
    String payloadOn = "ON";
    String payloadOff = "OFF";
    
    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);
        
        // Add binary sensor-specific fields
        doc["payload_on"] = payloadOn;
        doc["payload_off"] = payloadOff;
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
