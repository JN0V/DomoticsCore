#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Test component with optional dependencies
class ComponentWithOptionalDeps : public IComponent {
private:
    String name_;
    bool foundRequired = false;
    bool foundOptional = false;
    
public:
    ComponentWithOptionalDeps(const char* name) : name_(name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    
    std::vector<Dependency> getDependencies() const override {
        return {
            {"RequiredComponent", true},   // Required - will fail if missing
            {"OptionalComponent", false}   // Optional - OK if missing
        };
    }
    
    ComponentStatus begin() override {
        DLOG_I("TEST", "[%s] begin() called", name_.c_str());
        return ComponentStatus::Success;
    }
    
    void afterAllComponentsReady() override {
        DLOG_I("TEST", "[%s] afterAllComponentsReady() called", name_.c_str());
        
        // Check required dependency
        auto* required = getCore()->getComponent<IComponent>("RequiredComponent");
        if (required) {
            DLOG_I("TEST", "[%s] ✅ Found required dependency: RequiredComponent", name_.c_str());
            foundRequired = true;
        } else {
            DLOG_E("TEST", "[%s] ❌ Required dependency missing!", name_.c_str());
        }
        
        // Check optional dependency
        auto* optional = getCore()->getComponent<IComponent>("OptionalComponent");
        if (optional) {
            DLOG_I("TEST", "[%s] ✅ Found optional dependency: OptionalComponent", name_.c_str());
            foundOptional = true;
        } else {
            DLOG_I("TEST", "[%s] ℹ️ Optional dependency missing (OK)", name_.c_str());
        }
    }
    
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    
    bool didFindRequired() const { return foundRequired; }
    bool didFindOptional() const { return foundOptional; }
};

// Simple test component
class SimpleComponent : public IComponent {
private:
    String name_;
    
public:
    SimpleComponent(const char* name) : name_(name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    
    ComponentStatus begin() override {
        DLOG_I("TEST", "[%s] initialized", name_.c_str());
        return ComponentStatus::Success;
    }
    
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
};

Core core;
ComponentWithOptionalDeps* testComp = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I("TEST", "===========================================");
    DLOG_I("TEST", "Testing Optional Dependencies (v1.0.3)");
    DLOG_I("TEST", "===========================================\n");
    
    // Register required dependency
    DLOG_I("TEST", ">>> Registering RequiredComponent");
    core.addComponent(std::make_unique<SimpleComponent>("RequiredComponent"));
    
    // NOTE: We intentionally do NOT register OptionalComponent to test optional deps
    
    // Register test component with optional deps
    DLOG_I("TEST", ">>> Registering ComponentWithOptionalDeps");
    testComp = new ComponentWithOptionalDeps("TestComponent");
    core.addComponent(std::unique_ptr<ComponentWithOptionalDeps>(testComp));
    
    // Initialize
    CoreConfig config;
    config.deviceName = "OptionalDepsTest";
    config.logLevel = 3;
    
    DLOG_I("TEST", "\n>>> Initializing core...");
    if (!core.begin(config)) {
        DLOG_E("TEST", "❌ Core initialization FAILED!");
        return;
    }
    
    // Results
    DLOG_I("TEST", "\n===========================================");
    DLOG_I("TEST", "TEST RESULTS:");
    DLOG_I("TEST", "===========================================");
    DLOG_I("TEST", "Required dependency found: %s", 
           testComp->didFindRequired() ? "✅ YES" : "❌ NO");
    DLOG_I("TEST", "Optional dependency found: %s", 
           testComp->didFindOptional() ? "✅ YES (bonus)" : "ℹ️ NO (expected)");
    
    if (testComp->didFindRequired() && !testComp->didFindOptional()) {
        DLOG_I("TEST", "\n🎉 TEST PASSED! Optional dependencies work correctly.");
    } else {
        DLOG_E("TEST", "\n❌ TEST FAILED!");
    }
    DLOG_I("TEST", "===========================================\n");
}

void loop() {
    core.loop();
    delay(5000);
}
