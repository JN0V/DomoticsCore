/**
 * @file main.cpp
 * @brief RemoteConsole with WebUI Example
 *
 * Demonstrates RemoteConsole component with WebUI integration.
 * Features:
 * - Telnet-based remote console on port 23
 * - Real-time log streaming via telnet
 * - Built-in commands (help, info, logs, clear, level, filter, reboot, quit)
 * - WebUI for system monitoring and configuration
 * - WiFi AP fallback if STA connection fails
 */

#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/RemoteConsoleWebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

#define LOG_APP "APP"

// WiFi credentials
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// AP fallback
const char* AP_SSID = "DomoticsCore-Console";
const char* AP_PASSWORD = "console123";

Core core;

void setup() {
    // Initialize logging
    HAL::Platform::initializeLogging();

    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - RemoteConsole + WebUI");
    DLOG_I(LOG_APP, "========================================");

    // Connect to WiFi using HAL
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    HAL::WiFiHAL::init();
    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);
    HAL::WiFiHAL::connect(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (!HAL::WiFiHAL::isConnected() && attempts < 40) {
        HAL::Platform::delayMs(500);
        attempts++;
    }

    if (!HAL::WiFiHAL::isConnected()) {
        DLOG_W(LOG_APP, "WiFi STA connection failed, starting AP mode...");
        HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::AccessPoint);
        HAL::WiFiHAL::startAP(AP_SSID, AP_PASSWORD);
        DLOG_I(LOG_APP, "AP started: %s", AP_SSID);
    } else {
        DLOG_I(LOG_APP, "WiFi connected: %s", HAL::WiFiHAL::getLocalIP().c_str());
    }

    // Configure WebUI
    auto webui = std::make_unique<WebUIComponent>();

    // Configure RemoteConsole
    RemoteConsoleConfig consoleConfig;
    consoleConfig.enabled = true;
    consoleConfig.port = 23;  // Standard telnet port
    consoleConfig.maxClients = 3;  // Allow up to 3 concurrent telnet clients
    consoleConfig.colorOutput = true;  // ANSI colors in telnet
    consoleConfig.allowCommands = true;  // Enable built-in commands
    consoleConfig.defaultLogLevel = LOG_LEVEL_INFO;

    auto console = std::make_unique<RemoteConsoleComponent>(consoleConfig);

    // Add components to core
    core.addComponent(std::move(webui));
    core.addComponent(std::move(console));

    // Initialize
    core.begin();

    // Register WebUI provider for RemoteConsole
    auto* webuiPtr = core.getComponent<WebUIComponent>("WebUI");
    auto* consolePtr = core.getComponent<RemoteConsoleComponent>("RemoteConsole");
    if (webuiPtr && consolePtr) {
        auto* provider = new DomoticsCore::Components::WebUI::RemoteConsoleWebUI(consolePtr);
        webuiPtr->registerProviderWithComponent(provider, consolePtr);
        provider->init(webuiPtr);
    }

    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "System ready!");
    DLOG_I(LOG_APP, "Telnet: %s:%d", HAL::WiFiHAL::getLocalIP().c_str(), consoleConfig.port);
    DLOG_I(LOG_APP, "WebUI: http://%s", HAL::WiFiHAL::getLocalIP().c_str());
    DLOG_I(LOG_APP, "========================================");
}

void loop() {
    core.loop();

    // Example: Log periodic status
    static unsigned long lastStatusLog = 0;
    unsigned long now = HAL::Platform::getMillis();
    if (now - lastStatusLog > 30000) {  // Every 30 seconds
        lastStatusLog = now;
        DLOG_I(LOG_APP, "Uptime: %lu seconds, Free heap: %u bytes",
               now / 1000, HAL::getFreeHeap());
    }

    HAL::Platform::delayMs(10);
}
