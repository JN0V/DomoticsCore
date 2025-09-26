#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/SystemInfo.h>
#include <DomoticsCore/SystemInfoWebUI.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

std::unique_ptr<Core> core;

void setup() {
    DLOG_I(LOG_CORE, "=== DomoticsCore SystemInfoWithWebUI Starting ===");

    // Bring up a simple AP for demo access
    String apSSID = String("DomoticsCore-Sys-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (WiFi.softAP(apSSID.c_str())) {
        DLOG_I(LOG_CORE, "AP started: %s", apSSID.c_str());
        DLOG_I(LOG_CORE, "AP IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        DLOG_E(LOG_CORE, "Failed to start AP mode");
        return;
    }

    core.reset(new Core());

    // WebUI configuration
    WebUIConfig webCfg;
    webCfg.deviceName = "System Info With WebUI";
    webCfg.port = 80;
    webCfg.enableWebSocket = true;
    webCfg.wsUpdateInterval = 2000;

    core->addComponent(std::make_unique<WebUIComponent>(webCfg));

    // Add SystemInfo component
    core->addComponent(std::make_unique<SystemInfoComponent>());

    // Register SystemInfo WebUI provider
    auto* webui = core->getComponent<WebUIComponent>("WebUI");
    auto* sys = core->getComponent<SystemInfoComponent>("System Info");
    if (webui && sys) {
        webui->registerProviderWithComponent(new DomoticsCore::Components::WebUI::SystemInfoWebUI(sys), sys);
    }

    CoreConfig cfg; cfg.deviceName = "SystemInfoWithWebUI"; cfg.logLevel = 3;
    if (!core->begin(cfg)) {
        DLOG_E(LOG_CORE, "Core initialization failed");
        return;
    }

    DLOG_I(LOG_CORE, "=== Setup Complete ===");
    DLOG_I(LOG_CORE, "WebUI available at: http://192.168.4.1");
}

void loop() {
    if (core) core->loop();
}
