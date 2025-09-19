#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/WebUI.h>
#include <DomoticsCore/Components/IComponent.h>
#include <DomoticsCore/Components/IWebUIProvider.h>
#include <DomoticsCore/Utils/Timer.h>
#include <DomoticsCore/Components/SystemInfo.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

/**
 * Simple Demo LED Controller Component
 */
class DemoLEDController : public IComponent, public virtual IWebUIProvider {
private:
    int demoLedPin;
    bool demoLedState;
    bool manualControl = false;
    DomoticsCore::Utils::NonBlockingDelay demoBlinkTimer;
    
public:
    DemoLEDController(int pin = 2) : demoLedPin(pin), demoLedState(false), demoBlinkTimer(1000) {
        metadata.name = "Demo LED Controller";
        metadata.version = "1.0.0";
    }
    
    String getName() const override { return metadata.name; }
    
    ComponentStatus begin() override {
        pinMode(demoLedPin, OUTPUT);
        digitalWrite(demoLedPin, LOW);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (!manualControl && demoBlinkTimer.isReady()) {
            demoLedState = !demoLedState;
            digitalWrite(demoLedPin, demoLedState ? HIGH : LOW);
        }
    }
    
    ComponentStatus shutdown() override {
        digitalWrite(demoLedPin, LOW);
        return ComponentStatus::Success;
    }
    
    // WebUI Provider methods
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;

        // Dashboard context (for real-time data)
        contexts.push_back(WebUIContext::dashboard("led_dashboard", "LED Status")
            .withField(WebUIField("state", "State", WebUIFieldType::Display))
            .withRealTime(1000));

        // Settings context (for configuration/control)
        contexts.push_back(WebUIContext::settings("led_settings", "LED Controller")
            .withField(WebUIField("state_toggle", "LED", WebUIFieldType::Boolean, demoLedState ? "true" : "false"))
            .withField(WebUIField("pin_display", "GPIO Pin", WebUIFieldType::Display, String(demoLedPin), "", true)));

        return contexts;
    }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint, 
                             const String& method, const std::map<String, String>& params) override {
        if (contextId == "led_settings" && method == "POST") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");

            if (fieldIt != params.end() && valueIt != params.end()) {
                if (fieldIt->second == "state_toggle") {
                    manualControl = true; // User has taken control
                    demoLedState = (valueIt->second == "true");
                    digitalWrite(demoLedPin, demoLedState ? HIGH : LOW);
                    DLOG_I(LOG_CORE, "[LED Demo] Manual state change to: %s", demoLedState ? "ON" : "OFF");
                    return "{\"success\":true}";
                }
            }
        }
        return "{\"success\":false, \"error\":\"Invalid request\"}";
    }
    
    String getWebUIData(const String& contextId) override {
        return "{\"state\":" + String(demoLedState ? "true" : "false") + 
               ",\"pin\":" + String(demoLedPin) + 
               ",\"status\":\"" + String(demoLedState ? "ON" : "OFF") + "\"}";
    }

    String getWebUIName() const override { return metadata.name; }
    String getWebUIVersion() const override { return metadata.version; }
};


// Global component storage
std::unique_ptr<DomoticsCore::Components::WebUIComponent> webUIComponent;
std::unique_ptr<DemoLEDController> demoLedController;
std::unique_ptr<SystemInfoComponent> systemInfoComponent;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_CORE, "=== DomoticsCore WebUI Demo Starting ===");
    
    // Setup WiFi in AP mode
    String apSSID = "DomoticsCore-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    bool success = WiFi.softAP(apSSID.c_str());
    
    if (success) {
        DLOG_I(LOG_CORE, "AP started: %s", apSSID.c_str());
        DLOG_I(LOG_CORE, "AP IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
        DLOG_E(LOG_CORE, "Failed to start AP mode");
        return;
    }
    
    // Create WebUI component
    DomoticsCore::Components::WebUIConfig webUIConfig;
    webUIConfig.deviceName = "DomoticsCore WebUI Demo";
    webUIConfig.manufacturer = "DomoticsCore";
    webUIConfig.version = "v2.0.0-demo";
    webUIConfig.copyright = "© 2024 DomoticsCore";
    webUIConfig.port = 80;
    webUIConfig.enableWebSocket = true;
    webUIConfig.wsUpdateInterval = 5000;
    webUIConfig.useFileSystem = false;
    
    webUIComponent.reset(new DomoticsCore::Components::WebUIComponent(webUIConfig));
    
    // Create demo components
    demoLedController.reset(new DemoLEDController(2));
    systemInfoComponent.reset(new SystemInfoComponent());
    
    // Initialize components
    DLOG_I(LOG_CORE, "Initializing components...");
    
    DomoticsCore::Components::ComponentStatus status = webUIComponent->begin();
    if (status != DomoticsCore::Components::ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "WebUI initialization failed");
        return;
    }
    
    status = demoLedController->begin();
    if (status != DomoticsCore::Components::ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "LED Controller initialization failed");
        return;
    }
    
    status = systemInfoComponent->begin();
    if (status != DomoticsCore::Components::ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "System Info initialization failed");
        return;
    }
    
    // Register WebUI providers
    webUIComponent->registerProvider(demoLedController.get());
    webUIComponent->registerProvider(systemInfoComponent.get());
    
    DLOG_I(LOG_CORE, "=== Setup Complete ===");
    DLOG_I(LOG_CORE, "WebUI available at: http://192.168.4.1");
}

void loop() {
    // Run component loops
    if (webUIComponent) {
        webUIComponent->loop();
    }
    if (demoLedController) {
        demoLedController->loop();
    }
    if (systemInfoComponent) {
        systemInfoComponent->loop();
    }
    
    // System status reporting
    static DomoticsCore::Utils::NonBlockingDelay statusTimer(30000);
    if (statusTimer.isReady()) {
        DLOG_I(LOG_CORE, "=== System Status ===");
        DLOG_I(LOG_CORE, "Uptime: %lu seconds", millis() / 1000);
        DLOG_I(LOG_CORE, "Free heap: %d bytes", ESP.getFreeHeap());
        if (webUIComponent) {
            DLOG_I(LOG_CORE, "WebSocket clients: %d", webUIComponent->getWebSocketClients());
        }
        DLOG_I(LOG_CORE, "AP clients: %d", WiFi.softAPgetStationNum());
    }
}
