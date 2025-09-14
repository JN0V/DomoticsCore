#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/LED.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core* core = nullptr;

// LED Test Component - demonstrates comprehensive LED functionality
class LEDDemoComponent : public IComponent {
private:
    std::unique_ptr<LEDComponent> ledManager;
    Utils::NonBlockingDelay demoTimer;
    int currentDemo = 0;
    int maxDemos = 6;
    
public:
    LEDDemoComponent() 
        : demoTimer(5000) {  // Change demo every 5 seconds
        
        metadata.name = "LEDDemo";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "LED component demonstration with various effects";
        metadata.category = "Demo";
        metadata.tags = {"led", "demo", "effects", "hardware"};
    }
    
    String getName() const override {
        return metadata.name;
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_CORE, "[LEDDemo] Initializing LED demonstration component...");
        
        // Create LED manager
        ledManager.reset(new LEDComponent());
        
        // Configure LEDs for testing
        // Built-in LED (usually pin 2 on ESP32)
        ledManager->addSingleLED(2, "BuiltinLED", 255, false);
        
        // Additional single LEDs for demonstration
        ledManager->addSingleLED(4, "StatusLED", 255, false);
        ledManager->addSingleLED(16, "ActivityLED", 255, false);
        ledManager->addSingleLED(17, "ErrorLED", 255, false);
        
        // RGB LED demonstration (common cathode)
        ledManager->addRGBLED(18, 19, 21, "MainRGB", 255, false);
        
        // RGB LED with inverted logic (common anode)
        ledManager->addRGBLED(22, 23, 25, "SecondaryRGB", 255, true);
        
        // Initialize LED manager
        ComponentStatus status = ledManager->begin();
        if (status != ComponentStatus::Success) {
            DLOG_E(LOG_CORE, "[LEDDemo] Failed to initialize LED manager: %s", 
                         statusToString(status));
            setStatus(status);
            return status;
        }
        
        DLOG_I(LOG_CORE, "[LEDDemo] Initialized with %d LEDs", ledManager->getLEDCount());
        
        // List all configured LEDs
        auto ledNames = ledManager->getLEDNames();
        for (const auto& name : ledNames) {
            DLOG_I(LOG_CORE, "[LEDDemo] - LED: %s", name.c_str());
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
        DLOG_I(LOG_CORE, "[LEDDemo] Shutting down LED demonstration component...");
        
        if (ledManager) {
            ledManager->shutdown();
        }
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
private:
    void startDemo(int demoIndex) {
        if (!ledManager) return;
        
        DLOG_I(LOG_CORE, "[LEDDemo] Starting demo %d/%d", demoIndex + 1, maxDemos);
        
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
        DLOG_I(LOG_CORE, "[LEDDemo] Demo: Solid Colors");
        
        // Set single LEDs to different brightness levels
        ledManager->setLED("BuiltinLED", LEDColor::White(), 255);
        ledManager->setLED("StatusLED", LEDColor::Green(), 128);
        ledManager->setLED("ActivityLED", LEDColor::Blue(), 200);
        ledManager->setLED("ErrorLED", LEDColor::Red(), 64);
        
        // Set RGB LEDs to different colors
        ledManager->setLED("MainRGB", LEDColor::Yellow(), 200);
        ledManager->setLED("SecondaryRGB", LEDColor::Cyan(), 150);
    }
    
    void demoBlinkingEffects() {
        DLOG_I(LOG_CORE, "[LEDDemo] Demo: Blinking Effects");
        
        // Different blink speeds
        ledManager->setLED("BuiltinLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("BuiltinLED", LEDEffect::Blink, 1000);  // 1 second
        
        ledManager->setLED("StatusLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("StatusLED", LEDEffect::Blink, 500);   // 0.5 seconds
        
        ledManager->setLED("ActivityLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("ActivityLED", LEDEffect::Blink, 750);   // 0.75 seconds
        
        ledManager->setLED("ErrorLED", LEDColor::Red(), 255);
        ledManager->setLEDEffect("ErrorLED", LEDEffect::Blink, 250);     // 0.25 seconds
        
        // RGB LEDs blinking in different colors
        ledManager->setLED("MainRGB", LEDColor::Magenta(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Blink, 800);
        
        ledManager->setLED("SecondaryRGB", LEDColor::Yellow(), 255);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Blink, 1200);
    }
    
    void demoFadeEffects() {
        DLOG_I(LOG_CORE, "[LEDDemo] Demo: Fade Effects");
        
        // Smooth fading at different speeds
        ledManager->setLED("BuiltinLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("BuiltinLED", LEDEffect::Fade, 2000);
        
        ledManager->setLED("StatusLED", LEDColor::White(), 200);
        ledManager->setLEDEffect("StatusLED", LEDEffect::Fade, 1500);
        
        ledManager->setLED("ActivityLED", LEDColor::White(), 180);
        ledManager->setLEDEffect("ActivityLED", LEDEffect::Fade, 2500);
        
        ledManager->setLED("ErrorLED", LEDColor::Red(), 150);
        ledManager->setLEDEffect("ErrorLED", LEDEffect::Fade, 3000);
        
        // RGB fade effects
        ledManager->setLED("MainRGB", LEDColor::Blue(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Fade, 2200);
        
        ledManager->setLED("SecondaryRGB", LEDColor::Green(), 200);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Fade, 1800);
    }
    
    void demoPulseEffects() {
        DLOG_I(LOG_CORE, "[LEDDemo] Demo: Pulse Effects (Heartbeat)");
        
        // Heartbeat-like pulse effects
        ledManager->setLED("BuiltinLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("BuiltinLED", LEDEffect::Pulse, 2000);
        
        ledManager->setLED("StatusLED", LEDColor::White(), 200);
        ledManager->setLEDEffect("StatusLED", LEDEffect::Pulse, 1500);
        
        ledManager->setLED("ActivityLED", LEDColor::White(), 180);
        ledManager->setLEDEffect("ActivityLED", LEDEffect::Pulse, 2500);
        
        ledManager->setLED("ErrorLED", LEDColor::Red(), 255);
        ledManager->setLEDEffect("ErrorLED", LEDEffect::Pulse, 1000);
        
        // RGB pulse effects
        ledManager->setLED("MainRGB", LEDColor::White(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Pulse, 2200);
        
        ledManager->setLED("SecondaryRGB", LEDColor::Red(), 200);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Pulse, 1800);
    }
    
    void demoRainbowEffects() {
        DLOG_I(LOG_CORE, "[LEDDemo] Demo: Rainbow Effects (RGB LEDs only)");
        
        // Single LEDs stay white during rainbow demo
        ledManager->setLED("BuiltinLED", LEDColor::White(), 100);
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
        DLOG_I(LOG_CORE, "[LEDDemo] Demo: Breathing Effects");
        
        // Smooth breathing effects at different speeds
        ledManager->setLED("BuiltinLED", LEDColor::White(), 255);
        ledManager->setLEDEffect("BuiltinLED", LEDEffect::Breathing, 4000);
        
        ledManager->setLED("StatusLED", LEDColor::White(), 200);
        ledManager->setLEDEffect("StatusLED", LEDEffect::Breathing, 3000);
        
        ledManager->setLED("ActivityLED", LEDColor::White(), 180);
        ledManager->setLEDEffect("ActivityLED", LEDEffect::Breathing, 3500);
        
        ledManager->setLED("ErrorLED", LEDColor::Red(), 255);
        ledManager->setLEDEffect("ErrorLED", LEDEffect::Breathing, 5000);
        
        // RGB breathing effects
        ledManager->setLED("MainRGB", LEDColor::Blue(), 255);
        ledManager->setLEDEffect("MainRGB", LEDEffect::Breathing, 3200);
        
        ledManager->setLED("SecondaryRGB", LEDColor::Green(), 200);
        ledManager->setLEDEffect("SecondaryRGB", LEDEffect::Breathing, 2800);
    }
};

void setup() {
    // Serial initialization handled by Core
    
    // Create core with custom device name
    CoreConfig config;
    config.deviceName = "LEDDemoDevice";
    config.logLevel = 3; // INFO level
    
    core = new Core();
    
    // Add LED demonstration component
    DLOG_I(LOG_CORE, "Adding LED demonstration component...");
    core->addComponent(std::unique_ptr<LEDDemoComponent>(new LEDDemoComponent()));
    
    DLOG_I(LOG_CORE, "Starting core with %d components...", core->getComponentCount());
    
    if (!core->begin(config)) {
        DLOG_E(LOG_CORE, "Failed to initialize core!");
        return;
    }
    
    DLOG_I(LOG_CORE, "=== DomoticsCore LED Demo Ready ===");
    DLOG_I(LOG_CORE, "Expected LED connections:");
    DLOG_I(LOG_CORE, "- Pin 2:  Built-in LED");
    DLOG_I(LOG_CORE, "- Pin 4:  Status LED");
    DLOG_I(LOG_CORE, "- Pin 16: Activity LED");
    DLOG_I(LOG_CORE, "- Pin 17: Error LED");
    DLOG_I(LOG_CORE, "- Pins 18,19,21: RGB LED (common cathode)");
    DLOG_I(LOG_CORE, "- Pins 22,23,25: RGB LED (common anode)");
    DLOG_I(LOG_CORE, "Demo cycles every 5 seconds through 6 effects");
}

void loop() {
    if (core) {
        core->loop();
    }
    
    // Status reporting using non-blocking delay
    static Utils::NonBlockingDelay statusTimer(30000); // 30 seconds
    if (statusTimer.isReady()) {
        DLOG_I(LOG_SYSTEM, "=== LED Demo Status ===");
        DLOG_I(LOG_SYSTEM, "Uptime: %lu seconds", millis() / 1000);
        DLOG_I(LOG_SYSTEM, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_SYSTEM, "LED effects running...");
    }
}
