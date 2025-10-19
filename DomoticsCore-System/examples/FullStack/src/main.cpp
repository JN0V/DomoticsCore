/**
 * @file main.cpp
 * @brief DomoticsCore - Full Stack Example
 * 
 * This example demonstrates the FULL STACK configuration:
 * - WiFi (with automatic AP mode fallback)
 * - LED (automatic status visualization)
 * - RemoteConsole (telnet debugging)
 * - WebUI (web interface on port 8080)
 * - NTP (time synchronization)
 * - Storage (persistent configuration)
 * - MQTT (message broker integration)
 * - Home Assistant (auto-discovery)
 * - OTA (over-the-air updates)
 * 
 * Perfect for:
 * - Complete IoT solutions
 * - Home automation
 * - Production deployments with MQTT
 * - Enterprise applications
 * 
 * Requires:
 * - MQTT broker (e.g., Mosquitto)
 * - OTA password (for security)
 */

#include <DomoticsCore/System.h>

using namespace DomoticsCore;

#define LOG_APP "APP"

// ============================================================================
// CONFIGURATION
// ============================================================================

// WiFi credentials
const char* WIFI_SSID = "";  // Leave empty for AP mode
const char* WIFI_PASSWORD = "";

// MQTT broker (required for FullStack)
const char* MQTT_BROKER = ""; // Leave empty to disable MQTT
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "";  // Optional
const char* MQTT_PASSWORD = "";  // Optional

// OTA password (required for security)
const char* OTA_PASSWORD = "admin123";  // CHANGE THIS!

// ============================================================================
// YOUR APPLICATION CODE
// ============================================================================

// Example: Temperature sensor (simulated)
float readTemperature() {
    // TODO: Replace with real sensor (DHT22, DS18B20, etc.)
    return 22.5 + (random(0, 100) / 100.0);
}

// Example: Relay control
void setRelay(bool state) {
    // TODO: Replace with real relay control
    digitalWrite(5, state ? HIGH : LOW);
    DLOG_I(LOG_APP, "Relay: %s", state ? "ON" : "OFF");
}

// ============================================================================
// SYSTEM SETUP
// ============================================================================

System* domotics = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // ========================================================================
    // FULL STACK Configuration - Everything enabled!
    // ========================================================================
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "FullStackDevice";
    config.firmwareVersion = "1.0.0";
    config.wifiSSID = WIFI_SSID;
    config.wifiPassword = WIFI_PASSWORD;
    
    // MQTT configuration
    config.mqttBroker = MQTT_BROKER;
    config.mqttPort = MQTT_PORT;
    config.mqttUser = MQTT_USER;
    config.mqttPassword = MQTT_PASSWORD;
    config.mqttClientId = config.deviceName;  // Use device name as client ID
    
    // OTA configuration
    config.otaPassword = OTA_PASSWORD;
    
    // Note: FullStack includes EVERYTHING:
    // - WiFi, LED, Console (Minimal)
    // - WebUI, NTP, Storage (Standard)
    // - MQTT, Home Assistant, OTA (FullStack), SystemInfo
    //
    // Requires MQTT broker and OTA password!
    
    // Create system
    domotics = new System(config);
    
    // Register custom console commands
    domotics->registerCommand("temp", [](const String& args) {
        float temp = readTemperature();
        return String("Temperature: ") + String(temp, 1) + "°C\n";
    });
    
    domotics->registerCommand("relay", [](const String& args) {
        if (args == "on") {
            setRelay(true);
            return String("Relay turned ON\n");
        } else if (args == "off") {
            setRelay(false);
            return String("Relay turned OFF\n");
        } else {
            return String("Usage: relay on|off\n");
        }
    });
    
    // Initialize system (AUTOMATIC: WiFi, LED, Console, State management)
    if (!domotics->begin()) {
        DLOG_E(LOG_APP, "System initialization failed!");
        while (1) delay(1000);
    }
    
    // YOUR CUSTOM INITIALIZATION
    pinMode(5, OUTPUT);  // Relay pin
    
    DLOG_I(LOG_APP, "Application ready!");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

// Non-blocking timer for sensor readings
DomoticsCore::Utils::NonBlockingDelay sensorTimer(10000);  // 10 seconds

void loop() {
    // System loop (handles everything automatically)
    domotics->loop();
    
    // YOUR APPLICATION CODE HERE
    if (sensorTimer.isReady()) {
        float temp = readTemperature();
        DLOG_I(LOG_APP, "Temperature: %.1f°C", temp);
        
        // Example: Control relay based on temperature
        if (temp > 25.0) {
            setRelay(true);  // Turn on cooling
        } else if (temp < 20.0) {
            setRelay(false);  // Turn off cooling
        }
    }
}
