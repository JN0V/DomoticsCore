#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Simulate "early init" Storage component
class MockStorageComponent : public IComponent {
private:
    String name_ = "Storage";
    
public:
    MockStorageComponent() {
        metadata.name = name_;
    }

    ComponentStatus begin() override {
        DLOG_I("TEST", "[Storage] Initialized");
        return ComponentStatus::Success;
    }
    
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
};

// Custom component that declares optional dependency on Storage
class CustomComponent : public IComponent {
private:
    String name_;
    bool crashDetected = false;
    
public:
    CustomComponent(const String& name) : name_(name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    
    // This is what causes the bug according to report
    std::vector<Dependency> getDependencies() const override {
        return {
            {"Storage", false}  // Optional dependency on early-init component
        };
    }
    
    ComponentStatus begin() override {
        DLOG_I("TEST", "[%s] begin() called", name_.c_str());
        return ComponentStatus::Success;
    }
    
    void afterAllComponentsReady() override {
        DLOG_I("TEST", "[%s] afterAllComponentsReady() called", name_.c_str());
        
        // Try to access Storage
        auto* storage = getCore()->getComponent<MockStorageComponent>("Storage");
        if (storage) {
            DLOG_I("TEST", "[%s] ✅ Storage accessible", name_.c_str());
        } else {
            DLOG_E("TEST", "[%s] ❌ Storage NOT accessible!", name_.c_str());
        }
    }
    
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
};

Core core;
MockStorageComponent* storage = nullptr;
CustomComponent* customComp = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I("TEST", "===========================================");
    DLOG_I("TEST", "Reproducing Bug #2: Early-Init + Optional Deps");
    DLOG_I("TEST", "===========================================\n");
    
    // Scenario from bug report:
    // 1. Custom component added BEFORE begin() with optional dep on Storage
    DLOG_I("TEST", ">>> Step 1: Add custom component with optional dep on Storage");
    customComp = new CustomComponent("WaterMeter");
    core.addComponent(std::unique_ptr<CustomComponent>(customComp));
    
    // 2. Early-init Storage (simulating System behavior)
    DLOG_I("TEST", ">>> Step 2: Early-init Storage component");
    auto storagePtr = std::make_unique<MockStorageComponent>();
    storage = storagePtr.get();
    core.addComponent(std::move(storagePtr));
    
    // Initialize storage early (like System does)
    DLOG_I("TEST", ">>> Step 3: Manually initialize Storage (early-init)");
    if (storage->begin() == ComponentStatus::Success) {
        storage->setActive(true);
        DLOG_I("TEST", "[STORAGE] Storage component initialized (early) ✓");
    }
    
    // 3. Call begin() - this should trigger the bug
    DLOG_I("TEST", "\n>>> Step 4: Calling core.begin()...");
    CoreConfig config;
    config.deviceName = "Bug2Test";
    config.logLevel = 3;
    
    bool success = core.begin(config);
    
    if (!success) {
        DLOG_E("TEST", "\n❌ CRASH/FAILURE DETECTED!");
        DLOG_E("TEST", "Bug #2 reproduced successfully");
    } else {
        DLOG_I("TEST", "\n✅ No crash! Bug #2 might be fixed or scenario different");
    }
    
    DLOG_I("TEST", "===========================================\n");
}

void loop() {
    core.loop();
    delay(5000);
}
