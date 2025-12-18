#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_mqtt_component_creation() {
    MQTTComponent mqtt;
    printResult("MQTT component created", mqtt.metadata.name == "MQTT");
}

void test_mqtt_with_config() {
    MQTTConfig config;
    config.broker = "test.mosquitto.org";
    config.port = 1883;
    config.clientId = "test_client";
    config.autoReconnect = true;
    
    MQTTComponent mqtt(config);
    printResult("MQTT with config created", mqtt.metadata.name == "MQTT");
}

void test_mqtt_state_without_connection() {
    MQTTConfig config;
    config.broker = "";  // No broker = won't connect
    config.enabled = false;
    
    MQTTComponent mqtt(config);
    
    MQTTState state = mqtt.getState();
    
    printResult("MQTT state is Disconnected without broker", state == MQTTState::Disconnected);
}

void test_mqtt_non_blocking_loop() {
    Core core;
    
    MQTTConfig config;
    config.broker = "";  // No broker
    config.enabled = false;
    
    auto mqtt = std::make_unique<MQTTComponent>(config);
    
    core.addComponent(std::move(mqtt));
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
    printResult("MQTT loop is non-blocking", loopCount > 50);
    
    core.shutdown();
}

void test_mqtt_subscribe_without_connection() {
    Core core;
    
    MQTTConfig config;
    config.broker = "";
    config.enabled = false;
    
    auto mqtt = std::make_unique<MQTTComponent>(config);
    MQTTComponent* mqttPtr = mqtt.get();
    
    core.addComponent(std::move(mqtt));
    core.begin();
    
    // Should queue subscription for later
    bool result = mqttPtr->subscribe("test/topic", 0);
    
    printResult("MQTT subscribe queues without connection", result);
    
    core.shutdown();
}

void test_mqtt_publish_queuing() {
    Core core;
    
    MQTTConfig config;
    config.broker = "";
    config.enabled = false;
    config.maxQueueSize = 10;
    
    auto mqtt = std::make_unique<MQTTComponent>(config);
    MQTTComponent* mqttPtr = mqtt.get();
    
    core.addComponent(std::move(mqtt));
    core.begin();
    
    // Should queue message for later
    bool result = mqttPtr->publish("test/topic", "test payload", 0, false);
    size_t queueSize = mqttPtr->getQueuedMessageCount();
    
    printResult("MQTT publish queues when offline", result && queueSize > 0);
    
    core.shutdown();
}

void test_mqtt_statistics() {
    MQTTComponent mqtt;
    
    const MQTTStatistics& stats = mqtt.getStatistics();
    
    // Initial stats should be zero
    printResult("MQTT statistics initialized", 
                stats.connectCount == 0 && 
                stats.publishCount == 0 &&
                stats.receiveCount == 0);
}

void test_mqtt_exponential_backoff() {
    MQTTConfig config;
    config.reconnectDelay = 1000;
    config.maxReconnectDelay = 30000;
    config.autoReconnect = true;
    
    MQTTComponent mqtt(config);
    
    // Config should be stored correctly
    const MQTTConfig& storedConfig = mqtt.getConfig();
    
    printResult("MQTT exponential backoff configured", 
                storedConfig.reconnectDelay == 1000 &&
                storedConfig.maxReconnectDelay == 30000);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore MQTT Component Tests");
    Serial.println("========================================\n");
    
    test_mqtt_component_creation();
    test_mqtt_with_config();
    test_mqtt_state_without_connection();
    test_mqtt_non_blocking_loop();
    test_mqtt_subscribe_without_connection();
    test_mqtt_publish_queuing();
    test_mqtt_statistics();
    test_mqtt_exponential_backoff();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
