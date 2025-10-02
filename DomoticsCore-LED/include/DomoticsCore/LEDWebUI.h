#pragma once

#include <Arduino.h>
#include <DomoticsCore/IWebUIProvider.h>
#include <DomoticsCore/LED.h>
#include <ArduinoJson.h>
#include <DomoticsCore/BaseWebUIComponents.h>

namespace DomoticsCore {
namespace Components {

// Simple WebUI provider wrapper for LEDComponent
class LEDWebUI : public IWebUIProvider {
private:
    LEDComponent* led; // non-owning
    // Simple mirrored state for UI
    size_t selected = 0;
    bool enabled = false; // start OFF by default
    uint8_t brightness = 128;
    LEDEffect effect = LEDEffect::Solid;
    bool initialApplied = false;

public:
    explicit LEDWebUI(LEDComponent* comp) : led(comp) {}

    String getWebUIName() const override { return "LED"; }
    String getWebUIVersion() const override { return "1.0.0"; }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!led) return contexts;

        // Apply initial mirrored state once to ensure UI and hardware match
        if (!initialApplied && led->getLEDCount() > 0) {
            selected = 0;
            if (enabled) {
                led->enableLED(selected, true);
                led->setLED(selected, LEDColor::White(), brightness);
                if (effect != LEDEffect::Solid) {
                    led->setLEDEffect(selected, effect, 1000);
                }
            } else {
                led->setLED(selected, LEDColor::Off(), 0);
                led->enableLED(selected, false);
            }
            initialApplied = true;
        }

        // Status badge showing ON/OFF quickly at the top
        contexts.push_back(WebUIContext::statusBadge("led_status", "LED", "bulb-twotone")
            .withField(WebUIField("state", "State", WebUIFieldType::Status, enabled ? "ON" : "OFF"))
            .withRealTime(1000)
            .withCustomCss(R"(
                .status-indicator[data-context-id='led_status'] .status-icon { color: var(--text-secondary); }
                .status-indicator[data-context-id='led_status'].active .status-icon { color: #ffc107; filter: drop-shadow(0 0 6px rgba(255,193,7,0.6)); }
            )"));

        // Dashboard: selection + primary controls
        // Build LED names list for select options
        String namesCsv;
        String currentName = "";
        std::vector<String> nameOptions;
        {
            auto names = led->getLEDNames();
            // Fallback: generate default names if not provided
            if (names.empty()) {
                size_t count = led->getLEDCount();
                for (size_t i = 0; i < count; ++i) names.push_back(String("LED_") + String(i));
            }
            for (size_t i = 0; i < names.size(); ++i) {
                namesCsv += names[i];
                if (i + 1 < names.size()) namesCsv += ",";
                nameOptions.push_back(names[i]);
            }
            if (!names.empty()) {
                selected = selected < names.size() ? selected : 0;
                currentName = names[selected];
            }
        }
        contexts.push_back(WebUIContext::dashboard("led_dashboard", "LED Control")
            .withField(WebUIField("led_select", "LED", WebUIFieldType::Select, currentName).choices(nameOptions))
            .withField(WebUIField("enabled_toggle", "Enabled", WebUIFieldType::Boolean, enabled ? "true" : "false"))
            .withField(WebUIField("brightness", "Brightness", WebUIFieldType::Slider, String((int)brightness)).range(0, 255))
            .withField(WebUIField("effect", "Effect", WebUIFieldType::Select, effectToString(effect)).choices({"Solid","Blink","Fade","Pulse","Rainbow","Breathing"}))
            .withRealTime(1000));

        return contexts;
    }

    String getWebUIData(const String& contextId) override {
        JsonDocument doc;
        if (contextId == "led_dashboard") {
            // reflect current LED name as the selected value
            {
                auto names = led->getLEDNames();
                if (!names.empty()) {
                    size_t idx = selected < names.size() ? selected : 0;
                    doc["led_select"] = names[idx];
                } else {
                    doc["led_select"] = "";
                }
            }
            doc["enabled_toggle"] = enabled;
            doc["brightness"] = (int)brightness;
            doc["effect"] = effectToString(effect);
        } else if (contextId == "led_status") {
            doc["state"] = enabled ? "ON" : "OFF";
        }
        String json; serializeJson(doc, json); return json;
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params) override {
        if (!led || method != "POST") return "{\"success\":false}";
        auto fieldIt = params.find("field"); auto valueIt = params.find("value");
        if (fieldIt == params.end() || valueIt == params.end()) return "{\"success\":false}";
        const String& field = fieldIt->second;
        const String& value = valueIt->second;

        // Helper to clamp selected index
        auto clampIndex = [this](size_t idx){ size_t n = led->getLEDCount(); return (n == 0) ? (size_t)0 : (idx < n ? idx : n-1); };

        if (field == "led_select") {
            // value is the LED name; map back to index
            auto names = led->getLEDNames();
            for (size_t i = 0; i < names.size(); ++i) {
                if (names[i] == value) { selected = i; break; }
            }
            return "{\"success\":true}";
        }
        if (field == "enabled_toggle") {
            enabled = (value == "true");
            led->enableLED(selected, enabled);
            if (enabled) {
                // Apply brightness/color immediately when enabling
                led->setLED(selected, LEDColor::White(), brightness);
                if (effect != LEDEffect::Solid) {
                    led->setLEDEffect(selected, effect, 1000);
                }
            } else {
                // Turn off output when disabling
                led->setLED(selected, LEDColor::Off(), 0);
            }
            return "{\"success\":true}";
        }
        if (field == "brightness") {
            int b = value.toInt();
            if (b < 0) b = 0; if (b > 255) b = 255;
            brightness = (uint8_t)b;
            // Keep current color intent: ON -> white at brightness, OFF -> off
            if (enabled) led->setLED(selected, LEDColor::White(), brightness);
            else         led->setLED(selected, LEDColor::Off(), 0);
            return "{\"success\":true}";
        }
        if (field == "effect") {
            effect = stringToEffect(value);
            // Apply with default speed
            if (effect == LEDEffect::Solid) {
                if (enabled) led->setLED(selected, LEDColor::White(), brightness);
                else         led->setLED(selected, LEDColor::Off(), 0);
            } else {
                led->setLEDEffect(selected, effect, 1000);
            }
            return "{\"success\":true}";
        }
        return "{\"success\":false}";
    }

private:
    static String effectToString(LEDEffect e) {
        switch (e) {
            case LEDEffect::Solid: return "Solid";
            case LEDEffect::Blink: return "Blink";
            case LEDEffect::Fade: return "Fade";
            case LEDEffect::Pulse: return "Pulse";
            case LEDEffect::Rainbow: return "Rainbow";
            case LEDEffect::Breathing: return "Breathing";
            default: return "Solid";
        }
    }
    static LEDEffect stringToEffect(const String& s) {
        if (s == "Blink") return LEDEffect::Blink;
        if (s == "Fade") return LEDEffect::Fade;
        if (s == "Pulse") return LEDEffect::Pulse;
        if (s == "Rainbow") return LEDEffect::Rainbow;
        if (s == "Breathing") return LEDEffect::Breathing;
        return LEDEffect::Solid;
    }
};

} // namespace Components
} // namespace DomoticsCore
