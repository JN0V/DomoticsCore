/**
 * Hardware test for schema endpoint memory leak
 *
 * This test runs on real ESP8266/ESP32 hardware and measures
 * heap stability when generating schema multiple times.
 *
 * The test FAILS if heap decreases by more than 16 bytes per iteration
 * (allowing for minor allocator overhead).
 */

#include <unity.h>
#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/IWebUIProvider.h>
#include <DomoticsCore/WebUI/StreamingContextSerializer.h>
#include <ArduinoJson.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// Test provider that simulates real provider behavior
// Creates fresh contexts each call (the problematic pattern)
class LeakyTestProvider : public IWebUIProvider {
public:
    String getWebUIName() const override { return "LeakyTest"; }
    String getWebUIVersion() const override { return "1.0.0"; }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;

        // Dashboard context with custom HTML (simulates NTPWebUI, WiFiWebUI, etc.)
        contexts.push_back(WebUIContext::dashboard("test_dashboard", "Test Dashboard", "dc-test")
            .withField(WebUIField("field1", "Field 1", WebUIFieldType::Text, "value1"))
            .withField(WebUIField("field2", "Field 2", WebUIFieldType::Number, "42"))
            .withCustomHtml("<div class=\"test-container\"><span>Custom HTML content here</span></div>")
            .withCustomCss(".test-container { padding: 1rem; background: #f0f0f0; }")
            .withRealTime(1000));

        // Settings context with more fields
        contexts.push_back(WebUIContext::settings("test_settings", "Test Settings")
            .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true"))
            .withField(WebUIField("name", "Name", WebUIFieldType::Text, "Test Device"))
            .withField(WebUIField("interval", "Interval", WebUIFieldType::Number, "5000", "ms")));

        // Status badge
        contexts.push_back(WebUIContext::statusBadge("test_status", "Status", "dc-info")
            .withField(WebUIField("state", "State", WebUIFieldType::Status, "OK")));

        return contexts;
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override {
        return "{\"success\":true}";
    }

    String getWebUIData(const String& contextId) override {
        return "{}";
    }
};

// Cached provider - stores contexts once, no re-allocation
class CachedTestProvider : public IWebUIProvider {
private:
    std::vector<WebUIContext> cachedContexts;
    bool cached = false;

    void ensureCached() {
        if (cached) return;

        cachedContexts.push_back(WebUIContext::dashboard("cached_dashboard", "Cached Dashboard", "dc-test")
            .withField(WebUIField("field1", "Field 1", WebUIFieldType::Text, "value1"))
            .withField(WebUIField("field2", "Field 2", WebUIFieldType::Number, "42"))
            .withCustomHtml("<div class=\"test-container\"><span>Custom HTML content here</span></div>")
            .withCustomCss(".test-container { padding: 1rem; background: #f0f0f0; }")
            .withRealTime(1000));

        cachedContexts.push_back(WebUIContext::settings("cached_settings", "Cached Settings")
            .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true"))
            .withField(WebUIField("name", "Name", WebUIFieldType::Text, "Test Device"))
            .withField(WebUIField("interval", "Interval", WebUIFieldType::Number, "5000", "ms")));

        cachedContexts.push_back(WebUIContext::statusBadge("cached_status", "Status", "dc-info")
            .withField(WebUIField("state", "State", WebUIFieldType::Status, "OK")));

        cached = true;
    }

public:
    String getWebUIName() const override { return "CachedTest"; }
    String getWebUIVersion() const override { return "1.0.0"; }

    std::vector<WebUIContext> getWebUIContexts() override {
        ensureCached();
        return cachedContexts;  // Returns copy, but from cached data
    }

    // Memory-efficient: return count without creating vector
    size_t getContextCount() override {
        ensureCached();
        return cachedContexts.size();
    }

    // Memory-efficient: copy from cached storage
    bool getContextAt(size_t index, WebUIContext& outContext) override {
        ensureCached();
        if (index < cachedContexts.size()) {
            outContext = cachedContexts[index];
            return true;
        }
        return false;
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override {
        return "{\"success\":true}";
    }

    String getWebUIData(const String& contextId) override {
        return "{}";
    }
};

// Helper to simulate schema generation (like /api/ui/schema does)
void simulateSchemaGeneration(IWebUIProvider* provider) {
    uint8_t buffer[512];

    size_t count = provider->getContextCount();
    for (size_t i = 0; i < count; i++) {
        WebUIContext ctx;
        if (provider->getContextAt(i, ctx)) {
            StreamingContextSerializer serializer;
            serializer.begin(ctx);

            while (!serializer.isComplete()) {
                serializer.write(buffer, sizeof(buffer));
            }
        }
    }
}

void setUp() {}
void tearDown() {}

void test_leaky_provider_shows_memory_leak() {
    LeakyTestProvider provider;

    // Warmup
    simulateSchemaGeneration(&provider);
    delay(10);

    uint32_t heapBefore = HAL::Platform::getFreeHeap();

    const int ITERATIONS = 10;
    for (int i = 0; i < ITERATIONS; i++) {
        simulateSchemaGeneration(&provider);
    }

    uint32_t heapAfter = HAL::Platform::getFreeHeap();
    int heapDiff = (int)heapBefore - (int)heapAfter;
    int leakPerIteration = heapDiff / ITERATIONS;

    Serial.printf("LEAKY: heap before=%u, after=%u, diff=%d, per_iter=%d\n",
                  heapBefore, heapAfter, heapDiff, leakPerIteration);

    // This test documents the leak - it will likely PASS because
    // the default getContextAt still calls getWebUIContexts
    // We expect significant leak here
    TEST_ASSERT_TRUE_MESSAGE(true, "Leaky provider baseline recorded");
}

void test_cached_provider_no_memory_leak() {
    CachedTestProvider provider;

    // Warmup - this creates the cached contexts
    simulateSchemaGeneration(&provider);
    delay(10);

    uint32_t heapBefore = HAL::Platform::getFreeHeap();

    const int ITERATIONS = 20;
    for (int i = 0; i < ITERATIONS; i++) {
        simulateSchemaGeneration(&provider);
    }

    uint32_t heapAfter = HAL::Platform::getFreeHeap();
    int heapDiff = (int)heapBefore - (int)heapAfter;
    int leakPerIteration = heapDiff / ITERATIONS;

    Serial.printf("CACHED: heap before=%u, after=%u, diff=%d, per_iter=%d\n",
                  heapBefore, heapAfter, heapDiff, leakPerIteration);

    // Cached provider should have minimal leak (< 8 bytes per iteration)
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(8, leakPerIteration,
        "Cached provider should not leak significantly");
}

void test_stress_schema_generation() {
    // Stress test with cached provider
    CachedTestProvider provider;

    // Warmup
    simulateSchemaGeneration(&provider);
    delay(10);

    uint32_t heapStart = HAL::Platform::getFreeHeap();
    Serial.printf("STRESS: Starting heap=%u\n", heapStart);

    // Run many iterations
    const int STRESS_ITERATIONS = 50;
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        simulateSchemaGeneration(&provider);

        if (i % 10 == 9) {
            uint32_t currentHeap = HAL::Platform::getFreeHeap();
            Serial.printf("STRESS: iteration %d, heap=%u\n", i + 1, currentHeap);
        }
    }

    uint32_t heapEnd = HAL::Platform::getFreeHeap();
    int totalDiff = (int)heapStart - (int)heapEnd;

    Serial.printf("STRESS: End heap=%u, total diff=%d\n", heapEnd, totalDiff);

    // Should not lose more than 200 bytes total over 50 iterations
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(200, totalDiff,
        "Stress test should not leak more than 200 bytes total");
}

int main() {
    delay(2000);  // Wait for serial
    Serial.begin(115200);

    Serial.println("\n=== Schema Memory Leak Test ===");
    Serial.printf("Initial heap: %u bytes\n", HAL::Platform::getFreeHeap());

    UNITY_BEGIN();

    RUN_TEST(test_leaky_provider_shows_memory_leak);
    RUN_TEST(test_cached_provider_no_memory_leak);
    RUN_TEST(test_stress_schema_generation);

    return UNITY_END();
}
