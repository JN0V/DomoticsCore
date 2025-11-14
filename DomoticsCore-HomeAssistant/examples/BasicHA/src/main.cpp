/**
 * @file main.cpp
 * @brief Basic Home Assistant MQTT Discovery Example
 * 
 * Demonstrates:
 * - Automatic entity discovery in Home Assistant
 * - Sensor state publishing (temperature, humidity, uptime)
 * - Switch control (relay)
 * - Button trigger (restart)
 * - Device information and availability
 * 
 * Requirements:
 * - WiFi network
 * - MQTT broker
 * - Home Assistant with MQTT integration enabled
 */

#include <WiFi.h>  // Using ESP32 WiFi library for simple connection (DomoticsCore-Wifi not needed for basic examples)
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

// ========== Configuration ==========
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// MQTT broker
const char* MQTT_BROKER = "YourMQTTBroker";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "";          // Leave empty if no auth
const char* MQTT_PASSWORD = "";

// Hardware pins
const int RELAY_PIN = 2;             // Built-in LED as relay example
const int SENSOR_UPDATE_INTERVAL = 30000;  // Update sensors every 30 seconds

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay sensorTimer(SENSOR_UPDATE_INTERVAL);
Utils::NonBlockingDelay aliveTimer(5000);       // System alive message every 5 seconds
HomeAssistantComponent* haPtr = nullptr;
bool lastRelayState = false;  // Track relay state changes
bool initialStatePublished = false;  // Track if initial state sent to HA

// Simulated sensor readings (replace with real sensors)
float getTemperature() {
    // Simulate temperature reading (replace with real sensor)
    return 20.0 + (random(0, 100) / 10.0);  // 20-30°C
}

float getHumidity() {
    // Simulate humidity reading (replace with real sensor)
    return 40.0 + (random(0, 200) / 10.0);  // 40-60%
}


void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - Home Assistant Integration - Basic example");
    DLOG_I(LOG_APP, "========================================");
    
    // Initialize GPIO
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    
    // Connect to WiFi
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        DLOG_I(LOG_APP, "WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
    } else {
        DLOG_E(LOG_APP, "WiFi connection failed!");
        while (1) delay(1000);
    }
    // Configure MQTT
    MQTTConfig mqttCfg;
    mqttCfg.broker = MQTT_BROKER;
    mqttCfg.port = MQTT_PORT;
    mqttCfg.username = MQTT_USER;
    mqttCfg.password = MQTT_PASSWORD;
    mqttCfg.clientId = "esp32-ha-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    mqttCfg.enableLWT = true;
    mqttCfg.lwtTopic = "homeassistant/esp32-demo/availability";
    mqttCfg.lwtMessage = "offline";
    mqttCfg.lwtQoS = 1;
    mqttCfg.lwtRetain = true;
    
    core.addComponent(std::make_unique<MQTTComponent>(mqttCfg));
    
    // Configure Home Assistant (communicates with MQTT via EventBus)
    HAConfig haCfg;
    haCfg.nodeId = "esp32-demo";
    haCfg.deviceName = "ESP32 Demo Device";
    haCfg.manufacturer = "DomoticsCore";
    haCfg.model = "ESP32-DevKit";
    haCfg.swVersion = "1.0.0";
    haCfg.discoveryPrefix = "homeassistant";
    haCfg.configUrl = "http://" + WiFi.localIP().toString();
    haCfg.suggestedArea = "Office";
    
    auto ha = std::make_unique<HomeAssistantComponent>(haCfg);
    haPtr = ha.get();
    
    // ========== Add Entities ==========
    
    // Temperature sensor
    haPtr->addSensor("temperature", "Temperature", "°C", "temperature", "mdi:thermometer");
    
    // Humidity sensor
    haPtr->addSensor("humidity", "Humidity", "%", "humidity", "mdi:water-percent");
    
    // Uptime sensor
    haPtr->addSensor("uptime", "Uptime", "s", "", "mdi:clock-outline");
    
    // WiFi signal sensor
    haPtr->addSensor("wifi_signal", "WiFi Signal", "dBm", "signal_strength", "mdi:wifi");
    
    // Free heap sensor
    haPtr->addSensor("free_heap", "Free Heap", "bytes", "", "mdi:memory");
    
    // Relay switch (controllable from HA)
    haPtr->addSwitch("relay", "Relay", [](bool state) {
        digitalWrite(RELAY_PIN, state ? HIGH : LOW);
        DLOG_I(LOG_APP, "Relay set to: %s", state ? "ON" : "OFF");
        // NOTE: State is published separately in loop() to avoid recursion
    }, "mdi:electric-switch");
    
    // Restart button
    haPtr->addButton("restart", "Restart", []() {
        DLOG_I(LOG_APP, "Restart button pressed from Home Assistant");
        delay(1000);
        ESP.restart();
    }, "mdi:restart");
    
    core.addComponent(std::move(ha));
    
    // Initialize core
    if (!core.begin()) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        while (1) delay(1000);
    }
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "System ready!");
    DLOG_I(LOG_APP, "MQTT Broker: %s:%d", MQTT_BROKER, MQTT_PORT);
    DLOG_I(LOG_APP, "Node ID: %s", haCfg.nodeId.c_str());
    DLOG_I(LOG_APP, "Registered %d entities", haPtr->getStatistics().entityCount);
    DLOG_I(LOG_APP, "========================================");
}

// ========== Loop ==========

void loop() {
    // Run core loop (handles MQTT, component updates, etc.)
    core.loop();
    
    // Update sensor values periodically
    if (sensorTimer.isReady() && haPtr) {
        // Read and publish sensor values
        float temp = getTemperature();
        float humidity = getHumidity();
        uint32_t uptime = millis() / 1000;
        uint32_t freeHeap = ESP.getFreeHeap();
        
        int rssi = WiFi.RSSI();
        
        haPtr->publishState("temperature", temp);
        haPtr->publishState("humidity", humidity);
        haPtr->publishState("uptime", (float)uptime);
        haPtr->publishState("wifi_signal", (float)rssi);
        haPtr->publishState("free_heap", (float)freeHeap);
        
        DLOG_I(LOG_APP, "Published sensors: Temp=%.1f°C, Humidity=%.1f%%, Uptime=%ds",
               temp, humidity, uptime);
    }
    
    // Publish initial state once HA is ready
    if (!initialStatePublished && haPtr && haPtr->isReady()) {
        bool currentRelayState = digitalRead(RELAY_PIN) == HIGH;
        haPtr->publishState("relay", currentRelayState);
        lastRelayState = currentRelayState;
        initialStatePublished = true;
        DLOG_I(LOG_APP, "Published initial relay state: %s", currentRelayState ? "ON" : "OFF");
    }
    
    // Publish relay state only when it changes (not on timer!)
    if (haPtr && haPtr->isMQTTConnected()) {
        bool currentRelayState = digitalRead(RELAY_PIN) == HIGH;
        
        // Publish only on state change
        if (currentRelayState != lastRelayState) {
            haPtr->publishState("relay", currentRelayState);
            DLOG_I(LOG_APP, "Relay state changed: %s", currentRelayState ? "ON" : "OFF");
            lastRelayState = currentRelayState;
        }
    }
    
    // Heartbeat log every 5 seconds
    if (aliveTimer.isReady()) {
        DLOG_I(LOG_APP, "System alive, uptime: %ds, MQTT: %s", 
               millis()/1000, haPtr && haPtr->isMQTTConnected() ? "connected" : "disconnected");
    }
}
