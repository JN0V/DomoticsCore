/**
 * @file main.cpp
 * @brief Basic NTP example - Simple time synchronization
 * 
 * Demonstrates:
 * - NTP time synchronization
 * - Timezone configuration
 * - Formatted time strings
 * - Sync callbacks
 * - System uptime
 */

#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/NTP.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Custom application log tag
#define LOG_APP "APP"

// ========== Configuration ==========

// Credentials. Put a `secrets.h` at the repository root — or anywhere on the
// include path — to run this example against a real network without editing,
// and without committing, anything. Build with the root on the include path:
//     PLATFORMIO_BUILD_FLAGS="-I/path/to/DomoticsCore" pio run -e esp8266dev
// Without the file the placeholders below are unchanged.
#if defined(__has_include)
#  if __has_include(<secrets.h>)
#    include <secrets.h>
#  endif
#endif

#ifndef DC_WIFI_SSID
#  define DC_WIFI_SSID "YourWiFiSSID"
#endif
#ifndef DC_WIFI_PASSWORD
#  define DC_WIFI_PASSWORD "YourWiFiPassword"
#endif
#ifndef DC_MQTT_BROKER
#  define DC_MQTT_BROKER "mqtt.example.com"
#endif
#ifndef DC_MQTT_PORT
#  define DC_MQTT_PORT 1883
#endif
#ifndef DC_MQTT_USER
#  define DC_MQTT_USER ""
#endif
#ifndef DC_MQTT_PASSWORD
#  define DC_MQTT_PASSWORD ""
#endif

const char* WIFI_SSID = DC_WIFI_SSID;
const char* WIFI_PASSWORD = DC_WIFI_PASSWORD;

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay displayTimer(10000);  // Display every 10 seconds

// ========== Setup ==========

void setup() {
    HAL::initializeLogging(115200);
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - Basic NTP Example");
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
    
    // Configure NTP
    NTPConfig cfg;
    cfg.enabled = true;
    cfg.servers = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};
    cfg.syncInterval = 3600;  // Sync every hour
    cfg.timezone = Timezones::CET;  // Change to your timezone
    
    DLOG_I(LOG_APP, "Configuring NTP...");
    DLOG_I(LOG_APP, "  Servers: %s, %s, %s", 
           cfg.servers[0].c_str(), cfg.servers[1].c_str(), cfg.servers[2].c_str());
    DLOG_I(LOG_APP, "  Timezone: %s", cfg.timezone.c_str());
    DLOG_I(LOG_APP, "  Sync interval: %d seconds", cfg.syncInterval);
    
    // Create NTP component
    auto ntp = std::make_unique<NTPComponent>(cfg);
    auto* ntpPtr = ntp.get();
    
    // Register sync callback
    ntpPtr->onSync([ntpPtr](bool success) {
        if (success) {
            DLOG_I(LOG_APP, "✅ Time synchronized!");
            DLOG_I(LOG_APP, "Current time: %s", ntpPtr->getFormattedTime().c_str());
            DLOG_I(LOG_APP, "ISO 8601: %s", ntpPtr->getISO8601().c_str());
            DLOG_I(LOG_APP, "Timezone: %s (GMT%+d)", 
                   ntpPtr->getTimezone().c_str(), 
                   ntpPtr->getGMTOffset() / 3600);
            DLOG_I(LOG_APP, "DST active: %s", ntpPtr->isDST() ? "Yes" : "No");
        } else {
            DLOG_E(LOG_APP, "❌ Time sync failed!");
        }
    });
    
    // Add to core
    core.addComponent(std::move(ntp));
    
    // Initialize
    if (!core.begin()) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        while (1) HAL::delay(1000);
    }
    
    DLOG_I(LOG_APP, "System initialized. Waiting for time sync...");
}

// ========== Loop ==========

void loop() {
    core.loop();
    
    // Display time every 10 seconds
    if (displayTimer.isReady()) {
        
        auto* ntp = core.getComponent<NTPComponent>("NTP");
        if (ntp && ntp->isSynced()) {
            // Display current time in different formats
            DLOG_I(LOG_APP, "--- Current Time ---");
            DLOG_I(LOG_APP, "Full: %s", ntp->getFormattedTime("%Y-%m-%d %H:%M:%S").c_str());
            DLOG_I(LOG_APP, "Date: %s", ntp->getFormattedTime("%Y/%m/%d").c_str());
            DLOG_I(LOG_APP, "Time: %s", ntp->getFormattedTime("%H:%M:%S").c_str());
            DLOG_I(LOG_APP, "12h:  %s", ntp->getFormattedTime("%I:%M:%S %p").c_str());
            DLOG_I(LOG_APP, "Long: %s", ntp->getFormattedTime("%A, %B %d, %Y").c_str());
            DLOG_I(LOG_APP, "Unix: %lld", (long long)ntp->getUnixTime());
            DLOG_I(LOG_APP, "Uptime: %s", ntp->getFormattedUptime().c_str());
            
            // Next sync
            uint32_t nextSync = ntp->getNextSyncIn();
            if (nextSync > 0) {
                DLOG_I(LOG_APP, "Next sync in: %dm %ds", nextSync / 60, nextSync % 60);
            }
            
            // Statistics
            const auto& stats = ntp->getStatistics();
            DLOG_I(LOG_APP, "Stats: %d syncs, %d errors", stats.syncCount, stats.syncErrors);
            DLOG_I(LOG_APP, "-------------------");
        } else {
            DLOG_I(LOG_APP, "Time not synced yet...");
        }
    }
}
