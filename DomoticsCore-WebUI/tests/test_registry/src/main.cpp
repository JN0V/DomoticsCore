#include <Arduino.h>
#include <WiFi.h>
#include <unity.h>
#include <DomoticsCore/WebUI/ProviderRegistry.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// Mock Provider
class MockProvider : public IWebUIProvider {
public:
    String getWebUIName() const override { return "MockProvider"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    
    std::vector<WebUIContext> getWebUIContexts() override {
        return { WebUIContext::statusBadge("mock_ctx", "Mock Title", "icon") };
    }
    
    String getWebUIData(const String&) override { return "{}"; }
    
    String handleWebUIRequest(const String&, const String&, const String&, const std::map<String, String>&) override { return "{}"; }
};

// Mock Component
class MockComponent : public IComponent {
    MockProvider provider;
public:
    MockComponent() { metadata.name = "MockComp"; }
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    IWebUIProvider* getWebUIProvider() override { return &provider; }
};

ProviderRegistry* registry;
MockProvider* provider;

void setUp(void) {
    registry = new ProviderRegistry();
    provider = new MockProvider();
}

void tearDown(void) {
    delete registry;
    delete provider;
}

void test_register_provider() {
    registry->registerProvider(provider);
    auto* retrieved = registry->getProviderForContext("mock_ctx");
    TEST_ASSERT_EQUAL_PTR(provider, retrieved);
}

void test_provider_enable_disable() {
    registry->registerProvider(provider);
    
    // Enable
    auto res = registry->enableComponent("MockProvider", true);
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_TRUE(res.enabled);
    
    // Disable
    res = registry->enableComponent("MockProvider", false);
    TEST_ASSERT_TRUE(res.success);
    TEST_ASSERT_FALSE(res.enabled);
    
    // Verify context is removed
    auto* retrieved = registry->getProviderForContext("mock_ctx");
    TEST_ASSERT_NULL(retrieved);
}

void test_component_removal() {
    MockComponent comp;
    registry->registerProviderWithComponent(comp.getWebUIProvider(), &comp);
    
    TEST_ASSERT_NOT_NULL(registry->getProviderForContext("mock_ctx"));
    
    registry->handleComponentRemoved(&comp);
    TEST_ASSERT_NULL(registry->getProviderForContext("mock_ctx"));
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_register_provider);
    RUN_TEST(test_provider_enable_disable);
    RUN_TEST(test_component_removal);
    UNITY_END();
}

void loop() {}
