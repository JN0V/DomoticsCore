#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/ComponentRegistry.h>
#include <DomoticsCore/Components/IComponent.h>
#include <DomoticsCore/Utils/Timer.h>
#include <memory>
#include "CustomComponents.h"

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

std::unique_ptr<Core> core;

void setup() {
    // Create core with custom device name
    CoreConfig config;
    config.deviceName = "ComponentTestDevice";
    config.logLevel = 3; // INFO level
    
    core.reset(new Core());
    
    // Add custom components to demonstrate the system
    DLOG_I(LOG_CORE, "Adding custom components...");
    
    // Custom TestComponent instances - no dependencies
    core->addComponent(createTestComponent("ComponentA", 3000));
    
    // Component B - depends on A
    core->addComponent(createTestComponent("ComponentB", 4000, {"ComponentA"}));
    
    // Component C - depends on B (indirect dependency on A)
    core->addComponent(createTestComponent("ComponentC", 6000, {"ComponentB"}));
    
    // Add LED blinker component (hardware interaction example)
    core->addComponent(createLEDBlinker(LED_BUILTIN, 500)); // Fast blink
    
    // Uncomment to test WiFi component from library:
    // core->addComponent(std::make_unique<WiFiComponent>("YourWifiSSID", "YourPassword"));
    
    DLOG_I(LOG_CORE, "Starting core with %d components...", core->getComponentCount());
    
    if (!core->begin(config)) {
        DLOG_E(LOG_CORE, "Failed to initialize core!");
        return;
    }
    
    DLOG_I(LOG_CORE, "Setup complete - all components initialized");
    
    // Demonstrate component access
    auto* testComp = core->getComponent<TestComponent>("ComponentA");
    if (testComp) {
        testComp->logStatus();
    }
}

void loop() {
    if (core) {
        core->loop();
    }
    
    // Demonstrate runtime component interaction
    static unsigned long lastInteraction = 0;
    if (millis() - lastInteraction >= 15000) { // Every 15 seconds
        lastInteraction = millis();
        
        // Get and interact with ComponentA
        auto* compA = core->getComponent<TestComponent>("ComponentA");
        if (compA) {
            DLOG_I(LOG_CORE, "=== Component Interaction Demo ===");
            compA->logStatus();
            
            // Every 30 seconds, reset counter
            static int interactionCount = 0;
            interactionCount++;
            if (interactionCount % 2 == 0) {
                compA->resetCounter();
            }
        }
    }
}
