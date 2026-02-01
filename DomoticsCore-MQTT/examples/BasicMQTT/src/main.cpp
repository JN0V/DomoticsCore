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

#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
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
const char* TOPIC_STATUS = "home/mydevice/status";
const char* TOPIC_SENSOR = "home/mydevice/sensor/temperature";
const char* TOPIC_COMMAND = "home/mydevice/command/#";  // Wildcard: all commands

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay publishTimer(5000);  // Publish every 5 seconds

// ========== Setup ==========

void setup() {
    // Initialize logging system before any DLOG calls
    HAL::Platform::initializeLogging(115200);

    // ============================================================================
    // EXAMPLE: Basic MQTT Client with EventBus Integration
    // ============================================================================
    // This example demonstrates MQTT communication using EventBus:
    // - Connects to WiFi using HAL abstraction (ESP32/ESP8266 compatible)
    // - Configures MQTT with Last Will & Testament (LWT)
    // - Publishes status ("online") when connected
    // - Subscribes to command topics (home/mydevice/command/#)
    // - Publishes simulated sensor data every 5 seconds
    // - Handles incoming commands (LED on/off, restart)
    // - Uses pointer-based events (no stack overflow on ESP8266)
    // Expected Console Output:
    //   [APP] === Basic MQTT Example ===
    //   [APP] MQTT client with EventBus integration
    //   [APP] - WiFi connection using HAL (ESP32/ESP8266 compatible)
    //   [APP] - MQTT with Last Will & Testament (LWT)
    //   [APP] - Publish/Subscribe via EventBus
    //   [APP] - Sensor data published every 5 seconds
    //   [APP] - Command handling (LED on/off, restart)
    //   [APP] =====================================
    //   [APP] Connecting to WiFi: YourWiFiSSID
    //   [APP] ✓ WiFi connected! IP: 192.168.1.XXX
    //   [MQTT] Initializing
    //   [MQTT] Connected to mqtt.example.com:1883
    //   [APP] 📡 MQTT Connected!
    //   [APP]   ✓ Published: home/mydevice/status = online
    //   [APP]   ✓ Subscribed to: home/mydevice/command/#
    //   [APP] 📤 Published: home/mydevice/sensor/temperature = 25.3°C
    //   [APP]    Stats: 2 sent, 0 received, uptime 5s
    //   (Every 5 seconds: temperature publication with stats)
    //
    // To test command handling, publish to these topics:
    //   mosquitto_pub -h mqtt.example.com -t "home/mydevice/command/led" -m "on"
    //   mosquitto_pub -h mqtt.example.com -t "home/mydevice/command/led" -m "off"
    //   mosquitto_pub -h mqtt.example.com -t "home/mydevice/command/restart" -m "1"
    //
    // Expected output when receiving commands:
    //   [APP] 📨 Received command
    //   [APP]   Topic: home/mydevice/command/led
    //   [APP]   Payload: on
    //   [APP] 💡 LED ON
    // ============================================================================

    DLOG_I(LOG_APP, "=== Basic MQTT Example ===");
    DLOG_I(LOG_APP, "MQTT client with EventBus integration");
    DLOG_I(LOG_APP, "- WiFi connection using HAL (ESP32/ESP8266 compatible)");
    DLOG_I(LOG_APP, "- MQTT with Last Will & Testament (LWT)");
    DLOG_I(LOG_APP, "- Publish/Subscribe via EventBus");
    DLOG_I(LOG_APP, "- Sensor data published every 5 seconds");
    DLOG_I(LOG_APP, "- Command handling (LED on/off, restart)");
    DLOG_I(LOG_APP, "=====================================");

    // Connect to WiFi using HAL
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    HAL::WiFiHAL::init();
    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);
    HAL::WiFiHAL::connect(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (!HAL::WiFiHAL::isConnected() && attempts < 20) {
        HAL::Platform::delayMs(500);
        attempts++;
    }
    
    if (HAL::WiFiHAL::isConnected()) {
        DLOG_I(LOG_APP, "✓ WiFi connected! IP: %s", HAL::WiFiHAL::getLocalIP().c_str());
    } else {
        DLOG_E(LOG_APP, "✗ WiFi connection failed!");
        return;
    }
    
    // Configure MQTT
    MQTTConfig mqttConfig;
    mqttConfig.broker = MQTT_BROKER;
    mqttConfig.port = MQTT_PORT;
    mqttConfig.username = MQTT_USERNAME;
    mqttConfig.password = MQTT_PASSWORD;
    mqttConfig.clientId = "domotics-basic-" + String((uint32_t)HAL::Platform::getChipId(), HEX);
    mqttConfig.enabled = true;
    mqttConfig.autoReconnect = true;
    mqttConfig.enableLWT = true;
    mqttConfig.lwtTopic = TOPIC_STATUS;
    mqttConfig.lwtMessage = "offline";
    mqttConfig.lwtQoS = 1;
    mqttConfig.lwtRetain = true;
    
    DLOG_I(LOG_APP, "MQTT Configuration:");
    DLOG_I(LOG_APP, "  Broker: %s:%d", mqttConfig.broker.c_str(), mqttConfig.port);
    DLOG_I(LOG_APP, "  Client ID: %s", mqttConfig.clientId.c_str());
    DLOG_I(LOG_APP, "  Username: %s", mqttConfig.username.isEmpty() ? "(none)" : mqttConfig.username.c_str());
    DLOG_I(LOG_APP, "  LWT Topic: %s", mqttConfig.lwtTopic.c_str());
    
    // Create and register MQTT component
    core.addComponent(std::make_unique<MQTTComponent>(mqttConfig));
    
    // Register EventBus listeners BEFORE initializing components
    // Note: Capture core by pointer to avoid non-automatic storage duration warning
    Core* corePtr = &core;
    core.on<bool>("mqtt/connected", [corePtr](const bool&) {
        DLOG_I(LOG_APP, "📡 MQTT Connected!");
        
        // Publish online status via EventBus
        DomoticsCore::Components::MQTTPublishEvent pubEv{};
        strncpy(pubEv.topic, TOPIC_STATUS, sizeof(pubEv.topic) - 1);
        strncpy(pubEv.payload, "online", sizeof(pubEv.payload) - 1);
        pubEv.qos = 1;
        pubEv.retain = true;
        corePtr->emit("mqtt/publish", pubEv);
        DLOG_I(LOG_APP, "  ✓ Published: %s = online", TOPIC_STATUS);

        // Subscribe to commands via EventBus
        DomoticsCore::Components::MQTTSubscribeEvent subEv{};
        strncpy(subEv.topic, TOPIC_COMMAND, sizeof(subEv.topic) - 1);
        subEv.qos = 1;
        corePtr->emit("mqtt/subscribe", subEv);
        DLOG_I(LOG_APP, "  ✓ Subscribed to: %s", TOPIC_COMMAND);
    });
    
    core.on<bool>("mqtt/disconnected", [](const bool&) {
        DLOG_W(LOG_APP, "📡 MQTT Disconnected");
    });
    
    core.on<DomoticsCore::Components::MQTTMessageEvent>("mqtt/message", [](const DomoticsCore::Components::MQTTMessageEvent& ev) {
        // ev.topic and ev.payload are const char* pointers valid during this handler
        DLOG_I(LOG_APP, "📨 Received command");
        DLOG_I(LOG_APP, "  Topic: %s", ev.topic);
        DLOG_I(LOG_APP, "  Payload: %s", ev.payload);

        // Parse command - use strcmp for C strings
        String topic = String(ev.topic);  // Convert if needed for String methods
        String payload = String(ev.payload);

        if (topic.endsWith("/led")) {
            if (payload == "on") {
                DLOG_I(LOG_APP, "💡 LED ON");
                // digitalWrite(LED_PIN, HIGH);
            } else if (payload == "off") {
                DLOG_I(LOG_APP, "💡 LED OFF");
                // digitalWrite(LED_PIN, LOW);
            }
        } else if (topic.endsWith("/restart")) {
            DLOG_I(LOG_APP, "🔄 Restarting...");
            HAL::Platform::delayMs(1000);
            HAL::Platform::restart();
        }
    });
    
    // Initialize all components
    DLOG_I(LOG_APP, "Initializing components...");
    core.begin();
    
    DLOG_I(LOG_APP, "✓ Setup complete!");
    DLOG_I(LOG_APP, "Waiting for MQTT connection...");
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
                DLOG_I(LOG_APP, "📤 Published: %s = %.1f°C", TOPIC_SENSOR, temperature);
                
                // Show statistics
                const auto& stats = mqtt->getStatistics();
                DLOG_I(LOG_APP, "   Stats: %u sent, %u received, uptime %us",
                             stats.publishCount, stats.receiveCount, stats.uptime);
            }
        }
    }
    
    // No delay() - fully non-blocking
}
