/**
 * @file main.cpp
 * @brief DomoticsCore - Standard Example
 * 
 * This example demonstrates the STANDARD configuration:
 * - WiFi (with automatic AP mode fallback)
 * - LED (automatic status visualization)
 * - RemoteConsole (telnet debugging)
 * - WebUI (web interface on port 8080)
 * - NTP (time synchronization)
 * - Storage (persistent configuration)
 * 
 * Perfect for:
 * - Most applications
 * - Production deployments
 * - No external services needed
 * - Complete standalone IoT device
 * 
 * Everything works without MQTT broker or external dependencies!
 */

#include <DomoticsCore/System.h>

using namespace DomoticsCore;

#define LOG_APP "APP"

// ============================================================================
// CONFIGURATION
// ============================================================================

// Option 1: Leave empty for automatic AP mode on first boot
// Device will create AP "MyDevice-XXXX" and you configure via web interface
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// Option 2: Hardcode credentials (not recommended for production)
// const char* WIFI_SSID = "YOUR_WIFI_SSID";
// const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

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
    // STANDARD Configuration - WiFi, LED, Console, WebUI, NTP, Storage
    // ========================================================================
    SystemConfig config = SystemConfig::standard();
    config.deviceName = "StandardDevice";
    config.firmwareVersion = "1.0.0";
    config.wifiSSID = WIFI_SSID;
    config.wifiPassword = WIFI_PASSWORD;
    
    // Note: Standard includes:
    // - WebUI on port 8080 (http://<ip>:8080)
    // - NTP time sync (automatic)
    // - Storage for persistent config
    // - Everything from Minimal
    //
    // No external services needed - works standalone!
    
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
