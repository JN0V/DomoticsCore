#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/WifiWebUI.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;

/*
 * WifiWithWebUI Example
 *
 * Demonstrates WiFi component with WebUI interface:
 * - AP mode for initial setup (accessible at http://192.168.4.1:8080)
 * - Live WiFi network scanning and configuration
 * - Real-time status badges (WiFi STA and AP status)
 * - Settings panel for WiFi configuration
 */

Core core;

void setup() {
    DLOG_I(LOG_APP, "=== DomoticsCore WifiWithWebUI Starting ===");

    // Core initialized

    WebUIConfig webCfg; webCfg.deviceName = "WiFi With WebUI"; webCfg.wsUpdateInterval = 5000; // 5 sec - WiFi state changes infrequently
    core.addComponent(std::make_unique<WebUIComponent>(webCfg));

    // Start in AP mode for easy access (empty SSID means AP-only in WifiComponent implementation)
    core.addComponent(std::make_unique<WifiComponent>("", ""));

    // Register provider
    auto* webui = core.getComponent<WebUIComponent>("WebUI");
    auto* wifi = core.getComponent<WifiComponent>("Wifi");
    if (webui && wifi) {
        webui->registerProviderWithComponent(new DomoticsCore::Components::WebUI::WifiWebUI(wifi), wifi);
    }

    CoreConfig cfg; cfg.deviceName = "WifiWithWebUI"; cfg.logLevel = 3;
    core.begin(cfg);
}

void loop() {
    core.loop();
}
