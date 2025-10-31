#pragma once

#include <Arduino.h>
#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Custom application log tag
#define LOG_APP "APP"

/**
 * Example custom component showing how to build new behaviors with DomoticsCore
 * This demonstrates the component development pattern for library users
 */
class TestComponent : public IComponent {
private:
    String componentName;
    Utils::NonBlockingDelay heartbeatTimer;
    Utils::NonBlockingDelay workTimer;
    int counter;
    bool simulateWork;
    std::vector<String> dependencies;
    
public:
    /**
     * Constructor
     * @param name Component instance name
     * @param heartbeatInterval Heartbeat logging interval in ms (default: 5000)
     * @param deps List of component dependencies (optional)
     */
    TestComponent(const String& name, 
                  unsigned long heartbeatInterval = 5000,
                  const std::vector<String>& deps = {}) 
        : componentName(name), 
          heartbeatTimer(heartbeatInterval), 
          workTimer(2000),
          counter(0), 
          simulateWork(true),
          dependencies(deps) {
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_APP, "TestComponent '%s' initializing...", componentName.c_str());
        
        // Initialize component metadata
        metadata.name = componentName;
        metadata.version = "1.0.0-test";
        metadata.author = "DomoticsCore Example";
        metadata.description = "Test component for demonstration";
        
        // Define configuration parameters
        config.defineParameter(ConfigParam("heartbeat_interval", ConfigType::Integer, false, 
                                         String(heartbeatTimer.getInterval()), "Heartbeat interval in ms")
                              .min(1000).max(60000));
        config.defineParameter(ConfigParam("simulate_work", ConfigType::Boolean, false, "true", 
                                         "Enable work simulation"));
        
        // Validate configuration
        auto validation = validateConfig();
        if (!validation.isValid()) {
            DLOG_E(LOG_APP, "TestComponent '%s' config validation failed: %s", 
                   componentName.c_str(), validation.toString().c_str());
            setStatus(ComponentStatus::ConfigError);
            return ComponentStatus::ConfigError;
        }
        
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
    
    String getName() const override {
        return componentName;
    }
    
    String getVersion() const override {
        return "1.0.0-test";
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
        DLOG_I(LOG_APP, "  - Dependencies: %d", dependencies.size());
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
        digitalWrite(ledPin, LOW);
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_APP, "LEDBlinker initializing on pin %d...", ledPin);
        
        // Initialize component metadata
        metadata.name = "LEDBlinker";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore Example";
        metadata.description = "LED blinker component for hardware demonstration";
        
        // Define configuration parameters
        config.defineParameter(ConfigParam("blink_interval", ConfigType::Integer, false, 
                                         String(blinkTimer.getInterval()), "LED blink interval in ms")
                              .min(100).max(10000));
        config.defineParameter(ConfigParam("enabled", ConfigType::Boolean, false, "true", 
                                         "Enable LED blinking"));
        
        pinMode(ledPin, OUTPUT);
        digitalWrite(ledPin, LOW);
        ledState = false;
        setStatus(ComponentStatus::Success);
        DLOG_I(LOG_APP, "LEDBlinker initialized successfully");
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (blinkEnabled && blinkTimer.isReady()) {
            ledState = !ledState;
            digitalWrite(ledPin, ledState ? HIGH : LOW);
            DLOG_D(LOG_APP, "LED %s", ledState ? "ON" : "OFF");
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_APP, "LEDBlinker shutting down...");
        digitalWrite(ledPin, LOW);
        blinkEnabled = false;
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    String getName() const override {
        return "LEDBlinker";
    }
    
    void setBlinkInterval(unsigned long intervalMs) {
        blinkTimer.setInterval(intervalMs);
        DLOG_I(LOG_APP, "LED blink interval set to %lu ms", intervalMs);
    }
    
    void setEnabled(bool enabled) {
        blinkEnabled = enabled;
        if (!enabled) {
            digitalWrite(ledPin, LOW);
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
    unsigned long heartbeatInterval = 5000,
    const std::vector<String>& dependencies = {}
) {
    return std::unique_ptr<TestComponent>(new TestComponent(name, heartbeatInterval, dependencies));
}

inline std::unique_ptr<LEDBlinkerComponent> createLEDBlinker(int pin, unsigned long blinkInterval = 1000) {
    return std::unique_ptr<LEDBlinkerComponent>(new LEDBlinkerComponent(pin, blinkInterval));
}
