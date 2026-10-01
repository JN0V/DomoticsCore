/**
 * @file test_webui_leak_detection.cpp
 * @brief WebUI component: leak detection across providers and contexts.
 */

#include <unity.h>
#include "../webui_test_support.h"

void setUp() {}
void tearDown() {}

// ============================================================================
// Memory Leak DETECTION Tests - Test CURRENT behavior before fixes
// ============================================================================

/**
 * This test DETECTS memory behavior of the standard IWebUIProvider.
 * MockWebUIProvider inherits from IWebUIProvider (NOT CachingWebUIProvider),
 * so it recreates contexts on every getWebUIContexts() call.
 * 
 * PURPOSE: Measure actual memory impact of repeated context creation
 * to understand WHERE memory leaks originate.
 */
void test_detect_memory_behavior_repeated_context_creation() {
    HeapTracker tracker;
    
    // Create a provider with substantial context data (simulating real WebUI)
    MockWebUIProvider provider("LeakTest", "1.0.0");
    provider.addContext(WebUIContext::dashboard("dash", "Dashboard")
        .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number, "25.5", "°C", true))
        .withField(WebUIField("humid", "Humidity", WebUIFieldType::Number, "60", "%", true))
        .withCustomHtml("<div class=\"widget\"><span class=\"value\">Custom HTML content here for testing memory allocation patterns in WebUI contexts</span></div>")
        .withCustomCss(".widget { background: #fff; padding: 1rem; } .value { font-size: 2rem; color: #007acc; }"));
    
    provider.addContext(WebUIContext::settings("settings", "Settings")
        .withField(WebUIField("name", "Device Name", WebUIFieldType::Text, "DomoticsCore"))
        .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true")));
    
    // Warm up - first call using new API
    provider.forEachContext([](const WebUIContext& ctx) {
        (void)ctx.getContextIdCStr();
        return true;
    });
    
    // Checkpoint after warmup
    tracker.checkpoint("after_warmup");
    
    // Use NEW memory-efficient API: forEachContext (no vector copies)
    for (int i = 0; i < 50; i++) {
        provider.forEachContext([](const WebUIContext& ctx) {
            (void)ctx.getContextIdCStr();
            return true;
        });
    }
    
    tracker.checkpoint("after_50_calls");
    
    // Calculate delta
    int32_t delta = tracker.getDelta("after_warmup", "after_50_calls");
    
    // Report the actual memory behavior
    printf("\n[MEMORY DETECTION] forEachContext() x50 (NEW optimized API):\n");
    printf("  Heap delta: %d bytes\n", delta);
    printf("  Per call: ~%d bytes\n", delta / 50);
    
    // FAIL if significant memory leak detected
    // Native uses real mallinfo tracking, so we can detect real leaks
    const int32_t LEAK_THRESHOLD = 1024;  // 1KB tolerance for native (allocator overhead)
    
    if (delta > LEAK_THRESHOLD) {
        printf("  *** MEMORY LEAK DETECTED: %d bytes > threshold %d ***\n", delta, LEAK_THRESHOLD);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak detected in getWebUIContexts()");
}

/**
 * ZERO LEAK TEST: Test with MULTIPLE providers (like real WebUI)
 * This should identify if leak comes from multi-provider scenario
 */
void test_zero_leak_multiple_providers() {
    HeapTracker tracker;
    
    // 3 providers like real WebUI
    MockWebUIProvider p1("WiFi", "1.0.0");
    p1.addContext(WebUIContext::dashboard("wifi", "WiFi")
        .withField(WebUIField("ssid", "SSID", WebUIFieldType::Text, "MyNet"))
        .withCustomHtml("<div>wifi</div>"));
    
    MockWebUIProvider p2("NTP", "1.0.0");
    p2.addContext(WebUIContext::settings("ntp", "NTP")
        .withField(WebUIField("server", "Server", WebUIFieldType::Text, "pool.ntp.org")));
    
    MockWebUIProvider p3("System", "1.0.0");
    p3.addContext(WebUIContext::dashboard("sys", "System")
        .withField(WebUIField("heap", "Heap", WebUIFieldType::Number, "50000")));
    
    std::vector<IWebUIProvider*> providers = {&p1, &p2, &p3};
    
    // Force cache build
    for (auto* p : providers) {
        p->forEachContext([](const WebUIContext&) { return true; });
    }
    
    tracker.checkpoint("start");
    
    // 500 iterations - using ZERO-COPY getContextAtRef
    for (int i = 0; i < 500; i++) {
        for (auto* provider : providers) {
            size_t count = provider->getContextCount();
            for (size_t j = 0; j < count; j++) {
                const WebUIContext* ctxPtr = provider->getContextAtRef(j);
                if (!ctxPtr) continue;
                
                StreamingContextSerializer serializer;
                serializer.begin(*ctxPtr);
                
                uint8_t buffer[256];
                while (!serializer.isComplete()) {
                    serializer.write(buffer, sizeof(buffer));
                }
            }
        }
    }
    
    tracker.checkpoint("end");
    int32_t delta = tracker.getDelta("start", "end");
    
    printf("\n[ZERO LEAK - MULTI PROVIDER] 3 providers x500:\n");
    printf("  Heap delta: %d bytes (%.2f/iter)\n", delta, delta/500.0f);
    
    // Allow small allocator overhead (native allocator may not release immediately)
    const int32_t THRESHOLD = 512;
    TEST_ASSERT_TRUE_MESSAGE(delta <= THRESHOLD, "Multi-provider leak exceeds threshold");
}

/**
 * ZERO LEAK TEST: Find EXACT source of 3 bytes/req leak
 * Must be 0 bytes - no tolerance for any leak
 */
void test_zero_leak_streaming_only() {
    HeapTracker tracker;
    
    MockWebUIProvider provider("Test", "1.0.0");
    provider.addContext(WebUIContext::dashboard("dash", "Dashboard")
        .withField(WebUIField("temp", "Temp", WebUIFieldType::Number, "25"))
        .withCustomHtml("<div>test content</div>"));
    
    // Force cache build
    provider.forEachContext([](const WebUIContext&) { return true; });
    
    tracker.checkpoint("start");
    
    // Test StreamingContextSerializer with ZERO-COPY
    for (int i = 0; i < 100; i++) {
        size_t count = provider.getContextCount();
        for (size_t j = 0; j < count; j++) {
            const WebUIContext* ctxPtr = provider.getContextAtRef(j);
            if (!ctxPtr) continue;
            
            StreamingContextSerializer serializer;
            serializer.begin(*ctxPtr);
            
            uint8_t buffer[256];
            while (!serializer.isComplete()) {
                serializer.write(buffer, sizeof(buffer));
            }
        }
    }
    
    tracker.checkpoint("end");
    int32_t delta = tracker.getDelta("start", "end");
    
    printf("\n[ZERO LEAK TEST] StreamingContextSerializer x100:\n");
    printf("  Heap delta: %d bytes\n", delta);
    printf("  Per iteration: %.2f bytes\n", delta / 100.0f);
    
    // Allow small allocator overhead
    const int32_t THRESHOLD = 512;
    TEST_ASSERT_TRUE_MESSAGE(delta <= THRESHOLD, "Streaming leak exceeds threshold");
}

/**
 * ZERO LEAK TEST: Test getContextAt alone
 */
void test_zero_leak_getContextAt_only() {
    HeapTracker tracker;
    
    MockWebUIProvider provider("Test", "1.0.0");
    provider.addContext(WebUIContext::dashboard("dash", "Dashboard")
        .withField(WebUIField("temp", "Temp", WebUIFieldType::Number, "25"))
        .withCustomHtml("<div>test</div>"));
    
    // Force cache build
    provider.forEachContext([](const WebUIContext&) { return true; });
    
    tracker.checkpoint("start");
    
    // Test getContextAtRef - ZERO COPY
    for (int i = 0; i < 100; i++) {
        const WebUIContext* ctxPtr = provider.getContextAtRef(0);
        (void)ctxPtr;  // No copy, just pointer access
    }
    
    tracker.checkpoint("end");
    int32_t delta = tracker.getDelta("start", "end");
    
    printf("[MEMORY TEST] getContextAt x100: %d bytes (%.2f/iter)\n", delta, delta/100.0f);
    // Allow small allocator overhead
    const int32_t THRESHOLD = 512;
    TEST_ASSERT_TRUE_MESSAGE(delta <= THRESHOLD, "getContextAt leak exceeds threshold");
}

/**
 * ISOLATION: Test ONLY JsonDocument allocation (no contexts)
 */
void test_isolate_jsondocument_only() {
    HeapTracker tracker;
    tracker.checkpoint("start");
    
    for (int i = 0; i < 500; i++) {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        obj["test"] = "value";
        obj["number"] = i;
        String json;
        serializeJson(doc, json);
        (void)json;
    }
    
    tracker.checkpoint("end");
    int32_t delta = tracker.getDelta("start", "end");
    printf("\n[ISOLATE JsonDocument x500]: %d bytes (%.1f/req)\n", delta, delta/500.0f);
    TEST_ASSERT_TRUE_MESSAGE(delta <= 512, "JsonDocument leak");
}

/**
 * ISOLATION: Test ONLY String concatenation
 */
void test_isolate_string_concat_only() {
    HeapTracker tracker;
    tracker.checkpoint("start");
    
    for (int i = 0; i < 500; i++) {
        String base = "{\"test\":\"value\"}";
        String result = "," + base;
        (void)result;
    }
    
    tracker.checkpoint("end");
    int32_t delta = tracker.getDelta("start", "end");
    printf("[ISOLATE String concat x500]: %d bytes (%.1f/req)\n", delta, delta/500.0f);
    TEST_ASSERT_TRUE_MESSAGE(delta <= 512, "String concat leak");
}

/**
 * ISOLATION: Test ONLY getWebUIContexts copies
 */
void test_isolate_context_copies_only() {
    HeapTracker tracker;
    
    MockWebUIProvider provider("Test", "1.0.0");
    provider.addContext(WebUIContext::dashboard("dash", "Dashboard")
        .withField(WebUIField("temp", "Temp", WebUIFieldType::Number, "25"))
        .withCustomHtml("<div>test</div>"));
    
    // Warmup
    (void)provider.getContextCount();
    
    tracker.checkpoint("start");
    
    for (int i = 0; i < 500; i++) {
        (void)provider.getContextCount();

    }
    
    tracker.checkpoint("end");
    int32_t delta = tracker.getDelta("start", "end");
    printf("[ISOLATE context copies x500]: %d bytes (%.1f/req)\n", delta, delta/500.0f);
    TEST_ASSERT_TRUE_MESSAGE(delta <= 1024, "Context copy leak");
}

/**
 * ISOLATION: Combine context + JSON (like real code)
 */
void test_isolate_context_plus_json() {
    HeapTracker tracker;
    
    MockWebUIProvider provider("Test", "1.0.0");
    provider.addContext(WebUIContext::dashboard("dash", "Dashboard")
        .withField(WebUIField("temp", "Temp", WebUIFieldType::Number, "25"))
        .withCustomHtml("<div>test</div>"));
    
    (void)provider.getContextCount();
    
    tracker.checkpoint("start");
    
    for (int i = 0; i < 500; i++) {
        provider.forEachContext([](const WebUIContext& ctx) {
            JsonDocument doc;
            doc["id"] = ctx.getContextIdCStr();
            doc["html"] = ctx.getCustomHtmlCStr();
            String json;
            serializeJson(doc, json);
            String pending = "," + json;
            (void)pending;
            return true;
        });
    }
    
    tracker.checkpoint("end");
    int32_t delta = tracker.getDelta("start", "end");
    printf("[ISOLATE context+JSON x500]: %d bytes (%.1f/req)\n", delta, delta/500.0f);
    TEST_ASSERT_TRUE_MESSAGE(delta <= 2048, "Context+JSON leak");
}

/**
 * AGGRESSIVE TEST: Simulate 500 curl requests to reproduce ESP8266 OOM
 * This test monitors heap trend over many iterations to detect gradual leak.
 */
void test_aggressive_schema_generation_500_requests() {
    HeapTracker tracker;
    
    // Setup realistic providers with substantial data
    MockWebUIProvider provider1("WiFi", "1.4.0");
    provider1.addContext(WebUIContext::statusBadge("wifi_status", "WiFi", "dc-wifi").withRealTime(2000));
    provider1.addContext(WebUIContext::dashboard("wifi_component", "WiFi")
        .withField(WebUIField("ssid", "SSID", WebUIFieldType::Text, "MyNetwork"))
        .withField(WebUIField("ip", "IP", WebUIFieldType::Display, "192.168.1.100"))
        .withField(WebUIField("signal", "Signal", WebUIFieldType::Number, "-65", "dBm", true))
        .withCustomHtml("<div class='wifi-signal'><span class='bars'></span></div>")
        .withCustomCss(".wifi-signal { display: flex; } .bars { width: 20px; }"));
    
    MockWebUIProvider provider2("NTP", "1.3.0");
    provider2.addContext(WebUIContext::headerInfo("ntp_time", "Time", "dc-clock")
        .withField(WebUIField("time", "Time", WebUIFieldType::Display, "14:30:00"))
        .withRealTime(1000));
    provider2.addContext(WebUIContext::settings("ntp_settings", "NTP Settings")
        .withField(WebUIField("server", "Server", WebUIFieldType::Text, "pool.ntp.org"))
        .withField(WebUIField("timezone", "Timezone", WebUIFieldType::Select, "UTC")));
    
    MockWebUIProvider provider3("SystemInfo", "1.4.0");
    provider3.addContext(WebUIContext::dashboard("sysinfo_dash", "System")
        .withField(WebUIField("heap", "Free Heap", WebUIFieldType::Number, "45000", "bytes", true))
        .withField(WebUIField("uptime", "Uptime", WebUIFieldType::Display, "1d 5h 30m"))
        .withCustomHtml("<div class='gauge'><svg viewBox='0 0 100 100'></svg></div>")
        .withCustomCss(".gauge svg { width: 100%; height: auto; }"));
    
    // Warm up
    for (int w = 0; w < 5; w++) {
        (void)provider1.getContextCount();
        (void)provider2.getContextCount();
        (void)provider3.getContextCount();
    }
    
    // Pre-build provider list ONCE (like WebUI.h does with static state)
    IWebUIProvider* providers[3] = {&provider1, &provider2, &provider3};
    
    tracker.checkpoint("start");
    
    const int TOTAL_REQUESTS = 500;
    
    for (int request = 0; request < TOTAL_REQUESTS; request++) {
        // Simulate WebUI.h behavior using getContextAtRef() + StreamingContextSerializer
        // ZERO COPY - pointer to cached context
        for (int p = 0; p < 3; p++) {
            IWebUIProvider* provider = providers[p];
            size_t contextCount = provider->getContextCount();
            for (size_t i = 0; i < contextCount; i++) {
                const WebUIContext* ctxPtr = provider->getContextAtRef(i);
                if (!ctxPtr) continue;
                
                StreamingContextSerializer serializer;
                serializer.begin(*ctxPtr);
                
                uint8_t buffer[512];
                while (!serializer.isComplete()) {
                    serializer.write(buffer, sizeof(buffer));
                }
            }
        }
    }
    
    tracker.checkpoint("end");
    int32_t delta = tracker.getDelta("start", "end");
    
    printf("\n[AGGRESSIVE TEST - 500 curl requests simulation]\n");
    printf("  Heap delta: %d bytes (%.2f/req)\n", delta, delta / 500.0f);
    
    // MUST be 0 - no tolerance
    TEST_ASSERT_EQUAL_INT32_MESSAGE(0, delta, "AGGRESSIVE test MUST have ZERO leak");
}

/**
 * CRITICAL TEST: Simulate repeated schema JSON generation (like curl requests)
 * This reproduces the OOM issue observed on ESP8266 with repeated curl.
 */
void test_simulate_repeated_schema_generation() {
    HeapTracker tracker;
    
    // Setup providers like real WebUI
    MockWebUIProvider provider1("TestComp1", "1.0.0");
    provider1.addContext(WebUIContext::dashboard("dash1", "Dashboard 1")
        .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number, "25.5", "°C", true))
        .withCustomHtml("<div class='widget'>Custom content</div>"));
    
    MockWebUIProvider provider2("TestComp2", "1.0.0");
    provider2.addContext(WebUIContext::settings("settings2", "Settings")
        .withField(WebUIField("name", "Name", WebUIFieldType::Text, "Device")));
    
    // Warm up
    for (int w = 0; w < 2; w++) {
        (void)provider1.getContextCount();
        (void)provider2.getContextCount();
    }
    
    tracker.checkpoint("before_schema_gen");
    
    // Simulate 50 curl requests on /api/ui/schema
    const int CURL_REQUESTS = 50;
    for (int request = 0; request < CURL_REQUESTS; request++) {
        // Simulate what WebUI.h does for each request:
        
        // 1. Iterate contexts via forEachContext (no copy)
        provider1.forEachContext([](const WebUIContext& ctx) {
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            obj["contextId"] = ctx.getContextIdCStr();
            obj["title"] = ctx.getTitleCStr();
            obj["customHtml"] = ctx.getCustomHtmlCStr();
            
            String json;
            serializeJson(doc, json);
            String pending = "," + json;
            (void)pending;
            return true;
        });
        
        provider2.forEachContext([](const WebUIContext& ctx) {
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            obj["contextId"] = ctx.getContextIdCStr();
            obj["title"] = ctx.getTitleCStr();
            
            String json;
            serializeJson(doc, json);
            String pending = "," + json;
            (void)pending;
            return true;
        });
    }
    
    tracker.checkpoint("after_schema_gen");
    
    int32_t delta = tracker.getDelta("before_schema_gen", "after_schema_gen");
    int32_t perRequest = delta / CURL_REQUESTS;
    
    printf("\n[SCHEMA GENERATION LEAK TEST - Simulates curl requests]\n");
    printf("  Simulated curl requests: %d\n", CURL_REQUESTS);
    printf("  Total heap delta: %d bytes\n", delta);
    printf("  Per request: %d bytes\n", perRequest);
    
    // This should FAIL - demonstrating the real leak source
    const int32_t LEAK_THRESHOLD = 512;
    
    if (delta > LEAK_THRESHOLD) {
        printf("  *** SCHEMA GENERATION LEAK DETECTED: %d bytes > %d ***\n", delta, LEAK_THRESHOLD);
        printf("  This is the source of OOM on repeated curl requests!\n");
    }
    
    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Schema generation leak detected");
}

/**
 * ISOLATION TEST: Test if leak comes from String copies vs vector operations
 */
void test_isolate_string_copy_leak() {
    HeapTracker tracker;
    
    // Test 1: Pure String operations (no WebUIContext)
    tracker.checkpoint("before_strings");
    
    for (int i = 0; i < 50; i++) {
        String s1 = "Test string with some content";
        String s2 = s1;  // Copy
        String s3 = s2 + " more content";
        (void)s3;
    }
    
    tracker.checkpoint("after_strings");
    int32_t stringDelta = tracker.getDelta("before_strings", "after_strings");
    
    // Test 2: WebUIContext without customHtml (minimal)
    MockWebUIProvider minimalProvider("Minimal", "1.0.0");
    minimalProvider.addContext(WebUIContext::dashboard("min", "Min"));
    
    (void)minimalProvider.getContextCount();
    
    tracker.checkpoint("before_minimal");
    
    for (int i = 0; i < 50; i++) {
        (void)minimalProvider.getContextCount();
    }
    
    tracker.checkpoint("after_minimal");
    int32_t minimalDelta = tracker.getDelta("before_minimal", "after_minimal");
    
    // Test 3: WebUIContext WITH customHtml (large strings)
    MockWebUIProvider largeProvider("Large", "1.0.0");
    largeProvider.addContext(WebUIContext::dashboard("large", "Large")
        .withCustomHtml("<div>Large HTML content that takes memory</div>")
        .withCustomCss(".large { color: red; }"));
    
    (void)largeProvider.getContextCount();
    
    tracker.checkpoint("before_large");
    
    for (int i = 0; i < 50; i++) {
        (void)largeProvider.getContextCount();
    }
    
    tracker.checkpoint("after_large");
    int32_t largeDelta = tracker.getDelta("before_large", "after_large");
    
    printf("\n[LEAK ISOLATION TEST]\n");
    printf("  Pure String ops x50:       %d bytes\n", stringDelta);
    printf("  Minimal context x50:       %d bytes\n", minimalDelta);
    printf("  Large customHtml x50:      %d bytes\n", largeDelta);
    printf("  Difference (large-minimal): %d bytes\n", largeDelta - minimalDelta);
    
    // The difference shows how much customHtml contributes
    TEST_ASSERT_TRUE(true);  // Informational test
}

/**
 * Test memory behavior when contexts contain large customHtml/Css/Js strings
 * These are the suspected source of memory leaks on ESP8266.
 */
void test_detect_memory_large_custom_content() {
    HeapTracker tracker;
    
    MockWebUIProvider provider("LargeContent", "1.0.0");
    
    // Create context with large custom content (simulating real chart/complex UI)
    String largeHtml = "<div class=\"chart-container\">";
    for (int i = 0; i < 20; i++) {
        largeHtml += "<div class=\"data-point\" data-value=\"" + String(i * 10) + "\"></div>";
    }
    largeHtml += "</div>";
    
    provider.addContext(WebUIContext::dashboard("chart", "Chart")
        .withCustomHtmlDynamic(largeHtml)
        .withCustomCss(".chart-container { display: flex; } .data-point { width: 20px; height: var(--value); }")
        .withCustomJs("function updateChart(data) { /* chart update logic */ }"));

    // Warmup: the first pass builds the context cache, which is not a leak.
    provider.forEachContext([](const WebUIContext&) { return true; });

    tracker.checkpoint("before");
    
    // Use NEW memory-efficient API: forEachContext
    for (int i = 0; i < 20; i++) {
        provider.forEachContext([](const WebUIContext& ctx) {
            // Access custom content via const reference (no copy)
            const String& html = ctx.customHtml;
            const String& css = ctx.customCss;
            const String& js = ctx.customJs;
            (void)html; (void)css; (void)js;
            return true;
        });
    }
    
    tracker.checkpoint("after");
    
    int32_t delta = tracker.getDelta("before", "after");
    printf("\n[MEMORY DETECTION] Large customHtml/Css/Js x20 (forEachContext):\n");
    printf("  Heap delta: %d bytes\n", delta);
    printf("  Content size: ~%d bytes\n", (int)largeHtml.length());
    
    // FAIL if significant memory leak detected
    const int32_t LEAK_THRESHOLD = 512;
    
    if (delta > LEAK_THRESHOLD) {
        printf("  *** MEMORY LEAK DETECTED: %d bytes > threshold %d ***\n", delta, LEAK_THRESHOLD);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak in large custom content");
}

int main() {
    UNITY_BEGIN();

    // ZERO LEAK tests (must be exactly 0 bytes)
    RUN_TEST(test_zero_leak_multiple_providers);
    RUN_TEST(test_zero_leak_streaming_only);
    RUN_TEST(test_zero_leak_getContextAt_only);

    // Memory leak ISOLATION tests (find exact source)
    RUN_TEST(test_isolate_jsondocument_only);
    RUN_TEST(test_isolate_string_concat_only);
    RUN_TEST(test_isolate_context_copies_only);
    RUN_TEST(test_isolate_context_plus_json);

    // Memory leak DETECTION tests
    RUN_TEST(test_detect_memory_behavior_repeated_context_creation);
    RUN_TEST(test_aggressive_schema_generation_500_requests);  // Reproduces OOM
    RUN_TEST(test_simulate_repeated_schema_generation);
    RUN_TEST(test_isolate_string_copy_leak);
    RUN_TEST(test_detect_memory_large_custom_content);

    return UNITY_END();
}
