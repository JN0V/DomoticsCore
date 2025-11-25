#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Mock Storage
class MockStorageComponent : public IComponent {
public:
    MockStorageComponent() {
        metadata.name = "Storage";
        metadata.version = "1.0.0";
    }

    ComponentStatus begin() override {
        DLOG_I("TEST", "[Storage] begin() called");
        return ComponentStatus::Success;
    }
    
    String getString(const String& key, const String& defaultValue) {
        DLOG_I("TEST", "[Storage] getString('%s')", key.c_str());
        return defaultValue;  // Return default for simulation
    }
    
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
};

// Custom component with optional dependency (like WaterMeter)
class WaterMeterComponent : public IComponent {
private:
    MockStorageComponent* storage_ = nullptr;
    
public:
    WaterMeterComponent() {
        metadata.name = "WaterMeter";
        metadata.version = "1.0.0";
    }

    // Bug report says this causes crash
    std::vector<Dependency> getDependencies() const override {
        return {
            {"Storage", false}  // Optional dependency on Storage
        };
    }
    
    ComponentStatus begin() override {
        DLOG_I("TEST", "[WaterMeter] begin() called");
        return ComponentStatus::Success;
    }
    
    void afterAllComponentsReady() override {
        DLOG_I("TEST", "[WaterMeter] afterAllComponentsReady() called");
        
        // Try to access Storage
        storage_ = getCore()->getComponent<MockStorageComponent>("Storage");
        if (storage_) {
            DLOG_I("TEST", "[WaterMeter] ✅ Storage accessible in afterAllComponentsReady()");
            // Load data from storage
            String data = storage_->getString("pulse_count", "0");
            DLOG_I("TEST", "[WaterMeter] Loaded data: %s", data.c_str());
        } else {
            DLOG_W("TEST", "[WaterMeter] ⚠️ Storage not available (using defaults)");
        }
    }
    
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
};

Core core;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I("TEST", "===========================================");
    DLOG_I("TEST", "Reproducing EXACT System.begin() scenario");
    DLOG_I("TEST", "===========================================\n");
    
    // EXACT scenario from System.begin():
    
    // User adds custom component BEFORE System::begin()
    DLOG_I("TEST", ">>> USER CODE: Add WaterMeter component");
    auto waterMeter = std::make_unique<WaterMeterComponent>();
    core.addComponent(std::move(waterMeter));
    DLOG_I("TEST", "WaterMeter registered with optional dep on Storage\n");
    
    // Now simulate System::begin() behavior
    DLOG_I("TEST", ">>> SYSTEM: Early-init Storage component");
    auto storagePtr = std::make_unique<MockStorageComponent>();
    auto* storage = storagePtr.get();
    core.addComponent(std::move(storagePtr));
    
    // Early-init Storage (System does this)
    if (storage->begin() == ComponentStatus::Success) {
        storage->setActive(true);
        DLOG_I("TEST", "[STORAGE] Storage component initialized (early) ✓\n");
    }
    
    // Now call core.begin() - this is where dependency resolution happens
    DLOG_I("TEST", ">>> SYSTEM: Calling core.begin()...");
    DLOG_I("TEST", "This is where Bug #2 supposedly crashes\n");
    
    CoreConfig config;
    config.deviceName = "SystemScenarioTest";
    config.logLevel = 3;
    
    bool success = core.begin(config);
    
    DLOG_I("TEST", "\n===========================================");
    if (!success) {
        DLOG_E("TEST", "❌ CRASH/FAILURE! Bug #2 reproduced");
    } else {
        DLOG_I("TEST", "✅ SUCCESS! No crash detected");
        DLOG_I("TEST", "Bug #2 does NOT occur with current code");
    }
    DLOG_I("TEST", "===========================================\n");
}

void loop() {
    core.loop();
    delay(5000);
}
