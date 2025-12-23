#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/SystemInfo.h>
#include <DomoticsCore/SystemInfoWebUI.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;

Core core;

void setup() {
    HAL::initializeLogging(115200);

    DLOG_I(LOG_APP, "=== DomoticsCore SystemInfoWithWebUI Starting ===");

    // Bring up a simple AP for demo access
    String apSSID = String("DomoticsCore-Sys-") + HAL::formatChipIdHex();
    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::AccessPoint);
    if (HAL::WiFiHAL::startAP(apSSID.c_str())) {
        DLOG_I(LOG_APP, "AP started: %s", apSSID.c_str());
        DLOG_I(LOG_APP, "AP IP: %s", HAL::WiFiHAL::getAPIP().c_str());
    } else {
        DLOG_E(LOG_APP, "Failed to start AP mode");
        return;
    }

    // Core initialized

    // WebUI configuration
    WebUIConfig webCfg;
    webCfg.deviceName = "System Info With WebUI";
    webCfg.port = 80;
    webCfg.enableWebSocket = true;
    webCfg.wsUpdateInterval = 2000;

    core.addComponent(std::make_unique<WebUIComponent>(webCfg));

    // Add SystemInfo component
    core.addComponent(std::make_unique<SystemInfoComponent>());

    // Register SystemInfo WebUI provider
    auto* webui = core.getComponent<WebUIComponent>("WebUI");
    auto* sys = core.getComponent<SystemInfoComponent>("System Info");
    if (webui && sys) {
        webui->registerProviderWithComponent(new DomoticsCore::Components::WebUI::SystemInfoWebUI(sys), sys);
    }

    CoreConfig cfg; cfg.deviceName = "SystemInfoWithWebUI"; cfg.logLevel = 3;
    if (!core.begin(cfg)) {
        DLOG_E(LOG_APP, "Core initialization failed");
        return;
    }

    DLOG_I(LOG_APP, "=== Setup Complete ===");
    DLOG_I(LOG_APP, "WebUI available at: http://192.168.4.1");
}

void loop() {
    core.loop();
}
