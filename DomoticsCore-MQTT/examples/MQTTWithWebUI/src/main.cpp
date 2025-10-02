/**
 * @file main.cpp
 * @brief MQTT with WebUI Example
 * 
 * Demonstrates:
 * - MQTT client with web-based configuration
 * - Real-time connection status in WebUI
 * - Statistics monitoring
 * - Interactive MQTT testing through browser
 * - Using DomoticsCore logging system
 * - Non-blocking timers
 * 
 * Hardware: ESP32 Dev Board
 * 
 * Access:
 * - WebUI: http://<device-ip>
 * - Configure MQTT broker via Settings tab
 * - Monitor statistics in Components tab
 * 
 * Note: This example uses ESP32 native WiFi (not DomoticsCore-WiFi component)
 * for simplicity. In production, use DomoticsCore-WiFi for advanced features
 * like automatic reconnection, network scanning, and credential management.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/MQTTWebUI.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// ========== Configuration ==========

// WiFi credentials
const char* WIFI_SSID = "YourWifiSSID";
const char* WIFI_PASSWORD = "YourWifiPassword";

// Default MQTT configuration (can be changed via WebUI)
const char* MQTT_BROKER = "mqtt.example.com";
const uint16_t MQTT_PORT = 1883;

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay publishTimer(10000);  // Publish every 10 seconds

// ========== Setup ==========

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_CORE, "\n========================================");
    DLOG_I(LOG_CORE, "DomoticsCore - MQTT with WebUI");
    DLOG_I(LOG_CORE, "========================================\n");
    
    // Connect to WiFi (using ESP32 native WiFi for simplicity)
    // In production, use DomoticsCore-WiFi component
    DLOG_I(LOG_CORE, "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        DLOG_W(LOG_CORE, "✗ WiFi connection failed!");
        DLOG_I(LOG_CORE, "Starting AP mode for configuration...");
        WiFi.softAP("ESP32-MQTT-Setup");
        DLOG_I(LOG_CORE, "AP IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        DLOG_I(LOG_CORE, "✓ WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    }
    
    // Configure WebUI
    WebUIConfig webConfig;
    webConfig.deviceName = "ESP32 MQTT Device";
    webConfig.wsUpdateInterval = 2000;
    auto webui = std::make_unique<WebUIComponent>(webConfig);
    auto* webuiPtr = webui.get();
    core.addComponent(std::move(webui));
    
    // Configure MQTT
    MQTTConfig mqttConfig;
    mqttConfig.broker = MQTT_BROKER;
    mqttConfig.port = MQTT_PORT;
    mqttConfig.clientId = "esp32-webui-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    mqttConfig.enabled = true;
    mqttConfig.autoReconnect = true;
    mqttConfig.enableLWT = true;
    mqttConfig.lwtTopic = mqttConfig.clientId + "/status";
    mqttConfig.lwtMessage = "offline";
    mqttConfig.lwtQoS = 1;
    mqttConfig.lwtRetain = true;
    
    auto mqtt = std::make_unique<MQTTComponent>(mqttConfig);
    auto* mqttPtr = mqtt.get();
    core.addComponent(std::move(mqtt));
    
    // Register MQTT callbacks
    // Capture mqttPtr to avoid unnecessary getComponent() calls
    mqttPtr->onConnect([mqttPtr]() {
        DLOG_I(LOG_CORE, "\n📡 MQTT Connected!");
        
        // Publish online status
        String statusTopic = mqttPtr->getMQTTConfig().clientId + "/status";
        mqttPtr->publish(statusTopic, "online", 1, true);
        
        // Subscribe to command topics
        String commandTopic = mqttPtr->getMQTTConfig().clientId + "/command/#";
        mqttPtr->subscribe(commandTopic, 1);
        
        DLOG_I(LOG_CORE, "  ✓ Published online status");
        DLOG_I(LOG_CORE, "  ✓ Subscribed to commands\n");
    });
    
    mqttPtr->onDisconnect([]() {
        DLOG_W(LOG_CORE, "\n📡 MQTT Disconnected\n");
    });
    
    mqttPtr->onMessage("+/command/#", [](const String& topic, const String& payload) {
        DLOG_I(LOG_CORE, "📨 Command received: %s = %s", topic.c_str(), payload.c_str());
    });
    
    // Initialize components
    DLOG_I(LOG_CORE, "Initializing components...");
    core.begin();
    
    // Register WebUI provider for MQTT
    if (webuiPtr && mqttPtr) {
        webuiPtr->registerProviderWithComponent(new MQTTWebUI(mqttPtr), mqttPtr);
        DLOG_I(LOG_CORE, "✓ MQTT WebUI provider registered");
    }
    
    DLOG_I(LOG_CORE, "\n✓ Setup complete!\n");
    
    if (WiFi.status() == WL_CONNECTED) {
        DLOG_I(LOG_CORE, "========================================");
        DLOG_I(LOG_CORE, "WebUI: http://%s", WiFi.localIP().toString().c_str());
        DLOG_I(LOG_CORE, "========================================\n");
    } else {
        DLOG_I(LOG_CORE, "========================================");
        DLOG_I(LOG_CORE, "WebUI: http://%s", WiFi.softAPIP().toString().c_str());
        DLOG_I(LOG_CORE, "========================================\n");
    }
}

// ========== Loop ==========

void loop() {
    core.loop();
    
    // Publish telemetry periodically using NonBlockingDelay
    if (publishTimer.isReady()) {
        auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
        if (mqtt && mqtt->isConnected()) {
            // Publish JSON telemetry
            JsonDocument doc;
            doc["uptime"] = millis() / 1000;
            doc["freeHeap"] = ESP.getFreeHeap();
            doc["rssi"] = WiFi.RSSI();
            doc["temperature"] = 20.0 + (random(0, 100) / 10.0);
            
            String telemetryTopic = mqtt->getMQTTConfig().clientId + "/telemetry";
            if (mqtt->publishJSON(telemetryTopic, doc, 0, false)) {
                DLOG_I(LOG_CORE, "📤 Published telemetry");
            }
        }
    }
    
    // No delay() - fully non-blocking
}
