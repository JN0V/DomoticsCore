#pragma once

#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/MQTT.h>
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
 * @brief Configuration for Home Assistant component
 */
struct HAConfig {
    String nodeId = "esp32";            // Unique device ID
    String deviceName = "ESP32 Device";
    String manufacturer = "DomoticsCore";
    String model = "ESP32";
    String swVersion = "1.0.0";
    bool retainDiscovery = true;
    String discoveryPrefix = "homeassistant";
    String availabilityTopic = "";      // Auto-generated if empty
    String configUrl = "";              // Device configuration URL
    String suggestedArea = "";          // Suggested area in HA
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
     * @param mqtt MQTT component for communication
     * @param config HA configuration
     */
    HomeAssistantComponent(MQTTComponent* mqtt, const HAConfig& config = HAConfig())
        : mqtt(mqtt), config(config) {
        if (this->config.availabilityTopic.isEmpty()) {
            this->config.availabilityTopic = this->config.discoveryPrefix + "/" + 
                                            this->config.nodeId + "/availability";
        }
    }
    
    ~HomeAssistantComponent() override = default;
    
    // IComponent interface
    String getName() const override { return "HomeAssistant"; }
    String getVersion() const override { return "1.0.0"; }
    
    ComponentStatus begin() override {
        if (!mqtt) {
            DLOG_E(LOG_HA, "MQTT component not provided");
            return ComponentStatus::DependencyError;
        }
        
        DLOG_I(LOG_HA, "Initializing Home Assistant integration");
        DLOG_I(LOG_HA, "Node ID: %s", config.nodeId.c_str());
        DLOG_I(LOG_HA, "Discovery prefix: %s", config.discoveryPrefix.c_str());
        
        // Subscribe to command topics when MQTT connects
        mqtt->onConnect([this]() {
            DLOG_I(LOG_HA, "MQTT connected, publishing availability and discovery");
            setAvailable(true);
            publishDiscovery();
            subscribeToCommands();
        });
        
        // Handle disconnect
        mqtt->onDisconnect([this]() {
            DLOG_W(LOG_HA, "MQTT disconnected");
        });
        
        // If MQTT is already connected, publish discovery immediately
        if (mqtt->isConnected()) {
            DLOG_I(LOG_HA, "MQTT already connected, publishing discovery now");
            setAvailable(true);
            publishDiscovery();
            subscribeToCommands();
        }
        
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // Publish initial availability once MQTT is connected
        if (!availabilityPublished && mqtt && mqtt->isConnected()) {
            setAvailable(true);
            availabilityPublished = true;
        }
    }
    
    /**
     * @brief Check if component is ready (MQTT connected and availability published)
     * @return true if ready for state publishing
     */
    bool isReady() const {
        return availabilityPublished && mqtt && mqtt->isConnected();
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
                   const String& deviceClass = "", const String& icon = "") {
        auto sensor = std::make_unique<HASensor>(id, name, unit, deviceClass, icon);
        entities.push_back(std::move(sensor));
        stats.entityCount++;
        DLOG_I(LOG_HA, "Added sensor: %s", id.c_str());
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
        
        if (!mqtt || !mqtt->isConnected()) {
            DLOG_D(LOG_HA, "MQTT not connected, skipping publish for: %s", id.c_str());
            return;
        }
        
        // Set guard before MQTT publish to prevent re-entrant callbacks
        publishing = true;
        String topic = entity->getStateTopic(config.nodeId, config.discoveryPrefix);
        mqtt->publish(topic, state, entity->retained);
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
        
        if (!mqtt || !mqtt->isConnected()) {
            DLOG_D(LOG_HA, "MQTT not connected, skipping JSON publish");
            return;
        }
        
        // Set guard before MQTT publish to prevent re-entrant callbacks
        publishing = true;
        String payload;
        serializeJson(doc, payload);
        String topic = entity->getStateTopic(config.nodeId, config.discoveryPrefix);
        mqtt->publish(topic, payload, entity->retained);
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
        mqtt->publish(topic, payload, true);
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
        
        bool published = mqtt->publish(config.availabilityTopic, payload, true);
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
    }
    
    /**
     * @brief Remove discovery messages (makes entities disappear from HA)
     */
    void removeDiscovery() {
        DLOG_I(LOG_HA, "Removing discovery for all entities");
        
        for (const auto& entity : entities) {
            String topic = entity->getDiscoveryTopic(config.nodeId, config.discoveryPrefix);
            mqtt->publish(topic, "", config.retainDiscovery);  // Empty payload removes entity
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
    
    const HAConfig& getHAConfig() const { return config; }
    
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
        return mqtt && mqtt->isConnected();
    }
    
private:
    MQTTComponent* mqtt;
    HAConfig config;
    std::vector<std::unique_ptr<HAEntity>> entities;
    HAStatistics stats;
    volatile bool publishing = false;  // Re-entrancy guard (volatile to prevent optimization)
    bool availabilityPublished = false;  // Track if initial availability sent
    
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
        
        bool published = mqtt->publish(topic, payload, config.retainDiscovery);
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
        // Subscribe to wildcard command topics for all entity types
        String commandTopic = config.discoveryPrefix + "/+/" + config.nodeId + "/+/set";
        mqtt->subscribe(commandTopic);
        
        // Register message handler for command topics
        mqtt->onMessage(commandTopic, [this](const String& topic, const String& payload) {
            // CRITICAL: Prevent re-entrant calls during publish
            // PubSubClient calls loop() during publish(), which triggers this callback
            if (publishing) {
                DLOG_W(LOG_HA, "Skipping command during publish to prevent recursion");
                return;
            }
            handleCommand(topic, payload);
        });
        
        DLOG_D(LOG_HA, "Subscribed to commands: %s", commandTopic.c_str());
    }
    
    /**
     * @brief Handle incoming command
     */
    void handleCommand(const String& topic, const String& payload) {
        // Extract entity ID from topic
        // Format: homeassistant/{component}/{node_id}/{entity_id}/set
        int lastSlash = topic.lastIndexOf('/');
        if (lastSlash == -1) return;
        
        String topicWithoutSet = topic.substring(0, lastSlash);
        int prevSlash = topicWithoutSet.lastIndexOf('/');
        if (prevSlash == -1) return;
        
        String entityId = topicWithoutSet.substring(prevSlash + 1);
        
        HAEntity* entity = findEntity(entityId);
        if (!entity) {
            DLOG_W(LOG_HA, "Command for unknown entity: %s", entityId.c_str());
            return;
        }
        
        stats.commandsReceived++;
        DLOG_D(LOG_HA, "Command for %s: %s", entityId.c_str(), payload.c_str());
        
        // Route command to appropriate entity type
        if (entity->component == "switch") {
            HASwitch* sw = static_cast<HASwitch*>(entity);
            sw->handleCommand(payload);
        } else if (entity->component == "light") {
            HALight* light = static_cast<HALight*>(entity);
            light->handleCommand(payload);
        } else if (entity->component == "button") {
            HAButton* button = static_cast<HAButton*>(entity);
            button->handleCommand(payload);
        }
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
