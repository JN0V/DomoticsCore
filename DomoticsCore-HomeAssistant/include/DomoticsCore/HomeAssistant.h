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
        metadata.version = "2.0.3";
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
        
        // Subscribe to incoming MQTT messages.
        //
        // MEM-2: ev.topic and ev.payload are already char[], and every message the
        // shared client receives arrives here — before findEntity has decided the
        // message concerns HomeAssistant at all. Wrapping them in Strings asked the
        // allocator for the topic on every single message (38 characters and up,
        // where both cores' small-string buffer stops at 14), to parse text the
        // component was handed as characters. They are passed through as they are.
        on<DomoticsCore::Components::MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, [this](const DomoticsCore::Components::MQTTMessageEvent& ev) {
            handleCommand(ev.topic, ev.payload);
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
    void addSwitch(const String& id, const String& name, const String& icon = "",
                   bool autoPublishState = true, bool optimistic = false) {
        auto sw = std::make_unique<HASwitch>(id, name, icon);
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
    void addLight(const String& id, const String& name) {
        auto light = std::make_unique<HALight>(id, name);
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
    void addButton(const String& id, const String& name, const String& icon = "") {
        auto button = std::make_unique<HAButton>(id, name, icon);
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
        const String& icon = "mdi:shield-home",
        AlarmFeature features = AlarmFeature::ArmAway,
        const String& code = "",
        bool codeArmRequired = false,
        bool codeDisarmRequired = false,
        bool codeTriggerRequired = false) {
        auto panel = std::make_unique<HAAlarmControlPanel>(id, name, icon);
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
        
        char topic[HA_TOPIC_BUF_SIZE];
        entity->getStateTopic(topic, sizeof(topic), config.nodeId, config.discoveryPrefix);
        DLOG_D(LOG_HA, "Publishing state: %s = %s", id.c_str(), state.c_str());
        mqttPublish(topic, state, 0, entity->retained);
        stats.stateUpdates++;
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
        
        String payload;
        serializeJson(doc, payload);
        char topic[HA_TOPIC_BUF_SIZE];
        entity->getStateTopic(topic, sizeof(topic), config.nodeId, config.discoveryPrefix);
        mqttPublish(topic, payload, 0, entity->retained);
        stats.stateUpdates++;
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
        
        mqttPublish(config.availabilityTopic, payload, 0, true);
        DLOG_I(LOG_HA, "  Availability published");
        availabilityPublished = available;
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
     * @brief Find entity by ID given as a range of characters
     *
     * MEM-2: the command path knows the id as a slice of the topic buffer, and
     * building a String from it to call the overload above is the allocation this
     * lot removes. The comparison is on the full extent deliberately — the id is
     * copied into the event's 64-byte field afterwards, and looking up the
     * truncated copy instead would stop matching an entity registered with a
     * longer id, which is a behaviour change and not a cost one.
     */
    HAEntity* findEntity(const char* id, size_t len) {
        for (const auto& entity : entities) {
            if ((size_t)entity->id.length() == len && strncmp(entity->id.c_str(), id, len) == 0) {
                return entity.get();
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Publish MQTT message via EventBus
     */
    void mqttPublish(const char* topic, const String& payload, uint8_t qos = 0, bool retain = false) {
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
        
        mqttPublish(topic, payload, 0, config.retainDiscovery);
        // Handed to the EventBus, not published: mqttPublish() emits, and MQTT
        // consumes it later. This component never learns the outcome, so saying
        // "published" claimed something it cannot know — and did so even while
        // the rate limiter was silently discarding these very messages (BUG-29).
        DLOG_I(LOG_HA, "  Discovery queued for publish");
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
     *
     * MEM-2: takes the topic and payload as the characters MQTT delivered them as.
     * Everything up to and including the entity lookup — the work done for every
     * message on the shared client, most of which are not ours — now runs without
     * touching the allocator. The one String that survives is the temporary bound
     * to HAEntity::handleCommand(const String&), built after the message has been
     * accepted; that virtual keeps its signature, so a subclass written against it
     * still compiles and still runs.
     *
     * Private, so the parameter change is not a change to any published API.
     */
    void handleCommand(const char* topic, const char* payload) {
        DLOG_I(LOG_HA, "Received MQTT command - Topic: %s, Payload: %s", topic, payload);

        // Extract entity ID from topic
        // Format: homeassistant/{component}/{node_id}/{entity_id}/set
        const char* lastSlash = strrchr(topic, '/');
        if (!lastSlash) {
            DLOG_E(LOG_HA, "Invalid topic format - no trailing slash");
            return;
        }

        // Walk back from the last slash rather than from the end: the id is the
        // segment between the last two. The loop stops at `topic` itself, so a
        // topic whose only slash is its first character falls through to the
        // missing-id branch, as it did before.
        const char* secondLastSlash = nullptr;
        for (const char* p = lastSlash; p != topic; ) {
            --p;
            if (*p == '/') { secondLastSlash = p; break; }
        }
        if (!secondLastSlash) {
            DLOG_E(LOG_HA, "Invalid topic format - missing entity ID");
            return;
        }

        const char* idStart = secondLastSlash + 1;
        const size_t idLen = (size_t)(lastSlash - idStart);

        // Sized to the event field it will fill, so the truncation point is the
        // one strncpy() applied here before — 63 characters plus the terminator.
        // idLen keeps the true length, which the logs and the warning below both
        // need: they report the id as it was delivered, not the copy that was
        // stored, which is what the String version did and what an operator
        // reading them has to be able to match against the broker's traffic.
        // Hence %.*s over idStart everywhere the id is printed.
        char entityId[sizeof(HAEvents::HACommandEvent::entityId)];
        const size_t idCopy = idLen < sizeof(entityId) ? idLen : sizeof(entityId) - 1;
        memcpy(entityId, idStart, idCopy);
        entityId[idCopy] = '\0';

        DLOG_I(LOG_HA, "Extracted entity ID: '%.*s', looking up entity...", (int)idLen, idStart);
        HAEntity* entity = findEntity(idStart, idLen);
        if (!entity) {
            DLOG_W(LOG_HA, "Command for unknown entity: %.*s", (int)idLen, idStart);
            return;
        }

        stats.commandsReceived++;
        DLOG_D(LOG_HA, "Command for %.*s: %s", (int)idLen, idStart, payload);

        // R24: Virtual dispatch — replaces static_cast chain
        // R26: handleCommand returns false if command is invalid (e.g., button with wrong payload, light with garbage)
        bool valid = entity->handleCommand(payload);
        if (!valid) return;

        // R26: Emit ha/command EventBus event
        HAEvents::HACommandEvent ev{};
        strncpy(ev.entityId, entityId, sizeof(ev.entityId) - 1);
        strncpy(ev.component, entity->component.c_str(), sizeof(ev.component) - 1);
        strncpy(ev.command, payload, sizeof(ev.command) - 1);

        // Log warning if entity ID or payload was truncated. idLen is already
        // known; the payload is measured once here rather than on the discard
        // path, where nothing needs its length.
        const size_t payloadLen = strlen(payload);
        if (idLen >= sizeof(ev.entityId)) {
            DLOG_W(LOG_HA, "Entity ID truncated: %.*s (%zu > %zu)",
                   (int)idLen, idStart, idLen, sizeof(ev.entityId) - 1);
        }
        if (payloadLen >= sizeof(ev.command)) {
            DLOG_W(LOG_HA, "Command payload truncated for entity %.*s (%zu > %zu)",
                   (int)idLen, idStart, payloadLen, sizeof(ev.command) - 1);
        }

        // Populate code field for alarm_control_panel
        // Note: overwrites ev.command with parsed command (e.g., "ARM_AWAY" instead of raw "ARM_AWAY 1234")
        if (entity->component == "alarm_control_panel") {
            auto* alarm = static_cast<HAAlarmControlPanel*>(entity);
            strncpy(ev.command, alarm->lastCommand, sizeof(ev.command) - 1);
            strncpy(ev.code, alarm->lastCode, sizeof(ev.code) - 1);
        }

        emit(HAEvents::EVENT_COMMAND, ev);

        // Auto-publish for switches (moved from old if/else chain)
        if (entity->component == "switch") {
            auto* sw = static_cast<HASwitch*>(entity);
            if (!sw->optimistic && sw->autoPublishState) {
                // entity->id rather than the local buffer, which saves building
                // a String for the id and nothing else: publishState looks the
                // entity up again by String and builds one for the state. This
                // is the accepted path, where the cost was never the claim.
                publishState(entity->id, payload);
            }
        }
    }
};

} // namespace HomeAssistant
} // namespace Components
} // namespace DomoticsCore
