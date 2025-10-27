#pragma once

#include "HAEntity.h"

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

/**
 * @brief Home Assistant Sensor entity
 * 
 * Read-only numeric or text values (temperature, humidity, etc.)
 */
class HASensor : public HAEntity {
public:
    HASensor(const String& id, const String& name, 
             const String& unit = "", const String& deviceClass = "", const String& icon = "")
        : HAEntity(id, name, "sensor"), unit(unit) {
        this->deviceClass = deviceClass;
        this->icon = icon;
    }
    
    String unit;                // Unit of measurement (°C, %, W, etc.)
    String stateClass;          // measurement, total, total_increasing
    float expireAfter = 0;      // Seconds (0 = never expire)
    
    void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                              const String& discoveryPrefix,
                              const JsonObject& device,
                              const String& availabilityTopic) const override {
        // Call base implementation
        HAEntity::buildDiscoveryPayload(doc, nodeId, discoveryPrefix, device, availabilityTopic);
        
        // Add sensor-specific fields
        if (!unit.isEmpty()) {
            doc["unit_of_measurement"] = unit;
        }
        
        if (!stateClass.isEmpty()) {
            doc["state_class"] = stateClass;
        } else if (!unit.isEmpty()) {
            // Auto-set state_class for numeric sensors
            doc["state_class"] = "measurement";
        }
        
        if (expireAfter > 0) {
            doc["expire_after"] = (int)expireAfter;
        }
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
