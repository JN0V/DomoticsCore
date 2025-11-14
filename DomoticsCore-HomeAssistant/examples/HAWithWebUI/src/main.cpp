/**
 * @file main.cpp
 * @brief Home Assistant with WebUI Example
 * 
 * Demonstrates:
 * - HA auto-discovery with multiple entity types
 * - Web interface for configuration and monitoring
 * - Real-time sensor updates
 * - Controllable switches and lights
 * - Device information and availability
 * 
 * Requirements:
 * - WiFi network
 * - MQTT broker
 * - Home Assistant with MQTT integration
 */

#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/HomeAssistantWebUI.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"

using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;
using namespace DomoticsCore::Components::HomeAssistant;

// ========== Configuration ==========

// WiFi credentials
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// MQTT broker
const char* MQTT_BROKER = "mqtt.example.com";  // or IP address like "192.168.1.100"
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "";
const char* MQTT_PASSWORD = "";

// Hardware
const int RELAY_PIN = 2;  // Built-in LED on most ESP32 boards
const int SENSOR_UPDATE_INTERVAL = 30000;

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay sensorTimer(SENSOR_UPDATE_INTERVAL);
HomeAssistantComponent* haPtr = nullptr;
bool lastRelayState = false;
bool initialStatePublished = false;

// Simulated sensors
float getTemperature() { return 20.0 + (random(0, 100) / 10.0); }
float getHumidity() { return 40.0 + (random(0, 200) / 10.0); }

// ========== Setup ==========

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - HA with WebUI");
    DLOG_I(LOG_APP, "========================================");
    
    // Initialize hardware
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    
    // Connect to WiFi
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Utils::NonBlockingDelay wifiDelay(500);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        if (wifiDelay.isReady()) {
            attempts++;
        }
        yield();  // Feed watchdog
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        DLOG_E(LOG_APP, "Failed to connect to WiFi");
        while (1) delay(1000);
    }
    
    DLOG_I(LOG_APP, "WiFi connected: %s", WiFi.localIP().toString().c_str());
    
    // WebUI Component
    WebUIConfig webuiCfg;
    webuiCfg.deviceName = "ESP32 HA Demo";
    webuiCfg.port = 80;
    auto webui = std::make_unique<WebUIComponent>(webuiCfg);
    auto* webuiPtr = webui.get();
    core.addComponent(std::move(webui));
    
    // MQTT Component
    MQTTConfig mqttCfg;
    mqttCfg.broker = MQTT_BROKER;
    mqttCfg.port = MQTT_PORT;
    mqttCfg.username = MQTT_USER;
    mqttCfg.password = MQTT_PASSWORD;
    mqttCfg.clientId = "esp32-ha-webui-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    mqttCfg.enableLWT = true;
    mqttCfg.lwtTopic = "homeassistant/esp32-webui-demo/availability";
    mqttCfg.lwtMessage = "offline";
    mqttCfg.lwtQoS = 1;
    mqttCfg.lwtRetain = true;
    
    core.addComponent(std::make_unique<MQTTComponent>(mqttCfg));
    
    // Home Assistant Component (communicates with MQTT via EventBus)
    HAConfig haCfg;
    haCfg.nodeId = "esp32-webui-demo";
    haCfg.deviceName = "ESP32 WebUI Demo";
    haCfg.manufacturer = "DomoticsCore";
    haCfg.model = "ESP32-DevKit";
    haCfg.swVersion = "1.0.0";
    haCfg.configUrl = "http://" + WiFi.localIP().toString();
    haCfg.suggestedArea = "Office";
    
    auto ha = std::make_unique<HomeAssistantComponent>(haCfg);
    haPtr = ha.get();
    
    // Add entities
    haPtr->addSensor("temperature", "Temperature", "°C", "temperature", "mdi:thermometer");
    haPtr->addSensor("humidity", "Humidity", "%", "humidity", "mdi:water-percent");
    haPtr->addSensor("uptime", "Uptime", "s", "", "mdi:clock-outline");
    haPtr->addSensor("wifi_signal", "WiFi Signal", "dBm", "signal_strength", "mdi:wifi");
    haPtr->addSensor("free_heap", "Free Heap", "bytes", "", "mdi:memory");
    
    // Relay switch (controls built-in LED on pin 2)
    haPtr->addSwitch("relay", "Relay", [](bool state) {
        digitalWrite(RELAY_PIN, state ? HIGH : LOW);
        DLOG_I(LOG_APP, "Relay: %s", state ? "ON" : "OFF");
        // NOTE: State published separately in loop() to avoid recursion
    }, "mdi:electric-switch");
    
    // Restart button
    haPtr->addButton("restart", "Restart", []() {
        DLOG_I(LOG_APP, "Restart triggered from HA");
        delay(1000);
        ESP.restart();
    }, "mdi:restart");
    
    core.addComponent(std::move(ha));
    
    // Initialize core
    if (!core.begin()) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        while (1) delay(1000);
    }
    
    // Register HA WebUI provider
    if (webuiPtr && haPtr) {
        webuiPtr->registerProviderWithComponent(new HomeAssistantWebUI(haPtr), haPtr);
        DLOG_I(LOG_APP, "HA WebUI provider registered");
    }
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "System ready!");
    DLOG_I(LOG_APP, "Web interface: http://%s", WiFi.localIP().toString().c_str());
    DLOG_I(LOG_APP, "MQTT Broker: %s:%d", MQTT_BROKER, MQTT_PORT);
    DLOG_I(LOG_APP, "Registered %d HA entities (5 sensors, 1 switch, 1 button)", haPtr->getStatistics().entityCount);
    DLOG_I(LOG_APP, "========================================");
}

// ========== Loop ==========

void loop() {
    core.loop();
    
    // Update sensors periodically
    if (sensorTimer.isReady() && haPtr) {
        haPtr->publishState("temperature", getTemperature());
        haPtr->publishState("humidity", getHumidity());
        haPtr->publishState("uptime", (float)(millis() / 1000));
        haPtr->publishState("wifi_signal", (float)WiFi.RSSI());
        haPtr->publishState("free_heap", (float)ESP.getFreeHeap());
        
        const auto& stats = haPtr->getStatistics();
        DLOG_I(LOG_APP, "Sensors updated | States: %d, Commands: %d",
               stats.stateUpdates, stats.commandsReceived);
    }
    
    // Publish initial state once HA is ready
    if (!initialStatePublished && haPtr && haPtr->isReady()) {
        bool currentRelayState = digitalRead(RELAY_PIN) == HIGH;
        haPtr->publishState("relay", currentRelayState);
        lastRelayState = currentRelayState;
        initialStatePublished = true;
        DLOG_I(LOG_APP, "Published initial relay state: %s", currentRelayState ? "ON" : "OFF");
    }
    
    // Publish relay state only when it changes
    if (haPtr && haPtr->isMQTTConnected()) {
        bool currentRelayState = digitalRead(RELAY_PIN) == HIGH;
        
        if (currentRelayState != lastRelayState) {
            haPtr->publishState("relay", currentRelayState);
            DLOG_I(LOG_APP, "Relay state changed: %s", currentRelayState ? "ON" : "OFF");
            lastRelayState = currentRelayState;
        }
    }
}
