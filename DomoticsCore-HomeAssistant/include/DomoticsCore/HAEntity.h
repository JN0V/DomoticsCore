#pragma once

#include <ArduinoJson.h>

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
static constexpr size_t HA_TOPIC_BUF_SIZE = 128;

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

    // Discovery fields emitted only when set (OBS-5). A shared state topic with
    // a value_template lets several entities read one JSON payload; an entity
    // that must stay readable while the device is down opts out of availability.
    String entityCategory;      // "diagnostic" or "config"
    String stateTopicOverride;  // replaces the generated state topic
    String valueTemplate;       // applied to the state topic's payload
    String jsonAttributesTopic; // a JSON payload that becomes the entity's attributes
    bool useAvailability = true;
    
    // Topic generation (zero-heap: snprintf into caller-provided buffer)
    void getDiscoveryTopic(char* buf, size_t len, const char* nodeId, const char* discoveryPrefix = "homeassistant") const {
        buildTopic(buf, len, discoveryPrefix, nodeId, "config");
    }

    // The override wins here too, so a state published through the component
    // lands where the discovery config told Home Assistant to read.
    void getStateTopic(char* buf, size_t len, const char* nodeId, const char* discoveryPrefix = "homeassistant") const {
        if (!stateTopicOverride.isEmpty()) { snprintf(buf, len, "%s", stateTopicOverride.c_str()); return; }
        buildTopic(buf, len, discoveryPrefix, nodeId, "state");
    }

    void getCommandTopic(char* buf, size_t len, const char* nodeId, const char* discoveryPrefix = "homeassistant") const {
        buildTopic(buf, len, discoveryPrefix, nodeId, "set");
    }

    void getAttributesTopic(char* buf, size_t len, const char* nodeId, const char* discoveryPrefix = "homeassistant") const {
        if (!jsonAttributesTopic.isEmpty()) { snprintf(buf, len, "%s", jsonAttributesTopic.c_str()); return; }
        buildTopic(buf, len, discoveryPrefix, nodeId, "attributes");
    }

    /** @brief Home Assistant accepts two categories; anything else makes it reject the whole config. */
    bool entityCategoryIsValid() const {
        return entityCategory.isEmpty() || entityCategory == "diagnostic" || entityCategory == "config";
    }

    // Unique ID for HA (zero-heap)
    void getUniqueId(char* buf, size_t len, const char* nodeId) const {
        snprintf(buf, len, "%s_%s", nodeId, id.c_str());
    }
    
    // Discovery payload - to be implemented by derived classes
    virtual void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                                      const String& discoveryPrefix,
                                      const JsonObject& device,
                                      const String& availabilityTopic) const {
        char buf[HA_TOPIC_BUF_SIZE];
        doc["name"] = name;
        getUniqueId(buf, sizeof(buf), nodeId.c_str());
        doc["unique_id"] = buf;
        getStateTopic(buf, sizeof(buf), nodeId.c_str(), discoveryPrefix.c_str());   // the override, when set
        doc["state_topic"] = buf;
        
        if (!icon.isEmpty()) {
            doc["icon"] = icon;
        }
        
        if (!deviceClass.isEmpty()) {
            doc["device_class"] = deviceClass;
        }
        
        // Add device info
        doc["device"] = device;
        
        // Add availability
        if (useAvailability && !availabilityTopic.isEmpty()) {
            doc["availability_topic"] = availabilityTopic;
            doc["payload_available"] = "online";
            doc["payload_not_available"] = "offline";
        }

        if (!entityCategory.isEmpty() && entityCategoryIsValid()) doc["entity_category"] = entityCategory;
        if (!valueTemplate.isEmpty()) doc["value_template"] = valueTemplate;
        if (!jsonAttributesTopic.isEmpty()) doc["json_attributes_topic"] = jsonAttributesTopic;
    }

    /**
     * @brief Handle command from Home Assistant (virtual dispatch)
     * @param payload Raw MQTT command payload
     * @return true if command was valid and processed, false to skip EventBus emission
     */
    virtual bool handleCommand(const String& payload) { return true; }

protected:
    void buildTopic(char* buf, size_t len, const char* discoveryPrefix, const char* nodeId, const char* suffix) const {
        snprintf(buf, len, "%s/%s/%s/%s/%s", discoveryPrefix, component.c_str(), nodeId, id.c_str(), suffix);
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
