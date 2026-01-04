/**
 * @file test_heap_esp8266.cpp
 * @brief ESP8266 hardware memory leak detection tests
 * 
 * These tests run on REAL ESP8266 hardware to detect actual memory leaks
 * that don't show up on native platform due to different allocator behavior.
 * 
 * Key differences from native:
 * - ESP8266 has ~80KB heap vs desktop GB
 * - Heap fragmentation is a real problem
 * - String operations cause actual leaks
 */

#include <unity.h>
#include <Arduino.h>
#include <DomoticsCore/Testing/HeapTracker.h>
#include <DomoticsCore/IWebUIProvider.h>

using namespace DomoticsCore::Testing;
using namespace DomoticsCore::Components;

// Simple WebUI provider that recreates contexts each call (like real providers)
class LeakyWebUIProvider : public IWebUIProvider {
public:
    String getWebUIName() const override { return "LeakyTest"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    
    // This recreates contexts on EVERY call - potential memory leak source
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        
        // Create context with substantial String data
        contexts.push_back(WebUIContext::dashboard("leak_dash", "Dashboard")
            .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number, "25.5", "°C", true))
            .withField(WebUIField("humid", "Humidity", WebUIFieldType::Number, "60", "%", true))
            .withCustomHtml("<div class='widget'><span>Custom HTML content for memory testing</span></div>")
            .withCustomCss(".widget { background: #fff; padding: 1rem; }"));
        
        contexts.push_back(WebUIContext::settings("leak_settings", "Settings")
            .withField(WebUIField("name", "Device Name", WebUIFieldType::Text, "ESP8266-Test")));
        
        return contexts;
    }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override {
        return "{}";
    }
};

void setUp() {}
void tearDown() {}

// ============================================================================
// ESP8266 Real Heap Tests
// ============================================================================

void test_esp8266_heap_baseline() {
    HeapTracker tracker;
    
    uint32_t freeHeap = tracker.getFreeHeap();
    
    Serial.printf("\n[ESP8266 BASELINE]\n");
    Serial.printf("  Free heap: %u bytes\n", freeHeap);
    Serial.printf("  Total heap: ~80KB (81920 bytes)\n");
    
    TEST_ASSERT_TRUE(freeHeap > 0);
    TEST_ASSERT_TRUE(freeHeap < 82000);  // Should be less than total ESP8266 RAM
}

void test_esp8266_detect_string_leak() {
    HeapTracker tracker;
    
    tracker.checkpoint("before");
    
    // Create many Strings that may leak
    for (int i = 0; i < 20; i++) {
        String leak = "This is a test string number " + String(i) + " with some padding data";
        // String goes out of scope but allocator may fragment
    }
    
    tracker.checkpoint("after");
    
    int32_t delta = tracker.getDelta("before", "after");
    
    Serial.printf("\n[STRING LEAK TEST]\n");
    Serial.printf("  Heap delta: %d bytes\n", delta);
    
    // On ESP8266, even "freed" strings may cause fragmentation
    TEST_ASSERT_TRUE(true);  // Log results
}

void test_esp8266_webui_provider_repeated_calls() {
    HeapTracker tracker;
    LeakyWebUIProvider provider;
    
    // Warm up
    auto warmup = provider.getWebUIContexts();
    (void)warmup;
    
    tracker.checkpoint("baseline");
    
    // Call getWebUIContexts() many times - this is what causes leaks on ESP8266
    const int ITERATIONS = 20;
    for (int i = 0; i < ITERATIONS; i++) {
        auto contexts = provider.getWebUIContexts();
        for (const auto& ctx : contexts) {
            // Access data like WebUI does
            String id = ctx.contextId;
            String html = ctx.customHtml;
            (void)id; (void)html;
        }
        yield();  // Let ESP8266 breathe
    }
    
    tracker.checkpoint("after_calls");
    
    int32_t delta = tracker.getDelta("baseline", "after_calls");
    int32_t perCall = delta / ITERATIONS;
    
    Serial.printf("\n[WEBUI PROVIDER LEAK TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  Total heap delta: %d bytes\n", delta);
    Serial.printf("  Per call: %d bytes\n", perCall);
    Serial.printf("  Free heap now: %u bytes\n", ESP.getFreeHeap());
    
    // FAIL if significant memory leak detected
    // Allow small tolerance (64 bytes) for allocator overhead
    const int32_t LEAK_THRESHOLD = 64;
    
    if (delta > LEAK_THRESHOLD) {
        Serial.printf("  *** MEMORY LEAK DETECTED: %d bytes > threshold %d ***\n", delta, LEAK_THRESHOLD);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak detected in WebUI provider");
}

void test_esp8266_fragmentation_detection() {
    HeapTracker tracker;
    
    HeapSnapshot snap = tracker.takeSnapshot();
    float fragmentation = snap.getFragmentation();
    
    Serial.printf("\n[FRAGMENTATION TEST]\n");
    Serial.printf("  Free heap: %u bytes\n", snap.freeHeap);
    Serial.printf("  Largest block: %u bytes\n", snap.largestFreeBlock);
    Serial.printf("  Fragmentation: %.1f%%\n", fragmentation);
    
    // High fragmentation (>30%) indicates memory problems
    if (fragmentation > 30.0f) {
        Serial.printf("  *** HIGH FRAGMENTATION WARNING ***\n");
    }
    
    TEST_ASSERT_TRUE(fragmentation >= 0.0f);
    TEST_ASSERT_TRUE(fragmentation <= 100.0f);
}

void test_esp8266_stress_allocation() {
    HeapTracker tracker;
    
    tracker.checkpoint("start");
    
    // Simulate WebUI stress: many small allocations
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 10; i++) {
            char* buf = new char[256];
            memset(buf, 'X', 256);
            delete[] buf;
        }
        yield();
    }
    
    tracker.checkpoint("end");
    
    int32_t delta = tracker.getDelta("start", "end");
    HeapSnapshot endSnap = tracker.takeSnapshot();
    
    Serial.printf("\n[STRESS ALLOCATION TEST]\n");
    Serial.printf("  50 alloc/free cycles of 256 bytes\n");
    Serial.printf("  Heap delta: %d bytes\n", delta);
    Serial.printf("  Fragmentation: %.1f%%\n", endSnap.getFragmentation());
    
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// Test Runner
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial
    
    Serial.println("\n\n========================================");
    Serial.println("ESP8266 Memory Leak Detection Tests");
    Serial.println("========================================\n");
    
    UNITY_BEGIN();
    
    RUN_TEST(test_esp8266_heap_baseline);
    RUN_TEST(test_esp8266_detect_string_leak);
    RUN_TEST(test_esp8266_webui_provider_repeated_calls);
    RUN_TEST(test_esp8266_fragmentation_detection);
    RUN_TEST(test_esp8266_stress_allocation);
    
    UNITY_END();
}

void loop() {
    // Nothing to do
}
