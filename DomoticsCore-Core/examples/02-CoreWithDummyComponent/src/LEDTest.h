#pragma once

#include <DomoticsCore/LED.h>
#include <DomoticsCore/Logger.h>
#include <memory>

using namespace DomoticsCore::Components;

// Custom application log tag
#define LOG_APP "APP"

// LED Test Component - demonstrates comprehensive LED functionality
class LEDTestComponent : public IComponent {
private:
    std::unique_ptr<LEDComponent> ledManager;
    Utils::NonBlockingDelay demoTimer;
    int currentDemo = 0;
    int maxDemos = 6;
    
public:
    LEDTestComponent(const String& name) 
        : IComponent(name), demoTimer(5000) {  // Change demo every 5 seconds
        
        metadata.name = name;
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "LED component demonstration with various effects";
        metadata.category = "Test";
        metadata.tags = {"led", "test", "demo", "effects"};
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_APP, "[LEDTest] Initializing LED test component...");
        
        // Create LED manager
        ledManager = std::make_unique<LEDComponent>("LEDManager");
        
        // Configure LEDs for testing
        // Single LEDs on pins 2, 4, 16
        ledManager->addSingleLED(2, "StatusLED", 255, false);
        ledManager->addSingleLED(4, "ActivityLED", 255, false);
        ledManager->addSingleLED(16, "ErrorLED", 255, false);
        
        // RGB LED on pins 17, 18, 19 (common cathode)
        ledManager->addRGBLED(17, 18, 19, "MainRGB", 255, false);
        
        // RGB LED on pins 21, 22, 23 (common anode - inverted logic)
        ledManager->addRGBLED(21, 22, 23, "SecondaryRGB", 255, true);
        
        // Initialize LED manager
        ComponentStatus status = ledManager->begin();
        if (status != ComponentStatus::Success) {
            DLOG_I(LOG_APP, "[LEDTest] Failed to initialize LED manager: %s", 
                         statusToString(status).c_str());
            setStatus(status);
            return status;
        }
        
        DLOG_I(LOG_APP, "[LEDTest] Initialized with %d LEDs", ledManager->getLEDCount());
        
        // List all configured LEDs
        auto ledNames = ledManager->getLEDNames();
        for (const auto& name : ledNames) {
            DLOG_I(LOG_APP, "[LEDTest] - LED: %s", name.c_str());
        }
        
        // Start first demo
        startDemo(0);
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (getLastStatus() != ComponentStatus::Success || !ledManager) return;
        
        // Update LED manager
        ledManager->loop();
        
        // Change demo periodically
        if (demoTimer.isReady()) {
            currentDemo = (currentDemo + 1) % maxDemos;
            startDemo(currentDemo);
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_APP, "[LEDTest] Shutting down LED test component...");
        
        if (ledManager) {
            ledManager->shutdown();
        }
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
private:
    void startDemo(int demoIndex) {
        if (!ledManager) return;
        
        DLOG_I(LOG_APP, "[LEDTest] Starting demo %d/%d", demoIndex + 1, maxDemos);
        
        switch (demoIndex) {
            case 0:
                demoSolidColors();
                break;
            case 1:
                demoBlinkingEffects();
                break;
            case 2:
                demoFadeEffects();
                break;
            case 3:
                demoPulseEffects();
                break;
            case 4:
                demoRainbowEffects();
                break;
            case 5:
                demoBreathingEffects();
                break;
        }
    }
    
    void demoSolidColors() {
        DLOG_I(LOG_APP, "[LEDTest] Demo: Solid Colors");
        
        // Set single LEDs to different colors/brightness
        ledManager->setLED("StatusLED", LEDColor::Green(), 128);
        ledManager->setLED("ActivityLED", LEDColor::Blue(), 255);
        ledManager->setLED("ErrorLED", LEDColor::Red(), 64);
        
        // Set RGB LEDs to different colors
        ledManager->setLED("MainRGB", LEDColor::Yellow(), 200);
        ledManager->setLED("SecondaryRGB", LEDColor::Cyan(), 150);
    }
    
    void demoBlinkingEffects() {
        DLOG_I(LOG_APP, "[LEDTest] Demo: Blinking Effects");
        
        // Different blink speeds
        ledManager->setLED("StatusLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("StatusLED", LEDEffect::Blink, 1000);  // 1 second
        
        ledManager->setLED("ActivityLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("ActivityLED", LEDEffect::Blink, 500);   // 0.5 seconds
        
        ledManager->setLED("ErrorLED", LEDColor::Red(), 255);
        ledManager->setLEDEffect("ErrorLED", LEDEffect::Blink, 250);     // 0.25 seconds
        
        // RGB LEDs blinking in different colors
        ledManager->setLED("MainRGB", LEDColor::Magenta(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Blink, 750);
        
        ledManager->setLED("SecondaryRGB", LEDColor::Yellow(), 255);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Blink, 1500);
    }
    
    void demoFadeEffects() {
        DLOG_I(LOG_APP, "[LEDTest] Demo: Fade Effects");
        
        // Smooth fading at different speeds
        ledManager->setLED("StatusLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("StatusLED", LEDEffect::Fade, 2000);
        
        ledManager->setLED("ActivityLED", LEDColor::White(), 200);
        ledManager->setLEDEffect("ActivityLED", LEDEffect::Fade, 1500);
        
        ledManager->setLED("ErrorLED", LEDColor::Red(), 150);
        ledManager->setLEDEffect("ErrorLED", LEDEffect::Fade, 3000);
        
        // RGB fade effects
        ledManager->setLED("MainRGB", LEDColor::Blue(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Fade, 2500);
        
        ledManager->setLED("SecondaryRGB", LEDColor::Green(), 200);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Fade, 1800);
    }
    
    void demoPulseEffects() {
        DLOG_I(LOG_APP, "[LEDTest] Demo: Pulse Effects (Heartbeat)");
        
        // Heartbeat-like pulse effects
        ledManager->setLED("StatusLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("StatusLED", LEDEffect::Pulse, 2000);
        
        ledManager->setLED("ActivityLED", LEDColor::White(), 200);
        ledManager->setLEDEffect("ActivityLED", LEDEffect::Pulse, 1500);
        
        ledManager->setLED("ErrorLED", LEDColor::Red(), 255);
        ledManager->setLEDEffect("ErrorLED", LEDEffect::Pulse, 1000);
        
        // RGB pulse effects
        ledManager->setLED("MainRGB", LEDColor::White(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Pulse, 2500);
        
        ledManager->setLED("SecondaryRGB", LEDColor::Red(), 200);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Pulse, 1800);
    }
    
    void demoRainbowEffects() {
        DLOG_I(LOG_APP, "[LEDTest] Demo: Rainbow Effects (RGB only)");
        
        // Single LEDs stay white during rainbow demo
        ledManager->setLED("StatusLED", LEDColor::White(), 100);
        ledManager->setLED("ActivityLED", LEDColor::White(), 100);
        ledManager->setLED("ErrorLED", LEDColor::White(), 100);
        
        // RGB LEDs show rainbow effects at different speeds
        ledManager->setLED("MainRGB", LEDColor::White(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Rainbow, 3000);
        
        ledManager->setLED("SecondaryRGB", LEDColor::White(), 200);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Rainbow, 2000);
    }
    
    void demoBreathingEffects() {
        DLOG_I(LOG_APP, "[LEDTest] Demo: Breathing Effects");
        
        // Smooth breathing effects at different speeds
        ledManager->setLED("StatusLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("StatusLED", LEDEffect::Breathing, 4000);
        
        ledManager->setLED("ActivityLED", LEDColor::White(), 200);
        ledManager->setLEDEffect("ActivityLED", LEDEffect::Breathing, 3000);
        
        ledManager->setLED("ErrorLED", LEDColor::Red(), 255);
        ledManager->setLEDEffect("ErrorLED", LEDEffect::Breathing, 5000);
        
        // RGB breathing effects
        ledManager->setLED("MainRGB", LEDColor::Blue(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Breathing, 3500);
        
        ledManager->setLED("SecondaryRGB", LEDColor::Green(), 200);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Breathing, 2500);
    }
};

// Factory function for LED test component
inline std::unique_ptr<LEDTestComponent> createLEDTest() {
    return std::unique_ptr<LEDTestComponent>(new LEDTestComponent("LEDTest"));
}
