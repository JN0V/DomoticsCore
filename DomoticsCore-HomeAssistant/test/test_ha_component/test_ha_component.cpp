/**
 * @file test_ha_component.cpp
 * @brief Native unit tests for HomeAssistant component
 *
 * Tests cover:
 * - Events (HAEvents)
 * - Component creation and configuration
 * - Config get/set
 * - Entity management (sensors, switches, buttons, lights)
 * - Statistics
 * - Lifecycle (begin, loop, shutdown)
 * - Non-blocking behavior
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/HAEvents.h>
#include <DomoticsCore/ArduinoJsonString.h>  // String converters for ArduinoJson 7

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

// ============================================================================
// Event Tests
// ============================================================================

void test_ha_events_constants_defined() {
    // Verify event constants are defined and have expected values
    TEST_ASSERT_NOT_NULL(HAEvents::EVENT_DISCOVERY_PUBLISHED);
    TEST_ASSERT_NOT_NULL(HAEvents::EVENT_ENTITY_ADDED);

    TEST_ASSERT_EQUAL_STRING("ha/discovery_published", HAEvents::EVENT_DISCOVERY_PUBLISHED);
    TEST_ASSERT_EQUAL_STRING("ha/entity_added", HAEvents::EVENT_ENTITY_ADDED);
}

// ============================================================================
// Component Creation Tests
// ============================================================================

void test_ha_component_creation_default() {
    HomeAssistantComponent ha;

    TEST_ASSERT_EQUAL_STRING("HomeAssistant", ha.metadata.name);
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", ha.metadata.author);
    TEST_ASSERT_EQUAL_STRING("1.6.1", ha.metadata.version);
}

void test_ha_component_creation_with_config() {
    HAConfig config;
    config.nodeId = "test_node";
    config.deviceName = "Test Device";
    config.manufacturer = "TestMfg";
    config.model = "TestModel";
    config.swVersion = "2.0.0";

    HomeAssistantComponent ha(config);

    TEST_ASSERT_EQUAL_STRING("HomeAssistant", ha.metadata.name);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("test_node", cfg.nodeId.c_str());
    TEST_ASSERT_EQUAL_STRING("Test Device", cfg.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("TestMfg", cfg.manufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("TestModel", cfg.model.c_str());
    TEST_ASSERT_EQUAL_STRING("2.0.0", cfg.swVersion.c_str());
}

// ============================================================================
// Config Tests
// ============================================================================

void test_ha_config_defaults() {
    HAConfig config;

    // Default values from HAConfig struct in HomeAssistant.h
    TEST_ASSERT_EQUAL_STRING("myDeviceId", config.nodeId.c_str());
    TEST_ASSERT_EQUAL_STRING("My Device", config.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.manufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("MyDeviceModel", config.model.c_str());
    TEST_ASSERT_EQUAL_STRING("1.0.0", config.swVersion.c_str());
    TEST_ASSERT_TRUE(config.retainDiscovery);
    TEST_ASSERT_EQUAL_STRING("homeassistant", config.discoveryPrefix.c_str());
}

void test_ha_config_get_set() {
    HomeAssistantComponent ha;

    HAConfig newConfig;
    newConfig.nodeId = "new_node";
    newConfig.deviceName = "New Device";
    newConfig.discoveryPrefix = "custom_prefix";
    newConfig.retainDiscovery = false;

    ha.setConfig(newConfig);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("new_node", cfg.nodeId.c_str());
    TEST_ASSERT_EQUAL_STRING("New Device", cfg.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("custom_prefix", cfg.discoveryPrefix.c_str());
    TEST_ASSERT_FALSE(cfg.retainDiscovery);
}

void test_ha_availability_topic_auto_generated() {
    HAConfig config;
    config.nodeId = "test_device";
    config.discoveryPrefix = "homeassistant";
    // Leave availabilityTopic empty

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("homeassistant/test_device/availability", cfg.availabilityTopic.c_str());
}

void test_ha_availability_topic_custom() {
    HAConfig config;
    config.nodeId = "test_device";
    config.availabilityTopic = "custom/availability/topic";

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("custom/availability/topic", cfg.availabilityTopic.c_str());
}

void test_ha_config_url_and_area() {
    HAConfig config;
    config.configUrl = "http://192.168.1.100";
    config.suggestedArea = "Living Room";

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.100", cfg.configUrl.c_str());
    TEST_ASSERT_EQUAL_STRING("Living Room", cfg.suggestedArea.c_str());
}

// ============================================================================
// Entity Management Tests - Sensors
// ============================================================================

void test_ha_add_sensor_basic() {
    HomeAssistantComponent ha;

    ha.addSensor("temp", "Temperature");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_sensor_with_all_params() {
    HomeAssistantComponent ha;

    ha.addSensor("temperature", "Temperature", "°C", "temperature", "mdi:thermometer", "measurement");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_multiple_sensors() {
    HomeAssistantComponent ha;

    ha.addSensor("temp", "Temperature", "°C");
    ha.addSensor("humidity", "Humidity", "%");
    ha.addSensor("pressure", "Pressure", "hPa");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(3, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Binary Sensors
// ============================================================================

void test_ha_add_binary_sensor_basic() {
    HomeAssistantComponent ha;

    ha.addBinarySensor("motion", "Motion Sensor");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_binary_sensor_with_class() {
    HomeAssistantComponent ha;

    ha.addBinarySensor("door", "Door Sensor", "door", "mdi:door");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Switches
// ============================================================================

void test_ha_add_switch() {
    HomeAssistantComponent ha;
    bool switchState = false;

    ha.addSwitch("relay", "Relay Switch", [&switchState](bool state) {
        switchState = state;
    }, "mdi:electric-switch");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_switch_callback_captured() {
    HomeAssistantComponent ha;
    bool callbackCalled = false;
    bool lastState = false;

    ha.addSwitch("test_switch", "Test Switch", [&callbackCalled, &lastState](bool state) {
        callbackCalled = true;
        lastState = state;
    });

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
    // Callback is stored but not yet called
    TEST_ASSERT_FALSE(callbackCalled);
}

// ============================================================================
// Entity Management Tests - Lights
// ============================================================================

void test_ha_add_light() {
    HomeAssistantComponent ha;

    ha.addLight("light1", "Main Light", [](bool state, uint8_t brightness) {
        // Light control callback
    });

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Buttons
// ============================================================================

void test_ha_add_button() {
    HomeAssistantComponent ha;
    bool buttonPressed = false;

    ha.addButton("restart", "Restart", [&buttonPressed]() {
        buttonPressed = true;
    }, "mdi:restart");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Mixed Entities
// ============================================================================

void test_ha_add_multiple_entity_types() {
    HomeAssistantComponent ha;

    ha.addSensor("temp", "Temperature", "°C");
    ha.addBinarySensor("door", "Door", "door");
    ha.addSwitch("relay", "Relay", [](bool){});
    ha.addButton("restart", "Restart", [](){});
    ha.addLight("light", "Light", [](bool, uint8_t){});

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(5, stats.entityCount);
}

// ============================================================================
// Statistics Tests
// ============================================================================

void test_ha_statistics_initial() {
    HomeAssistantComponent ha;

    const auto& stats = ha.getStatistics();

    TEST_ASSERT_EQUAL_UINT32(0, stats.entityCount);
    TEST_ASSERT_EQUAL_UINT32(0, stats.discoveryCount);
    TEST_ASSERT_EQUAL_UINT32(0, stats.stateUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, stats.commandsReceived);
}

void test_ha_statistics_after_adding_entities() {
    HomeAssistantComponent ha;

    ha.addSensor("s1", "Sensor 1");
    ha.addSensor("s2", "Sensor 2");
    ha.addSwitch("sw1", "Switch 1", [](bool){});

    const auto& stats = ha.getStatistics();

    TEST_ASSERT_EQUAL_UINT32(3, stats.entityCount);
    TEST_ASSERT_EQUAL_UINT32(0, stats.discoveryCount);  // Not published yet
    TEST_ASSERT_EQUAL_UINT32(0, stats.stateUpdates);    // No states published
}

// ============================================================================
// Connection Status Tests
// ============================================================================

void test_ha_mqtt_not_connected_initial() {
    HomeAssistantComponent ha;

    TEST_ASSERT_FALSE(ha.isMQTTConnected());
}

void test_ha_not_ready_without_mqtt() {
    HomeAssistantComponent ha;

    TEST_ASSERT_FALSE(ha.isReady());
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

void test_ha_begin_returns_success() {
    HomeAssistantComponent ha;

    ComponentStatus status = ha.begin();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);

    ha.shutdown();
}

void test_ha_shutdown_returns_success() {
    HomeAssistantComponent ha;
    ha.begin();

    ComponentStatus status = ha.shutdown();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);
}

void test_ha_full_lifecycle() {
    Core core;

    HAConfig config;
    config.nodeId = "test_lifecycle";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSensor("test_sensor", "Test Sensor");

    core.addComponent(std::move(ha));

    bool beginResult = core.begin();
    TEST_ASSERT_TRUE(beginResult);

    // Run a few loops
    for (int i = 0; i < 10; i++) {
        core.loop();
    }

    core.shutdown();
}

// ============================================================================
// Non-blocking Tests
// ============================================================================

void test_ha_loop_non_blocking() {
    Core core;

    auto ha = std::make_unique<HomeAssistantComponent>();

    core.addComponent(std::move(ha));
    core.begin();

    // Run several loop iterations to verify non-blocking
    unsigned long start = HAL::Platform::getMillis();
    int loopCount = 0;
    while (HAL::Platform::getMillis() - start < 100) {
        core.loop();
        loopCount++;
        HAL::Platform::delayMs(1);
    }

    // Should have run many loops (non-blocking)
    // HA loop() is empty - all via EventBus, so should be very fast
    TEST_ASSERT_GREATER_THAN(50, loopCount);

    core.shutdown();
}

// ============================================================================
// Device Info Tests
// ============================================================================

void test_ha_set_device_info() {
    HomeAssistantComponent ha;

    ha.setDeviceInfo("Custom Name", "Custom Model", "Custom Manufacturer", "3.0.0");

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("Custom Name", cfg.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("Custom Model", cfg.model.c_str());
    TEST_ASSERT_EQUAL_STRING("Custom Manufacturer", cfg.manufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("3.0.0", cfg.swVersion.c_str());
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_ha_no_entities() {
    HomeAssistantComponent ha;

    ComponentStatus status = ha.begin();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(0, stats.entityCount);

    ha.shutdown();
}

void test_ha_component_no_dependencies() {
    HomeAssistantComponent ha;

    auto deps = ha.getDependencies();
    // HA component has no explicit dependencies (communicates via EventBus)
    TEST_ASSERT_EQUAL(0, deps.size());
}

void test_ha_empty_config_fields() {
    HAConfig config;
    config.configUrl = "";
    config.suggestedArea = "";

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_TRUE(cfg.configUrl.isEmpty());
    TEST_ASSERT_TRUE(cfg.suggestedArea.isEmpty());
}

void test_ha_special_characters_in_node_id() {
    HAConfig config;
    config.nodeId = "device-with_mixed-chars123";

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("device-with_mixed-chars123", cfg.nodeId.c_str());
}

// ============================================================================
// HASwitch autoPublishState - Unit Tests (Task 3)
// ============================================================================

void test_switch_auto_publish_default_true() {
    HASwitch sw("test", "Test Switch");
    TEST_ASSERT_TRUE(sw.autoPublishState);
}

void test_switch_auto_publish_set_false() {
    HASwitch sw("test", "Test Switch");
    sw.autoPublishState = false;
    TEST_ASSERT_FALSE(sw.autoPublishState);
}

void test_switch_handle_command_calls_callback() {
    bool callbackCalled = false;
    bool lastState = false;

    HASwitch sw("test", "Test Switch", [&](bool state) {
        callbackCalled = true;
        lastState = state;
    });

    sw.handleCommand("ON");
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_TRUE(lastState);

    callbackCalled = false;
    sw.handleCommand("OFF");
    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_FALSE(lastState);
}

void test_switch_handle_command_no_callback_no_crash() {
    HASwitch sw("test", "Test Switch", nullptr);
    // Should not crash
    sw.handleCommand("ON");
    sw.handleCommand("OFF");
    TEST_ASSERT_TRUE(true);  // If we get here, no crash
}

// ============================================================================
// HASwitch autoPublishState - Integration Tests (Task 4)
// ============================================================================

// Helper: emit mqtt/connected and drain the event queue
static void simulateMqttConnect(Core& core) {
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    // Drain all queued events (connect -> availability -> discovery -> subscribe)
    for (int i = 0; i < 5; i++) core.loop();
}

// Helper: emit mqtt/message to simulate a switch command and drain
static void simulateSwitchCommand(Core& core, const char* nodeId,
                                   const char* entityId, const char* payload) {
    MQTTMessageEvent msg{};
    String topic = String("homeassistant/switch/") + nodeId + "/" + entityId + "/set";
    strncpy(msg.topic, topic.c_str(), MQTT_EVENT_TOPIC_SIZE - 1);
    msg.topic[MQTT_EVENT_TOPIC_SIZE - 1] = '\0';
    strncpy(msg.payload, payload, MQTT_EVENT_PAYLOAD_SIZE - 1);
    msg.payload[MQTT_EVENT_PAYLOAD_SIZE - 1] = '\0';
    core.emit<MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, msg);
    // Drain: message -> handleCommand -> possible publishState
    for (int i = 0; i < 5; i++) core.loop();
}

void test_switch_command_auto_publishes_state() {
    // AC 1: Default switch auto-publishes state after command
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1", [](bool) {});
    core.addComponent(std::move(ha));
    core.begin();

    // Subscribe to publish events
    bool statePublished = false;
    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
                capturedPayload = ev.payload;
            }
        });

    // Connect MQTT (drains availability + discovery noise)
    simulateMqttConnect(core);
    statePublished = false;  // Reset after connect noise

    // Send switch command
    simulateSwitchCommand(core, "test_node", "sw1", "ON");

    TEST_ASSERT_TRUE(statePublished);
    TEST_ASSERT_EQUAL_STRING("ON", capturedPayload.c_str());

    core.shutdown();
}

void test_switch_command_no_auto_publish_when_disabled() {
    // AC 2: autoPublishState=false -> no auto-publish (RED until Phase 3 fix)
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    bool callbackCalled = false;
    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1", [&](bool) { callbackCalled = true; },
                  "", false);  // autoPublishState = false
    core.addComponent(std::move(ha));
    core.begin();

    // Subscribe before connect
    bool statePublished = false;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
            }
        });

    simulateMqttConnect(core);
    statePublished = false;  // Reset after connect noise

    simulateSwitchCommand(core, "test_node", "sw1", "ON");

    TEST_ASSERT_TRUE(callbackCalled);
    TEST_ASSERT_FALSE(statePublished);  // Should NOT auto-publish

    core.shutdown();
}

void test_switch_optimistic_overrides_auto_publish() {
    // AC 4: optimistic=true suppresses auto-publish regardless of autoPublishState
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1", [](bool) {},
                  "", true, true);  // autoPublishState=true, optimistic=true
    core.addComponent(std::move(ha));
    core.begin();

    bool statePublished = false;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
            }
        });

    simulateMqttConnect(core);
    statePublished = false;

    simulateSwitchCommand(core, "test_node", "sw1", "ON");

    TEST_ASSERT_FALSE(statePublished);  // Optimistic suppresses publish

    core.shutdown();
}

void test_switch_manual_publish_after_auto_disabled() {
    // AC 3: Manual publishState() works even when autoPublishState=false
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSwitch("sw1", "Switch 1", [](bool) {},
                  "", false);  // autoPublishState = false
    core.addComponent(std::move(ha));
    core.begin();

    bool statePublished = false;
    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);
    statePublished = false;

    // Command arrives, no auto-publish
    simulateSwitchCommand(core, "test_node", "sw1", "ON");
    TEST_ASSERT_FALSE(statePublished);

    // Consumer manually publishes state
    haPtr->publishState("sw1", true);
    for (int i = 0; i < 5; i++) core.loop();  // Drain
    TEST_ASSERT_TRUE(statePublished);
    TEST_ASSERT_EQUAL_STRING("ON", capturedPayload.c_str());

    core.shutdown();
}

void test_switch_optimistic_true_auto_publish_false() {
    // Interaction matrix: optimistic=true, autoPublishState=false -> no publish
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1", [](bool) {},
                  "", false, true);  // autoPublishState=false, optimistic=true
    core.addComponent(std::move(ha));
    core.begin();

    bool statePublished = false;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
            }
        });

    simulateMqttConnect(core);
    statePublished = false;

    simulateSwitchCommand(core, "test_node", "sw1", "ON");

    TEST_ASSERT_FALSE(statePublished);  // Both flags suppress publish

    core.shutdown();
}

// ============================================================================
// publishState() overload resolution tests (bug 008)
// ============================================================================

void test_publish_state_const_char_ptr() {
    // Bug: publishState(id, const char*) was resolving to bool overload,
    // publishing "ON" instead of the actual string value.
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("alarm", "Alarm State");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    // Pass a const char* — must NOT resolve to bool overload
    haPtr->publishState("alarm", AlarmPanelState::Arming);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("arming", capturedPayload.c_str());

    core.shutdown();
}

void test_publish_state_constexpr_char_ptr() {
    // Verify constexpr const char* values work correctly
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("alarm", "Alarm State");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    haPtr->publishState("alarm", AlarmPanelState::ArmedAway);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("armed_away", capturedPayload.c_str());

    core.shutdown();
}

void test_publish_state_bool_still_works() {
    // Ensure the bool overload is not broken by the new const char* overload
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addBinarySensor("fault", "Fault");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    haPtr->publishState("fault", true);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("ON", capturedPayload.c_str());

    capturedPayload = "";
    haPtr->publishState("fault", false);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("OFF", capturedPayload.c_str());

    core.shutdown();
}

void test_publish_state_string_still_works() {
    // Ensure the String overload still works
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("status", "Status");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    haPtr->publishState("status", String("custom_value"));
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("custom_value", capturedPayload.c_str());

    core.shutdown();
}

void test_publish_state_string_literal() {
    // String literals are const char[], which decay to const char*
    Core core;
    HAConfig config;
    config.nodeId = "test_node";

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("alarm", "Alarm");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    haPtr->publishState("alarm", "triggered");
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("triggered", capturedPayload.c_str());

    core.shutdown();
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp() {}
void tearDown() {}

int runAllTests() {
    UNITY_BEGIN();

    // Event tests
    RUN_TEST(test_ha_events_constants_defined);

    // Component creation tests
    RUN_TEST(test_ha_component_creation_default);
    RUN_TEST(test_ha_component_creation_with_config);

    // Config tests
    RUN_TEST(test_ha_config_defaults);
    RUN_TEST(test_ha_config_get_set);
    RUN_TEST(test_ha_availability_topic_auto_generated);
    RUN_TEST(test_ha_availability_topic_custom);
    RUN_TEST(test_ha_config_url_and_area);

    // Entity management tests - Sensors
    RUN_TEST(test_ha_add_sensor_basic);
    RUN_TEST(test_ha_add_sensor_with_all_params);
    RUN_TEST(test_ha_add_multiple_sensors);

    // Entity management tests - Binary Sensors
    RUN_TEST(test_ha_add_binary_sensor_basic);
    RUN_TEST(test_ha_add_binary_sensor_with_class);

    // Entity management tests - Switches
    RUN_TEST(test_ha_add_switch);
    RUN_TEST(test_ha_add_switch_callback_captured);

    // Entity management tests - Lights
    RUN_TEST(test_ha_add_light);

    // Entity management tests - Buttons
    RUN_TEST(test_ha_add_button);

    // Entity management tests - Mixed
    RUN_TEST(test_ha_add_multiple_entity_types);

    // Statistics tests
    RUN_TEST(test_ha_statistics_initial);
    RUN_TEST(test_ha_statistics_after_adding_entities);

    // Connection status tests
    RUN_TEST(test_ha_mqtt_not_connected_initial);
    RUN_TEST(test_ha_not_ready_without_mqtt);

    // Lifecycle tests
    RUN_TEST(test_ha_begin_returns_success);
    RUN_TEST(test_ha_shutdown_returns_success);
    RUN_TEST(test_ha_full_lifecycle);

    // Non-blocking tests
    RUN_TEST(test_ha_loop_non_blocking);

    // Device info tests
    RUN_TEST(test_ha_set_device_info);

    // Edge cases
    RUN_TEST(test_ha_no_entities);
    RUN_TEST(test_ha_component_no_dependencies);
    RUN_TEST(test_ha_empty_config_fields);
    RUN_TEST(test_ha_special_characters_in_node_id);

    // HASwitch autoPublishState - Unit tests
    RUN_TEST(test_switch_auto_publish_default_true);
    RUN_TEST(test_switch_auto_publish_set_false);
    RUN_TEST(test_switch_handle_command_calls_callback);
    RUN_TEST(test_switch_handle_command_no_callback_no_crash);

    // HASwitch autoPublishState - Integration tests
    RUN_TEST(test_switch_command_auto_publishes_state);
    RUN_TEST(test_switch_command_no_auto_publish_when_disabled);
    RUN_TEST(test_switch_optimistic_overrides_auto_publish);
    RUN_TEST(test_switch_manual_publish_after_auto_disabled);
    RUN_TEST(test_switch_optimistic_true_auto_publish_false);

    // publishState() overload resolution tests (bug 008)
    RUN_TEST(test_publish_state_const_char_ptr);
    RUN_TEST(test_publish_state_constexpr_char_ptr);
    RUN_TEST(test_publish_state_bool_still_works);
    RUN_TEST(test_publish_state_string_still_works);
    RUN_TEST(test_publish_state_string_literal);

    return UNITY_END();
}

#ifdef ARDUINO
void setup() { runAllTests(); }
void loop() {}
#else
int main(int argc, char** argv) { return runAllTests(); }
#endif
