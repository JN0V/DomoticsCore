#include <unity.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <cstring>

#ifdef NATIVE_BUILD
typedef std::string String;
inline unsigned long millis() { static unsigned long t = 0; return t += 10; }

namespace DomoticsCore {
namespace Mocks {

// ============================================================================
// Mock WebUI Provider Interface
// ============================================================================
class IWebUIProvider {
public:
    virtual std::string getWebUIData(const std::string& contextId) = 0;
    virtual bool hasDataChanged(const std::string& contextId) = 0;
    virtual ~IWebUIProvider() = default;
};

// ============================================================================
// Mock Provider for testing
// ============================================================================
class MockProvider : public IWebUIProvider {
public:
    std::string data = "{}";
    bool changed = true;
    int getDataCallCount = 0;
    
    std::string getWebUIData(const std::string& contextId) override {
        getDataCallCount++;
        return data;
    }
    
    bool hasDataChanged(const std::string& contextId) override {
        return changed;
    }
    
    void setData(const std::string& newData) {
        data = newData;
        changed = true;
    }
    
    void markUnchanged() { changed = false; }
};

} // namespace Mocks
} // namespace DomoticsCore
#endif

using namespace DomoticsCore::Mocks;

// ============================================================================
// WebUI Provider Registry Logic Under Test
// ============================================================================

class ProviderRegistryUnderTest {
public:
    std::map<std::string, IWebUIProvider*> providers;
    
    void registerProvider(const std::string& contextId, IWebUIProvider* provider) {
        providers[contextId] = provider;
    }
    
    void unregisterProvider(const std::string& contextId) {
        providers.erase(contextId);
    }
    
    IWebUIProvider* getProvider(const std::string& contextId) {
        auto it = providers.find(contextId);
        return it != providers.end() ? it->second : nullptr;
    }
    
    std::map<std::string, IWebUIProvider*>& getProviders() {
        return providers;
    }
    
    int getProviderCount() const { return providers.size(); }
};

// ============================================================================
// WebUI Data Aggregation Logic Under Test
// ============================================================================

class WebUIDataAggregatorUnderTest {
public:
    ProviderRegistryUnderTest* registry;
    bool forceUpdate = false;
    
    // Simulates the 8KB buffer behavior
    static const size_t BUFFER_SIZE = 8192;
    
    WebUIDataAggregatorUnderTest(ProviderRegistryUnderTest* reg) : registry(reg) {}
    
    std::string aggregateData() {
        std::string result = "{\"contexts\":{";
        bool first = true;
        size_t totalSize = result.length();
        
        for (auto& pair : registry->getProviders()) {
            const std::string& contextId = pair.first;
            IWebUIProvider* provider = pair.second;
            
            // Delta check - skip unchanged unless forced
            if (!forceUpdate && !provider->hasDataChanged(contextId)) {
                continue;
            }
            
            std::string data = provider->getWebUIData(contextId);
            if (data.empty() || data == "{}") continue;
            
            // Check buffer capacity
            size_t needed = contextId.length() + data.length() + 5;
            if (totalSize + needed >= BUFFER_SIZE - 10) {
                // Buffer full, stop
                break;
            }
            
            if (!first) result += ",";
            result += "\"" + contextId + "\":" + data;
            totalSize += needed;
            first = false;
        }
        
        result += "}}";
        return result;
    }
    
    void setForceUpdate(bool force) { forceUpdate = force; }
};

// ============================================================================
// Tests
// ============================================================================

ProviderRegistryUnderTest* registry = nullptr;
WebUIDataAggregatorUnderTest* aggregator = nullptr;

void setUp(void) {
    registry = new ProviderRegistryUnderTest();
    aggregator = new WebUIDataAggregatorUnderTest(registry);
}

void tearDown(void) {
    delete aggregator;
    delete registry;
    aggregator = nullptr;
    registry = nullptr;
}

// T137: Test WebUI provider registration
void test_webui_provider_registration(void) {
    MockProvider provider1;
    MockProvider provider2;
    
    registry->registerProvider("system", &provider1);
    registry->registerProvider("sensors", &provider2);
    
    TEST_ASSERT_EQUAL(2, registry->getProviderCount());
    TEST_ASSERT_EQUAL(&provider1, registry->getProvider("system"));
    TEST_ASSERT_EQUAL(&provider2, registry->getProvider("sensors"));
}

// T137b: Test provider unregistration
void test_webui_provider_unregistration(void) {
    MockProvider provider;
    
    registry->registerProvider("test", &provider);
    TEST_ASSERT_EQUAL(1, registry->getProviderCount());
    
    registry->unregisterProvider("test");
    TEST_ASSERT_EQUAL(0, registry->getProviderCount());
    TEST_ASSERT_NULL(registry->getProvider("test"));
}

// T138: Test WebUI data aggregation
void test_webui_data_aggregation(void) {
    MockProvider provider1;
    provider1.setData("{\"temp\":25}");
    
    MockProvider provider2;
    provider2.setData("{\"humidity\":60}");
    
    registry->registerProvider("sensor1", &provider1);
    registry->registerProvider("sensor2", &provider2);
    
    std::string result = aggregator->aggregateData();
    
    // Should contain both contexts
    TEST_ASSERT_TRUE(result.find("\"sensor1\"") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("\"sensor2\"") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("\"temp\":25") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("\"humidity\":60") != std::string::npos);
}

// T139: Test WebUI delta updates
void test_webui_delta_updates(void) {
    MockProvider provider1;
    provider1.setData("{\"temp\":25}");
    provider1.markUnchanged();  // No changes
    
    MockProvider provider2;
    provider2.setData("{\"humidity\":60}");
    // provider2 has changes (default)
    
    registry->registerProvider("sensor1", &provider1);
    registry->registerProvider("sensor2", &provider2);
    
    std::string result = aggregator->aggregateData();
    
    // Should only contain sensor2 (sensor1 unchanged)
    TEST_ASSERT_TRUE(result.find("\"sensor2\"") != std::string::npos);
    TEST_ASSERT_FALSE(result.find("\"sensor1\"") != std::string::npos);
    
    // Force update should include both
    aggregator->setForceUpdate(true);
    result = aggregator->aggregateData();
    
    TEST_ASSERT_TRUE(result.find("\"sensor1\"") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("\"sensor2\"") != std::string::npos);
}

// T139b: Test getDataCallCount only called for changed providers
void test_webui_delta_avoids_unnecessary_calls(void) {
    MockProvider provider1;
    provider1.markUnchanged();
    
    MockProvider provider2;
    // provider2 has changes
    
    registry->registerProvider("p1", &provider1);
    registry->registerProvider("p2", &provider2);
    
    aggregator->aggregateData();
    
    // Provider1 should not have been called (unchanged)
    TEST_ASSERT_EQUAL(0, provider1.getDataCallCount);
    // Provider2 should have been called
    TEST_ASSERT_EQUAL(1, provider2.getDataCallCount);
}

// T140: Test WebUI 8KB buffer behavior
void test_webui_buffer_truncation(void) {
    // Create providers with large data
    std::vector<std::unique_ptr<MockProvider>> providers;
    
    // Each provider returns ~1KB of data
    std::string largeData(1000, 'x');
    largeData = "{\"data\":\"" + largeData + "\"}";
    
    for (int i = 0; i < 15; i++) {  // 15 * 1KB > 8KB
        auto provider = std::make_unique<MockProvider>();
        provider->setData(largeData);
        registry->registerProvider("ctx" + std::to_string(i), provider.get());
        providers.push_back(std::move(provider));
    }
    
    std::string result = aggregator->aggregateData();
    
    // Result should be under buffer size
    TEST_ASSERT_TRUE(result.length() < WebUIDataAggregatorUnderTest::BUFFER_SIZE);
    
    // Should have truncated some contexts
    int contextCount = 0;
    for (int i = 0; i < 15; i++) {
        if (result.find("\"ctx" + std::to_string(i) + "\"") != std::string::npos) {
            contextCount++;
        }
    }
    
    // Should have less than all 15 contexts
    TEST_ASSERT_TRUE(contextCount < 15);
    TEST_ASSERT_TRUE(contextCount > 0);  // But at least some
}

// T136: Test empty providers skipped
void test_webui_empty_providers_skipped(void) {
    MockProvider emptyProvider;
    emptyProvider.setData("{}");
    
    MockProvider normalProvider;
    normalProvider.setData("{\"value\":42}");
    
    registry->registerProvider("empty", &emptyProvider);
    registry->registerProvider("normal", &normalProvider);
    
    std::string result = aggregator->aggregateData();
    
    // Empty provider should be skipped
    TEST_ASSERT_FALSE(result.find("\"empty\"") != std::string::npos);
    TEST_ASSERT_TRUE(result.find("\"normal\"") != std::string::npos);
}

// Additional: Test no providers
void test_webui_no_providers(void) {
    std::string result = aggregator->aggregateData();
    
    TEST_ASSERT_EQUAL_STRING("{\"contexts\":{}}", result.c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_webui_provider_registration);
    RUN_TEST(test_webui_provider_unregistration);
    RUN_TEST(test_webui_data_aggregation);
    RUN_TEST(test_webui_delta_updates);
    RUN_TEST(test_webui_delta_avoids_unnecessary_calls);
    RUN_TEST(test_webui_buffer_truncation);
    RUN_TEST(test_webui_empty_providers_skipped);
    RUN_TEST(test_webui_no_providers);
    
    return UNITY_END();
}
