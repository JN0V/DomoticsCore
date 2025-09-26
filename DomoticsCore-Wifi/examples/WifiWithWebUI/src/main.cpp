#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/WifiWebUI.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

std::unique_ptr<Core> core;

void setup() {
    DLOG_I(LOG_CORE, "=== DomoticsCore WifiWithWebUI Starting ===");

    core.reset(new Core());

    WebUIConfig webCfg; webCfg.deviceName = "WiFi With WebUI"; webCfg.wsUpdateInterval = 2000;
    core->addComponent(std::make_unique<WebUIComponent>(webCfg));

    // Start in AP mode for easy access (empty SSID means AP-only in WifiComponent implementation)
    core->addComponent(std::make_unique<WifiComponent>("", ""));

    // Register provider
    auto* webui = core->getComponent<WebUIComponent>("WebUI");
    auto* wifi = core->getComponent<WifiComponent>("Wifi");
    if (webui && wifi) {
        webui->registerProviderWithComponent(new DomoticsCore::Components::WebUI::WifiWebUI(wifi), wifi);
    }

    CoreConfig cfg; cfg.deviceName = "WifiWithWebUI"; cfg.logLevel = 3;
    core->begin(cfg);
}

void loop() {
    if (core) core->loop();
}
