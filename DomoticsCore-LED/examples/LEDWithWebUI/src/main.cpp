#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/LED.h>
#include <DomoticsCore/LEDWebUI.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

std::unique_ptr<Core> core;

void setup() {
    DLOG_I(LOG_CORE, "=== DomoticsCore LEDWithWebUI Starting ===");

    // Bring up a simple AP for demo access
    String apSSID = String("DomoticsCore-LED-") + String((uint32_t)ESP.getEfuseMac(), HEX);
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
    webCfg.deviceName = "LED With WebUI";
    webCfg.port = 80;
    webCfg.enableWebSocket = true;
    webCfg.wsUpdateInterval = 2000;

    // Register core components: WebUI and LED
    core->addComponent(std::make_unique<WebUIComponent>(webCfg));

    auto led = std::make_unique<LEDComponent>();
    // Single on-board LED on GPIO2 by default (change as needed)
    led->addSingleLED(2, "onboard");
    core->addComponent(std::move(led));

    // Hook up LED WebUI provider
    auto* webui = core->getComponent<WebUIComponent>("WebUI");
    auto* ledComp = core->getComponent<LEDComponent>("LEDComponent");
    if (webui && ledComp) {
        // Create provider wrapper and register it with owning component for lifecycle awareness
        webui->registerProviderWithComponent(new LEDWebUI(ledComp), ledComp);
    }

    CoreConfig cfg; cfg.deviceName = "LEDWithWebUI"; cfg.logLevel = 3;
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
