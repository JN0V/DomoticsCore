#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

class SimpleComponent : public IComponent {
public:
    SimpleComponent(const String& name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { return {}; }
};

class FailingComponent : public IComponent {
public:
    FailingComponent() {
        metadata.name = "FailingComp";
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { return ComponentStatus::ConfigError; }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { return {}; }
};

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_component_count_after_init() {
    Core core;
    core.addComponent(std::make_unique<SimpleComponent>("A"));
    core.addComponent(std::make_unique<SimpleComponent>("B"));
    core.addComponent(std::make_unique<SimpleComponent>("C"));
    core.begin();
    printResult("Component count correct after init", core.getComponentCount() == 3);
}

void test_get_component_after_init() {
    Core core;
    core.addComponent(std::make_unique<SimpleComponent>("MyComponent"));
    core.begin();
    IComponent* comp = core.getComponent("MyComponent");
    printResult("Component retrievable after init", comp != nullptr);
    printResult("Component has correct name", comp && comp->metadata.name == "MyComponent");
}

void test_remove_component() {
    Core core;
    core.addComponent(std::make_unique<SimpleComponent>("ToRemove"));
    core.addComponent(std::make_unique<SimpleComponent>("ToKeep"));
    core.begin();
    printResult("Initial count is 2", core.getComponentCount() == 2);
    bool removed = core.removeComponent("ToRemove");
    printResult("removeComponent returns true", removed);
    printResult("Count after remove is 1", core.getComponentCount() == 1);
    printResult("Removed component not found", core.getComponent("ToRemove") == nullptr);
    printResult("Kept component still exists", core.getComponent("ToKeep") != nullptr);
}

void test_begin_fails_on_component_failure() {
    Core core;
    core.addComponent(std::make_unique<FailingComponent>());
    bool result = core.begin();
    printResult("begin() returns false on component failure", !result);
}

void test_remove_nonexistent_component() {
    Core core;
    core.addComponent(std::make_unique<SimpleComponent>("Exists"));
    core.begin();
    bool removed = core.removeComponent("DoesNotExist");
    printResult("Remove nonexistent returns false", !removed);
    printResult("Existing component unaffected", core.getComponent("Exists") != nullptr);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore System Ready Tests");
    Serial.println("========================================\n");
    
    test_component_count_after_init();
    test_get_component_after_init();
    test_remove_component();
    test_begin_fails_on_component_failure();
    test_remove_nonexistent_component();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
