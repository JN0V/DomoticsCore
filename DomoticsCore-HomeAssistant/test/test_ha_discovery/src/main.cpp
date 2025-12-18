#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_ha_component_creation() {
    HomeAssistantComponent ha;
    printResult("HomeAssistant component created", ha.metadata.name == "HomeAssistant");
}

void test_ha_with_config() {
    HAConfig config;
    config.nodeId = "test_device";
    config.deviceName = "Test Device";
    config.manufacturer = "DomoticsCore";
    config.model = "ESP32";
    config.swVersion = "1.0.0";
    
    HomeAssistantComponent ha(config);
    
    const HAConfig& current = ha.getConfig();
    
    printResult("HA config stored correctly", 
                current.nodeId == "test_device" &&
                current.deviceName == "Test Device");
}

void test_ha_add_sensor() {
    HomeAssistantComponent ha;
    
    ha.addSensor("temperature", "Temperature", "°C", "temperature", "mdi:thermometer");
    
    const auto& stats = ha.getStatistics();
    
    printResult("HA add sensor works", stats.entityCount == 1);
}

void test_ha_add_binary_sensor() {
    HomeAssistantComponent ha;
    
    ha.addBinarySensor("motion", "Motion Sensor", "motion", "mdi:motion-sensor");
    
    const auto& stats = ha.getStatistics();
    
    printResult("HA add binary sensor works", stats.entityCount == 1);
}

void test_ha_add_switch() {
    HomeAssistantComponent ha;
    
    bool switchState = false;
    ha.addSwitch("relay", "Relay Switch", [&switchState](bool state) {
        switchState = state;
    }, "mdi:electric-switch");
    
    const auto& stats = ha.getStatistics();
    
    printResult("HA add switch works", stats.entityCount == 1);
}

void test_ha_add_button() {
    HomeAssistantComponent ha;
    
    bool buttonPressed = false;
    ha.addButton("reboot", "Reboot", [&buttonPressed]() {
        buttonPressed = true;
    }, "mdi:restart");
    
    const auto& stats = ha.getStatistics();
    
    printResult("HA add button works", stats.entityCount == 1);
}

void test_ha_multiple_entities() {
    HomeAssistantComponent ha;
    
    ha.addSensor("temp", "Temperature", "°C");
    ha.addSensor("humidity", "Humidity", "%");
    ha.addBinarySensor("door", "Door Sensor");
    ha.addSwitch("light", "Light", [](bool){});
    
    const auto& stats = ha.getStatistics();
    
    printResult("HA multiple entities work", stats.entityCount == 4);
}

void test_ha_mqtt_not_connected() {
    HomeAssistantComponent ha;
    
    // Without MQTT connection, should not be ready
    bool ready = ha.isReady();
    bool connected = ha.isMQTTConnected();
    
    printResult("HA not ready without MQTT", !ready && !connected);
}

void test_ha_non_blocking_loop() {
    Core core;
    
    auto ha = std::make_unique<HomeAssistantComponent>();
    
    core.addComponent(std::move(ha));
    core.begin();
    
    // Run several loop iterations to verify non-blocking
    unsigned long start = millis();
    int loopCount = 0;
    while (millis() - start < 100) {
        core.loop();
        loopCount++;
        delay(1);
    }
    
    // Should have run many loops (non-blocking)
    // HA loop() is empty - all via EventBus, so should be very fast
    printResult("HA loop is non-blocking", loopCount > 50);
    
    core.shutdown();
}

void test_ha_availability_topic() {
    HAConfig config;
    config.nodeId = "test_node";
    config.discoveryPrefix = "homeassistant";
    
    HomeAssistantComponent ha(config);
    
    const HAConfig& current = ha.getConfig();
    
    // Should auto-generate availability topic
    bool hasAvailTopic = !current.availabilityTopic.isEmpty();
    
    printResult("HA availability topic generated", hasAvailTopic);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore HomeAssistant Tests");
    Serial.println("========================================\n");
    
    test_ha_component_creation();
    test_ha_with_config();
    test_ha_add_sensor();
    test_ha_add_binary_sensor();
    test_ha_add_switch();
    test_ha_add_button();
    test_ha_multiple_entities();
    test_ha_mqtt_not_connected();
    test_ha_non_blocking_loop();
    test_ha_availability_topic();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
