#pragma once

#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/MQTT.h>  // Only for event structures
#include <DomoticsCore/Events.h>
#include "HAEntity.h"
#include "HASensor.h"
#include "HABinarySensor.h"
#include "HASwitch.h"
#include "HALight.h"
#include "HAButton.h"
#include <vector>
#include <memory>

namespace DomoticsCore {
namespace Components {
namespace HomeAssistant {

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
    String nodeId = "esp32";                        // Unique device ID (derived from deviceName)
    String deviceName = "ESP32 Device";             // Device display name (from SystemConfig.deviceName)
    String manufacturer = "DomoticsCore";           // Manufacturer name (from SystemConfig.manufacturer)
    String model = "ESP32";                         // Hardware model (from SystemConfig.model, auto-detected via ESP.getChipModel())
    String swVersion = "1.0.0";                     // Firmware version (from SystemConfig.firmwareVersion)
    
    // Home Assistant specific settings
    bool retainDiscovery = true;                    // Retain discovery messages
    String discoveryPrefix = "homeassistant";       // MQTT discovery prefix
    String availabilityTopic = "";                  // Auto-generated if empty
    String configUrl = "";                          // Device configuration URL
    String suggestedArea = "";                      // Suggested area in HA
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
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "Home Assistant MQTT Discovery integration";
        if (this->config.availabilityTopic.isEmpty()) {
            this->config.availabilityTopic = this->config.discoveryPrefix + "/" + 
                                            this->config.nodeId + "/availability";
        }
    }
    
    ~HomeAssistantComponent() override = default;
    
    // IComponent interface
    ComponentStatus begin() override {
        DLOG_I(LOG_HA, "Initializing Home Assistant integration");
        DLOG_I(LOG_HA, "Node ID: %s", config.nodeId.c_str());
        DLOG_I(LOG_HA, "Discovery prefix: %s", config.discoveryPrefix.c_str());
        
        // Subscribe to MQTT events via EventBus
        on<bool>(DomoticsCore::Events::EVENT_MQTT_CONNECTED, [this](const bool&) {
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
        
        on<bool>(DomoticsCore::Events::EVENT_MQTT_DISCONNECTED, [this](const bool&) {
            DLOG_W(LOG_HA, "MQTT disconnected (via EventBus)");
            mqttConnected = false;
        });
        
        // Subscribe to incoming MQTT messages
        on<DomoticsCore::Components::MQTTMessageEvent>(DomoticsCore::Events::EVENT_MQTT_MESSAGE, [this](const DomoticsCore::Components::MQTTMessageEvent& ev) {
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
        emit(DomoticsCore::Events::EVENT_HA_ENTITY_ADDED, id);
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
        if (mqttConnected) {
            republishEntity(id);
        }
    }
    
    /**
     * @brief Add a switch entity
     */
    void addSwitch(const String& id, const String& name,
                   std::function<void(bool)> commandCallback, const String& icon = "") {
        auto sw = std::make_unique<HASwitch>(id, name, commandCallback, icon);
        entities.push_back(std::move(sw));
        stats.entityCount++;
        DLOG_I(LOG_HA, "Added switch: %s", id.c_str());
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
        if (mqttConnected) {
            republishEntity(id);
        }
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
        String topic = entity->getStateTopic(config.nodeId, config.discoveryPrefix);
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
        String topic = entity->getStateTopic(config.nodeId, config.discoveryPrefix);
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
        String topic = entity->getAttributesTopic(config.nodeId, config.discoveryPrefix);
        mqttPublish(topic, payload, 0, true);
    }
    
    // ========== Availability ==========
    
    /**
     * @brief Set device availability status
     */
    void setAvailable(bool available) {
        String payload = available ? "online" : "offline";
        DLOG_I(LOG_HA, "Publishing availability:");
        DLOG_I(LOG_HA, "  Topic: %s", config.availabilityTopic.c_str());
        DLOG_I(LOG_HA, "  Payload: %s", payload.c_str());
        
        bool published = mqttPublish(config.availabilityTopic, payload, 0, true);
        if (published) {
            DLOG_I(LOG_HA, "  ✓ Availability published");
        } else {
            DLOG_E(LOG_HA, "  ✗ Failed to publish availability!");
        }
    }
    
    // ========== Discovery ==========
    
    /**
     * @brief Publish discovery messages for all entities
     */
    void publishDiscovery() {
        DLOG_I(LOG_HA, "Publishing discovery for %d entities", entities.size());
        
        // Build device info once
        JsonDocument deviceDoc;
        JsonObject device = deviceDoc.to<JsonObject>();
        buildDeviceInfo(device);
        
        for (const auto& entity : entities) {
            publishEntityDiscovery(entity.get(), device);
        }
        
        stats.discoveryCount++;
        
        // Emit event for monitoring
        emit(DomoticsCore::Events::EVENT_HA_DISCOVERY_PUBLISHED, (int)entities.size());
    }
    
    /**
     * @brief Remove discovery messages (makes entities disappear from HA)
     */
    void removeDiscovery() {
        DLOG_I(LOG_HA, "Removing discovery for all entities");
        
        for (const auto& entity : entities) {
            String topic = entity->getDiscoveryTopic(config.nodeId, config.discoveryPrefix);
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
        if (config.availabilityTopic.isEmpty()) {
            config.availabilityTopic = config.discoveryPrefix + "/" + config.nodeId + "/availability";
        }
    }
    
    /**
     * @brief Get current HomeAssistant configuration
     * @return Current HAConfig
     */
    const HAConfig& getConfig() const { return config; }
    
    void setDeviceInfo(const String& name, const String& model,
                       const String& manufacturer, const String& swVersion) {
        config.deviceName = name;
        config.model = model;
        config.manufacturer = manufacturer;
        config.swVersion = swVersion;
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
    bool mqttPublish(const String& topic, const String& payload, uint8_t qos = 0, bool retain = false) {
        DomoticsCore::Components::MQTTPublishEvent ev{};
        strncpy(ev.topic, topic.c_str(), sizeof(ev.topic) - 1);
        ev.topic[sizeof(ev.topic) - 1] = '\0';
        strncpy(ev.payload, payload.c_str(), sizeof(ev.payload) - 1);
        ev.payload[sizeof(ev.payload) - 1] = '\0';
        ev.qos = qos;
        ev.retain = retain;
        emit(DomoticsCore::Events::EVENT_MQTT_PUBLISH, ev);
        return true;  // EventBus emit is fire-and-forget
    }
    
    /**
     * @brief Build device information JSON
     */
    void buildDeviceInfo(JsonObject& device) {
        JsonArray identifiers = device["identifiers"].to<JsonArray>();
        identifiers.add(config.nodeId);
        
        device["name"] = config.deviceName;
        device["model"] = config.model;
        device["manufacturer"] = config.manufacturer;
        device["sw_version"] = config.swVersion;
        
        if (!config.configUrl.isEmpty()) {
            device["configuration_url"] = config.configUrl;
        }
        
        if (!config.suggestedArea.isEmpty()) {
            device["suggested_area"] = config.suggestedArea;
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
        String topic = entity->getDiscoveryTopic(config.nodeId, config.discoveryPrefix);
        
        DLOG_I(LOG_HA, "Publishing discovery for '%s':", entity->id.c_str());
        DLOG_I(LOG_HA, "  Topic: %s", topic.c_str());
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
        String commandTopic = config.discoveryPrefix + "/+/" + config.nodeId + "/+/set";
        
        DomoticsCore::Components::MQTTSubscribeEvent ev{};
        strncpy(ev.topic, commandTopic.c_str(), sizeof(ev.topic) - 1);
        ev.topic[sizeof(ev.topic) - 1] = '\0';
        ev.qos = 0;
        emit(DomoticsCore::Events::EVENT_MQTT_SUBSCRIBE, ev);
        
        DLOG_D(LOG_HA, "Subscribed to commands via EventBus: %s", commandTopic.c_str());
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
            if (!sw->optimistic) {
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
        }
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
