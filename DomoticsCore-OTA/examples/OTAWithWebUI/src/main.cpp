/**
 * @file main.cpp
 * @brief OTAWithWebUI Example - OTA firmware updates with WebUI interface
 *
 * This example demonstrates the OTAComponent integrated with WebUI for
 * browser-based firmware management. Uses the built-in WebUI upload only
 * (no HTTP download - that requires platform-specific HTTPClient code).
 *
 * Features demonstrated:
 * - WiFi Access Point using HAL
 * - OTA component with WebUI integration
 * - OTAWebUI provider for browser-based firmware upload
 * - Progress display during updates
 *
 * Expected output:
 * [I] [APP] ========================================
 * [I] [APP] DomoticsCore - OTAWithWebUI Example
 * [I] [APP] ========================================
 * [I] [APP] Free heap: XXXXX bytes
 * [I] [APP] AP SSID: DomoticsCore-OTA-XXXXXXXX
 * [I] [APP] AP IP: 192.168.4.1
 * [I] [WebUI] Server started on port 80
 * [I] [OTA] Component initialized
 * [I] [APP] ========================================
 * [I] [APP] Setup complete!
 * [I] [APP] WebUI: http://192.168.4.1/
 * [I] [APP] Free heap: XXXXX bytes
 * [I] [APP] ========================================
 *
 * Usage:
 * 1. Connect to WiFi AP "DomoticsCore-OTA-XXXXXXXX"
 * 2. Open http://192.168.4.1/ in browser
 * 3. Navigate to OTA section
 * 4. Upload firmware.bin file
 * 5. Device will reboot automatically after successful update
 *
 * Hardware: ESP32 or ESP8266
 */

#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/OTA.h>
#include <DomoticsCore/OTAWebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Credentials. Put a `secrets.h` at the repository root — or anywhere on the
// include path — to reach this example from a LAN instead of joining its access
// point. Build with the root on the include path:
//     PLATFORMIO_BUILD_FLAGS="-I/path/to/DomoticsCore" pio run -e esp8266dev
// Without the file the example behaves exactly as before: access point only.
#if defined(__has_include)
#  if __has_include(<secrets.h>)
#    include <secrets.h>
#  endif
#endif

#ifndef DC_WIFI_SSID
#  define DC_WIFI_SSID ""
#endif
#ifndef DC_WIFI_PASSWORD
#  define DC_WIFI_PASSWORD ""
#endif


#define LOG_APP "APP"

Core core;

void setup() {
    // Initialize logging first
    HAL::Platform::initializeLogging();

    // ============================================================================
    // EXAMPLE: OTA With WebUI
    // ============================================================================
    // This example demonstrates OTA firmware updates via WebUI interface.
    // It creates a WiFi Access Point and serves a web interface for uploading
    // firmware files directly from your browser.
    //
    // Note: This example uses WebUI upload only. For HTTP/HTTPS download-based
    // OTA, the application layer needs to implement manifest fetcher and
    // downloader callbacks using platform-specific HTTPClient libraries.
    //
    // Expected: AP started, WebUI at http://192.168.4.1/, OTA upload section
    // Usage: Connect to AP, open WebUI, upload firmware.bin in OTA section
    // ============================================================================

    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - OTAWithWebUI Example");
    DLOG_I(LOG_APP, "Browser-based OTA firmware upload");
    DLOG_I(LOG_APP, "Expected: AP + WebUI at http://192.168.4.1/");
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "Free heap: %lu bytes", (unsigned long)HAL::Platform::getFreeHeap());

    // Start soft AP for easy access using HAL
    char apSSID[32];
    snprintf(apSSID, sizeof(apSSID), "DomoticsCore-OTA-%08X", (uint32_t)HAL::Platform::getChipId());

    HAL::WiFiHAL::init();

    // This example is an access point by design: updating a device in the field
    // is the point, and that needs no infrastructure. Define DC_OTA_PREFER_STA
    // (in secrets.h) to join an existing network instead, which is what makes
    // the upload endpoint reachable from a LAN for testing. Off by default, so
    // the access-point path stays the one that ships and the one exercised.
    bool staConnected = false;
#ifdef DC_OTA_PREFER_STA
    if (strlen(DC_WIFI_SSID) > 0) {
        DLOG_I(LOG_APP, "Connecting to WiFi: %s", DC_WIFI_SSID);
        HAL::WiFiHAL::connect(DC_WIFI_SSID, DC_WIFI_PASSWORD);
        unsigned long start = HAL::Platform::getMillis();
        while (!HAL::WiFiHAL::isConnected() && (HAL::Platform::getMillis() - start) < 15000) {
            HAL::Platform::delayMs(100);
        }
        staConnected = HAL::WiFiHAL::isConnected();
    }
#endif

    if (staConnected) {
        DLOG_I(LOG_APP, "Connected. Upload endpoint: http://%s/api/ota/upload",
               HAL::WiFiHAL::getLocalIP().c_str());
    } else {
        HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::AccessPoint);
        HAL::WiFiHAL::startAP(apSSID);
        DLOG_I(LOG_APP, "AP SSID: %s", apSSID);
        DLOG_I(LOG_APP, "AP IP: %s", HAL::WiFiHAL::getAPIP().c_str());
    }

    // Web UI component
    WebUIConfig webCfg;
    webCfg.setDeviceName("OTA With WebUI");
    webCfg.wsUpdateInterval = 2000;
    core.addComponent(std::make_unique<WebUIComponent>(webCfg));

    // OTA component - WebUI upload enabled by default
    OTAConfig otaCfg;
    otaCfg.enableWebUIUpload = true;  // Enable browser-based upload
    otaCfg.autoReboot = true;         // Reboot automatically after update
    core.addComponent(std::make_unique<OTAComponent>(otaCfg));

    CoreConfig cfg;
    cfg.deviceName = "OTAWithWebUI";
    cfg.logLevel = 3;
    core.begin(cfg);

    // Register OTA WebUI provider AFTER core begin (so WebUI server is started)
    auto* webui = core.getComponent<WebUIComponent>("WebUI");
    auto* ota = core.getComponent<OTAComponent>("OTA");
    if (webui && ota) {
        auto* otaWebUI = new DomoticsCore::Components::WebUI::OTAWebUI(ota);
        otaWebUI->init(webui);
        webui->registerProviderWithComponent(otaWebUI, ota);
    }

    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "Setup complete!");
    DLOG_I(LOG_APP, "----------------------------------------");
    DLOG_I(LOG_APP, "1. Connect to WiFi: %s", apSSID);
    DLOG_I(LOG_APP, "2. Open: http://%s/", HAL::WiFiHAL::getAPIP().c_str());
    DLOG_I(LOG_APP, "3. Navigate to OTA section");
    DLOG_I(LOG_APP, "4. Upload firmware.bin file");
    DLOG_I(LOG_APP, "----------------------------------------");
    DLOG_I(LOG_APP, "Or use curl:");
    DLOG_I(LOG_APP, "  curl -F 'firmware=@fw.bin' http://%s/api/ota/upload", HAL::WiFiHAL::getAPIP().c_str());
    DLOG_I(LOG_APP, "----------------------------------------");
    DLOG_I(LOG_APP, "Free heap: %lu bytes", (unsigned long)HAL::Platform::getFreeHeap());
    DLOG_I(LOG_APP, "========================================");
}

void loop() {
    core.loop();
}
