#pragma once

#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>
#include <DomoticsCore/Platform_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Custom application log tag
#define LOG_APP "APP"

/**
 * Example configuration struct for TestComponent
 */
struct TestComponentConfig {
    unsigned long heartbeatInterval = 5000;  // Heartbeat interval in ms
    unsigned long workInterval = 2000;       // Work iteration interval in ms
    bool enableWork = true;                  // Enable work simulation
    int maxIterations = 0;                   // Max iterations (0 = unlimited)
};

/**
 * Example custom component showing how to build new behaviors with DomoticsCore
 * This demonstrates the component development pattern for library users
 */
class TestComponent : public IComponent {
private:
    String componentName;
    TestComponentConfig config;
    Utils::NonBlockingDelay heartbeatTimer;
    Utils::NonBlockingDelay workTimer;
    int counter;
    bool simulateWork;
    std::vector<String> dependencies;
    
public:
    /**
     * Constructor
     * @param name Component instance name
     * @param cfg Component configuration
     * @param deps List of component dependencies (optional)
     */
    TestComponent(const String& name, 
                  const TestComponentConfig& cfg = TestComponentConfig(),
                  const std::vector<String>& deps = {}) 
        : componentName(name),
          config(cfg),
          heartbeatTimer(cfg.heartbeatInterval), 
          workTimer(cfg.workInterval),
          counter(0), 
          simulateWork(cfg.enableWork),
          dependencies(deps) {
        // Initialize component metadata in constructor for dependency resolution
        metadata.name = componentName;
        metadata.version = "1.0.0-test";
        metadata.author = "DomoticsCore Example";
        metadata.description = "Test component for demonstration";
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_APP, "TestComponent '%s' initializing...", componentName.c_str());
        
        // Simulate some initialization work
        delay(50);
        
        counter = 0;
        setStatus(ComponentStatus::Success);
        DLOG_I(LOG_APP, "TestComponent '%s' initialized successfully", componentName.c_str());
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // Heartbeat logging
        if (heartbeatTimer.isReady()) {
            DLOG_I(LOG_APP, "TestComponent '%s' heartbeat - counter: %d, uptime: %lu ms", 
                   componentName.c_str(), counter, millis());
        }
        
        // Simulate periodic work
        if (simulateWork && workTimer.isReady()) {
            counter++;
            DLOG_D(LOG_APP, "TestComponent '%s' doing work iteration %d", 
                   componentName.c_str(), counter);
            
            // Simulate different work patterns
            if (counter % 10 == 0) {
                DLOG_I(LOG_APP, "TestComponent '%s' milestone reached: %d iterations", 
                       componentName.c_str(), counter);
            }
            
            if (counter % 25 == 0) {
                DLOG_W(LOG_APP, "TestComponent '%s' warning: high iteration count (%d)", 
                       componentName.c_str(), counter);
            }
            
            // Check max iterations limit
            if (config.maxIterations > 0 && counter >= config.maxIterations) {
                simulateWork = false;
                DLOG_I(LOG_APP, "TestComponent '%s' reached max iterations (%d)", 
                       componentName.c_str(), config.maxIterations);
            }
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_APP, "TestComponent '%s' shutting down...", componentName.c_str());
        simulateWork = false;
        setStatus(ComponentStatus::Success);
        DLOG_I(LOG_APP, "TestComponent '%s' shutdown complete - final counter: %d", 
               componentName.c_str(), counter);
        return ComponentStatus::Success;
    }
    
    
    std::vector<Dependency> getDependencies() const override {
        // Convert String vector to Dependency vector (implicit conversion per element)
        std::vector<Dependency> deps;
        for (const auto& dep : dependencies) {
            deps.push_back(dep);  // Implicit conversion String -> Dependency
        }
        return deps;
    }
    
    // Test-specific methods
    int getCounter() const {
        return counter;
    }
    
    void resetCounter() {
        counter = 0;
        DLOG_I(LOG_APP, "TestComponent '%s' counter reset", componentName.c_str());
    }
    
    void setWorkEnabled(bool enabled) {
        simulateWork = enabled;
        DLOG_I(LOG_APP, "TestComponent '%s' work %s", 
               componentName.c_str(), enabled ? "enabled" : "disabled");
    }
    
    void setHeartbeatInterval(unsigned long intervalMs) {
        heartbeatTimer.setInterval(intervalMs);
        DLOG_I(LOG_APP, "TestComponent '%s' heartbeat interval set to %lu ms", 
               componentName.c_str(), intervalMs);
    }
    
    void setWorkInterval(unsigned long intervalMs) {
        workTimer.setInterval(intervalMs);
        DLOG_I(LOG_APP, "TestComponent '%s' work interval set to %lu ms", 
               componentName.c_str(), intervalMs);
    }
    
    void triggerError() {
        DLOG_E(LOG_APP, "TestComponent '%s' simulated error triggered!", componentName.c_str());
    }
    
    void logStatus() {
        DLOG_I(LOG_APP, "TestComponent '%s' status:", componentName.c_str());
        DLOG_I(LOG_APP, "  - Counter: %d", counter);
        DLOG_I(LOG_APP, "  - Work enabled: %s", simulateWork ? "yes" : "no");
        DLOG_I(LOG_APP, "  - Heartbeat interval: %lu ms", heartbeatTimer.getInterval());
        DLOG_I(LOG_APP, "  - Work interval: %lu ms", workTimer.getInterval());
        DLOG_I(LOG_APP, "  - Dependencies: %zu", dependencies.size());
        for (const auto& dep : dependencies) {
            DLOG_I(LOG_APP, "    - %s", dep.c_str());
        }
    }
};

/**
 * Example LED blinker component showing hardware interaction
 */
class LEDBlinkerComponent : public IComponent {
private:
    int ledPin;
    Utils::NonBlockingDelay blinkTimer;
    bool ledState;
    bool blinkEnabled;
    
public:
    LEDBlinkerComponent(int pin, unsigned long blinkInterval = 1000) 
        : ledPin(pin), blinkTimer(blinkInterval), ledState(false), blinkEnabled(true) {
        pinMode(ledPin, OUTPUT);
        digitalWrite(ledPin, HAL::ledBuiltinOff());  // Turn LED off initially
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_APP, "LEDBlinker initializing on pin %d...", ledPin);
        
        // Initialize component metadata
        metadata.name = "LEDBlinker";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore Example";
        metadata.description = "LED blinker component for hardware demonstration";
        
        pinMode(ledPin, OUTPUT);
        digitalWrite(ledPin, HAL::ledBuiltinOff());  // Turn LED off initially
        ledState = false;
        setStatus(ComponentStatus::Success);
        DLOG_I(LOG_APP, "LEDBlinker initialized successfully");
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (blinkEnabled && blinkTimer.isReady()) {
            ledState = !ledState;
            digitalWrite(ledPin, ledState ? HAL::ledBuiltinOn() : HAL::ledBuiltinOff());
            DLOG_D(LOG_APP, "LED %s", ledState ? "ON" : "OFF");
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_APP, "LEDBlinker shutting down...");
        digitalWrite(ledPin, HAL::ledBuiltinOff());
        blinkEnabled = false;
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    void setBlinkInterval(unsigned long intervalMs) {
        blinkTimer.setInterval(intervalMs);
        DLOG_I(LOG_APP, "LED blink interval set to %lu ms", intervalMs);
    }
    
    void setEnabled(bool enabled) {
        blinkEnabled = enabled;
        if (!enabled) {
            digitalWrite(ledPin, HAL::ledBuiltinOff());
            ledState = false;
        }
        DLOG_I(LOG_APP, "LED blinking %s", enabled ? "enabled" : "disabled");
    }
};

/**
 * Factory functions for easy component creation
 */
inline std::unique_ptr<TestComponent> createTestComponent(
    const String& name, 
    const TestComponentConfig& config = TestComponentConfig(),
    const std::vector<String>& dependencies = {}
) {
    return std::unique_ptr<TestComponent>(new TestComponent(name, config, dependencies));
}

// Convenience overload for backward compatibility
inline std::unique_ptr<TestComponent> createTestComponent(
    const String& name, 
    unsigned long heartbeatInterval,
    const std::vector<String>& dependencies = {}
) {
    TestComponentConfig cfg;
    cfg.heartbeatInterval = heartbeatInterval;
    return std::unique_ptr<TestComponent>(new TestComponent(name, cfg, dependencies));
}

inline std::unique_ptr<LEDBlinkerComponent> createLEDBlinker(int pin, unsigned long blinkInterval = 1000) {
    return std::unique_ptr<LEDBlinkerComponent>(new LEDBlinkerComponent(pin, blinkInterval));
}
