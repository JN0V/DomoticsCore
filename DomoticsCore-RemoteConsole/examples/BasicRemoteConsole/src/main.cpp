/**
 * @file main.cpp
 * @brief Basic RemoteConsole Example
 * 
 * Demonstrates:
 * - Telnet-based remote console
 * - Real-time log streaming
 * - Command execution
 * - Runtime log level control
 */

#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

#define LOG_APP "APP"

// WiFi credentials
const char* WIFI_SSID = "YourWifiSSID";
const char* WIFI_PASSWORD = "YourWifiPassword";

// Core and components
Core core;
Utils::NonBlockingDelay logTimer(5000);  // Log every 5 seconds

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - RemoteConsole Example");
    DLOG_I(LOG_APP, "========================================");
    
    // Connect to WiFi
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        DLOG_E(LOG_APP, "WiFi connection failed!");
        while (1) delay(1000);
    }
    
    DLOG_I(LOG_APP, "WiFi connected: %s", WiFi.localIP().toString().c_str());
    
    // Configure RemoteConsole
    RemoteConsoleConfig config;
    config.enabled = true;
    config.port = 23;
    config.bufferSize = 500;
    config.colorOutput = true;
    config.defaultLogLevel = LOG_LEVEL_INFO;
    
    auto console = std::make_unique<RemoteConsoleComponent>(config);
    auto* consolePtr = console.get();
    
    // Register custom command
    consolePtr->registerCommand("test", [](const String& args) {
        return "Test command executed with args: " + args + "\n";
    });
    
    consolePtr->registerCommand("sensors", [](const String& args) {
        String result = "\nSensor Values:\n";
        result += "  Temperature: 22.5°C\n";
        result += "  Humidity: 45%\n";
        result += "  Pressure: 1013 hPa\n";
        return result;
    });
    
    // Add to core
    core.addComponent(std::move(console));
    
    // Initialize
    if (!core.begin()) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        while (1) delay(1000);
    }
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "System ready!");
    DLOG_I(LOG_APP, "Telnet: %s:%d", WiFi.localIP().toString().c_str(), config.port);
    DLOG_I(LOG_APP, "========================================");
}

void loop() {
    core.loop();
    
    // Generate some logs periodically
    if (logTimer.isReady()) {
        static int counter = 0;
        counter++;
        
        DLOG_I(LOG_APP, "Periodic log message #%d", counter);
        DLOG_D(LOG_APP, "Debug info: heap=%d, uptime=%ds", 
               ESP.getFreeHeap(), millis() / 1000);
        
        if (counter % 3 == 0) {
            DLOG_W(LOG_APP, "Warning: This is a test warning message");
        }
        
        if (counter % 5 == 0) {
            DLOG_E(LOG_APP, "Error: This is a test error message");
        }
    }
}
