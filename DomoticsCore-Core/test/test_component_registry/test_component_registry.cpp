#include <unity.h>
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
        return {Dependency{"ComponentA", true}}; 
    }
    
    bool beginCalled = false;
};

class ComponentD : public IComponent {
public:
    ComponentD() {
        metadata.name = "ComponentD";
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { 
        beginCalled = true;
        return ComponentStatus::Success; 
    }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { 
        return {Dependency{"ComponentX", false}}; 
    }
    
    bool beginCalled = false;
};

Core* testCore = nullptr;

void setUp(void) {
    testCore = new Core();
}

void tearDown(void) {
    delete testCore;
    testCore = nullptr;
}

void test_register_component(void) {
    auto compA = std::make_unique<ComponentA>();
    bool result = testCore->addComponent(std::move(compA));
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, testCore->getComponentCount());
}

void test_register_duplicate_fails(void) {
    auto compA1 = std::make_unique<ComponentA>();
    auto compA2 = std::make_unique<ComponentA>();
    
    bool result1 = testCore->addComponent(std::move(compA1));
    TEST_ASSERT_TRUE(result1);
    
    bool result2 = testCore->addComponent(std::move(compA2));
    TEST_ASSERT_FALSE(result2);
    TEST_ASSERT_EQUAL(1, testCore->getComponentCount());
}

void test_get_component_by_name(void) {
    auto compA = std::make_unique<ComponentA>();
    testCore->addComponent(std::move(compA));
    
    IComponent* found = testCore->getComponent("ComponentA");
    IComponent* notFound = testCore->getComponent("NonExistent");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_NULL(notFound);
}

void test_dependency_order_simple(void) {
    auto compB = std::make_unique<ComponentB>();
    auto compA = std::make_unique<ComponentA>();
    
    testCore->addComponent(std::move(compB));
    testCore->addComponent(std::move(compA));
    
    testCore->begin();
    
    ComponentA* compAPtr = static_cast<ComponentA*>(testCore->getComponent("ComponentA"));
    ComponentB* compBPtr = static_cast<ComponentB*>(testCore->getComponent("ComponentB"));
    
    TEST_ASSERT_TRUE(compAPtr->beginCalled);
    TEST_ASSERT_TRUE(compBPtr->beginCalled);
}

void test_missing_required_dependency_fails(void) {
    auto compB = std::make_unique<ComponentB>();
    testCore->addComponent(std::move(compB));
    
    bool result = testCore->begin();
    TEST_ASSERT_FALSE(result);
}

void test_optional_dependency_ok_when_missing(void) {
    auto compD = std::make_unique<ComponentD>();
    testCore->addComponent(std::move(compD));
    
    bool result = testCore->begin();
    TEST_ASSERT_TRUE(result);
    
    ComponentD* compDPtr = static_cast<ComponentD*>(testCore->getComponent("ComponentD"));
    TEST_ASSERT_TRUE(compDPtr->beginCalled);
}

void test_component_count(void) {
    TEST_ASSERT_EQUAL(0, testCore->getComponentCount());
    testCore->addComponent(std::make_unique<ComponentA>());
    TEST_ASSERT_EQUAL(1, testCore->getComponentCount());
}

void test_circular_dependency_fails(void) {
    auto testCore = std::make_unique<Core>();
    
    class CircularA : public IComponent {
    public:
        CircularA() {
            metadata.name = "CircularA";
            metadata.version = "1.0.0";
        }
        ComponentStatus begin() override { return ComponentStatus::Success; }
        void loop() override {}
        ComponentStatus shutdown() override { return ComponentStatus::Success; }
        std::vector<Dependency> getDependencies() const override { 
            return {Dependency("CircularB", true)}; 
        }
    };
    
    class CircularB : public IComponent {
    public:
        CircularB() {
            metadata.name = "CircularB";
            metadata.version = "1.0.0";
        }
        ComponentStatus begin() override { return ComponentStatus::Success; }
        void loop() override {}
        ComponentStatus shutdown() override { return ComponentStatus::Success; }
        std::vector<Dependency> getDependencies() const override { 
            return {Dependency("CircularA", true)}; 
        }
    };
    
    testCore->addComponent(std::make_unique<CircularA>());
    testCore->addComponent(std::make_unique<CircularB>());
    
    // Should fail due to circular dependency
    bool result = testCore->begin();
    TEST_ASSERT_FALSE(result);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_register_component);
    RUN_TEST(test_register_duplicate_fails);
    RUN_TEST(test_get_component_by_name);
    RUN_TEST(test_dependency_order_simple);
    RUN_TEST(test_missing_required_dependency_fails);
    RUN_TEST(test_optional_dependency_ok_when_missing);
    RUN_TEST(test_component_count);
    RUN_TEST(test_circular_dependency_fails);
    
    return UNITY_END();
}
