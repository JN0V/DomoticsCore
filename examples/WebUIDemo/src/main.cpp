#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/WebUI.h>
#include <DomoticsCore/Components/WiFi.h>
#include <DomoticsCore/Components/Storage.h>
#include <DomoticsCore/Components/LED.h>
#include <DomoticsCore/Components/WebUIHelpers.h>
#include <DomoticsCore/Utils/Timer.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Forward declarations
class WebUIWiFiComponent;
class WebUIStorageComponent; 
class WebUILEDComponent;

// Global component pointers for loop access
WebUIWiFiComponent* wifiPtr = nullptr;
WebUIComponent* webUIPtr = nullptr;
WebUIStorageComponent* storagePtr = nullptr;
WebUILEDComponent* ledPtr = nullptr;

/**
 * Enhanced LED component with WebUI integration
 */
class WebUILEDComponent : public LEDComponent, public virtual IWebUIProvider {
private:
    uint8_t currentBrightness = 128;
    String currentEffect = "solid";
    String currentColor = "#ff0000";
    
public:
    WebUILEDComponent() : LEDComponent() {
        // Add a test LED
        LEDConfig config;
        config.pin = 2;
        config.name = "TestLED";
        config.invertLogic = true;
        addLED(config);
        
        // Set initial state
        setLED(0, LEDColor::Red(), currentBrightness);
    }
    
    WebUISection getWebUISection() override {
        WebUISection section("led", "LED Control", "fas fa-lightbulb", "hardware");
        
        section.withField(WebUIField("brightness", "Brightness", WebUIFieldType::Number, 
                                   String(currentBrightness), "", false))
                         
               .withField(WebUIField("color", "Color", WebUIFieldType::Color, 
                                   currentColor, "", false))
                         
               .withField(WebUIField("effect", "Effect", WebUIFieldType::Select, 
                                   currentEffect, "", false))
                         
               .withAPI("/api/led")
               .withRealTime(2000);
        
        return section;
    }
    
    String handleWebUIRequest(const String& endpoint, const String& method, 
                            const std::map<String, String>& params) override {
        JsonDocument response;
        
        if (method == "GET") {
            response["brightness"] = currentBrightness;
            response["effect"] = currentEffect;
            response["color"] = currentColor;
            
        } else if (method == "POST") {
            bool updated = false;
            
            if (params.find("brightness") != params.end()) {
                int newBrightness = params.at("brightness").toInt();
                if (newBrightness >= 0 && newBrightness <= 255) {
                    currentBrightness = newBrightness;
                    setLED(0, LEDColor::Red(), newBrightness);
                    updated = true;
                }
            }
            
            if (params.find("effect") != params.end()) {
                String newEffect = params.at("effect");
                currentEffect = newEffect;
                
                LEDEffect effect = LEDEffect::Solid;
                if (newEffect == "blink") effect = LEDEffect::Blink;
                else if (newEffect == "fade") effect = LEDEffect::Fade;
                else if (newEffect == "pulse") effect = LEDEffect::Pulse;
                else if (newEffect == "breathing") effect = LEDEffect::Breathing;
                
                setLEDEffect(0, effect);
                updated = true;
            }
            
            if (params.find("color") != params.end()) {
                String colorStr = params.at("color");
                currentColor = colorStr;
                
                // Convert hex color to RGB
                if (colorStr.startsWith("#") && colorStr.length() == 7) {
                    long color = strtol(colorStr.substring(1).c_str(), NULL, 16);
                    uint8_t r = (color >> 16) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t b = color & 0xFF;
                    setLED(0, LEDColor(r, g, b), currentBrightness);
                    updated = true;
                }
            }
            
            response["success"] = updated;
            response["message"] = updated ? "LED updated successfully" : "No changes made";
        }
        
        String result;
        serializeJson(response, result);
        return result;
    }
    
    String getWebUIData() override {
        JsonDocument doc;
        doc["brightness"] = currentBrightness;
        doc["effect"] = currentEffect;
        doc["color"] = currentColor;
        doc["status"] = "Active";
        
        String result;
        serializeJson(doc, result);
        return result;
    }
};

/**
 * Enhanced Storage component with WebUI integration
 */
class WebUIStorageComponent : public StorageComponent, public virtual IWebUIProvider {
public:
    WebUIStorageComponent(const StorageConfig& config) : StorageComponent(config) {}

    
    WebUISection getWebUISection() override {
        WebUISection section("storage", "Storage Management", "fas fa-database", "settings");
        
        section.withField(WebUIField("namespace", "Namespace", WebUIFieldType::Display, 
                                   getNamespace(), "", true))
                         
               .withField(WebUIField("entries", "Entries Used", WebUIFieldType::Display, 
                                   String(getEntryCount()), "", true))
                         
               .withField(WebUIField("free_entries", "Free Entries", WebUIFieldType::Display, 
                                   String(getFreeEntries()), "", true))
                         
               .withAPI("/api/storage")
               .withRealTime(5000);
        
        return section;
    }
    
    String handleWebUIRequest(const String& endpoint, const String& method, 
                            const std::map<String, String>& params) override {
        JsonDocument response;
        
        if (endpoint == "/api/storage") {
            response["namespace"] = getNamespace();
            response["entries"] = getEntryCount();
            response["free_entries"] = getFreeEntries();
            
            // Add storage info
            JsonObject info = response["info"].to<JsonObject>();
            info["type"] = "NVS Preferences";
            info["readonly"] = false; // Storage is read-write in demo
        }
        
        String result;
        serializeJson(response, result);
        return result;
    }
    
    String getWebUIData() override {
        JsonDocument doc;
        doc["entries"] = getEntryCount();
        doc["free_entries"] = getFreeEntries();
        
        String result;
        serializeJson(doc, result);
        return result;
    }
};

/**
 * Enhanced WiFi component with WebUI integration
 */
class WebUIWiFiComponent : public WiFiComponent, public virtual IWebUIProvider {
public:
    WebUIWiFiComponent() : WiFiComponent("", "") {}

    
    WebUISection getWebUISection() override {
        WebUISection section("wifi", "WiFi Settings", "fas fa-wifi", "settings");
        
        section.withField(WebUIField("ssid", "Current SSID", WebUIFieldType::Display, 
                                   getSSID(), "", true))
                         
               .withField(WebUIField("ip_address", "IP Address", WebUIFieldType::Display, 
                                   getLocalIP(), "", true))
                         
               .withField(WebUIField("signal_strength", "Signal Strength", WebUIFieldType::Display, 
                                   String(getRSSI()) + " dBm", "dBm", true))
                         
               .withField(WebUIField("connection_status", "Status", WebUIFieldType::Status, 
                                   isConnected() ? "Connected" : "Disconnected", "", true))
                         
               .withAPI("/api/wifi")
               .withRealTime(3000);
        
        return section;
    }
    
    String handleWebUIRequest(const String& endpoint, const String& method, 
                            const std::map<String, String>& params) override {
        JsonDocument response;
        
        if (method == "GET") {
            response["ssid"] = getSSID();
            response["ip_address"] = getLocalIP();
            response["signal_strength"] = getRSSI();
            response["connection_status"] = isConnected() ? "Connected" : "Disconnected";
            response["connected"] = isConnected();
        } else if (method == "POST") {
            auto ssidIt = params.find("ssid");
            auto passwordIt = params.find("password");
            
            if (ssidIt != params.end() && passwordIt != params.end()) {
                // Note: WiFi configuration would need to be implemented
                // For demo purposes, just return success
                response["status"] = "success";
                response["message"] = "WiFi configuration updated";
            } else {
                response["status"] = "error";
                response["message"] = "Missing SSID or password";
            }
        }
        
        String result;
        serializeJson(response, result);
        return result;
    }
    
    String getWebUIData() override {
        JsonDocument doc;
        doc["ssid"] = getSSID();
        doc["ip_address"] = getLocalIP();
        doc["signal_strength"] = getRSSI();
        doc["connected"] = isConnected();
        doc["connection_status"] = isConnected() ? "Connected" : "Disconnected";
        
        String result;
        serializeJson(doc, result);
        return result;
    }
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_CORE, "=== DomoticsCore WebUI Demo Starting ===");
    
    // Create and configure WiFi component (also serves as network provider)
    auto wifiComponent = std::unique_ptr<WebUIWiFiComponent>(new WebUIWiFiComponent());
    wifiPtr = wifiComponent.get();
    
    // Create WebUI component FIRST with network provider dependency injection
    WebUIConfig webUIConfig;
    webUIConfig.deviceName = "WebUI Demo Device";
    webUIConfig.port = 80;
    webUIConfig.enableWebSocket = true;
    webUIConfig.wsUpdateInterval = 2000;
    
    // WebUI is now network-agnostic - inject WiFi as network provider
    auto webUIComponent = std::unique_ptr<WebUIComponent>(new WebUIComponent(webUIConfig, wifiPtr));
    webUIPtr = webUIComponent.get();
    
    // Create and configure Storage component
    StorageConfig storageConfig;
    storageConfig.namespace_name = "webui_demo";
    storageConfig.maxEntries = 50;
    
    auto storageComponent = std::unique_ptr<WebUIStorageComponent>(new WebUIStorageComponent(storageConfig));
    storagePtr = storageComponent.get();
    
    // Create and configure LED component
    auto ledComponent = std::unique_ptr<WebUILEDComponent>(new WebUILEDComponent());
    ledPtr = ledComponent.get();
    
    // Initialize components in correct order
    DLOG_I(LOG_CORE, "Initializing WiFi component...");
    ComponentStatus status = wifiPtr->begin();
    if (status != ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "WiFi initialization failed with status: %d", (int)status);
        return;
    }
    
    DLOG_I(LOG_CORE, "Initializing WebUI component...");
    status = webUIPtr->begin();
    if (status != ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "WebUI initialization failed with status: %d", (int)status);
        return;
    }
    
    DLOG_I(LOG_CORE, "Initializing Storage component...");
    status = storagePtr->begin();
    if (status != ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "Storage initialization failed with status: %d", (int)status);
        return;
    }
    
    DLOG_I(LOG_CORE, "Initializing LED component...");
    status = ledPtr->begin();
    if (status != ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "LED initialization failed with status: %d", (int)status);
        return;
    }
    
    // Register WebUI providers AFTER WebUI is initialized
    DLOG_I(LOG_CORE, "Registering WebUI providers...");
    webUIPtr->registerProvider(ledPtr);
    webUIPtr->registerProvider(wifiPtr);
    webUIPtr->registerProvider(storagePtr);
    
    DLOG_I(LOG_CORE, "=== WebUI Demo Setup Complete ===");
    if (wifiPtr->isConnected()) {
        DLOG_I(LOG_CORE, "WebUI available at: http://%s", wifiPtr->getLocalIP().c_str());
    } else {
        DLOG_I(LOG_CORE, "WebUI available at: http://192.168.4.1 (AP mode)");
    }
}

void loop() {
    // Run component loops manually since we're not using Core
    if (wifiPtr) wifiPtr->loop();
    if (webUIPtr) webUIPtr->loop();
    if (storagePtr) storagePtr->loop();
    if (ledPtr) ledPtr->loop();
    
    // System status reporting
    static Utils::NonBlockingDelay statusTimer(30000); // 30 seconds
    if (statusTimer.isReady()) {
        DLOG_I(LOG_CORE, "=== WebUI Demo System Status ===");
        DLOG_I(LOG_CORE, "Uptime: %lu seconds", millis() / 1000);
        DLOG_I(LOG_CORE, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_CORE, "WebSocket clients: %d", webUIPtr->getWebSocketClients());
        
        // Show component status
        DLOG_I(LOG_CORE, "Component Status:");
        DLOG_I(LOG_CORE, "- WebUI: Running on port %d", webUIPtr->getPort());
        DLOG_I(LOG_CORE, "- WiFi: %s", wifiPtr->isConnected() ? "Connected" : "Disconnected");
        DLOG_I(LOG_CORE, "- LED: Active");
        DLOG_I(LOG_CORE, "- Storage: Active");
    }
}
