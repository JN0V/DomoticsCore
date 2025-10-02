#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/StorageWebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core core;

void setup() {
    DLOG_I(LOG_CORE, "=== DomoticsCore StorageWithWebUI Starting ===");

    // AP mode for quick access
    String apSSID = String("DomoticsCore-Storage-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    WiFi.softAP(apSSID.c_str());
    DLOG_I(LOG_CORE, "AP IP: %s", WiFi.softAPIP().toString().c_str());

    // Core initialized

    WebUIConfig webCfg; webCfg.deviceName = "Storage With WebUI"; webCfg.wsUpdateInterval = 3000;
    core.addComponent(std::make_unique<WebUIComponent>(webCfg));

    // Storage component
    StorageConfig scfg; scfg.namespace_name = "domotics"; scfg.maxEntries = 100; scfg.autoCommit = true;
    core.addComponent(std::make_unique<StorageComponent>(scfg));

    // Register provider
    auto* webui = core.getComponent<WebUIComponent>("WebUI");
    auto* storage = core.getComponent<StorageComponent>("Storage");
    if (webui && storage) {
        webui->registerProviderWithComponent(new DomoticsCore::Components::WebUI::StorageWebUI(storage), storage);
    }

    CoreConfig cfg; cfg.deviceName = "StorageWithWebUI"; cfg.logLevel = 3;
    core.begin(cfg);

    DLOG_I(LOG_CORE, "WebUI at http://192.168.4.1");
}

void loop() {
    core.loop();
}
