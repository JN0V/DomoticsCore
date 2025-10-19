/**
 * @file main.cpp
 * @brief DomoticsCore - Minimal Example
 * 
 * This example demonstrates the MINIMAL configuration:
 * - WiFi (with automatic AP mode fallback)
 * - LED (automatic status visualization)
 * - RemoteConsole (telnet debugging)
 * 
 * Perfect for:
 * - Simple sensors
 * - Basic automation
 * - Learning DomoticsCore
 * - Quick prototyping
 * 
 * Just ~50 lines of code for a complete IoT device!
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
    // MINIMAL Configuration - WiFi, LED, Console only
    // ========================================================================
    SystemConfig config = SystemConfig::minimal();
    config.deviceName = "MinimalDevice";
    config.firmwareVersion = "1.0.0";
    config.wifiSSID = WIFI_SSID;
    config.wifiPassword = WIFI_PASSWORD;
    
    // Note: If WIFI_SSID is empty, system will:
    // 1. Start AP mode: "MyDevice-XXXX"
    // 2. You configure via web interface at http://192.168.4.1/wifi
    // 3. Credentials saved to Storage
    // 4. Next boot: connects to your WiFi automatically
    
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
        DLOG_E(LOG_APP, "LED will continue blinking to show error state");
        // Don't halt - let loop() run so LED can show error pattern
    } else {
        DLOG_I(LOG_APP, "System initialized successfully!");
        DLOG_I(LOG_APP, "LED should now be showing breathing pattern (3s cycle)");
        DLOG_I(LOG_APP, "Watch the LED on GPIO 2 - it should slowly fade in and out");
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
