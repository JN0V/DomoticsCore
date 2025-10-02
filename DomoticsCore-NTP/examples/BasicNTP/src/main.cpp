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

#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/NTP.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// ========== Configuration ==========

const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay displayTimer(10000);  // Display every 10 seconds

// ========== Setup ==========

void setup() {
    Serial.begin(115200);  // Required for ESP32 logging output
    delay(1000);  // Allow serial to initialize
    
    DLOG_I(LOG_CORE, "\n========================================");
    DLOG_I(LOG_CORE, "DomoticsCore - Basic NTP Example");
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
    
    // Configure NTP
    NTPConfig cfg;
    cfg.enabled = true;
    cfg.servers = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};
    cfg.syncInterval = 3600;  // Sync every hour
    cfg.timezone = Timezones::CET;  // Change to your timezone
    
    DLOG_I(LOG_CORE, "Configuring NTP...");
    DLOG_I(LOG_CORE, "  Servers: %s, %s, %s", 
           cfg.servers[0].c_str(), cfg.servers[1].c_str(), cfg.servers[2].c_str());
    DLOG_I(LOG_CORE, "  Timezone: %s", cfg.timezone.c_str());
    DLOG_I(LOG_CORE, "  Sync interval: %d seconds", cfg.syncInterval);
    
    // Create NTP component
    auto ntp = std::make_unique<NTPComponent>(cfg);
    auto* ntpPtr = ntp.get();
    
    // Register sync callback
    ntpPtr->onSync([ntpPtr](bool success) {
        if (success) {
            DLOG_I(LOG_CORE, "\n✅ Time synchronized!");
            DLOG_I(LOG_CORE, "Current time: %s", ntpPtr->getFormattedTime().c_str());
            DLOG_I(LOG_CORE, "ISO 8601: %s", ntpPtr->getISO8601().c_str());
            DLOG_I(LOG_CORE, "Timezone: %s (GMT%+d)", 
                   ntpPtr->getTimezone().c_str(), 
                   ntpPtr->getGMTOffset() / 3600);
            DLOG_I(LOG_CORE, "DST active: %s\n", ntpPtr->isDST() ? "Yes" : "No");
        } else {
            DLOG_E(LOG_CORE, "❌ Time sync failed!");
        }
    });
    
    // Add to core
    core.addComponent(std::move(ntp));
    
    // Initialize
    if (!core.begin()) {
        DLOG_E(LOG_CORE, "Failed to initialize core!");
        while (1) delay(1000);
    }
    
    DLOG_I(LOG_CORE, "System initialized. Waiting for time sync...");
}

// ========== Loop ==========

void loop() {
    core.loop();
    
    // Display time every 10 seconds
    if (displayTimer.isReady()) {
        
        auto* ntp = core.getComponent<NTPComponent>("NTP");
        if (ntp && ntp->isSynced()) {
            // Display current time in different formats
            DLOG_I(LOG_CORE, "--- Current Time ---");
            DLOG_I(LOG_CORE, "Full: %s", ntp->getFormattedTime("%Y-%m-%d %H:%M:%S").c_str());
            DLOG_I(LOG_CORE, "Date: %s", ntp->getFormattedTime("%Y/%m/%d").c_str());
            DLOG_I(LOG_CORE, "Time: %s", ntp->getFormattedTime("%H:%M:%S").c_str());
            DLOG_I(LOG_CORE, "12h:  %s", ntp->getFormattedTime("%I:%M:%S %p").c_str());
            DLOG_I(LOG_CORE, "Long: %s", ntp->getFormattedTime("%A, %B %d, %Y").c_str());
            DLOG_I(LOG_CORE, "Unix: %ld", ntp->getUnixTime());
            DLOG_I(LOG_CORE, "Uptime: %s", ntp->getFormattedUptime().c_str());
            
            // Next sync
            uint32_t nextSync = ntp->getNextSyncIn();
            if (nextSync > 0) {
                DLOG_I(LOG_CORE, "Next sync in: %dm %ds", nextSync / 60, nextSync % 60);
            }
            
            // Statistics
            const auto& stats = ntp->getStatistics();
            DLOG_I(LOG_CORE, "Stats: %d syncs, %d errors", stats.syncCount, stats.syncErrors);
            DLOG_I(LOG_CORE, "-------------------");
        } else {
            DLOG_I(LOG_CORE, "Time not synced yet...");
        }
    }
}
