#include <unity.h>
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

Core* testCore = nullptr;

void setUp(void) {
    testCore = new Core();
}

void tearDown(void) {
    delete testCore;
    testCore = nullptr;
}

void test_component_count_after_init(void) {
    testCore->addComponent(std::make_unique<SimpleComponent>("A"));
    testCore->addComponent(std::make_unique<SimpleComponent>("B"));
    testCore->addComponent(std::make_unique<SimpleComponent>("C"));
    testCore->begin();
    TEST_ASSERT_EQUAL(3, testCore->getComponentCount());
}

void test_get_component_after_init(void) {
    testCore->addComponent(std::make_unique<SimpleComponent>("MyComponent"));
    testCore->begin();
    IComponent* comp = testCore->getComponent("MyComponent");
    TEST_ASSERT_NOT_NULL(comp);
    TEST_ASSERT_EQUAL_STRING("MyComponent", comp->metadata.name.c_str());
}

void test_remove_component(void) {
    testCore->addComponent(std::make_unique<SimpleComponent>("ToRemove"));
    testCore->addComponent(std::make_unique<SimpleComponent>("ToKeep"));
    testCore->begin();
    TEST_ASSERT_EQUAL(2, testCore->getComponentCount());
    
    bool removed = testCore->removeComponent("ToRemove");
    TEST_ASSERT_TRUE(removed);
    TEST_ASSERT_EQUAL(1, testCore->getComponentCount());
    TEST_ASSERT_NULL(testCore->getComponent("ToRemove"));
    TEST_ASSERT_NOT_NULL(testCore->getComponent("ToKeep"));
}

void test_begin_fails_on_component_failure(void) {
    testCore->addComponent(std::make_unique<FailingComponent>());
    bool result = testCore->begin();
    TEST_ASSERT_FALSE(result);
}

void test_remove_nonexistent_component(void) {
    testCore->addComponent(std::make_unique<SimpleComponent>("Exists"));
    testCore->begin();
    bool removed = testCore->removeComponent("DoesNotExist");
    TEST_ASSERT_FALSE(removed);
    TEST_ASSERT_NULL(testCore->getComponent("NonExistent"));
}

void test_device_id_configuration(void) {
    auto testCore = std::make_unique<Core>();
    String customDeviceId = "test-device-123";
    
    // Test custom device ID
    CoreConfig config;
    config.deviceId = customDeviceId;
    testCore->begin(config);
    
    // Verify device ID is set
    TEST_ASSERT_EQUAL_STRING(customDeviceId.c_str(), testCore->getDeviceId().c_str());
}

void test_logging_initialization(void) {
    // Test that logging can be initialized without crashing
    HAL::initializeLogging(115200);
    
    // Test basic logging functionality
    DLOG_I("TEST", "Test log message");
    
    // Should not crash
    TEST_PASS();
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_component_count_after_init);
    RUN_TEST(test_get_component_after_init);
    RUN_TEST(test_remove_component);
    RUN_TEST(test_begin_fails_on_component_failure);
    RUN_TEST(test_remove_nonexistent_component);
    RUN_TEST(test_device_id_configuration);
    RUN_TEST(test_logging_initialization);
    
    return UNITY_END();
}
