/**
 * @file main.cpp
 * @brief Basic MQTT Example
 * 
 * Demonstrates:
 * - MQTT client setup with custom broker
 * - Publishing sensor data periodically
 * - Subscribing to command topics
 * - Handling incoming messages
 * - Using DomoticsCore logging system
 * 
 * Hardware: ESP32 Dev Board
 * 
 * Configuration:
 * - Update WiFi credentials below
 * - Update MQTT broker address
 * - Optional: Update MQTT username/password
 * 
 * Note: This example uses ESP32 native WiFi (not DomoticsCore-WiFi component)
 * for simplicity and to show standalone MQTT usage without full component stack.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// ========== Configuration ==========

// WiFi credentials
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// MQTT broker
const char* MQTT_BROKER = "mqtt.example.com";  // or IP address like "192.168.1.100"
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "";  // Leave empty if no auth required
const char* MQTT_PASSWORD = "";

// MQTT topics
const char* TOPIC_STATUS = "home/esp32/status";
const char* TOPIC_SENSOR = "home/esp32/sensor/temperature";
const char* TOPIC_COMMAND = "home/esp32/command/#";  // Wildcard: all commands

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay publishTimer(5000);  // Publish every 5 seconds

// ========== Setup ==========

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_CORE, "\n========================================");
    DLOG_I(LOG_CORE, "DomoticsCore - Basic MQTT Example");
    DLOG_I(LOG_CORE, "========================================\n");
    
    // Connect to WiFi (using ESP32 native WiFi for simplicity)
    // In production, use DomoticsCore-WiFi component for full features
    DLOG_I(LOG_CORE, "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        DLOG_I(LOG_CORE, "✓ WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        DLOG_E(LOG_CORE, "✗ WiFi connection failed!");
        return;
    }
    
    // Configure MQTT
    MQTTConfig mqttConfig;
    mqttConfig.broker = MQTT_BROKER;
    mqttConfig.port = MQTT_PORT;
    mqttConfig.username = MQTT_USERNAME;
    mqttConfig.password = MQTT_PASSWORD;
    mqttConfig.clientId = "esp32-basic-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    mqttConfig.enabled = true;
    mqttConfig.autoReconnect = true;
    mqttConfig.enableLWT = true;
    mqttConfig.lwtTopic = TOPIC_STATUS;
    mqttConfig.lwtMessage = "offline";
    mqttConfig.lwtQoS = 1;
    mqttConfig.lwtRetain = true;
    
    DLOG_I(LOG_CORE, "MQTT Configuration:");
    DLOG_I(LOG_CORE, "  Broker: %s:%d", mqttConfig.broker.c_str(), mqttConfig.port);
    DLOG_I(LOG_CORE, "  Client ID: %s", mqttConfig.clientId.c_str());
    DLOG_I(LOG_CORE, "  Username: %s", mqttConfig.username.isEmpty() ? "(none)" : mqttConfig.username.c_str());
    DLOG_I(LOG_CORE, "  LWT Topic: %s\n", mqttConfig.lwtTopic.c_str());
    
    // Create and register MQTT component
    auto mqtt = std::make_unique<MQTTComponent>(mqttConfig);
    auto* mqttPtr = mqtt.get();
    core.addComponent(std::move(mqtt));
    
    // Register callbacks BEFORE initializing components
    // Note: We capture mqttPtr (raw pointer) which remains valid as long as
    // the component exists in core. This avoids unnecessary getComponent() calls.
    
    // On connect: publish online status and subscribe to commands
    mqttPtr->onConnect([mqttPtr]() {
        DLOG_I(LOG_CORE, "\n📡 MQTT Connected!");
        
        // Publish online status
        mqttPtr->publish(TOPIC_STATUS, "online", 1, true);
        DLOG_I(LOG_CORE, "  ✓ Published: %s = online", TOPIC_STATUS);
        
        // Subscribe to commands
        if (mqttPtr->subscribe(TOPIC_COMMAND, 1)) {
            DLOG_I(LOG_CORE, "  ✓ Subscribed to: %s\n", TOPIC_COMMAND);
        }
    });
    
    // On disconnect
    mqttPtr->onDisconnect([]() {
        DLOG_W(LOG_CORE, "\n📡 MQTT Disconnected\n");
    });
    
    // On message received
    mqttPtr->onMessage(TOPIC_COMMAND, [](const String& topic, const String& payload) {
        DLOG_I(LOG_CORE, "📨 Received command");
        DLOG_I(LOG_CORE, "  Topic: %s", topic.c_str());
        DLOG_I(LOG_CORE, "  Payload: %s\n", payload.c_str());
        
        // Parse command
        if (topic.endsWith("/led")) {
            if (payload == "on") {
                DLOG_I(LOG_CORE, "💡 LED ON");
                // digitalWrite(LED_PIN, HIGH);
            } else if (payload == "off") {
                DLOG_I(LOG_CORE, "💡 LED OFF");
                // digitalWrite(LED_PIN, LOW);
            }
        } else if (topic.endsWith("/restart")) {
            DLOG_I(LOG_CORE, "🔄 Restarting...");
            delay(1000);
            ESP.restart();
        }
    });
    
    // Initialize all components
    DLOG_I(LOG_CORE, "Initializing components...");
    core.begin();
    
    DLOG_I(LOG_CORE, "\n✓ Setup complete!\n");
    DLOG_I(LOG_CORE, "Waiting for MQTT connection...\n");
}

// ========== Loop ==========

void loop() {
    core.loop();
    
    // Publish sensor data periodically using NonBlockingDelay
    if (publishTimer.isReady()) {
        auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
        if (mqtt && mqtt->isConnected()) {
            // Simulate temperature sensor reading
            float temperature = 20.0 + (random(0, 100) / 10.0);  // 20.0 - 30.0°C
            
            // Publish as string
            String payload = String(temperature, 1);
            if (mqtt->publish(TOPIC_SENSOR, payload, 0, false)) {
                DLOG_I(LOG_CORE, "📤 Published: %s = %.1f°C", TOPIC_SENSOR, temperature);
                
                // Show statistics
                const auto& stats = mqtt->getStatistics();
                DLOG_I(LOG_CORE, "   Stats: %lu sent, %lu received, uptime %lus\n",
                             stats.publishCount, stats.receiveCount, stats.uptime);
            }
        }
    }
    
    // No delay() - fully non-blocking
}
