/**
 * @file main.cpp
 * @brief NTP with WebUI example - Full web interface
 *
 * Demonstrates:
 * - NTP component with WebUI integration
 * - Real-time clock display on web interface
 * - Web-based configuration (servers, timezone, sync interval)
 * - Manual sync button
 * - Statistics dashboard
 *
 * Access the web interface at: http://<device-ip>
 */

#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/NTP.h>
#include <DomoticsCore/NTPWebUI.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// ========== Configuration ==========

// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay logTimer(60000);  // Log every minute

// ========== Setup ==========

void setup() {
    HAL::initializeLogging(115200);

    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - NTP with WebUI");
    DLOG_I(LOG_APP, "========================================");

    // Connect to WiFi using HAL
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    HAL::WiFiHAL::init();
    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);
    HAL::WiFiHAL::connect(WIFI_SSID, WIFI_PASSWORD);

    while (!HAL::WiFiHAL::isConnected()) {
        HAL::delay(500);
        DLOG_D(LOG_APP, ".");
    }

    DLOG_I(LOG_APP, "WiFi connected!");
    DLOG_I(LOG_APP, "IP address: %s", HAL::WiFiHAL::getLocalIP().c_str());

    // Configure WebUI
    WebUIConfig webuiCfg;
    webuiCfg.deviceName = "NTP Demo";
    webuiCfg.port = 80;

    auto webui = std::make_unique<WebUIComponent>(webuiCfg);
    auto* webuiPtr = webui.get();

    core.addComponent(std::move(webui));

    // Configure NTP
    NTPConfig ntpCfg;
    ntpCfg.enabled = true;
    ntpCfg.servers = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};
    ntpCfg.syncInterval = 3600;  // 1 hour
    ntpCfg.timezone = Timezones::CET;  // Change to your timezone

    auto ntp = std::make_unique<NTPComponent>(ntpCfg);
    auto* ntpPtr = ntp.get();

    // Register sync callback
    ntpPtr->onSync([ntpPtr](bool success) {
        if (success) {
            DLOG_I(LOG_APP, "Time synced: %s", ntpPtr->getFormattedTime().c_str());
        } else {
            DLOG_E(LOG_APP, "Time sync failed!");
        }
    });

    core.addComponent(std::move(ntp));

    // Initialize core
    if (!core.begin()) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        while (1) HAL::delay(1000);
    }

    // Register NTP WebUI provider (time will appear in header info zone automatically)
    if (webuiPtr && ntpPtr) {
        webuiPtr->registerProviderWithComponent(new NTPWebUI(ntpPtr), ntpPtr);
        DLOG_I(LOG_APP, "NTP WebUI provider registered");
    }

    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "System ready!");
    DLOG_I(LOG_APP, "Web interface: http://%s", HAL::WiFiHAL::getLocalIP().c_str());
    DLOG_I(LOG_APP, "========================================");
}

// ========== Loop ==========

void loop() {
    core.loop();

    // Log time periodically
    if (logTimer.isReady()) {

        auto* ntp = core.getComponent<NTPComponent>("NTP");
        if (ntp && ntp->isSynced()) {
            DLOG_I(LOG_APP, "[%s] Uptime: %s",
                   ntp->getFormattedTime().c_str(),
                   ntp->getFormattedUptime().c_str());
        }
    }
}
