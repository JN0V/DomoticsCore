#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

class LifecycleTestComponent : public IComponent {
public:
    LifecycleTestComponent(const String& name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    
    ComponentStatus begin() override { 
        beginCalled = true;
        return ComponentStatus::Success; 
    }
    void loop() override { loopCalled = true; }
    ComponentStatus shutdown() override { 
        shutdownCalled = true;
        return ComponentStatus::Success; 
    }
    std::vector<Dependency> getDependencies() const override { return deps; }
    void addDependency(const String& name) { deps.push_back(Dependency{name, true}); }
    void afterAllComponentsReady() override { afterReadyCalled = true; }
    
    bool beginCalled = false;
    bool loopCalled = false;
    bool shutdownCalled = false;
    bool afterReadyCalled = false;
private:
    std::vector<Dependency> deps;
};

std::vector<String> shutdownOrder;

class ShutdownTracker : public IComponent {
public:
    ShutdownTracker(const String& name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { 
        shutdownOrder.push_back(metadata.name);
        return ComponentStatus::Success; 
    }
    std::vector<Dependency> getDependencies() const override { return deps; }
    void addDependency(const String& name) { deps.push_back(Dependency{name, true}); }
private:
    std::vector<Dependency> deps;
};

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_begin_called_on_init() {
    Core core;
    auto comp = std::make_unique<LifecycleTestComponent>("TestComp");
    LifecycleTestComponent* ptr = comp.get();
    core.addComponent(std::move(comp));
    printResult("begin() not called before init", !ptr->beginCalled);
    core.begin();
    printResult("begin() called after init", ptr->beginCalled);
}

void test_loop_called() {
    Core core;
    auto comp = std::make_unique<LifecycleTestComponent>("TestComp");
    LifecycleTestComponent* ptr = comp.get();
    core.addComponent(std::move(comp));
    core.begin();
    printResult("loop() not called before core.loop()", !ptr->loopCalled);
    core.loop();
    printResult("loop() called after core.loop()", ptr->loopCalled);
}

void test_shutdown_called() {
    Core core;
    auto comp = std::make_unique<LifecycleTestComponent>("TestComp");
    LifecycleTestComponent* ptr = comp.get();
    core.addComponent(std::move(comp));
    core.begin();
    printResult("shutdown() not called before core.shutdown()", !ptr->shutdownCalled);
    core.shutdown();
    printResult("shutdown() called after core.shutdown()", ptr->shutdownCalled);
}

void test_shutdown_reverse_order() {
    shutdownOrder.clear();
    Core core;
    auto compC = std::make_unique<ShutdownTracker>("C");
    compC->addDependency("B");
    auto compB = std::make_unique<ShutdownTracker>("B");
    compB->addDependency("A");
    auto compA = std::make_unique<ShutdownTracker>("A");
    core.addComponent(std::move(compC));
    core.addComponent(std::move(compB));
    core.addComponent(std::move(compA));
    core.begin();
    core.shutdown();
    bool correctOrder = shutdownOrder.size() == 3 &&
                        shutdownOrder[0] == "C" &&
                        shutdownOrder[1] == "B" &&
                        shutdownOrder[2] == "A";
    printResult("Shutdown in reverse dependency order", correctOrder);
}

void test_after_all_components_ready() {
    Core core;
    auto comp = std::make_unique<LifecycleTestComponent>("TestComp");
    LifecycleTestComponent* ptr = comp.get();
    core.addComponent(std::move(comp));
    printResult("afterAllComponentsReady() not called before init", !ptr->afterReadyCalled);
    core.begin();
    printResult("afterAllComponentsReady() called after init", ptr->afterReadyCalled);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore Lifecycle Events Tests");
    Serial.println("========================================\n");
    
    test_begin_called_on_init();
    test_loop_called();
    test_shutdown_called();
    test_shutdown_reverse_order();
    test_after_all_components_ready();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
