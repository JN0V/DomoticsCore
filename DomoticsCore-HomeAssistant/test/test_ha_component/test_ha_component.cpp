/**
 * @file test_ha_component.cpp
 * @brief HomeAssistant component: creation, configuration, entity management, statistics, lifecycle and edge cases.
 */

#include <unity.h>
#include "../ha_test_support.h"

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
    // The exact version is enforced by tools/check_versions.py, which compares
    // library.json against every metadata.version in the component's sources.
    // Repeating the literal here only makes the test stale at the next release.
    TEST_ASSERT_NOT_NULL(ha.metadata.version);
    TEST_ASSERT_NOT_EQUAL('\0', ha.metadata.version[0]);
}

void test_ha_component_creation_with_config() {
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    HA::setField(config.deviceName, "Test Device", sizeof(config.deviceName));
    HA::setField(config.manufacturer, "TestMfg", sizeof(config.manufacturer));
    HA::setField(config.model, "TestModel", sizeof(config.model));
    HA::setField(config.swVersion, "2.0.0", sizeof(config.swVersion));

    HomeAssistantComponent ha(config);

    TEST_ASSERT_EQUAL_STRING("HomeAssistant", ha.metadata.name);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("test_node", cfg.nodeId);
    TEST_ASSERT_EQUAL_STRING("Test Device", cfg.deviceName);
    TEST_ASSERT_EQUAL_STRING("TestMfg", cfg.manufacturer);
    TEST_ASSERT_EQUAL_STRING("TestModel", cfg.model);
    TEST_ASSERT_EQUAL_STRING("2.0.0", cfg.swVersion);
}

// ============================================================================
// Config Tests
// ============================================================================

void test_ha_config_defaults() {
    HAConfig config;

    // Default values from HAConfig struct in HomeAssistant.h
    TEST_ASSERT_EQUAL_STRING("myDeviceId", config.nodeId);
    TEST_ASSERT_EQUAL_STRING("My Device", config.deviceName);
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.manufacturer);
    TEST_ASSERT_EQUAL_STRING("MyDeviceModel", config.model);
    TEST_ASSERT_EQUAL_STRING("1.0.0", config.swVersion);
    TEST_ASSERT_TRUE(config.retainDiscovery);
    TEST_ASSERT_EQUAL_STRING("homeassistant", config.discoveryPrefix);
    TEST_ASSERT_EQUAL_STRING("", config.availabilityTopic);
    TEST_ASSERT_EQUAL_STRING("", config.configUrl);
    TEST_ASSERT_EQUAL_STRING("", config.suggestedArea);
}

void test_ha_config_get_set() {
    HomeAssistantComponent ha;

    HAConfig newConfig;
    HA::setField(newConfig.nodeId, "new_node", sizeof(newConfig.nodeId));
    HA::setField(newConfig.deviceName, "New Device", sizeof(newConfig.deviceName));
    HA::setField(newConfig.discoveryPrefix, "custom_prefix", sizeof(newConfig.discoveryPrefix));
    newConfig.retainDiscovery = false;

    ha.setConfig(newConfig);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("new_node", cfg.nodeId);
    TEST_ASSERT_EQUAL_STRING("New Device", cfg.deviceName);
    TEST_ASSERT_EQUAL_STRING("custom_prefix", cfg.discoveryPrefix);
    TEST_ASSERT_FALSE(cfg.retainDiscovery);
}

void test_ha_availability_topic_auto_generated() {
    HAConfig config;
    HA::setField(config.nodeId, "test_device", sizeof(config.nodeId));
    HA::setField(config.discoveryPrefix, "homeassistant", sizeof(config.discoveryPrefix));
    // Leave availabilityTopic empty

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("homeassistant/test_device/availability", cfg.availabilityTopic);
}

void test_ha_availability_topic_custom() {
    HAConfig config;
    HA::setField(config.nodeId, "test_device", sizeof(config.nodeId));
    HA::setField(config.availabilityTopic, "custom/availability/topic", sizeof(config.availabilityTopic));

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("custom/availability/topic", cfg.availabilityTopic);
}

void test_ha_config_url_and_area() {
    HAConfig config;
    HA::setField(config.configUrl, "http://192.168.1.100", sizeof(config.configUrl));
    HA::setField(config.suggestedArea, "Living Room", sizeof(config.suggestedArea));

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.100", cfg.configUrl);
    TEST_ASSERT_EQUAL_STRING("Living Room", cfg.suggestedArea);
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

    ha.addSwitch("relay", "Relay Switch", "mdi:electric-switch");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_switch_entity_registered() {
    HomeAssistantComponent ha;

    ha.addSwitch("test_switch", "Test Switch");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Lights
// ============================================================================

void test_ha_add_light() {
    HomeAssistantComponent ha;

    ha.addLight("light1", "Main Light");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Buttons
// ============================================================================

void test_ha_add_button() {
    HomeAssistantComponent ha;

    ha.addButton("restart", "Restart", "mdi:restart");

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
    ha.addSwitch("relay", "Relay");
    ha.addButton("restart", "Restart");
    ha.addLight("light", "Light");

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
    ha.addSwitch("sw1", "Switch 1");

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
    HA::setField(config.nodeId, "test_lifecycle", sizeof(config.nodeId));

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
    TEST_ASSERT_EQUAL_STRING("Custom Name", cfg.deviceName);
    TEST_ASSERT_EQUAL_STRING("Custom Model", cfg.model);
    TEST_ASSERT_EQUAL_STRING("Custom Manufacturer", cfg.manufacturer);
    TEST_ASSERT_EQUAL_STRING("3.0.0", cfg.swVersion);
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
    config.configUrl[0] = '\0';
    config.suggestedArea[0] = '\0';

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("", cfg.configUrl);
    TEST_ASSERT_EQUAL_STRING("", cfg.suggestedArea);
}

void test_ha_special_characters_in_node_id() {
    HAConfig config;
    HA::setField(config.nodeId, "device-with_mixed-chars123", sizeof(config.nodeId));

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("device-with_mixed-chars123", cfg.nodeId);
}

static int runAllTests() {
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
    RUN_TEST(test_ha_add_switch_entity_registered);

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

    return UNITY_END();
}

int main(int argc, char** argv) { return runAllTests(); }
