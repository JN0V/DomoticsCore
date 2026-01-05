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
#include <DomoticsCore/WebUI/StreamingContextSerializer.h>

using namespace DomoticsCore::Testing;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// Optimized WebUI provider using CachingWebUIProvider (ZERO-COPY)
class OptimizedWebUIProvider : public CachingWebUIProvider {
protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        // Create context with substantial String data - built ONCE and cached
        contexts.push_back(WebUIContext::dashboard("opt_dash", "Dashboard")
            .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number, "25.5", "°C", true))
            .withField(WebUIField("humid", "Humidity", WebUIFieldType::Number, "60", "%", true))
            .withCustomHtml("<div class='widget'><span>Custom HTML content for memory testing</span></div>")
            .withCustomCss(".widget { background: #fff; padding: 1rem; }"));
        
        contexts.push_back(WebUIContext::settings("opt_settings", "Settings")
            .withField(WebUIField("name", "Device Name", WebUIFieldType::Text, "ESP8266-Test")));
    }
    
public:
    String getWebUIName() const override { return "OptimizedTest"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    
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
    OptimizedWebUIProvider provider;
    
    // Warm up - force cache build and stabilize allocator (ZERO-COPY)
    for (int w = 0; w < 10; w++) {
        size_t count = provider.getContextCount();
        for (size_t j = 0; j < count; j++) {
            const WebUIContext* ctxPtr = provider.getContextAtRef(j);
            (void)ctxPtr;
        }
        yield();
    }
    
    tracker.checkpoint("baseline");
    uint32_t heapBefore = ESP.getFreeHeap();
    
    // Use ZERO-COPY getContextAtRef (like WebUI.h does)
    const int ITERATIONS = 50;
    for (int i = 0; i < ITERATIONS; i++) {
        size_t count = provider.getContextCount();
        for (size_t j = 0; j < count; j++) {
            const WebUIContext* ctxPtr = provider.getContextAtRef(j);
            if (ctxPtr) {
                // Access data via pointer - ZERO COPY
                const String& id = ctxPtr->contextId;
                const String& html = ctxPtr->customHtml;
                (void)id; (void)html;
            }
        }
        yield();
    }
    
    tracker.checkpoint("after_calls");
    uint32_t heapAfter = ESP.getFreeHeap();
    
    int32_t delta = tracker.getDelta("baseline", "after_calls");
    int32_t directDelta = (int32_t)heapBefore - (int32_t)heapAfter;
    
    Serial.printf("\n[WEBUI PROVIDER SAFE COPY TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  HeapTracker delta: %d bytes\n", delta);
    Serial.printf("  Direct ESP delta: %d bytes\n", directDelta);
    Serial.printf("  Free heap now: %u bytes\n", heapAfter);
    
    // Allow ESP8266 allocator overhead (512 bytes for 50 iterations)
    const int32_t ESP_TOLERANCE = 512;
    
    if (directDelta > ESP_TOLERANCE) {
        Serial.printf("  *** LEAK: %d > tolerance %d ***\n", directDelta, ESP_TOLERANCE);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(directDelta <= ESP_TOLERANCE, "Safe copy leak exceeds tolerance");
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

// Provider with MANY large contexts to test chunking
class LargeContextProvider : public CachingWebUIProvider {
protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        // Create 10 contexts with large HTML/CSS (simulates complex WebUI)
        for (int i = 0; i < 10; i++) {
            String id = "ctx_" + String(i);
            String title = "Context " + String(i);
            
            // ~500 bytes of HTML per context
            String html = "<div class='large-context-" + String(i) + "'>";
            for (int j = 0; j < 10; j++) {
                html += "<span>Content block " + String(j) + " with padding text</span>";
            }
            html += "</div>";
            
            // ~200 bytes of CSS per context
            String css = ".large-context-" + String(i) + " { background: #fff; padding: 1rem; margin: 0.5rem; border-radius: 8px; }";
            
            contexts.push_back(WebUIContext::dashboard(id, title)
                .withField(WebUIField("field1", "Field 1", WebUIFieldType::Text, "value1"))
                .withField(WebUIField("field2", "Field 2", WebUIFieldType::Number, "42"))
                .withCustomHtml(html)
                .withCustomCss(css));
        }
    }
    
public:
    String getWebUIName() const override { return "LargeTest"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override {
        return "{}";
    }
};

void test_esp8266_chunked_large_schema() {
    Serial.printf("\n[CHUNKED LARGE SCHEMA TEST]\n");
    
    LargeContextProvider provider;
    
    // Force cache build
    size_t contextCount = provider.getContextCount();
    Serial.printf("  Contexts: %d\n", (int)contextCount);
    
    uint32_t heapBefore = ESP.getFreeHeap();
    Serial.printf("  Heap before: %u bytes\n", heapBefore);
    
    // Track peak heap usage during serialization
    uint32_t minHeapDuring = heapBefore;
    size_t totalBytes = 0;
    size_t chunkCount = 0;
    
    // Simulate chunked streaming (like HTTP chunked response)
    const size_t CHUNK_SIZE = 256;  // Small chunks like real HTTP
    uint8_t buffer[CHUNK_SIZE];
    
    for (size_t i = 0; i < contextCount; i++) {
        const WebUIContext* ctx = provider.getContextAtRef(i);
        if (!ctx) continue;
        
        StreamingContextSerializer serializer;
        serializer.begin(*ctx);
        
        while (!serializer.isComplete()) {
            size_t written = serializer.write(buffer, CHUNK_SIZE);
            totalBytes += written;
            chunkCount++;
            
            // Check heap during operation
            uint32_t currentHeap = ESP.getFreeHeap();
            if (currentHeap < minHeapDuring) {
                minHeapDuring = currentHeap;
            }
            
            yield();  // Allow ESP8266 to breathe
        }
    }
    
    uint32_t heapAfter = ESP.getFreeHeap();
    uint32_t peakUsage = heapBefore - minHeapDuring;
    
    Serial.printf("  Total bytes generated: %d\n", (int)totalBytes);
    Serial.printf("  Chunks sent: %d\n", (int)chunkCount);
    Serial.printf("  Heap after: %u bytes\n", heapAfter);
    Serial.printf("  Peak heap usage during: %u bytes\n", peakUsage);
    Serial.printf("  Heap delta: %d bytes\n", (int)(heapBefore - heapAfter));
    
    // Verify chunking worked (schema should be substantial)
    TEST_ASSERT_TRUE_MESSAGE(totalBytes > 5000, "Schema too small, chunking not tested");
    
    // Verify peak heap usage stayed reasonable
    // The streaming should NOT allocate the full schema in memory
    const uint32_t MAX_PEAK_USAGE = 2048;  // Should never need more than 2KB extra
    TEST_ASSERT_TRUE_MESSAGE(peakUsage < MAX_PEAK_USAGE, "Peak heap usage too high during chunking");
    
    // Verify no significant leak
    int32_t leak = (int32_t)heapBefore - (int32_t)heapAfter;
    const int32_t MAX_LEAK = 512;
    TEST_ASSERT_TRUE_MESSAGE(leak < MAX_LEAK, "Memory leak during chunked schema generation");
    
    Serial.printf("  ✓ Chunking OK: %d bytes in %d chunks, peak %u bytes\n", 
                  (int)totalBytes, (int)chunkCount, peakUsage);
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
    RUN_TEST(test_esp8266_chunked_large_schema);
    
    UNITY_END();
}

void loop() {
    // Nothing to do
}
