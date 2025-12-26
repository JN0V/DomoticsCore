/**
 * @file main.cpp
 * @brief BasicOTA Example - OTA firmware updates without WebUI
 *
 * This example demonstrates the OTAComponent for Over-The-Air firmware updates.
 * It configures URL-based OTA checking without WebUI dependency.
 *
 * Features demonstrated:
 * - WiFi connection using HAL
 * - OTA component configuration with periodic checking
 * - Automatic update checks at configured intervals
 *
 * Expected output:
 * [I] [APP] ========================================
 * [I] [APP] DomoticsCore - BasicOTA Example
 * [I] [APP] ========================================
 * [I] [APP] Free heap: XXXXX bytes
 * [I] [APP] Connecting to WiFi: YourWiFiSSID
 * [I] [APP] WiFi connected! IP: 192.168.x.x
 * [I] [OTA] Component initialized
 * [I] [APP] ========================================
 * [I] [APP] Setup complete!
 * [I] [APP] OTA checking every 60s (when provider set)
 * [I] [APP] Free heap: XXXXX bytes
 * [I] [APP] ========================================
 *
 * Note: URL-based OTA download requires implementing HTTPClient callbacks:
 * - ota->setManifestFetcher(...) - to fetch update manifest JSON
 * - ota->setDownloader(...) - to download firmware binary
 * These use platform-specific HTTPClient and must be implemented by the app.
 *
 * Hardware: ESP32 or ESP8266
 */

#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/OTA.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

#define LOG_APP "APP"

// WiFi credentials (replace with your own)
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// OTA update URL (replace with your firmware server)
const char* OTA_UPDATE_URL = "http://your-server.com/firmware.bin";

Core core;

void setup() {
    // Initialize logging first
    HAL::Platform::initializeLogging();

    // ============================================================================
    // EXAMPLE: Basic OTA
    // ============================================================================
    // This example demonstrates OTA configuration with periodic update checks.
    // It connects to WiFi and starts the OTA component with automatic checking.
    //
    // Note: URL-based OTA download requires manifest fetcher and downloader
    // callbacks to be implemented using platform-specific HTTPClient.
    // This example demonstrates component configuration only.
    //
    // Expected: WiFi connection, OTA component init, periodic check logs
    // ============================================================================

    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - BasicOTA Example");
    DLOG_I(LOG_APP, "OTA with periodic update checking");
    DLOG_I(LOG_APP, "Expected: WiFi connect, OTA init, periodic checks");
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "Free heap: %lu bytes", (unsigned long)HAL::Platform::getFreeHeap());

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
        DLOG_E(LOG_APP, "WiFi connection failed!");
        return;
    }

    DLOG_I(LOG_APP, "WiFi connected! IP: %s", HAL::WiFiHAL::getLocalIP().c_str());

    // Configure OTA component with periodic checking
    OTAConfig otaConfig;
    otaConfig.updateUrl = OTA_UPDATE_URL;
    otaConfig.checkIntervalMs = 60000;  // Check every 60 seconds
    otaConfig.requireTLS = false;       // Allow HTTP for testing
    otaConfig.autoReboot = true;
    otaConfig.enableWebUIUpload = false;  // No WebUI

    core.addComponent(std::make_unique<OTAComponent>(otaConfig));

    if (core.begin() != true) {
        DLOG_E(LOG_APP, "Core initialization failed!");
        return;
    }

    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "Setup complete!");
    DLOG_I(LOG_APP, "----------------------------------------");
    DLOG_I(LOG_APP, "OTA configured with:");
    DLOG_I(LOG_APP, "  Update URL: %s", OTA_UPDATE_URL);
    DLOG_I(LOG_APP, "  Check interval: 60s");
    DLOG_I(LOG_APP, "  Auto reboot: enabled");
    DLOG_I(LOG_APP, "----------------------------------------");
    DLOG_I(LOG_APP, "Note: Set ota->setManifestFetcher() and");
    DLOG_I(LOG_APP, "      ota->setDownloader() for URL-based OTA");
    DLOG_I(LOG_APP, "----------------------------------------");
    DLOG_I(LOG_APP, "Free heap: %lu bytes", (unsigned long)HAL::Platform::getFreeHeap());
    DLOG_I(LOG_APP, "========================================");
}

void loop() {
    core.loop();
    // OTA checks run automatically at configured interval
    // OTA component handles state transitions and logging
}
