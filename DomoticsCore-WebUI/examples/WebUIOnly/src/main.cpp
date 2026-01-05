#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/SystemInfo.h>
#include <DomoticsCore/SystemInfoWebUI.h>
#include <DomoticsCore/BaseWebUIComponents.h>
#include <DomoticsCore/IWebUIProvider.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

/**
 * Simple Demo LED Component (no WebUI here)
 */
class DemoLEDComponent : public IComponent {
private:
    bool demoLedState;
    bool manualControl = false;
    DomoticsCore::Utils::NonBlockingDelay demoBlinkTimer;
    
public:
    DemoLEDComponent(int pin = LED_BUILTIN) : demoLedState(false), demoBlinkTimer(1000) {
        (void)pin; // Ignored - always use LED_BUILTIN via HAL
        metadata.name = "Demo LED Controller";
        metadata.version = "1.0.0";
    }
    
    const char* getTypeKey() const override { return "Demo LED Controller"; }
    
    ComponentStatus begin() override {
        HAL::Platform::pinMode(LED_BUILTIN, OUTPUT);
        HAL::Platform::digitalWrite(LED_BUILTIN, HAL::Platform::ledBuiltinOff()); // Start OFF
        // Publish initial sticky state so late subscribers receive current value
        emit<bool>("led/state", demoLedState, /*sticky=*/true);
        // Subscribe to EventBus command to allow cross-component control
        // Any component can publish "led/set" with a bool payload to change LED state
        on<bool>("led/set", [this](const bool& desired){ this->setState(desired); }, /*replayLast=*/false);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // The loop is now empty to prevent automatic blinking and respect manual control.
    }
    
    ComponentStatus shutdown() override {
        HAL::Platform::digitalWrite(LED_BUILTIN, HAL::Platform::ledBuiltinOff());
        return ComponentStatus::Success;
    }
    // Simple API for UI wrapper
    void setState(bool on) {
        manualControl = true;
        demoLedState = on;
        // Use HAL functions for correct polarity on all platforms
        HAL::Platform::digitalWrite(LED_BUILTIN, demoLedState ? HAL::Platform::ledBuiltinOn() : HAL::Platform::ledBuiltinOff());
        DLOG_I(LOG_APP, "[LED Demo] Manual state change to: %s", demoLedState ? "ON" : "OFF");
        // Publish sticky state so newcomers can immediately get the latest value
        emit<bool>("led/state", demoLedState, /*sticky=*/true);
    }
    // Event-driven API: publish command to the bus (used by WebUI to decouple)
    void requestSet(bool on) {
        DLOG_I(LOG_APP, "[LED Demo] requestSet called with: %s", on ? "true" : "false");
        // Do not change state directly here; let the EventBus subscription handle it
        emit<bool>("led/set", on, /*sticky=*/false);
    }
    bool isOn() const { return demoLedState; }
    int getPin() const { return LED_BUILTIN; }
};

// LED WebUI (composition) - uses CachingWebUIProvider to prevent memory leaks
class LEDWebUI : public CachingWebUIProvider {
private:
    DemoLEDComponent* led; // non-owning
public:
    explicit LEDWebUI(DemoLEDComponent* comp) : led(comp) {}

    String getWebUIName() const override { return "LED"; }
    String getWebUIVersion() const override { return "1.0.0"; }

protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        if (!led) return;

        // Dashboard card - simple version without customHtml for testing
        contexts.push_back(WebUIContext::dashboard("led_dashboard", "LED Control")
            .withField(WebUIField("state_toggle_dashboard", "LED", WebUIFieldType::Boolean, "false"))
            .withRealTime(1000));

        // Header status badge using WebUIContext - placeholder values
        contexts.push_back(WebUIContext::statusBadge("led_status", "LED", "bulb-twotone")
            .withField(WebUIField("state", "State", WebUIFieldType::Status, "OFF"))
            .withRealTime(1000)
            .withCustomCss(R"(
                .status-indicator[data-context-id='led_status'] .status-icon { color: var(--text-secondary); }
                .status-indicator[data-context-id='led_status'].active .status-icon { color: #ffc107; filter: drop-shadow(0 0 6px rgba(255,193,7,0.6)); }
            )"));

        // Settings context - simple version
        contexts.push_back(WebUIContext::settings("led_settings", "LED Controller")
            .withField(WebUIField("state_toggle_settings", "LED", WebUIFieldType::Boolean, "false"))
            .withField(WebUIField("pin_display", "GPIO Pin", WebUIFieldType::Display, String(LED_BUILTIN).c_str(), "", true)));
    }

public:
    String getWebUIData(const String& contextId) override {
        if (!led) return "{}";
        if (contextId == "led_dashboard" || contextId == "led_settings") {
            JsonDocument doc; doc["state_toggle_dashboard"] = led->isOn(); doc["state_toggle_settings"] = led->isOn(); doc["gpio_pin"] = led->getPin(); String json; serializeJson(doc, json); return json;
        } else if (contextId == "led_status") {
            JsonDocument doc; doc["state"] = led->isOn() ? "ON" : "OFF"; String json; serializeJson(doc, json); return json;
        }
        return "{}";
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params) override {
        DLOG_I(LOG_APP, "[LEDWebUI] handleRequest: ctx=%s, method=%s", contextId.c_str(), method.c_str());
        if (!led) return "{\"success\":false}";
        if ((contextId == "led_settings" || contextId == "led_dashboard") && method == "POST") {
            auto fieldIt = params.find("field"); auto valueIt = params.find("value");
            if (fieldIt != params.end() && valueIt != params.end()) {
                DLOG_I(LOG_APP, "[LEDWebUI] field=%s, value=%s", fieldIt->second.c_str(), valueIt->second.c_str());
                if (fieldIt->second == "state_toggle_dashboard" || fieldIt->second == "state_toggle_settings") {
                    // Decoupled: publish command on EventBus; LED component will handle via subscription
                    led->requestSet(valueIt->second == "true");
                    return "{\"success\":true}";
                }
            }
        }
        return "{\"success\":false, \"error\":\"Invalid request\"}";
    }
};


// WiFi credentials - set these for STA mode, leave empty for AP-only mode
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// Global Core
Core core;

void setup() {
    HAL::Platform::initializeLogging(115200);

    DLOG_I(LOG_APP, "=== DomoticsCore WebUI Demo Starting ===");

    // Initialize WiFi HAL
    HAL::WiFiHAL::init();

    // Check if STA credentials were provided
    const char* staSSID = WIFI_SSID;
    const char* staPass = WIFI_PASSWORD;
    bool staConnected = false;

    if (strlen(staSSID) > 0) {
        // Try to connect to the specified WiFi network
        DLOG_I(LOG_APP, "Connecting to WiFi: %s", staSSID);
        HAL::WiFiHAL::connect(staSSID, staPass);

        // Wait for connection (up to 15 seconds)
        unsigned long startTime = HAL::Platform::getMillis();
        while (!HAL::WiFiHAL::isConnected() && (HAL::Platform::getMillis() - startTime) < 15000) {
            HAL::Platform::delayMs(100);
        }
        staConnected = HAL::WiFiHAL::isConnected();

        if (staConnected) {
            DLOG_I(LOG_APP, "Connected to WiFi!");
            DLOG_I(LOG_APP, "IP: %s", HAL::WiFiHAL::getLocalIP().c_str());
        } else {
            DLOG_W(LOG_APP, "Failed to connect to WiFi, falling back to AP mode");
        }
    }

    // Start AP mode if STA failed or wasn't configured
    if (!staConnected) {
        String apSSID = "DomoticsCore-" + String((uint32_t)HAL::Platform::getChipId(), HEX);
        bool apSuccess = HAL::WiFiHAL::startAP(apSSID.c_str());

        if (apSuccess) {
            DLOG_I(LOG_APP, "AP started: %s", apSSID.c_str());
            DLOG_I(LOG_APP, "AP IP: %s", HAL::WiFiHAL::getAPIP().c_str());
        } else {
            DLOG_E(LOG_APP, "Failed to start AP mode");
            return;
        }
    }
    
    // Create Core
    // Core initialized

    // Create WebUI component
    DomoticsCore::Components::WebUIConfig webUIConfig;
    webUIConfig.deviceName = "DomoticsCore WebUI Demo";
    // Note: manufacturer, version, copyright have been moved to SystemInfo component
    webUIConfig.port = 80;
    webUIConfig.enableWebSocket = true;
    webUIConfig.wsUpdateInterval = 2000;
    webUIConfig.useFileSystem = false;
    
    // Register components in Core (WebUI + demo components)
    core.addComponent(std::make_unique<DomoticsCore::Components::WebUIComponent>(webUIConfig));
    core.addComponent(std::make_unique<DemoLEDComponent>(LED_BUILTIN));
    core.addComponent(std::make_unique<SystemInfoComponent>());

    // Register LED UI wrapper factory before core.begin (composition)
    auto* webui = core.getComponent<DomoticsCore::Components::WebUIComponent>("WebUI");
    if (webui) {
        DLOG_I(LOG_APP, "[APP] Registering Demo LED Controller provider factory");
        webui->registerProviderFactory("Demo LED Controller", [](IComponent* c) -> IWebUIProvider* {
            DLOG_I(LOG_APP, "[APP] Creating LEDWebUI for component: %s", c ? c->metadata.name.c_str() : "null");
            return new LEDWebUI(static_cast<DemoLEDComponent*>(c));
        });
        webui->registerProviderFactory("system_info", [](IComponent* c) -> IWebUIProvider* {
            return new WebUI::SystemInfoWebUI(static_cast<SystemInfoComponent*>(c));
        });
    }

    CoreConfig cfg; cfg.deviceName = "DomoticsCore WebUI Demo"; cfg.logLevel = 3;
    if (!core.begin(cfg)) {
        DLOG_E(LOG_APP, "Core initialization failed");
        return;
    }

    DLOG_I(LOG_APP, "=== Setup Complete ===");
    DLOG_I(LOG_APP, "WebUI available at: http://192.168.4.1");
}

void loop() {
    core.loop();
    
    // System status reporting
    static DomoticsCore::Utils::NonBlockingDelay statusTimer(30000);
    if (statusTimer.isReady()) {
        DLOG_I(LOG_APP, "=== System Status ===");
        DLOG_I(LOG_APP, "Uptime: %lu seconds", HAL::Platform::getMillis() / 1000);
        DLOG_I(LOG_APP, "Free heap: %u bytes", HAL::Platform::getFreeHeap());
        // WebSocket client count can be retrieved from WebUI provider if needed
        DLOG_I(LOG_APP, "AP clients: %d", HAL::WiFiHAL::getAPStationCount());
    }
}
