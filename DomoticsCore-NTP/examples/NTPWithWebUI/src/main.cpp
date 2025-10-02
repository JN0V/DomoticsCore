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

#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/NTP.h>
#include <DomoticsCore/NTPWebUI.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// ========== Configuration ==========

// WiFi credentials
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay logTimer(60000);  // Log every minute

// ========== Setup ==========

void setup() {
    Serial.begin(115200);  // Required for ESP32 logging output
    delay(1000);  // Allow serial to initialize
    
    DLOG_I(LOG_CORE, "\n========================================");
    DLOG_I(LOG_CORE, "DomoticsCore - NTP with WebUI");
    DLOG_I(LOG_CORE, "========================================\n");
    
    // Connect to WiFi
    DLOG_I(LOG_CORE, "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DLOG_D(LOG_CORE, ".");
    }
    
    DLOG_I(LOG_CORE, "\nWiFi connected!");
    DLOG_I(LOG_CORE, "IP address: %s", WiFi.localIP().toString().c_str());
    
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
            DLOG_I(LOG_CORE, "✅ Time synced: %s", ntpPtr->getFormattedTime().c_str());
        } else {
            DLOG_E(LOG_CORE, "❌ Time sync failed!");
        }
    });
    
    core.addComponent(std::move(ntp));
    
    // Initialize core
    if (!core.begin()) {
        DLOG_E(LOG_CORE, "Failed to initialize core!");
        while (1) delay(1000);
    }
    
    // Register NTP WebUI provider (time will appear in header info zone automatically)
    if (webuiPtr && ntpPtr) {
        webuiPtr->registerProviderWithComponent(new NTPWebUI(ntpPtr), ntpPtr);
        DLOG_I(LOG_CORE, "NTP WebUI provider registered");
    }
    
    DLOG_I(LOG_CORE, "\n========================================");
    DLOG_I(LOG_CORE, "System ready!");
    DLOG_I(LOG_CORE, "Web interface: http://%s", WiFi.localIP().toString().c_str());
    DLOG_I(LOG_CORE, "========================================\n");
}

// ========== Loop ==========

void loop() {
    core.loop();
    
    // Log time periodically
    if (logTimer.isReady()) {
        
        auto* ntp = core.getComponent<NTPComponent>("NTP");
        if (ntp && ntp->isSynced()) {
            DLOG_I(LOG_CORE, "[%s] Uptime: %s", 
                   ntp->getFormattedTime().c_str(),
                   ntp->getFormattedUptime().c_str());
        }
    }
}
