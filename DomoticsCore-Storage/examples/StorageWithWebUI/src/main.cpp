#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/StorageWebUI.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;

Core core;

void setup() {
    DLOG_I(LOG_APP, "=== DomoticsCore StorageWithWebUI Starting ===");

    // AP mode for quick access - use HAL for chip ID and WiFi
    String apSSID = String("DomoticsCore-Storage-") + HAL::formatChipIdHex();
    HAL::WiFiHAL::startAP(apSSID.c_str());
    DLOG_I(LOG_APP, "AP IP: %s", HAL::WiFiHAL::getAPIP().c_str());

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

    DLOG_I(LOG_APP, "WebUI at http://192.168.4.1");
}

void loop() {
    core.loop();
}
