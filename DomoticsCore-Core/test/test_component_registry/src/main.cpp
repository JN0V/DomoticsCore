#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

class ComponentA : public IComponent {
public:
    ComponentA() {
        metadata.name = "ComponentA";
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { 
        beginCalled = true;
        return ComponentStatus::Success; 
    }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { return {}; }
    
    bool beginCalled = false;
};

class ComponentB : public IComponent {
public:
    ComponentB() {
        metadata.name = "ComponentB";
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { 
        beginCalled = true;
        return ComponentStatus::Success; 
    }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override {
        return { Dependency{"ComponentA", true} };
    }
    
    bool beginCalled = false;
};

std::vector<String> initOrder;

class TrackedComponent : public IComponent {
public:
    TrackedComponent(const String& name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { 
        initOrder.push_back(metadata.name);
        return ComponentStatus::Success; 
    }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { return deps; }
    
    void addDependency(const String& name, bool required = true) {
        deps.push_back(Dependency{name, required});
    }
    
private:
    std::vector<Dependency> deps;
};

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    if (passed) {
        Serial.printf("✓ %s\n", testName);
        testsPassed++;
    } else {
        Serial.printf("✗ %s\n", testName);
        testsFailed++;
    }
}

void test_register_component() {
    Core core;
    auto compA = std::make_unique<ComponentA>();
    bool result = core.addComponent(std::move(compA));
    printResult("Register single component", result && core.getComponentCount() == 1);
}

void test_register_duplicate_fails() {
    Core core;
    auto compA1 = std::make_unique<ComponentA>();
    auto compA2 = std::make_unique<ComponentA>();
    core.addComponent(std::move(compA1));
    bool result = core.addComponent(std::move(compA2));
    printResult("Duplicate registration fails", !result && core.getComponentCount() == 1);
}

void test_get_component_by_name() {
    Core core;
    auto compA = std::make_unique<ComponentA>();
    core.addComponent(std::move(compA));
    IComponent* found = core.getComponent("ComponentA");
    IComponent* notFound = core.getComponent("NonExistent");
    printResult("Get component by name", found != nullptr && notFound == nullptr);
}

void test_dependency_order_simple() {
    initOrder.clear();
    Core core;
    
    auto compC = std::make_unique<TrackedComponent>("C");
    compC->addDependency("B");
    auto compB = std::make_unique<TrackedComponent>("B");
    compB->addDependency("A");
    auto compA = std::make_unique<TrackedComponent>("A");
    
    core.addComponent(std::move(compC));
    core.addComponent(std::move(compB));
    core.addComponent(std::move(compA));
    core.begin();
    
    bool correctOrder = initOrder.size() == 3 &&
                        initOrder[0] == "A" &&
                        initOrder[1] == "B" &&
                        initOrder[2] == "C";
    printResult("Dependency order (A->B->C)", correctOrder);
}

void test_missing_required_dependency_fails() {
    Core core;
    auto compB = std::make_unique<ComponentB>();
    core.addComponent(std::move(compB));
    bool result = core.begin();
    printResult("Missing required dependency fails", !result);
}

void test_optional_dependency_ok_when_missing() {
    initOrder.clear();
    Core core;
    auto comp = std::make_unique<TrackedComponent>("Main");
    comp->addDependency("Optional", false);
    core.addComponent(std::move(comp));
    bool result = core.begin();
    printResult("Optional dependency OK when missing", result && initOrder.size() == 1);
}

void test_component_count() {
    Core core;
    printResult("Initial count is 0", core.getComponentCount() == 0);
    core.addComponent(std::make_unique<ComponentA>());
    printResult("Count after 1 add is 1", core.getComponentCount() == 1);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n========================================");
    Serial.println("DomoticsCore ComponentRegistry Tests");
    Serial.println("========================================\n");
    
    test_register_component();
    test_register_duplicate_fails();
    test_get_component_by_name();
    test_dependency_order_simple();
    test_missing_required_dependency_fails();
    test_optional_dependency_ok_when_missing();
    test_component_count();
    
    Serial.println("\n========================================");
    Serial.printf("Results: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println("========================================");
    
    if (testsFailed == 0) {
        Serial.println("🎉 ALL TESTS PASSED!");
    } else {
        Serial.println("❌ SOME TESTS FAILED");
    }
}

void loop() {
    delay(1000);
}
