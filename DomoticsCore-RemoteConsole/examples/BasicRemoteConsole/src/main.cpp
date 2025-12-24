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

#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

#define LOG_APP "APP"

// WiFi credentials
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// Core and components
Core core;
Utils::NonBlockingDelay logTimer(5000);  // Log every 5 seconds

void setup() {
    HAL::Platform::initializeLogging();
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - RemoteConsole Example");
    DLOG_I(LOG_APP, "========================================");
    
    // Connect to WiFi using HAL
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    HAL::WiFiHAL::init();
    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);
    HAL::WiFiHAL::connect(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (!HAL::WiFiHAL::isConnected() && attempts < 40) {
        HAL::Platform::delayMs(500);
        DLOG_D(LOG_APP, ".");
        attempts++;
    }
    
    if (!HAL::WiFiHAL::isConnected()) {
        DLOG_E(LOG_APP, "WiFi connection failed!");
        while (1) HAL::Platform::delayMs(1000);
    }
    
    DLOG_I(LOG_APP, "WiFi connected: %s", HAL::WiFiHAL::getLocalIP().c_str());
    
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
        while (1) HAL::Platform::delayMs(1000);
    }
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "System ready!");
    DLOG_I(LOG_APP, "Telnet: %s:%d", HAL::WiFiHAL::getLocalIP().c_str(), config.port);
    DLOG_I(LOG_APP, "========================================");
}

void loop() {
    core.loop();
    
    // Generate some logs periodically
    if (logTimer.isReady()) {
        static int counter = 0;
        counter++;
        
        DLOG_I(LOG_APP, "Periodic log message #%d", counter);
        DLOG_D(LOG_APP, "Debug info: heap=%d, uptime=%lds", 
            HAL::getFreeHeap(), HAL::getMillis() / 1000);
        
        if (counter % 3 == 0) {
            DLOG_W(LOG_APP, "Warning: This is a test warning message");
        }
        
        if (counter % 5 == 0) {
            DLOG_E(LOG_APP, "Error: This is a test error message");
        }
    }
}
