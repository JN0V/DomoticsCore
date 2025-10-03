#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

/**
 * @brief Base class for Home Assistant entities
 * 
 * Provides common functionality for all entity types:
 * - Discovery topic generation
 * - State/command topic generation
 * - Device information
 * - Availability
 */
class HAEntity {
public:
    HAEntity(const String& id, const String& name, const String& component)
        : id(id), name(name), component(component) {}
    
    virtual ~HAEntity() = default;
    
    // Entity properties
    String id;                  // Unique entity ID (e.g., "temperature")
    String name;                // Display name (e.g., "Temperature")
    String component;           // HA component type (sensor, switch, etc.)
    String icon;                // mdi:icon-name
    String deviceClass;         // HA device class
    bool retained = true;       // Retain MQTT messages
    
    // Topic generation
    String getDiscoveryTopic(const String& nodeId, const String& discoveryPrefix = "homeassistant") const {
        return discoveryPrefix + "/" + component + "/" + nodeId + "/" + id + "/config";
    }
    
    String getStateTopic(const String& nodeId, const String& discoveryPrefix = "homeassistant") const {
        return discoveryPrefix + "/" + component + "/" + nodeId + "/" + id + "/state";
    }
    
    String getCommandTopic(const String& nodeId, const String& discoveryPrefix = "homeassistant") const {
        return discoveryPrefix + "/" + component + "/" + nodeId + "/" + id + "/set";
    }
    
    String getAttributesTopic(const String& nodeId, const String& discoveryPrefix = "homeassistant") const {
        return discoveryPrefix + "/" + component + "/" + nodeId + "/" + id + "/attributes";
    }
    
    // Unique ID for HA
    String getUniqueId(const String& nodeId) const {
        return nodeId + "_" + id;
    }
    
    // Discovery payload - to be implemented by derived classes
    virtual void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                                      const String& discoveryPrefix,
                                      const JsonObject& device,
                                      const String& availabilityTopic) const {
        doc["name"] = name;
        doc["unique_id"] = getUniqueId(nodeId);
        doc["state_topic"] = getStateTopic(nodeId, discoveryPrefix);
        
        if (!icon.isEmpty()) {
            doc["icon"] = icon;
        }
        
        if (!deviceClass.isEmpty()) {
            doc["device_class"] = deviceClass;
        }
        
        // Add device info
        doc["device"] = device;
        
        // Add availability
        if (!availabilityTopic.isEmpty()) {
            doc["availability_topic"] = availabilityTopic;
            doc["payload_available"] = "online";
            doc["payload_not_available"] = "offline";
        }
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
