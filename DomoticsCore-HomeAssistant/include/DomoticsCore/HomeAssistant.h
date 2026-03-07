#pragma once

/**
 * @file HomeAssistant.h
 * @brief Home Assistant MQTT Discovery component.
 * 
 * @example DomoticsCore-HomeAssistant/examples/BasicHA/src/main.cpp
 * @example DomoticsCore-HomeAssistant/examples/HAWithWebUI/src/main.cpp
 */

#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/MQTT.h>  // For event structures (MQTTPublishEvent, MQTTSubscribeEvent, MQTTMessageEvent)
#include <DomoticsCore/MQTTEvents.h>  // For MQTT event names
#include "HAEvents.h"  // For HA event names
#include "HAEntity.h"
#include "HASensor.h"
#include "HABinarySensor.h"
#include "HASwitch.h"
#include "HALight.h"
#include "HAButton.h"
#include "HAAlarmControlPanel.h"
#include <vector>
#include <memory>

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

namespace HA {
constexpr size_t MAX_NODE_ID         = 33;   // 32 chars + null (MQTT client ID limit)
constexpr size_t MAX_DEVICE_NAME     = 65;   // 64 chars + null (HA device registry)
constexpr size_t MAX_MANUFACTURER    = 33;   // 32 chars + null
constexpr size_t MAX_MODEL           = 33;   // 32 chars + null
constexpr size_t MAX_SW_VERSION      = 17;   // 16 chars + null (semver with pre-release)
constexpr size_t MAX_DISCOVERY_PREFIX = 33;   // 32 chars + null
constexpr size_t MAX_AVAIL_TOPIC     = 129;  // 128 chars + null (generated topic)
constexpr size_t MAX_CONFIG_URL      = 129;  // 128 chars + null (http://IP:port)
constexpr size_t MAX_SUGGESTED_AREA  = 33;   // 32 chars + null

inline void setField(char* dest, const char* src, size_t maxLen) {
    if (!src) { dest[0] = '\0'; return; }
    size_t srcLen = strlen(src);
    if (srcLen >= maxLen) {
        DLOG_W("HA", "Field truncated: '%.*s...' (max %zu)", (int)(maxLen - 1), src, maxLen - 1);
    }
    strncpy(dest, src, maxLen - 1);
    dest[maxLen - 1] = '\0';
}
} // namespace HA

/**
 * @brief Event for entity added to Home Assistant
 */
struct HAEntityAddedEvent {
    char id[64];           // Entity ID
    char component[32];    // Component type (sensor, switch, etc.)
};

/**
 * @brief Configuration for Home Assistant component
 */
struct HAConfig {
    // Device identity (populated by System.h from SystemConfig)
    char nodeId[HA::MAX_NODE_ID];
    char deviceName[HA::MAX_DEVICE_NAME];
    char manufacturer[HA::MAX_MANUFACTURER];
    char model[HA::MAX_MODEL];
    char swVersion[HA::MAX_SW_VERSION];

    // Home Assistant specific settings
    bool retainDiscovery = true;
    char discoveryPrefix[HA::MAX_DISCOVERY_PREFIX];
    char availabilityTopic[HA::MAX_AVAIL_TOPIC];
    char configUrl[HA::MAX_CONFIG_URL];
    char suggestedArea[HA::MAX_SUGGESTED_AREA];

    HAConfig() : retainDiscovery(true) {
        HA::setField(nodeId, "myDeviceId", sizeof(nodeId));
        HA::setField(deviceName, "My Device", sizeof(deviceName));
        HA::setField(manufacturer, "DomoticsCore", sizeof(manufacturer));
        HA::setField(model, "MyDeviceModel", sizeof(model));
        HA::setField(swVersion, "1.0.0", sizeof(swVersion));
        HA::setField(discoveryPrefix, "homeassistant", sizeof(discoveryPrefix));
        availabilityTopic[0] = '\0';
        configUrl[0] = '\0';
        suggestedArea[0] = '\0';
    }
};

/**
 * @brief Home Assistant MQTT Discovery Component
 * 
 * Provides automatic entity registration and state management for Home Assistant.
 * Supports sensors, switches, lights, buttons, and more via MQTT discovery protocol.
 */
class HomeAssistantComponent : public IComponent {
public:
    /**
     * @brief Construct HomeAssistant component
     * @param config HA configuration
     */
    HomeAssistantComponent(const HAConfig& config = HAConfig())
        : config(config) {
        // Initialize component metadata immediately for dependency resolution
        metadata.name = "HomeAssistant";
        metadata.version = "1.6.1";
        metadata.author = "DomoticsCore";
        metadata.description = "Home Assistant MQTT Discovery integration";
        if (this->config.availabilityTopic[0] == '\0') {
            int written = snprintf(this->config.availabilityTopic, HA::MAX_AVAIL_TOPIC,
                                   "%s/%s/availability", this->config.discoveryPrefix, this->config.nodeId);
            if (written >= (int)HA::MAX_AVAIL_TOPIC) {
                DLOG_W("HA", "availabilityTopic truncated (%d chars, max %zu)",
                       written, HA::MAX_AVAIL_TOPIC - 1);
            }
        }
    }
    
    ~HomeAssistantComponent() override = default;
    
    // IComponent interface
    ComponentStatus begin() override {
        DLOG_I(LOG_HA, "Initializing Home Assistant integration");
        DLOG_I(LOG_HA, "Node ID: %s", config.nodeId);
        DLOG_I(LOG_HA, "Discovery prefix: %s", config.discoveryPrefix);
        
        // Subscribe to MQTT events via EventBus
        on<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, [this](const bool&) {
            DLOG_I(LOG_HA, "MQTT connected (via EventBus), publishing availability");
            mqttConnected = true;
            setAvailable(true);
            subscribeToCommands();
            
            if (stats.entityCount > 0) {
                DLOG_I(LOG_HA, "Publishing HA discovery after MQTT connect");
                publishDiscovery();
            } else {
                DLOG_W(LOG_HA, "No entities registered yet; skipping discovery on connect");
            }
        });
        
        on<bool>(DomoticsCore::MQTTEvents::EVENT_DISCONNECTED, [this](const bool&) {
            DLOG_W(LOG_HA, "MQTT disconnected (via EventBus)");
            mqttConnected = false;
        });
        
        // Subscribe to incoming MQTT messages
        on<DomoticsCore::Components::MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, [this](const DomoticsCore::Components::MQTTMessageEvent& ev) {
            handleCommand(String(ev.topic), String(ev.payload));
        });
        
        // Note: Initial MQTT state will be signaled via mqtt/connected event
        
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // Nothing to do in loop - all communication via EventBus
    }
    
    /**
     * @brief Check if component is ready (MQTT connected and availability published)
     * @return true if ready for state publishing
     */
    bool isReady() const {
        return availabilityPublished && mqttConnected;
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_HA, "Shutting down");
        setAvailable(false);
        removeDiscovery();
        return ComponentStatus::Success;
    }
    
    // ========== Entity Management ==========
    
    /**
     * @brief Add a sensor entity
     */
    void addSensor(const String& id, const String& name, const String& unit = "", 
                   const String& deviceClass = "", const String& icon = "", const String& stateClass = "") {
        auto sensor = std::make_unique<HASensor>(id, name, unit, deviceClass, icon);
        if (!stateClass.isEmpty()) {
            sensor->stateClass = stateClass;
        }
        entities.push_back(std::move(sensor));
        stats.entityCount++;
        DLOG_I(LOG_HA, "Added sensor: %s", id.c_str());
        {
            HAEntityAddedEvent ev{};
            snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
            snprintf(ev.component, sizeof(ev.component), "sensor");
            emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
        }
        if (mqttConnected) {
            republishEntity(id);
        }
    }
    
    /**
     * @brief Add a binary sensor entity
     */
    void addBinarySensor(const String& id, const String& name,
                         const String& deviceClass = "", const String& icon = "") {
        auto sensor = std::make_unique<HABinarySensor>(id, name, deviceClass, icon);
        entities.push_back(std::move(sensor));
        stats.entityCount++;
        DLOG_I(LOG_HA, "Added binary sensor: %s", id.c_str());
        {
            HAEntityAddedEvent ev{};
            snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
            snprintf(ev.component, sizeof(ev.component), "binary_sensor");
            emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
        }
        if (mqttConnected) {
            republishEntity(id);
        }
    }

    /**
     * @brief Add a switch entity
     */
    void addSwitch(const String& id, const String& name,
                   std::function<void(bool)> commandCallback, const String& icon = "",
                   bool autoPublishState = true, bool optimistic = false) {
        auto sw = std::make_unique<HASwitch>(id, name, commandCallback, icon);
        sw->autoPublishState = autoPublishState;
        sw->optimistic = optimistic;
        entities.push_back(std::move(sw));
        stats.entityCount++;
        DLOG_I(LOG_HA, "Added switch: %s", id.c_str());
        {
            HAEntityAddedEvent ev{};
            snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
            snprintf(ev.component, sizeof(ev.component), "switch");
            emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
        }
        if (mqttConnected) {
            republishEntity(id);
        }
    }

    /**
     * @brief Add a light entity
     */
    void addLight(const String& id, const String& name,
                  std::function<void(bool, uint8_t)> commandCallback) {
        auto light = std::make_unique<HALight>(id, name, commandCallback);
        entities.push_back(std::move(light));
        stats.entityCount++;
        DLOG_I(LOG_HA, "Added light: %s", id.c_str());
        {
            HAEntityAddedEvent ev{};
            snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
            snprintf(ev.component, sizeof(ev.component), "light");
            emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
        }
        if (mqttConnected) {
            republishEntity(id);
        }
    }

    /**
     * @brief Add a button entity
     */
    void addButton(const String& id, const String& name,
                   std::function<void()> pressCallback, const String& icon = "") {
        auto button = std::make_unique<HAButton>(id, name, pressCallback, icon);
        entities.push_back(std::move(button));
        stats.entityCount++;
        DLOG_I(LOG_HA, "Added button: %s", id.c_str());
        {
            HAEntityAddedEvent ev{};
            snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
            snprintf(ev.component, sizeof(ev.component), "button");
            emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
        }
        if (mqttConnected) {
            republishEntity(id);
        }
    }

    /**
     * @brief Add an alarm control panel entity
     */
    void addAlarmControlPanel(
        const String& id, const String& name,
        const std::function<void(const String& command, const String& code)>& commandCallback,
        const String& icon = "mdi:shield-home",
        AlarmFeature features = AlarmFeature::ArmAway,
        const String& code = "",
        bool codeArmRequired = false,
        bool codeDisarmRequired = false,
        bool codeTriggerRequired = false) {
        auto panel = std::make_unique<HAAlarmControlPanel>(id, name, commandCallback, icon);
        panel->supportedFeatures = features;
        panel->code = code;
        panel->codeArmRequired = codeArmRequired;
        panel->codeDisarmRequired = codeDisarmRequired;
        panel->codeTriggerRequired = codeTriggerRequired;
        entities.push_back(std::move(panel));
        stats.entityCount++;
        DLOG_I(LOG_HA, "Added alarm_control_panel: %s", id.c_str());
        {
            HAEntityAddedEvent ev{};
            snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
            snprintf(ev.component, sizeof(ev.component), "alarm_control_panel");
            emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
        }
        if (mqttConnected) { republishEntity(id); }
    }

    // ========== State Publishing ==========

    /**
     * @brief Publish entity state (string) - INTERNAL IMPLEMENTATION
     */
    void publishState(const String& id, const String& state) {
        HAEntity* entity = findEntity(id);
        if (!entity) {
            DLOG_W(LOG_HA, "Entity not found: %s", id.c_str());
            return;
        }
        
        if (!mqttConnected) {
            DLOG_D(LOG_HA, "MQTT not connected, skipping publish for: %s", id.c_str());
            return;
        }
        
        // Set guard before MQTT publish to prevent re-entrant callbacks
        publishing = true;
        char topic[HA_TOPIC_BUF_SIZE];
        entity->getStateTopic(topic, sizeof(topic), config.nodeId, config.discoveryPrefix);
        DLOG_D(LOG_HA, "Publishing state: %s = %s", id.c_str(), state.c_str());
        mqttPublish(topic, state, 0, entity->retained);
        stats.stateUpdates++;
        publishing = false;
    }
    
    /**
     * @brief Publish entity state (numeric)
     */
    void publishState(const String& id, float value) {
        publishState(id, String(value, 2));
    }
    
    /**
     * @brief Publish entity state (boolean)
     */
    void publishState(const String& id, bool state) {
        publishState(id, String(state ? "ON" : "OFF"));
    }

    /**
     * @brief Publish entity state (const char*) — prevents implicit bool conversion
     */
    void publishState(const String& id, const char* state) {
        publishState(id, String(state));
    }

    /**
     * @brief Publish entity state with JSON (for lights with brightness)
     */
    void publishStateJson(const String& id, const JsonDocument& doc) {
        HAEntity* entity = findEntity(id);
        if (!entity) return;
        
        if (!mqttConnected) {
            DLOG_D(LOG_HA, "MQTT not connected, skipping JSON publish");
            return;
        }
        
        // Set guard before MQTT publish to prevent re-entrant callbacks
        publishing = true;
        String payload;
        serializeJson(doc, payload);
        char topic[HA_TOPIC_BUF_SIZE];
        entity->getStateTopic(topic, sizeof(topic), config.nodeId, config.discoveryPrefix);
        mqttPublish(topic, payload, 0, entity->retained);
        stats.stateUpdates++;
        publishing = false;
    }
    
    /**
     * @brief Publish entity attributes (additional metadata)
     */
    void publishAttributes(const String& id, const JsonDocument& attributes) {
        HAEntity* entity = findEntity(id);
        if (!entity) return;
        
        String payload;
        serializeJson(attributes, payload);
        char topic[HA_TOPIC_BUF_SIZE];
        entity->getAttributesTopic(topic, sizeof(topic), config.nodeId, config.discoveryPrefix);
        mqttPublish(topic, payload, 0, true);
    }
    
    // ========== Availability ==========
    
    /**
     * @brief Set device availability status
     */
    void setAvailable(bool available) {
        String payload = available ? "online" : "offline";
        DLOG_I(LOG_HA, "Publishing availability:");
        DLOG_I(LOG_HA, "  Topic: %s", config.availabilityTopic);
        DLOG_I(LOG_HA, "  Payload: %s", payload.c_str());
        
        bool published = mqttPublish(config.availabilityTopic, payload, 0, true);
        if (published) {
            DLOG_I(LOG_HA, "  ✓ Availability published");
            availabilityPublished = available;  // Fix: track availability state for isReady()
        } else {
            DLOG_E(LOG_HA, "  ✗ Failed to publish availability!");
        }
    }
    
    // ========== Discovery ==========
    
    /**
     * @brief Publish discovery messages for all entities
     */
    void publishDiscovery() {
        DLOG_I(LOG_HA, "Publishing discovery for %zu entities", entities.size());
        
        // Build device info once
        JsonDocument deviceDoc;
        JsonObject device = deviceDoc.to<JsonObject>();
        buildDeviceInfo(device);
        
        for (const auto& entity : entities) {
            publishEntityDiscovery(entity.get(), device);
        }
        
        stats.discoveryCount++;
        
        // Emit event for monitoring
        emit(DomoticsCore::HAEvents::EVENT_DISCOVERY_PUBLISHED, (int)entities.size());
    }
    
    /**
     * @brief Remove discovery messages (makes entities disappear from HA)
     */
    void removeDiscovery() {
        DLOG_I(LOG_HA, "Removing discovery for all entities");
        
        for (const auto& entity : entities) {
            char topic[HA_TOPIC_BUF_SIZE];
            entity->getDiscoveryTopic(topic, sizeof(topic), config.nodeId, config.discoveryPrefix);
            mqttPublish(topic, "", 0, config.retainDiscovery);  // Empty payload removes entity
        }
    }
    
    /**
     * @brief Republish single entity discovery
     */
    void republishEntity(const String& id) {
        HAEntity* entity = findEntity(id);
        if (!entity) return;
        
        JsonDocument deviceDoc;
        JsonObject device = deviceDoc.to<JsonObject>();
        buildDeviceInfo(device);
        
        publishEntityDiscovery(entity, device);
    }
    
    // ========== Configuration ==========
    
    void setConfig(const HAConfig& cfg) {
        config = cfg;
        if (config.availabilityTopic[0] == '\0') {
            int written = snprintf(config.availabilityTopic, HA::MAX_AVAIL_TOPIC,
                                   "%s/%s/availability", config.discoveryPrefix, config.nodeId);
            if (written >= (int)HA::MAX_AVAIL_TOPIC) {
                DLOG_W("HA", "availabilityTopic truncated (%d chars, max %zu)",
                       written, HA::MAX_AVAIL_TOPIC - 1);
            }
        }
    }
    
    /**
     * @brief Get current HomeAssistant configuration
     * @return Current HAConfig
     */
    const HAConfig& getConfig() const { return config; }
    
    void setDeviceInfo(const char* name, const char* model,
                       const char* manufacturer, const char* swVersion) {
        HA::setField(config.deviceName, name, sizeof(config.deviceName));
        HA::setField(config.model, model, sizeof(config.model));
        HA::setField(config.manufacturer, manufacturer, sizeof(config.manufacturer));
        HA::setField(config.swVersion, swVersion, sizeof(config.swVersion));
    }
    
    // ========== Statistics ==========
    
    struct HAStatistics {
        uint32_t entityCount = 0;
        uint32_t discoveryCount = 0;
        uint32_t stateUpdates = 0;
        uint32_t commandsReceived = 0;
    };
    
    const HAStatistics& getStatistics() const { return stats; }
    
    /**
     * @brief Check if MQTT is connected
     * @return true if MQTT connection is active
     */
    bool isMQTTConnected() const {
        return mqttConnected;
    }
    
private:
    HAConfig config;
    std::vector<std::unique_ptr<HAEntity>> entities;
    HAStatistics stats;
    volatile bool publishing = false;  // Re-entrancy guard (volatile to prevent optimization)
    bool availabilityPublished = false;  // Track if initial availability sent
    bool mqttConnected = false;  // Track MQTT connection state via EventBus
    char commandTopicFilter[HA_TOPIC_BUF_SIZE] = {};  // Stored to keep pointer valid for EventBus
    
    /**
     * @brief Find entity by ID
     */
    HAEntity* findEntity(const String& id) {
        for (const auto& entity : entities) {
            if (entity->id == id) {
                return entity.get();
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Publish MQTT message via EventBus
     */
    bool mqttPublish(const char* topic, const String& payload, uint8_t qos = 0, bool retain = false) {
        using namespace DomoticsCore::Components;
        MQTTPublishEvent ev{};

        // Copy strings into fixed-size buffers
        strncpy(ev.topic, topic, MQTT_EVENT_TOPIC_SIZE - 1);
        ev.topic[MQTT_EVENT_TOPIC_SIZE - 1] = '\0';
        strncpy(ev.payload, payload.c_str(), MQTT_EVENT_PAYLOAD_SIZE - 1);
        ev.payload[MQTT_EVENT_PAYLOAD_SIZE - 1] = '\0';
        ev.qos = qos;
        ev.retain = retain;

        emit(DomoticsCore::MQTTEvents::EVENT_PUBLISH, ev);
        return true;
    }
    
    /**
     * @brief Build device information JSON
     */
    void buildDeviceInfo(JsonObject& device) {
        JsonArray identifiers = device["identifiers"].to<JsonArray>();
        identifiers.add((const char*)config.nodeId);

        device["name"] = (const char*)config.deviceName;
        device["model"] = (const char*)config.model;
        device["manufacturer"] = (const char*)config.manufacturer;
        device["sw_version"] = (const char*)config.swVersion;

        if (config.configUrl[0] != '\0') {
            device["configuration_url"] = (const char*)config.configUrl;
        }

        if (config.suggestedArea[0] != '\0') {
            device["suggested_area"] = (const char*)config.suggestedArea;
        }
    }
    
    /**
     * @brief Publish discovery message for a single entity
     */
    void publishEntityDiscovery(HAEntity* entity, const JsonObject& device) {
        JsonDocument doc;
        entity->buildDiscoveryPayload(doc, config.nodeId, config.discoveryPrefix, 
                                     device, config.availabilityTopic);
        
        String payload;
        serializeJson(doc, payload);
        char topic[HA_TOPIC_BUF_SIZE];
        entity->getDiscoveryTopic(topic, sizeof(topic), config.nodeId, config.discoveryPrefix);

        DLOG_I(LOG_HA, "Publishing discovery for '%s':", entity->id.c_str());
        DLOG_I(LOG_HA, "  Topic: %s", topic);
        DLOG_I(LOG_HA, "  Payload size: %d bytes", payload.length());
        DLOG_D(LOG_HA, "  Payload: %s", payload.c_str());
        
        bool published = mqttPublish(topic, payload, 0, config.retainDiscovery);
        if (published) {
            DLOG_I(LOG_HA, "  ✓ Published successfully");
        } else {
            DLOG_E(LOG_HA, "  ✗ Failed to publish!");
        }
    }
    
    /**
     * @brief Subscribe to all command topics
     */
    void subscribeToCommands() {
        // Subscribe to wildcard command topics for all entity types via EventBus
        snprintf(commandTopicFilter, sizeof(commandTopicFilter), "%s/+/%s/+/set",
                 config.discoveryPrefix, config.nodeId);

        using namespace DomoticsCore::Components;
        MQTTSubscribeEvent ev{};
        strncpy(ev.topic, commandTopicFilter, MQTT_EVENT_TOPIC_SIZE - 1);
        ev.topic[MQTT_EVENT_TOPIC_SIZE - 1] = '\0';
        ev.qos = 0;

        emit(DomoticsCore::MQTTEvents::EVENT_SUBSCRIBE, ev);

        DLOG_D(LOG_HA, "Subscribed to commands via EventBus: %s", commandTopicFilter);
        // Message handling is done via "mqtt/message" event listener in begin()
    }
    
    /**
     * @brief Handle incoming command
     */
    void handleCommand(const String& topic, const String& payload) {
        DLOG_I(LOG_HA, "Received MQTT command - Topic: %s, Payload: %s", topic.c_str(), payload.c_str());
        
        // Extract entity ID from topic
        // Format: homeassistant/{component}/{node_id}/{entity_id}/set
        int lastSlash = topic.lastIndexOf('/');
        if (lastSlash == -1) {
            DLOG_E(LOG_HA, "Invalid topic format - no trailing slash");
            return;
        }
        
        int secondLastSlash = topic.lastIndexOf('/', lastSlash - 1);
        if (secondLastSlash == -1) {
            DLOG_E(LOG_HA, "Invalid topic format - missing entity ID");
            return;
        }
        
        String entityId = topic.substring(secondLastSlash + 1, lastSlash);
        
        DLOG_I(LOG_HA, "Extracted entity ID: '%s', looking up entity...", entityId.c_str());
        HAEntity* entity = findEntity(entityId);
        if (!entity) {
            DLOG_W(LOG_HA, "Command for unknown entity: %s", entityId.c_str());
            return;
        }
        
        stats.commandsReceived++;
        DLOG_D(LOG_HA, "Command for %s: %s", entityId.c_str(), payload.c_str());
        
        // Route command to appropriate entity type and auto-publish state
        if (entity->component == "switch") {
            HASwitch* sw = static_cast<HASwitch*>(entity);
            sw->handleCommand(payload);
            
            // Auto-publish state after command execution
            // This ensures HA immediately sees the state change
            if (!sw->optimistic && sw->autoPublishState) {
                publishState(entityId, payload);
                DLOG_D(LOG_HA, "Auto-published switch state: %s = %s", entityId.c_str(), payload.c_str());
            }
        } else if (entity->component == "light") {
            HALight* light = static_cast<HALight*>(entity);
            light->handleCommand(payload);
            // TODO: Auto-publish light state (needs JSON state)
        } else if (entity->component == "button") {
            HAButton* button = static_cast<HAButton*>(entity);
            button->handleCommand(payload);
            // Buttons don't have state
        } else if (entity->component == "alarm_control_panel") {
            entity->handleCommand(payload);
            // No auto-publish: consumer manages alarm state
        } else {
            DLOG_W(LOG_HA, "No command handler for component type: %s", entity->component.c_str());
        }
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
