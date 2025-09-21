#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/WebUI.h>
#include <DomoticsCore/Components/WebUI/SystemInfoWebUI.h>
#include <DomoticsCore/Components/WebUI/BaseWebUIComponents.h>
#include <DomoticsCore/Components/IWebUIProvider.h>
#include <DomoticsCore/Utils/Timer.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

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
        // The loop is now empty to prevent automatic blinking and respect manual control.
    }
    
    ComponentStatus shutdown() override {
        digitalWrite(demoLedPin, LOW);
        return ComponentStatus::Success;
    }
    
    // WebUI Provider methods
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        
        // Dashboard card with custom bulb visualization
        contexts.push_back(WebUIContext::dashboard("led_dashboard", "LED Control")
            .withField(WebUIField("state_toggle_dashboard", "LED", WebUIFieldType::Boolean, demoLedState ? "true" : "false"))
            .withRealTime(1000)
            .withCustomHtml(R"(
                <div class="card-header">
                    <h3 class="card-title">LED Control</h3>
                </div>
                <div class="card-content led-dashboard">
                    <div class="led-bulb-container">
                        <svg class="led-bulb" viewBox="0 0 1024 1024">
                            <use href="#bulb-twotone"/>
                        </svg>
                    </div>
                    <div class="field-row">
                        <span class="field-label">LED:</span>
                        <label class="toggle-switch">
                            <input type="checkbox" id="state_toggle_dashboard">
                            <span class="slider"></span>
                        </label>
                    </div>
                </div>
            )")
            .withCustomCss(R"(
                .led-dashboard .led-bulb-container {
                    display: flex;
                    justify-content: center;
                    margin-bottom: 1rem;
                }
                .led-dashboard .led-bulb {
                    width: 64px;
                    height: 64px;
                    transition: all 0.3s ease;
                    filter: drop-shadow(0 0 8px rgba(255, 193, 7, 0.3));
                }
                .led-dashboard .led-bulb.on {
                    color: #ffc107;
                    filter: drop-shadow(0 0 16px rgba(255, 193, 7, 0.8));
                }
                .led-dashboard .led-bulb.off {
                    color: #6c757d;
                    filter: none;
                }
            )")
            .withCustomJs(R"(
                // Update bulb appearance based on LED state
                function updateLEDBulb() {
                    const bulb = document.querySelector('.led-dashboard .led-bulb');
                    const toggle = document.querySelector('#state_toggle_dashboard');
                    if (bulb && toggle) {
                        bulb.classList.toggle('on', toggle.checked);
                        bulb.classList.toggle('off', !toggle.checked);
                    }
                }
                // Update on toggle change
                document.addEventListener('change', function(e) {
                    if (e.target.id === 'state_toggle_dashboard') {
                        updateLEDBulb();
                    }
                });
                // Initial update
                setTimeout(updateLEDBulb, 100);
            )"));

        // Header status badge using BaseWebUIComponents
        contexts.push_back(DomoticsCore::Components::WebUI::BaseWebUIComponents::createStatusBadge("led_status", "LED", "bulb-twotone")
            .withField(WebUIField("state", "State", WebUIFieldType::Status, demoLedState ? "ON" : "OFF"))
            .withRealTime(1000)
            .withCustomCss(R"(
                .status-indicator[data-context-id='led_status'] .status-icon { color: var(--text-secondary); }
                .status-indicator[data-context-id='led_status'].active .status-icon { color: #ffc107; filter: drop-shadow(0 0 6px rgba(255,193,7,0.6)); }
            )"));

        // Settings context with detailed controls
        contexts.push_back(WebUIContext::settings("led_settings", "LED Controller")
            .withField(WebUIField("state_toggle_settings", "LED", WebUIFieldType::Boolean, demoLedState ? "true" : "false"))
            .withField(WebUIField("pin_display", "GPIO Pin", WebUIFieldType::Display, String(demoLedPin), "", true))
            .withCustomHtml(R"(
                <div class="card-header">
                    <h3 class="card-title">LED Controller</h3>
                </div>
                <div class="card-content led-settings">
                    <div class="led-status-display">
                        <svg class="led-bulb-small" viewBox="0 0 1024 1024">
                            <use href="#bulb-twotone"/>
                        </svg>
                        <span class="led-status-text">OFF</span>
                    </div>
                    <div class="field-row">
                        <span class="field-label">LED:</span>
                        <label class="toggle-switch">
                            <input type="checkbox" id="state_toggle_settings">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="field-row">
                        <span class="field-label">GPIO Pin:</span>
                        <span class="field-value" data-field-name="pin_display">)" + String(demoLedPin) + R"(</span>
                    </div>
                </div>
            )")
            .withCustomCss(R"(
                .led-settings .led-status-display {
                    display: flex;
                    align-items: center;
                    gap: 0.5rem;
                    margin-bottom: 1rem;
                    padding: 0.5rem;
                    background: rgba(255, 255, 255, 0.05);
                    border-radius: 0.5rem;
                }
                .led-settings .led-bulb-small {
                    width: 24px;
                    height: 24px;
                    transition: all 0.3s ease;
                }
                .led-settings .led-bulb-small.on {
                    color: #ffc107;
                    filter: drop-shadow(0 0 4px rgba(255, 193, 7, 0.6));
                }
                .led-settings .led-bulb-small.off {
                    color: #6c757d;
                }
                .led-settings .led-status-text {
                    font-weight: 600;
                    font-size: 0.9rem;
                }
                .led-settings .led-status-text.on {
                    color: #ffc107;
                }
                .led-settings .led-status-text.off {
                    color: #6c757d;
                }
            )")
            .withCustomJs(R"(
                // Update settings display based on LED state
                function updateLEDSettings() {
                    const bulb = document.querySelector('.led-settings .led-bulb-small');
                    const statusText = document.querySelector('.led-settings .led-status-text');
                    const toggle = document.querySelector('#state_toggle_settings');
                    if (bulb && statusText && toggle) {
                        const isOn = toggle.checked;
                        bulb.classList.toggle('on', isOn);
                        bulb.classList.toggle('off', !isOn);
                        statusText.classList.toggle('on', isOn);
                        statusText.classList.toggle('off', !isOn);
                        statusText.textContent = isOn ? 'ON' : 'OFF';
                    }
                }
                // Update on toggle change
                document.addEventListener('change', function(e) {
                    if (e.target.id === 'state_toggle_settings') {
                        updateLEDSettings();
                    }
                });
                // Initial update
                setTimeout(updateLEDSettings, 100);
            )"));

        return contexts;
    }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint, 
                             const String& method, const std::map<String, String>& params) override {
        if ((contextId == "led_settings" || contextId == "led_dashboard") && method == "POST") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");

            if (fieldIt != params.end() && valueIt != params.end()) {
                if (fieldIt->second == "state_toggle_dashboard" || fieldIt->second == "state_toggle_settings") {
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
        if (contextId == "led_dashboard" || contextId == "led_settings") {
            JsonDocument doc;
            doc["state_toggle_dashboard"] = demoLedState;
            doc["state_toggle_settings"] = demoLedState;
            doc["gpio_pin"] = demoLedPin;
            String json;
            serializeJson(doc, json);
            return json;
        } else if (contextId == "led_status") {
            JsonDocument doc;
            doc["state"] = demoLedState ? "ON" : "OFF";
            String json;
            serializeJson(doc, json);
            return json;
        }
        return "{}";
    }

    String getWebUIName() const override { return metadata.name; }
    String getWebUIVersion() const override { return metadata.version; }
};

// No wrapper needed! SystemInfoComponent now includes WebUI functionality by default


// Global component storage
std::unique_ptr<DomoticsCore::Components::WebUIComponent> webUIComponent;
std::unique_ptr<DemoLEDController> demoLedController;
std::unique_ptr<WebUI::SystemInfoWebUI> systemInfoWebUI;

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
    webUIConfig.wsUpdateInterval = 2000;
    webUIConfig.useFileSystem = false;
    
    webUIComponent.reset(new DomoticsCore::Components::WebUIComponent(webUIConfig));
    
    // Create demo components
    demoLedController.reset(new DemoLEDController(2));
    systemInfoWebUI.reset(new WebUI::SystemInfoWebUI());
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
    
    status = systemInfoWebUI->begin();
    if (status != DomoticsCore::Components::ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "System Info initialization failed");
        return;
    }
    
    // Register WebUI providers
    webUIComponent->registerProvider(webUIComponent.get()); // Register itself
    webUIComponent->registerProvider(demoLedController.get());
    webUIComponent->registerProvider(systemInfoWebUI.get());
    
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
    if (systemInfoWebUI) {
        systemInfoWebUI->loop();
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
