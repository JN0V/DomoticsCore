#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/WebUI.h>
#include <DomoticsCore/Components/IWebUIProvider.h>
#include <DomoticsCore/Components/IComponent.h>
#include <DomoticsCore/Utils/Timer.h>
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
        if (demoBlinkTimer.isReady()) {
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
        
        WebUIContext ledControl = WebUIContext::dashboard("led_control", "LED Control", "fas fa-lightbulb");
        ledControl.withField(WebUIField("state", "LED State", WebUIFieldType::Boolean, 
                                       demoLedState ? "true" : "false", "", false))
                 .withField(WebUIField("pin", "Pin", WebUIFieldType::Number, 
                                     String(demoLedPin), "", true))
                 .withRealTime(1000);
        contexts.push_back(ledControl);
        
        return contexts;
    }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint, 
                             const String& method, const std::map<String, String>& params) override {
        return "{\"success\":true,\"message\":\"LED demo request handled\"}";
    }
    
    String getWebUIData(const String& contextId) override {
        return "{\"state\":" + String(demoLedState ? "true" : "false") + 
               ",\"pin\":" + String(demoLedPin) + 
               ",\"status\":\"" + String(demoLedState ? "ON" : "OFF") + "\"}";
    }
};

/**
 * Simple Demo System Info Component
 */
class DemoSystemInfo : public IComponent, public virtual IWebUIProvider {
private:
    DomoticsCore::Utils::NonBlockingDelay demoUpdateTimer;
    
public:
    DemoSystemInfo() : demoUpdateTimer(5000) {
        metadata.name = "Demo System Info";
        metadata.version = "1.0.0";
    }
    
    String getName() const override { return metadata.name; }
    
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override { /* Update system info periodically */ }
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    
    // WebUI Provider methods
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        
        WebUIContext sysInfo = WebUIContext::dashboard("system_info", "System Info", "fas fa-microchip");
        sysInfo.withField(WebUIField("uptime", "Uptime", WebUIFieldType::Display, 
                                   String(millis() / 1000) + "s", "", true))
              .withField(WebUIField("heap", "Free Heap", WebUIFieldType::Display, 
                                  String(ESP.getFreeHeap()) + " bytes", "", true))
              .withRealTime(5000);
        contexts.push_back(sysInfo);
        
        return contexts;
    }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint, 
                             const String& method, const std::map<String, String>& params) override {
        return "{\"uptime\":" + String(millis() / 1000) + 
               ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
    }
    
    String getWebUIData(const String& contextId) override {
        return "{\"uptime\":" + String(millis() / 1000) + 
               ",\"heap\":" + String(ESP.getFreeHeap()) + 
               ",\"chip\":\"" + ESP.getChipModel() + "\"}";
    }
};

// Global component storage
std::unique_ptr<DomoticsCore::Components::WebUIComponent> webUIComponent;
std::unique_ptr<DemoLEDController> demoLedController;
std::unique_ptr<DemoSystemInfo> demoSystemInfo;

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
    demoSystemInfo.reset(new DemoSystemInfo());
    
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
    
    status = demoSystemInfo->begin();
    if (status != DomoticsCore::Components::ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "System Info initialization failed");
        return;
    }
    
    // Register WebUI providers
    webUIComponent->registerProvider("led_control", demoLedController.get());
    webUIComponent->registerProvider("system_info", demoSystemInfo.get());
    
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
    if (demoSystemInfo) {
        demoSystemInfo->loop();
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
