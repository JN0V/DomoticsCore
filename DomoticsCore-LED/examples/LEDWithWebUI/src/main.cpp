#include <Arduino.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/LED.h>
#include <DomoticsCore/LEDWebUI.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;

Core core;

void setup() {
    Serial.begin(115200);
    delay(500);
    
    // ============================================================================
    // EXAMPLE: LED with WebUI Demonstration
    // ============================================================================
    // This example demonstrates LED control via web interface:
    // - WiFi AP mode for direct device access (no router needed)
    // - WebUI dashboard for real-time LED control
    // - WebSocket updates for live LED status
    // - Platform-specific LED polarity handling
    // Expected: Access web dashboard at http://192.168.4.1 to control LED
    // ============================================================================
    
    DLOG_I(LOG_APP, "=== LED with WebUI Demonstration ===");
    DLOG_I(LOG_APP, "LED control via web interface demonstration:");
    DLOG_I(LOG_APP, "- WiFi AP mode for direct device access");
    DLOG_I(LOG_APP, "- WebUI dashboard for real-time LED control");
    DLOG_I(LOG_APP, "- WebSocket updates for live LED status");
    DLOG_I(LOG_APP, "- Platform-specific LED polarity handling");
    DLOG_I(LOG_APP, "Access web dashboard at: http://192.168.4.1");
    DLOG_I(LOG_APP, "======================================");
    
    DLOG_I(LOG_APP, "=== DomoticsCore LEDWithWebUI Starting ===");
    
    // Setup WiFi in AP mode using HAL for demo access
    String apSSID = "DomoticsCore-LED-" + String((uint32_t)HAL::Platform::getChipId(), HEX);
    HAL::WiFiHAL::init();
    bool success = HAL::WiFiHAL::startAP(apSSID.c_str());
    
    if (success) {
        DLOG_I(LOG_APP, "AP started: %s", apSSID.c_str());
        DLOG_I(LOG_APP, "AP IP: %s", HAL::WiFiHAL::getAPIP().c_str());
    } else {
        DLOG_E(LOG_APP, "Failed to start AP mode");
        return;
    }

    // Core initialized

    // WebUI configuration
    WebUIConfig webCfg;
    webCfg.deviceName = "LED With WebUI";
    webCfg.port = 80;
    webCfg.enableWebSocket = true;
    webCfg.wsUpdateInterval = 2000;

    // Register core components: WebUI and LED
    core.addComponent(std::make_unique<WebUIComponent>(webCfg));

    auto led = std::make_unique<LEDComponent>();
    // Built-in LED with platform-specific polarity via HAL
    led->addSingleLED(LED_BUILTIN, "BuiltinLED", 255, HAL::isInternalLEDInverted());
    core.addComponent(std::move(led));

    // Hook up LED WebUI provider
    auto* webui = core.getComponent<WebUIComponent>("WebUI");
    auto* ledComp = core.getComponent<LEDComponent>("LEDComponent");
    if (webui && ledComp) {
        // Create provider wrapper and register it with owning component for lifecycle awareness
        webui->registerProviderWithComponent(new LEDWebUI(ledComp), ledComp);
    }

    CoreConfig cfg; cfg.deviceName = "LEDWithWebUI"; cfg.logLevel = 3;
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
