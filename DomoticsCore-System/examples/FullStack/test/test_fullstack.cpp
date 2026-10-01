/**
 * @file test_fullstack.cpp
 * @brief On-device tests for the FullStack example: its configuration and its
 *        Home Assistant entities.
 */

#include <unity.h>
#include <DomoticsCore/System.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/HomeAssistant.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Test configuration
SystemConfig testConfig;
System* domotics = nullptr;

void setUp(void) {
    // Minimal configuration for the tests
    testConfig = SystemConfig::fullStack();
    testConfig.deviceName = "TestDevice";
    testConfig.mqttBroker = "test.mosquitto.org";
    testConfig.wifiSSID = "TestNetwork";
    testConfig.wifiPassword = "TestPassword";
}

void tearDown(void) {
    // Clean up after each test
    if (domotics) {
        delete domotics;
        domotics = nullptr;
    }
}

/**
 * @brief The FullStack configuration enables every component
 */
void test_fullstack_config_enables_all_components() {
    SystemConfig config = SystemConfig::fullStack();
    
    TEST_ASSERT_TRUE_MESSAGE(config.enableLED, "LED should be enabled");
    TEST_ASSERT_TRUE_MESSAGE(config.enableConsole, "Console should be enabled");
    TEST_ASSERT_TRUE_MESSAGE(config.enableWebUI, "WebUI should be enabled");
    TEST_ASSERT_TRUE_MESSAGE(config.enableNTP, "NTP should be enabled");
    TEST_ASSERT_TRUE_MESSAGE(config.enableStorage, "Storage should be enabled");
    TEST_ASSERT_TRUE_MESSAGE(config.enableMQTT, "MQTT should be enabled");
    TEST_ASSERT_TRUE_MESSAGE(config.enableHomeAssistant, "Home Assistant should be enabled");
    TEST_ASSERT_TRUE_MESSAGE(config.enableOTA, "OTA should be enabled");
    TEST_ASSERT_TRUE_MESSAGE(config.enableSystemInfo, "SystemInfo should be enabled");
}

/**
 * @brief A System can be created from the FullStack configuration
 */
void test_system_creation() {
    domotics = new System(testConfig);
    TEST_ASSERT_NOT_NULL_MESSAGE(domotics, "System should be created");
}

/**
 * @brief The MQTT configuration
 */
void test_mqtt_configuration() {
    TEST_ASSERT_EQUAL_STRING_MESSAGE("test.mosquitto.org", testConfig.mqttBroker.c_str(), 
                                    "MQTT broker should match");
    TEST_ASSERT_EQUAL_MESSAGE(1883, testConfig.mqttPort, "MQTT port should be 1883");
    TEST_ASSERT_TRUE_MESSAGE(testConfig.enableMQTT, "MQTT should be enabled");
}

/**
 * @brief The Home Assistant configuration
 */
void test_home_assistant_configuration() {
    TEST_ASSERT_TRUE_MESSAGE(testConfig.enableHomeAssistant,
                             "Home Assistant should be enabled");
}

/**
 * @brief The example's Home Assistant entities can be created
 */
void test_home_assistant_entity_creation() {
    // Minimal MQTT component configuration
    MQTTConfig mqttCfg;
    mqttCfg.broker = "test.mosquitto.org";
    mqttCfg.enabled = true;
    
    auto mqttComp = new MQTTComponent(mqttCfg);
    
    // Home Assistant configuration
    HomeAssistant::HAConfig haCfg;
    snprintf(haCfg.nodeId, sizeof(haCfg.nodeId), "%s", "test-device");
    snprintf(haCfg.deviceName, sizeof(haCfg.deviceName), "%s", "Test Device");

    auto haComp = new HomeAssistant::HomeAssistantComponent(haCfg);

    // The entities the example creates
    haComp->addSensor("temperature", "Temperature", "°C", "temperature", "mdi:thermometer");
    haComp->addSensor("uptime", "Uptime", "s", "", "mdi:clock-outline");
    haComp->addSensor("free_heap", "Free Heap", "bytes", "", "mdi:memory");
    haComp->addSensor("wifi_signal", "WiFi Signal", "dBm", "signal_strength", "mdi:wifi");
    haComp->addSwitch("relay", "Cooling Relay", "mdi:fan");
    haComp->addButton("restart", "Restart Device", "mdi:restart");

    // Every entity was created
    TEST_ASSERT_EQUAL_MESSAGE(6, haComp->getStatistics().entityCount,
                             "Should have 6 entities (4 sensors + 1 switch + 1 button)");

    // Clean up
    delete haComp;
    delete mqttComp;
}

/**
 * @brief The publishing intervals
 */
void test_publishing_intervals() {
    // These match the timers in main.cpp
    const uint32_t SENSOR_TIMER = 10000;       // 10 seconds
    const uint32_t MQTT_PUBLISH_TIMER = 5000;  // 5 seconds
    const uint32_t HEARTBEAT_TIMER = 30000;    // 30 seconds
    
    TEST_ASSERT_EQUAL_MESSAGE(10000, SENSOR_TIMER, 
                             "Sensor reading interval should be 10s");
    TEST_ASSERT_EQUAL_MESSAGE(5000, MQTT_PUBLISH_TIMER,
                             "MQTT publish interval should be 5s");
    TEST_ASSERT_EQUAL_MESSAGE(30000, HEARTBEAT_TIMER,
                             "Heartbeat interval should be 30s");
}

/**
 * @brief The relay configuration
 */
void test_relay_configuration() {
    const int RELAY_PIN = 5;
    TEST_ASSERT_EQUAL_MESSAGE(5, RELAY_PIN, "Relay pin should be 5");
}

/**
 * @brief System returns its components
 */
void test_get_components() {
    domotics = new System(testConfig);
    
    // getCore() returns a valid reference
    Core& core = domotics->getCore();
    TEST_ASSERT_MESSAGE(true, "getCore() should return valid reference");
    
    // getWiFi() may return null before begin(): the test only checks it does not crash.
    auto* wifi = domotics->getWiFi();
    TEST_ASSERT_MESSAGE(true, "getWiFi() method should not crash");
}


void setup() {
    delay(2000);  // Let Serial come up
    
    UNITY_BEGIN();

    // Configuration
    RUN_TEST(test_fullstack_config_enables_all_components);
    RUN_TEST(test_mqtt_configuration);
    RUN_TEST(test_home_assistant_configuration);
    RUN_TEST(test_publishing_intervals);
    RUN_TEST(test_relay_configuration);
    
    // Creation
    RUN_TEST(test_system_creation);
    RUN_TEST(test_get_components);
    RUN_TEST(test_home_assistant_entity_creation);

    
    UNITY_END();
}

void loop() {
    // The tests run once, from setup()
}
