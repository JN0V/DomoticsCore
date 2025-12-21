#include <DomoticsCore/Core.h>
#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/ComponentRegistry.h>
#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Timer.h>
#include "CustomComponents.h"

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;

Core core;

void setup() {
    // Initialize early for logging before core initialization
    HAL::initializeLogging(115200);
    
    // ============================================================================
    // EXAMPLE 02: Core with Custom Components
    // ============================================================================
    // This example demonstrates custom component development:
    // - ComponentA: No dependencies, 3-second heartbeat
    // - ComponentB: Depends on ComponentA, 4-second heartbeat  
    // - ComponentC: Depends on ComponentB, 6-second heartbeat
    // - LEDBlinker: Blinks built-in LED every 500ms
    // Expected: Components initialize in dependency order, regular heartbeat logs
    // ============================================================================
    
    DLOG_I(LOG_APP, "=== Core with Custom Components Example ===");
    DLOG_I(LOG_APP, "ComponentA: 3s heartbeat (no dependencies)");
    DLOG_I(LOG_APP, "ComponentB: 4s heartbeat (depends on A)");
    DLOG_I(LOG_APP, "ComponentC: 6s heartbeat (depends on B)");
    DLOG_I(LOG_APP, "LEDBlinker: 500ms LED blink");
    DLOG_I(LOG_APP, "==========================================");

    DLOG_I(LOG_APP, "Adding test components...");
    
    // Create components with different configurations
    TestComponentConfig cfgA;
    cfgA.heartbeatInterval = 3000;  // 3 seconds
    cfgA.workInterval = 1000;       // 1 second work cycle
    cfgA.enableWork = true;
    
    TestComponentConfig cfgB;
    cfgB.heartbeatInterval = 4000;  // 4 seconds
    cfgB.workInterval = 1500;       // 1.5 second work cycle
    cfgB.enableWork = true;
    
    TestComponentConfig cfgC;
    cfgC.heartbeatInterval = 6000;  // 6 seconds
    cfgC.workInterval = 2000;       // 2 second work cycle
    cfgC.enableWork = true;

    // Add components with dependencies
    core.addComponent(createTestComponent("ComponentA", cfgA, {}));
    core.addComponent(createTestComponent("ComponentB", cfgB, {"ComponentA"}));
    core.addComponent(createTestComponent("ComponentC", cfgC, {"ComponentB"}));
    
    // Add LED blinker component
    core.addComponent(createLEDBlinker(LED_BUILTIN, 500));

    DLOG_I(LOG_APP, "Starting core with %d components...", 4);
    
    CoreConfig coreCfg;
    coreCfg.deviceName = "ComponentTestDevice";
    coreCfg.logLevel = 3;
    
    if (!core.begin(coreCfg)) {
        DLOG_E(LOG_APP, "Core initialization failed!");
        return;
    }
    
    DLOG_I(LOG_APP, "Setup complete - all components initialized");
    
    // Demonstrate component access
    auto* testComp = core.getComponent<TestComponent>("ComponentA");
    if (testComp) {
        testComp->logStatus();
    }
}

void loop() {
    core.loop();
    
    // Demonstrate runtime component interaction
    static unsigned long lastInteraction = 0;
    static bool removedC = false;
    if (millis() - lastInteraction >= 15000) { // Every 15 seconds
        lastInteraction = millis();
        
        // Get and interact with ComponentA
        auto* compA = core.getComponent<TestComponent>("ComponentA");
        if (compA) {
            DLOG_I(LOG_APP, "=== Component Interaction Demo ===");
            compA->logStatus();
            
            // Every 30 seconds, reset counter
            static int interactionCount = 0;
            interactionCount++;
            if (interactionCount % 2 == 0) {
                compA->resetCounter();
            }
        }

        // After ~30 seconds, remove ComponentC to demonstrate runtime removal
        if (!removedC && millis() > 30000) {
            DLOG_I(LOG_APP, "Attempting to remove ComponentC at runtime...");
            bool ok = core.removeComponent("ComponentC");
            DLOG_I(LOG_APP, "%s", ok ? "ComponentC removed successfully" : "ComponentC remove failed");
            removedC = true;
        }
    }
}
