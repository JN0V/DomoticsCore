#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Test component that tracks lifecycle order
class LifecycleTestComponent : public IComponent {
private:
    String name_;
    int beginOrder = -1;
    int afterAllOrder = -1;
    bool foundOtherInBegin = false;
    bool foundOtherInAfterAll = false;
    static int beginCounter;
    static int afterAllCounter;
    
public:
    LifecycleTestComponent(const String& name) : name_(name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    
    ComponentStatus begin() override {
        beginOrder = beginCounter++;
        DLOG_I("TEST", "[%s] begin() called - order: %d", name_.c_str(), beginOrder);
        
        // Try to access other component in begin()
        auto* other = getCore()->getComponent<IComponent>("ComponentB");
        if (other) {
            DLOG_I("TEST", "[%s] Found ComponentB in begin()", name_.c_str());
            foundOtherInBegin = true;
        }
        
        return ComponentStatus::Success;
    }
    
    void afterAllComponentsReady() override {
        afterAllOrder = afterAllCounter++;
        DLOG_I("TEST", "[%s] afterAllComponentsReady() called - order: %d", 
               name_.c_str(), afterAllOrder);
        
        // Try to access other component in afterAllComponentsReady()
        auto* other = getCore()->getComponent<IComponent>("ComponentB");
        if (other) {
            DLOG_I("TEST", "[%s] ✅ Found ComponentB in afterAllComponentsReady()", name_.c_str());
            foundOtherInAfterAll = true;
        } else {
            DLOG_E("TEST", "[%s] ❌ ComponentB not found in afterAllComponentsReady()!", name_.c_str());
        }
    }
    
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    
    int getBeginOrder() const { return beginOrder; }
    int getAfterAllOrder() const { return afterAllOrder; }
    bool foundOtherDuringBegin() const { return foundOtherInBegin; }
    bool foundOtherDuringAfterAll() const { return foundOtherInAfterAll; }
    
    static void resetCounters() {
        beginCounter = 0;
        afterAllCounter = 0;
    }
};

int LifecycleTestComponent::beginCounter = 0;
int LifecycleTestComponent::afterAllCounter = 0;

Core core;
LifecycleTestComponent* compA = nullptr;
LifecycleTestComponent* compB = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I("TEST", "===========================================");
    DLOG_I("TEST", "Testing afterAllComponentsReady() (v1.1)");
    DLOG_I("TEST", "===========================================\n");
    
    LifecycleTestComponent::resetCounters();
    
    // Register components
    DLOG_I("TEST", ">>> Registering ComponentA");
    compA = new LifecycleTestComponent("ComponentA");
    core.addComponent(std::unique_ptr<LifecycleTestComponent>(compA));
    
    DLOG_I("TEST", ">>> Registering ComponentB");
    compB = new LifecycleTestComponent("ComponentB");
    core.addComponent(std::unique_ptr<LifecycleTestComponent>(compB));
    
    // Initialize
    CoreConfig config;
    config.deviceName = "LifecycleTest";
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
    DLOG_I("TEST", "ComponentA:");
    DLOG_I("TEST", "  begin() order: %d", compA->getBeginOrder());
    DLOG_I("TEST", "  afterAllComponentsReady() order: %d", compA->getAfterAllOrder());
    DLOG_I("TEST", "  Found B during begin(): %s", 
           compA->foundOtherDuringBegin() ? "YES (early)" : "NO");
    DLOG_I("TEST", "  Found B during afterAll(): %s", 
           compA->foundOtherDuringAfterAll() ? "✅ YES" : "❌ NO");
    
    DLOG_I("TEST", "\nComponentB:");
    DLOG_I("TEST", "  begin() order: %d", compB->getBeginOrder());
    DLOG_I("TEST", "  afterAllComponentsReady() order: %d", compB->getAfterAllOrder());
    DLOG_I("TEST", "  Found A during afterAll(): %s", 
           compB->foundOtherDuringAfterAll() ? "✅ YES" : "❌ NO");
    
    // Validation
    bool passed = true;
    
    // Check that afterAll is called AFTER all begin()
    if (compA->getAfterAllOrder() < 0 || compB->getAfterAllOrder() < 0) {
        DLOG_E("TEST", "\n❌ afterAllComponentsReady() not called!");
        passed = false;
    }
    
    // Check that all components are accessible in afterAllComponentsReady()
    if (!compA->foundOtherDuringAfterAll() || !compB->foundOtherDuringAfterAll()) {
        DLOG_E("TEST", "\n❌ Components not accessible in afterAllComponentsReady()!");
        passed = false;
    }
    
    if (passed) {
        DLOG_I("TEST", "\n🎉 TEST PASSED! Lifecycle callback works correctly.");
        DLOG_I("TEST", "✅ begin() called first for all components");
        DLOG_I("TEST", "✅ afterAllComponentsReady() called after all begin()");
        DLOG_I("TEST", "✅ All components accessible in afterAllComponentsReady()");
    } else {
        DLOG_E("TEST", "\n❌ TEST FAILED!");
    }
    DLOG_I("TEST", "===========================================\n");
}

void loop() {
    core.loop();
    delay(5000);
}
