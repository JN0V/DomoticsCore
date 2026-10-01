/**
 * @file test_webui_caching_provider.cpp
 * @brief WebUI component: CachingWebUIProvider memory, with HeapTracker.
 */

#include <unity.h>
#include "../webui_test_support.h"

void setUp() {}
void tearDown() {}

// ============================================================================
// CachingWebUIProvider Memory Tests (HeapTracker Integration)
// ============================================================================

// Test implementation of CachingWebUIProvider for memory testing
class TestCachingProvider : public CachingWebUIProvider {
public:
    int buildCount = 0;
    
protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        buildCount++;
        // Create contexts with substantial data to detect leaks
        contexts.push_back(WebUIContext::dashboard("test_dash", "Dashboard")
            .withField(WebUIField("field1", "Field 1", WebUIFieldType::Text, "value1"))
            .withField(WebUIField("field2", "Field 2", WebUIFieldType::Number, "42"))
            .withCustomHtml("<div class='test'>Custom HTML Content</div>")
            .withCustomCss(".test { color: red; }"));
        
        contexts.push_back(WebUIContext::settings("test_settings", "Settings")
            .withField(WebUIField("setting1", "Setting", WebUIFieldType::Boolean, "true")));
    }
    
public:
    String getWebUIName() const override { return "TestProvider"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override {
        return "{}";
    }
};

void test_caching_provider_builds_once() {
    TestCachingProvider provider;
    
    // First call should trigger build
    TEST_ASSERT_EQUAL(2, provider.getContextCount());
    TEST_ASSERT_EQUAL(1, provider.buildCount);
    
    // Second call should use cache
    TEST_ASSERT_EQUAL(2, provider.getContextCount());
    TEST_ASSERT_EQUAL(1, provider.buildCount); // Still 1, not rebuilt
    
    // Third call - still cached
    TEST_ASSERT_EQUAL(2, provider.getContextCount());
    TEST_ASSERT_EQUAL(1, provider.buildCount);
}

void test_caching_provider_memory_stable_100_calls() {
    HeapTracker tracker;
    TestCachingProvider provider;
    
    // First call builds cache
    (void)provider.getContextCount();
    
    // Checkpoint after cache is built
    tracker.checkpoint("after_cache");
    
    // Call 100 times - should not allocate new memory
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL(2, provider.getContextCount());
    }
    
    tracker.checkpoint("after_100_calls");
    
    // Assert no heap growth (tolerance for internal std::vector copies)
    MemoryTestResult result = tracker.assertStable("after_cache", "after_100_calls", 1024);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

void test_caching_provider_invalidate_rebuilds() {
    TestCachingProvider provider;
    
    // Build cache
    (void)provider.getContextCount();
    TEST_ASSERT_EQUAL(1, provider.buildCount);
    
    // Invalidate
    provider.invalidateContextCache();
    
    // Next call should rebuild
    (void)provider.getContextCount();
    TEST_ASSERT_EQUAL(2, provider.buildCount);
}

void test_caching_provider_foreach_no_rebuild() {
    TestCachingProvider provider;
    
    // Build via getWebUIContexts
    (void)provider.getContextCount();
    TEST_ASSERT_EQUAL(1, provider.buildCount);
    
    // forEachContext should use cache
    int contextCount = 0;
    provider.forEachContext([&contextCount](const WebUIContext& ctx) {
        contextCount++;
        return true;
    });
    
    TEST_ASSERT_EQUAL(2, contextCount);
    TEST_ASSERT_EQUAL(1, provider.buildCount); // No rebuild
}

void test_caching_provider_get_context_at() {
    TestCachingProvider provider;
    
    WebUIContext ctx;
    bool found = provider.getContextAt(0, ctx);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("test_dash", ctx.getContextIdCStr());
    
    found = provider.getContextAt(1, ctx);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("test_settings", ctx.getContextIdCStr());
    
    found = provider.getContextAt(2, ctx);
    TEST_ASSERT_FALSE(found);
    
    // All via cache - only 1 build
    TEST_ASSERT_EQUAL(1, provider.buildCount);
}

void test_caching_provider_memory_lifecycle() {
    HeapTracker tracker;
    
    tracker.checkpoint("before");
    
    // Create and destroy multiple providers
    for (int i = 0; i < 10; i++) {
        TestCachingProvider provider;
        (void)provider.getContextCount();
        // Provider destroyed at end of scope
    }
    
    tracker.checkpoint("after");
    
    MemoryTestResult result = tracker.assertStable("before", "after", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

/**
 * Test: forEachContext with copy assignment (simulates /api/ui/context issue)
 * This test demonstrates the memory cost of copying a context inside forEachContext
 */
void test_foreach_context_with_copy_assignment() {
    HeapTracker tracker;
    
    // Create provider with contexts (TestCachingProvider has built-in contexts)
    TestCachingProvider provider;
    
    // Warmup - trigger cache build and do initial copies
    WebUIContext warmupCopy;
    provider.forEachContext([&](const WebUIContext& ctx) {
        warmupCopy = ctx;
        return true;
    });
    
    tracker.checkpoint("start");
    
    // Simulate /api/ui/context behavior: find context and COPY it
    for (int i = 0; i < 100; i++) {
        WebUIContext foundContext;  // Stack allocation
        bool found = false;
        
        provider.forEachContext([&](const WebUIContext& ctx) {
            if (strcmp(ctx.getContextIdCStr(), "test_dash") == 0) {  // Use existing context ID
                foundContext = ctx;  // COPY - this is the potential leak source
                found = true;
                return false;
            }
            return true;
        });
        
        TEST_ASSERT_TRUE(found);
        // foundContext goes out of scope here - should be deallocated
    }
    
    tracker.checkpoint("end");
    
    int32_t delta = tracker.getDelta("start", "end");
    printf("\n[forEachContext with copy x100]: %d bytes (%.1f/req)\n", delta, delta/100.0f);
    
    // Should be stable - copies are stack-allocated and freed each iteration
    TEST_ASSERT_TRUE_MESSAGE(delta <= 256, "forEachContext copy leak detected");
}

/**
 * Test: Rapid refresh schema generation (simulates browser F5 spam)
 * 
 * This test reproduces the real-world scenario where:
 * 1. User rapidly refreshes the page (F5 spam)
 * 2. Each refresh triggers: schema request + WebSocket connect
 * 3. Previous requests may be interrupted (client disconnects)
 * 4. Memory should remain stable despite incomplete operations
 * 
 * Real behavior observed on ESP8266:
 * - Heap drops from 11216 to 3240 bytes during rapid refresh
 * - OOM crash when heap goes below ~2000 bytes
 */
void test_rapid_refresh_schema_generation() {
    HeapTracker tracker;
    
    // Create provider with realistic content (like LEDWebUI + SystemInfoWebUI)
    TestCachingProvider provider;
    
    // Warmup - let caches build
    provider.forEachContext([](const WebUIContext& ctx) { return true; });
    
    tracker.checkpoint("start");
    
    // Simulate 50 rapid page refreshes
    // Each refresh does: 1) start schema gen, 2) possibly interrupt, 3) start new one
    for (int refresh = 0; refresh < 50; refresh++) {
        // Simulate partial schema generation (like interrupted by disconnect)
        // This is what happens when user refreshes before schema completes
        
        // 1. Get schema contexts (simulates /api/ui/schema start)
        std::vector<const WebUIContext*> contextPtrs;
        size_t idx = 0;
        while (const WebUIContext* ctx = provider.getContextAtRef(idx++)) {
            contextPtrs.push_back(ctx);
        }
        
        // 2. Serialize only SOME contexts (simulates interrupted transfer)
        // About 30% of refreshes complete, 70% are interrupted
        size_t contextsToSerialize = (refresh % 3 == 0) ? contextPtrs.size() : contextPtrs.size() / 2;
        
        for (size_t i = 0; i < contextsToSerialize && i < contextPtrs.size(); i++) {
            StreamingContextSerializer serializer;
            serializer.begin(*contextPtrs[i]);
            
            // Serialize to local buffer (simulates chunked response)
            uint8_t buffer[256];
            while (!serializer.isComplete()) {
                size_t written = serializer.write(buffer, sizeof(buffer));
                if (written == 0) break;
                // In real code, this would be sent to client
                // If client disconnects, we just stop here
            }
        }
        
        // 3. Simulate WebSocket data send (getWebUIData allocates Strings)
        for (size_t i = 0; i < contextPtrs.size(); i++) {
            // This simulates getWebUIData() which creates JSON strings
            JsonDocument doc;
            doc["test_field"] = "test_value";
            doc["iteration"] = refresh;
            String json;
            serializeJson(doc, json);
            // json goes out of scope - should be freed
        }
    }
    
    tracker.checkpoint("end");
    
    int32_t delta = tracker.getDelta("start", "end");
    printf("\n[Rapid refresh x50]: %d bytes delta (%.1f/refresh)\n", delta, delta/50.0f);
    
    // Allow small tolerance for allocator overhead
    // If this fails, there's a memory leak during rapid refresh
    TEST_ASSERT_TRUE_MESSAGE(delta <= 512, 
        "Memory leak detected during rapid refresh simulation - "
        "heap should be stable after 50 page refreshes");
}


void test_many_providers_memory_usage() {
    HeapTracker tracker;
    
    tracker.checkpoint("before_provider");
    
    // Create provider with 16 contexts (like Standard example)
    MultiContextProvider* provider = new MultiContextProvider();
    
    tracker.checkpoint("after_create");
    
    // Force context build (cache warmup)
    size_t contextCount = 0;
    provider->forEachContext([&contextCount](const WebUIContext& ctx) { 
        contextCount++; 
        return true; 
    });
    
    tracker.checkpoint("after_warmup");
    
    printf("\n[Many providers test]: %zu contexts created\n", contextCount);
    TEST_ASSERT_EQUAL(15, contextCount);  // 5 Wifi + 3 NTP + 3 SystemInfo + 2 Console + 2 WebUI
    
    // Measure schema serialization memory
    tracker.checkpoint("before_schema");
    
    size_t totalSchemaSize = 0;
    size_t idx = 0;
    while (const WebUIContext* ctx = provider->getContextAtRef(idx++)) {
        StreamingContextSerializer serializer;
        serializer.begin(*ctx);
        
        uint8_t buffer[512];
        while (!serializer.isComplete()) {
            size_t written = serializer.write(buffer, sizeof(buffer));
            totalSchemaSize += written;
            if (written == 0) break;
        }
    }
    
    tracker.checkpoint("after_schema");
    
    printf("[Many providers test]: Schema size = %zu bytes\n", totalSchemaSize);
    printf("[Many providers test]: Peak memory for schema serialization = %d bytes\n", 
           (int)tracker.getDelta("before_schema", "after_schema"));
    
    // Cleanup
    delete provider;
    
    tracker.checkpoint("after_cleanup");
    
    int32_t totalDelta = tracker.getDelta("before_provider", "after_cleanup");
    printf("[Many providers test]: Total memory delta = %d bytes\n", totalDelta);
    
    // Memory should be mostly released (allow 4KB for allocator overhead/fragmentation on native)
    // On ESP8266, streaming serialization (0 bytes peak) is what matters most
    TEST_ASSERT_TRUE_MESSAGE(totalDelta <= 4096, 
        "Memory leak after provider cleanup - should be under 4KB");
    
    // Schema should not be too large (target < 10KB for ESP8266 with ~6KB free heap)
    TEST_ASSERT_TRUE_MESSAGE(totalSchemaSize < 10000, 
        "Schema too large for ESP8266 - consider reducing contexts or fields");
}

/**
 * @brief Test rapid consecutive schema serializations (simulates browser page load)
 * 
 * Browser may send multiple requests rapidly on page load. Previously this
 * caused 429 errors due to rate limiting. Now we just reset incomplete requests.
 */
void test_rapid_consecutive_schema_requests() {
    // Create a provider with contexts
    class TestProvider : public CachingWebUIProvider {
    protected:
        void buildContexts(std::vector<WebUIContext>& ctxs) override {
            ctxs.push_back(WebUIContext::dashboard("test1", "Test 1"));
            ctxs.push_back(WebUIContext::settings("test2", "Test 2"));
        }
    public:
        String getWebUIName() const override { return "TestProvider"; }
        String getWebUIVersion() const override { return "1.0.0"; }
        String getWebUIData(const String&) override { return "{}"; }
        String handleWebUIRequest(const String&, const String&, const String&, const std::map<String, String>&) override { return "{}"; }
        bool hasDataChanged(const String&) override { return false; }
    };

    TestProvider provider;
    
    // Simulate 10 rapid consecutive schema serializations (like browser page loads)
    for (int i = 0; i < 10; i++) {
        size_t idx = 0;
        while (const WebUIContext* ctx = provider.getContextAtRef(idx++)) {
            StreamingContextSerializer serializer;
            serializer.begin(*ctx);
            
            uint8_t buffer[512];
            while (!serializer.isComplete()) {
                size_t written = serializer.write(buffer, sizeof(buffer));
                if (written == 0) break;
            }
        }
    }
    
    // If we got here without crash or hang, the test passes
    // Previously, rate limiting would block rapid requests with 429
    TEST_PASS_MESSAGE("Rapid consecutive requests handled without blocking");
}


void test_caching_provider_get_context_by_id_from_cache() {
    MockWebUIProvider provider("Test", "1.0.0");
    provider.addContext(WebUIContext::dashboard("dash_1", "Dashboard"));
    provider.addContext(WebUIContext::settings("settings_1", "Settings"));

    // Force cache build via forEachContext
    provider.forEachContext([](const WebUIContext&) { return true; });

    // getWebUIContext should return correct match from cache
    WebUIContext found = provider.getWebUIContext("dash_1");
    TEST_ASSERT_EQUAL_STRING("dash_1", found.getContextIdCStr());
    TEST_ASSERT_EQUAL_STRING("Dashboard", found.getTitleCStr());

    // Second context also found
    WebUIContext found2 = provider.getWebUIContext("settings_1");
    TEST_ASSERT_EQUAL_STRING("settings_1", found2.getContextIdCStr());

    // Lookup non-existent returns empty
    WebUIContext notFound = provider.getWebUIContext("nonexistent");
    TEST_ASSERT_TRUE(strlen(notFound.getContextIdCStr()) == 0);
}

int main() {
    UNITY_BEGIN();

    // CachingWebUIProvider memory tests (HeapTracker)
    RUN_TEST(test_caching_provider_builds_once);
    RUN_TEST(test_caching_provider_memory_stable_100_calls);
    RUN_TEST(test_caching_provider_invalidate_rebuilds);
    RUN_TEST(test_caching_provider_foreach_no_rebuild);
    RUN_TEST(test_caching_provider_get_context_at);
    RUN_TEST(test_caching_provider_memory_lifecycle);

    // Context copy issue test (forEachContext with copy assignment)
    RUN_TEST(test_foreach_context_with_copy_assignment);

    // Rapid refresh simulation test (reproduces page reload scenario)
    RUN_TEST(test_rapid_refresh_schema_generation);

    // Many providers simulation (Standard example scenario)
    RUN_TEST(test_many_providers_memory_usage);

    // Rapid consecutive requests (regression test for 429 rate limiting issue)
    RUN_TEST(test_rapid_consecutive_schema_requests);

    // WebUIConfig char[] optimization tests (Phase 1)
    RUN_TEST(test_caching_provider_get_context_by_id_from_cache);

    return UNITY_END();
}
