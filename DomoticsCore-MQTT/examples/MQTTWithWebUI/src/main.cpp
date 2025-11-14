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

// Custom application log tag
#define LOG_APP "APP"
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
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - MQTT with WebUI");
    DLOG_I(LOG_APP, "========================================");
    
    // Connect to WiFi (using ESP32 native WiFi for simplicity)
    // In production, use DomoticsCore-WiFi component
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        DLOG_W(LOG_APP, "✗ WiFi connection failed!");
        DLOG_I(LOG_APP, "Starting AP mode for configuration...");
        WiFi.softAP("ESP32-MQTT-Setup");
        DLOG_I(LOG_APP, "AP IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        DLOG_I(LOG_APP, "✓ WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
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
    auto* mqttPtr = mqtt.get();  // Keep for WebUI provider
    core.addComponent(std::move(mqtt));
    
    // Register EventBus listeners (capture clientId before config is moved)
    String clientId = mqttConfig.clientId;
    
    core.on<bool>("mqtt/connected", [&core, clientId](const bool&) {
        DLOG_I(LOG_APP, "📡 MQTT Connected!");
        
        // Publish online status via EventBus
        DomoticsCore::Components::MQTTPublishEvent pubEv{};
        String statusTopic = clientId + "/status";
        strncpy(pubEv.topic, statusTopic.c_str(), sizeof(pubEv.topic) - 1);
        strncpy(pubEv.payload, "online", sizeof(pubEv.payload) - 1);
        pubEv.qos = 1;
        pubEv.retain = true;
        core.emit("mqtt/publish", pubEv);
        
        // Subscribe to command topics via EventBus
        DomoticsCore::Components::MQTTSubscribeEvent subEv{};
        String commandTopic = clientId + "/command/#";
        strncpy(subEv.topic, commandTopic.c_str(), sizeof(subEv.topic) - 1);
        subEv.qos = 1;
        core.emit("mqtt/subscribe", subEv);
        
        DLOG_I(LOG_APP, "  ✓ Published online status");
        DLOG_I(LOG_APP, "  ✓ Subscribed to commands");
    });
    
    core.on<bool>("mqtt/disconnected", [](const bool&) {
        DLOG_W(LOG_APP, "📡 MQTT Disconnected");
    });
    
    core.on<DomoticsCore::Components::MQTTMessageEvent>("mqtt/message", [](const DomoticsCore::Components::MQTTMessageEvent& ev) {
        DLOG_I(LOG_APP, "📨 Command received: %s = %s", ev.topic, ev.payload);
    });
    
    // Initialize components
    DLOG_I(LOG_APP, "Initializing components...");
    core.begin();
    
    // Register WebUI provider for MQTT
    if (webuiPtr && mqttPtr) {
        webuiPtr->registerProviderWithComponent(new MQTTWebUI(mqttPtr), mqttPtr);
        DLOG_I(LOG_APP, "✓ MQTT WebUI provider registered");
    }
    
    DLOG_I(LOG_APP, "✓ Setup complete!");
    
    if (WiFi.status() == WL_CONNECTED) {
        DLOG_I(LOG_APP, "========================================");
        DLOG_I(LOG_APP, "WebUI: http://%s", WiFi.localIP().toString().c_str());
        DLOG_I(LOG_APP, "========================================");
    } else {
        DLOG_I(LOG_APP, "========================================");
        DLOG_I(LOG_APP, "WebUI: http://%s", WiFi.softAPIP().toString().c_str());
        DLOG_I(LOG_APP, "========================================");
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
                DLOG_I(LOG_APP, "📤 Published telemetry");
            }
        }
    }
    
    // No delay() - fully non-blocking
}
