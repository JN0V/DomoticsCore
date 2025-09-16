#pragma once

#include "../include/DomoticsCore/Components/IComponent.h"
#include "../include/DomoticsCore/Components/WebUIHelpers.h"

namespace DomoticsCore {
namespace Components {

/**
 * Example component with optional WebUI support
 * This component will work with or without the WebUI component
 */
class ExampleComponent : public IComponent WEBUI_PROVIDER_INHERITANCE {
private:
    bool ledState = false;
    int brightness = 128;
    String color = "#FF0000";

public:
    ExampleComponent() {
        metadata.name = "Example Component";
        metadata.version = "1.0.0";
        metadata.description = "Example component with optional WebUI";
    }

    ComponentStatus begin() override {
        // Component initialization logic
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }

    void loop() override {
        // Component main loop logic
    }

    ComponentStatus shutdown() override {
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }

    String getName() const override {
        return metadata.name;
    }

    // Regular component methods (always available)
    void setLED(bool state) { ledState = state; }
    bool getLEDState() const { return ledState; }
    void setBrightness(int value) { brightness = value; }
    int getBrightness() const { return brightness; }

#if DOMOTICSCORE_WEBUI_ENABLED
    // WebUI methods (only compiled when WebUI is enabled)
    WebUISection getWebUISection() WEBUI_OVERRIDE {
        WebUISection section("example", "Example Device", "fas fa-lightbulb", "devices");
        
        section.withField(WebUIField("state", "LED State", WebUIFieldType::Boolean, 
                                   ledState ? "true" : "false"))
               .withField(WebUIField("brightness", "Brightness", WebUIFieldType::Slider, 
                                   String(brightness), "%").range(0, 255))
               .withField(WebUIField("color", "Color", WebUIFieldType::Color, color))
               .withAPI("/api/example")
               .withRealTime(2000);
        
        return section;
    }

    String handleWebUIRequest(const String& endpoint, const String& method, 
                            const std::map<String, String>& params) WEBUI_OVERRIDE {
        if (endpoint == "/api/example") {
            if (method == "POST") {
                // Handle updates
                auto stateIt = params.find("state");
                if (stateIt != params.end()) {
                    setLED(stateIt->second == "true");
                }
                
                auto brightnessIt = params.find("brightness");
                if (brightnessIt != params.end()) {
                    setBrightness(brightnessIt->second.toInt());
                }
                
                auto colorIt = params.find("color");
                if (colorIt != params.end()) {
                    color = colorIt->second;
                }
                
                return "{\"status\":\"success\"}";
            }
        }
        return "{\"error\":\"not found\"}";
    }

    String getWebUIData() WEBUI_OVERRIDE {
        return "{\"state\":" + String(ledState ? "true" : "false") + 
               ",\"brightness\":" + String(brightness) + 
               ",\"color\":\"" + color + "\"}";
    }

    bool isWebUIEnabled() WEBUI_OVERRIDE {
        return true; // This component supports WebUI when available
    }
#endif
};

} // namespace Components
} // namespace DomoticsCore
