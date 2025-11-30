#pragma once

/**
 * @file LED.h
 * @brief Declares the DomoticsCore LED component for single-color and RGB LED control.
 * 
 * @example DomoticsCore-LED/examples/BasicLED/src/main.cpp
 * @example DomoticsCore-LED/examples/LEDWithWebUI/src/main.cpp
 */

#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Timer.h>
#include <Arduino.h>
#include <vector>

namespace DomoticsCore {
namespace Components {

// LED effect types
enum class LEDEffect {
    Solid,      // Constant brightness
    Blink,      // On/off blinking
    Fade,       // Smooth fade in/out
    Pulse,      // Heartbeat-like pulse
    Rainbow,    // Color cycling (RGB LEDs)
    Breathing   // Smooth breathing effect
};

// LED color structure for RGB LEDs
struct LEDColor {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    
    LEDColor() = default;
    LEDColor(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) {}
    
    // Predefined colors
    static LEDColor White() { return LEDColor(255, 255, 255); }
    static LEDColor Red() { return LEDColor(255, 0, 0); }
    static LEDColor Green() { return LEDColor(0, 255, 0); }
    static LEDColor Blue() { return LEDColor(0, 0, 255); }
    static LEDColor Yellow() { return LEDColor(255, 255, 0); }
    static LEDColor Cyan() { return LEDColor(0, 255, 255); }
    static LEDColor Magenta() { return LEDColor(255, 0, 255); }
    static LEDColor Off() { return LEDColor(0, 0, 0); }
};

// Individual LED configuration
struct LEDConfig {
    int pin = -1;
    bool isRGB = false;
    int redPin = -1;
    int greenPin = -1;
    int bluePin = -1;
    uint8_t maxBrightness = 255;
    bool invertLogic = false;  // true for common anode RGB LEDs
    String name = "";
};

// LED state for effects
struct LEDState {
    LEDColor currentColor;
    uint8_t brightness = 0;
    LEDEffect effect = LEDEffect::Solid;
    unsigned long effectSpeed = 1000;  // milliseconds
    bool enabled = true;
    
    // Effect state variables
    unsigned long lastUpdate = 0;
    float effectPhase = 0.0;
    bool effectDirection = true;
};

/**
 * @class DomoticsCore::Components::LEDComponent
 * @brief Drives one or more LEDs (single-color or RGB) with PWM brightness and effects.
 *
 * Manages pin initialization, supports named LEDs, and provides built-in effects updated via a
 * non-blocking timer. Can be paired with a WebUI provider to expose UI controls.
 */
class LEDComponent : public IComponent {
private:
    std::vector<LEDConfig> ledConfigs;
    std::vector<LEDState> ledStates;
    Utils::NonBlockingDelay updateTimer;

public:
    /**
     * @brief Construct a new LEDComponent with a 20 Hz update timer.
     */
    LEDComponent() : updateTimer(50) {  // 20Hz update rate
        // Set component metadata
        metadata.name = "LEDComponent";
        metadata.version = "1.2.1";
        metadata.author = "DomoticsCore";
        metadata.description = "Multi-LED management with PWM control and effects";
        metadata.category = "Hardware";
        metadata.tags = {"led", "pwm", "effects", "hardware"};
    }
    
    // Required IComponent methods
    
    // Component lifecycle
    /**
     * @brief Validate pin assignments, initialize hardware, and reset state.
     */
    ComponentStatus begin() override {
        DLOG_I(LOG_LED, "Initializing...");
        
        // Validate LED pin assignments
        if (!validateLEDPins()) {
            setStatus(ComponentStatus::ConfigError);
            return ComponentStatus::ConfigError;
        }
        
        // Initialize hardware pins
        initializePins();
        
        // Initialize LED states
        ledStates.resize(ledConfigs.size());
        for (size_t i = 0; i < ledStates.size(); i++) {
            ledStates[i].currentColor = LEDColor::Off();
            ledStates[i].brightness = 0;
            ledStates[i].effect = LEDEffect::Solid;
            ledStates[i].effectSpeed = 1000;
            ledStates[i].enabled = true;
        }
        
        DLOG_I(LOG_LED, "Initialized %d LEDs successfully", ledConfigs.size());
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    /**
     * @brief Update LED effects when the non-blocking timer fires.
     */
    void loop() override {
        if (getLastStatus() != ComponentStatus::Success) return;

        if (updateTimer.isReady()) {
            updateEffects();
        }
    }
    
    /**
     * @brief Turn off all LEDs and release resources.
     */
    ComponentStatus shutdown() override {
        DLOG_I(LOG_LED, "Shutting down...");

        // Turn off all LEDs
        for (size_t i = 0; i < ledConfigs.size(); i++) {
            setLEDOutput(i, LEDColor::Off(), 0);
        }
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    // Configuration setup
    /**
     * @brief Add a fully-specified LED configuration (single or RGB).
     */
    void addLED(const LEDConfig& config) {
        ledConfigs.push_back(config);
    }

    /**
     * @brief Convenience helper to register a single-channel LED.
     */
    void addSingleLED(int pin, const String& name = "", uint8_t maxBrightness = 255, bool invertLogic = false) {
        LEDConfig config;
        config.pin = pin;
        config.isRGB = false;
        config.name = name.isEmpty() ? ("LED_" + String(ledConfigs.size())) : name;
        config.maxBrightness = maxBrightness;
        config.invertLogic = invertLogic;
        addLED(config);
    }

    /**
     * @brief Register a three-channel RGB LED using discrete GPIO pins.
     */
    void addRGBLED(int redPin, int greenPin, int bluePin, const String& name = "", 
                   uint8_t maxBrightness = 255, bool invertLogic = false) {
        LEDConfig config;
        config.isRGB = true;
        config.redPin = redPin;
        config.greenPin = greenPin;
        config.bluePin = bluePin;
        config.name = name.isEmpty() ? ("RGB_" + String(ledConfigs.size())) : name;
        config.maxBrightness = maxBrightness;
        config.invertLogic = invertLogic;
        addLED(config);
    }
    
    // LED control methods
    /**
     * @brief Set LED color/brightness and clear any active effect.
     */
    bool setLED(size_t ledIndex, const LEDColor& color, uint8_t brightness = 255) {
        if (ledIndex >= ledStates.size()) return false;

        ledStates[ledIndex].currentColor = color;
        ledStates[ledIndex].brightness = brightness;
        ledStates[ledIndex].effect = LEDEffect::Solid;
        return true;
    }

    /**
     * @brief Lookup an LED by name and assign color/brightness.
     */
    bool setLED(const String& name, const LEDColor& color, uint8_t brightness = 255) {
        for (size_t i = 0; i < ledConfigs.size(); i++) {
            if (ledConfigs[i].name == name) {
                return setLED(i, color, brightness);
            }
        }
        return false;
    }

    /**
     * @brief Apply an animated effect to an LED by index.
     */
    bool setLEDEffect(size_t ledIndex, LEDEffect effect, unsigned long speed = 1000) {
        if (ledIndex >= ledStates.size()) return false;
        
        // Reference state and config for convenience
        auto &state = ledStates[ledIndex];
        const auto &config = ledConfigs[ledIndex];

        // Apply effect parameters
        state.effect = effect;
        state.effectSpeed = speed;
        state.effectPhase = 0.0;
        state.lastUpdate = millis();

        // Ensure LED is enabled when an effect is applied
        state.enabled = true;

        // If no brightness has been set yet, default to the configured max brightness
        // This fixes the issue where effects appear to do nothing because brightness was 0.
        if (state.brightness == 0) {
            state.brightness = config.maxBrightness;
        }

        // If color was never set (Off), default to White so single LEDs (non-RGB)
        // actually emit light when effects run. For RGB, White is also a sensible default.
        if (state.currentColor.red == 0 && state.currentColor.green == 0 && state.currentColor.blue == 0) {
            state.currentColor = LEDColor::White();
        }

        return true;
    }

    /**
     * @brief Apply an animated effect to an LED by name.
     */
    bool setLEDEffect(const String& name, LEDEffect effect, unsigned long speed = 1000) {
        for (size_t i = 0; i < ledConfigs.size(); i++) {
            if (ledConfigs[i].name == name) {
                return setLEDEffect(i, effect, speed);
            }
        }
        return false;
    }

    /**
     * @brief Enable or disable an LED by index (disabled LEDs are forced off).
     */
    bool enableLED(size_t ledIndex, bool enabled = true) {
        if (ledIndex >= ledStates.size()) return false;
        ledStates[ledIndex].enabled = enabled;
        if (!enabled) {
            setLEDOutput(ledIndex, LEDColor::Off(), 0);
        }
        return true;
    }

    /**
     * @brief Enable or disable an LED by name.
     */
    bool enableLED(const String& name, bool enabled = true) {
        for (size_t i = 0; i < ledConfigs.size(); i++) {
            if (ledConfigs[i].name == name) {
                return enableLED(i, enabled);
            }
        }
        return false;
    }

    /**
     * @brief Number of configured LEDs (single or RGB entries).
     */
    size_t getLEDCount() const { return ledConfigs.size(); }

    /**
     * @brief Retrieve friendly names for all configured LEDs.
     */
    std::vector<String> getLEDNames() const {
        std::vector<String> names;
        for (const auto& config : ledConfigs) {
            names.push_back(config.name);
        }
        return names;
    }

    /**
     * @brief Compose a human-readable description of an LED state.
     */
    String getLEDStatus(size_t ledIndex) const {
        if (ledIndex >= ledStates.size()) return "Invalid index";

        const auto& state = ledStates[ledIndex];
        const auto& config = ledConfigs[ledIndex];
        
        String status = "LED '" + config.name + "': ";
        status += state.enabled ? "Enabled" : "Disabled";
        
        if (state.enabled) {
            status += ", Color: RGB(" + String(state.currentColor.red) + "," + 
                     String(state.currentColor.green) + "," + String(state.currentColor.blue) + ")";
            status += ", Brightness: " + String(state.brightness);
            status += ", Effect: " + getEffectName(state.effect);
        }
        
        return status;
    }
    
    String getEffectName(LEDEffect effect) const {
        switch (effect) {
            case LEDEffect::Solid: return "Solid";
            case LEDEffect::Blink: return "Blink";
            case LEDEffect::Fade: return "Fade";
            case LEDEffect::Pulse: return "Pulse";
            case LEDEffect::Rainbow: return "Rainbow";
            case LEDEffect::Breathing: return "Breathing";
            default: return "Unknown";
        }
    }

private:
    // Private helper methods
    bool validateLEDPins() {
        for (size_t i = 0; i < ledConfigs.size(); i++) {
            const auto& config = ledConfigs[i];
            
            if (config.isRGB) {
                if (config.redPin < 0 || config.greenPin < 0 || config.bluePin < 0) {
                    DLOG_E(LOG_LED, "Invalid RGB pins for LED '%s': R=%d, G=%d, B=%d", 
                                config.name.c_str(), config.redPin, config.greenPin, config.bluePin);
                    return false;
                }
            } else {
                if (config.pin < 0) {
                    DLOG_E(LOG_LED, "Invalid pin for LED '%s': %d", config.name.c_str(), config.pin);
                    return false;
                }
            }
        }
        return true;
    }
    
    void initializePins() {
        for (const auto& config : ledConfigs) {
            if (config.isRGB) {
                pinMode(config.redPin, OUTPUT);
                pinMode(config.greenPin, OUTPUT);
                pinMode(config.bluePin, OUTPUT);
                setPWMOutput(config.redPin, 0, config.invertLogic);
                setPWMOutput(config.greenPin, 0, config.invertLogic);
                setPWMOutput(config.bluePin, 0, config.invertLogic);
            } else {
                pinMode(config.pin, OUTPUT);
                setPWMOutput(config.pin, 0, config.invertLogic);
            }
        }
    }
    
    void setPWMOutput(int pin, uint8_t value, bool invert) {
        if (pin < 0) return;
        uint8_t outputValue = invert ? (255 - value) : value;
        analogWrite(pin, outputValue);
    }
    
    void setLEDOutput(size_t ledIndex, const LEDColor& color, uint8_t brightness) {
        if (ledIndex >= ledConfigs.size()) return;
        
        const auto& config = ledConfigs[ledIndex];
        uint8_t scaledBrightness = map(brightness, 0, 255, 0, config.maxBrightness);
        
        if (config.isRGB) {
            uint8_t red = map(color.red, 0, 255, 0, scaledBrightness);
            uint8_t green = map(color.green, 0, 255, 0, scaledBrightness);
            uint8_t blue = map(color.blue, 0, 255, 0, scaledBrightness);
            
            setPWMOutput(config.redPin, red, config.invertLogic);
            setPWMOutput(config.greenPin, green, config.invertLogic);
            setPWMOutput(config.bluePin, blue, config.invertLogic);
        } else {
            uint8_t value = (color.red > 0 || color.green > 0 || color.blue > 0) ? scaledBrightness : 0;
            setPWMOutput(config.pin, value, config.invertLogic);
        }
    }
    
    void updateEffects() {
        unsigned long currentTime = millis();
        
        for (size_t i = 0; i < ledStates.size(); i++) {
            auto& state = ledStates[i];
            
            if (!state.enabled || state.effect == LEDEffect::Solid) {
                if (state.effect == LEDEffect::Solid) {
                    setLEDOutput(i, state.currentColor, state.brightness);
                }
                continue;
            }
            
            // Calculate effect phase (0.0 to 1.0)
            float deltaTime = (currentTime - state.lastUpdate) / (float)state.effectSpeed;
            state.effectPhase += deltaTime;
            
            if (state.effectPhase > 1.0) {
                state.effectPhase = 0.0;
            }
            
            state.lastUpdate = currentTime;
            
            // Apply effect
            LEDColor outputColor = state.currentColor;
            uint8_t outputBrightness = state.brightness;
            
            switch (state.effect) {
                case LEDEffect::Blink:
                    outputBrightness = (state.effectPhase < 0.5) ? state.brightness : 0;
                    break;
                    
                case LEDEffect::Fade:
                    outputBrightness = (uint8_t)(state.brightness * (sin(state.effectPhase * 2 * PI) + 1) / 2);
                    break;
                    
                case LEDEffect::Pulse:
                    if (state.effectPhase < 0.3) {
                        outputBrightness = (uint8_t)(state.brightness * sin(state.effectPhase * PI / 0.3));
                    } else if (state.effectPhase < 0.5) {
                        outputBrightness = (uint8_t)(state.brightness * sin((state.effectPhase - 0.3) * PI / 0.2));
                    } else {
                        outputBrightness = 0;
                    }
                    break;
                    
                case LEDEffect::Rainbow:
                    if (ledConfigs[i].isRGB) {
                        float hue = state.effectPhase * 360.0;
                        // Simple HSV to RGB conversion
                        if (hue < 120) {
                            outputColor = LEDColor(255 - hue * 2.125, hue * 2.125, 0);
                        } else if (hue < 240) {
                            outputColor = LEDColor(0, 255 - (hue - 120) * 2.125, (hue - 120) * 2.125);
                        } else {
                            outputColor = LEDColor((hue - 240) * 2.125, 0, 255 - (hue - 240) * 2.125);
                        }
                        outputBrightness = state.brightness;
                    }
                    break;
                    
                case LEDEffect::Breathing:
                    outputBrightness = (uint8_t)(state.brightness * (1 - cos(state.effectPhase * 2 * PI)) / 2);
                    break;
                    
                default:
                    break;
            }
            
            setLEDOutput(i, outputColor, outputBrightness);
        }
    }

};

} // namespace Components
} // namespace DomoticsCore
