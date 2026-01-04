/**
 * @file test_heap_esp8266.cpp
 * @brief ESP8266 hardware memory leak detection tests for Storage component
 */

#include <unity.h>
#include <Arduino.h>
#include <DomoticsCore/Testing/HeapTracker.h>
#include <DomoticsCore/Storage.h>

using namespace DomoticsCore::Testing;
using namespace DomoticsCore::Components;

// Mock storage backend for testing
class TestStorage : public StorageComponent {
public:
    TestStorage() : StorageComponent() {}
};

void setUp() {}
void tearDown() {}

// ============================================================================
// Storage ESP8266 Memory Leak Tests
// ============================================================================

void test_storage_heap_baseline() {
    HeapTracker tracker;
    
    uint32_t freeHeap = tracker.getFreeHeap();
    
    Serial.printf("\n[STORAGE HEAP BASELINE]\n");
    Serial.printf("  Free heap: %u bytes\n", freeHeap);
    
    TEST_ASSERT_TRUE(freeHeap > 0);
    TEST_ASSERT_TRUE(freeHeap < 82000);
}

void test_storage_repeated_operations() {
    HeapTracker tracker;
    TestStorage storage;
    
    // Warm up
    storage.putString("warmup", "value");
    storage.getString("warmup");
    storage.remove("warmup");
    
    tracker.checkpoint("baseline");
    
    // Repeated storage operations
    const int ITERATIONS = 20;
    for (int i = 0; i < ITERATIONS; i++) {
        String key = "key" + String(i);
        String value = "value" + String(i) + "_with_some_padding_data";
        
        storage.putString(key.c_str(), value);
        String read = storage.getString(key.c_str());
        storage.remove(key.c_str());
        
        yield();
    }
    
    tracker.checkpoint("after_ops");
    
    int32_t delta = tracker.getDelta("baseline", "after_ops");
    int32_t perOp = delta / ITERATIONS;
    
    Serial.printf("\n[STORAGE OPERATIONS LEAK TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  Total heap delta: %d bytes\n", delta);
    Serial.printf("  Per operation: %d bytes\n", perOp);
    Serial.printf("  Free heap now: %u bytes\n", ESP.getFreeHeap());
    
    // FAIL if significant memory leak detected
    const int32_t LEAK_THRESHOLD = 64;
    
    if (delta > LEAK_THRESHOLD) {
        Serial.printf("  *** MEMORY LEAK DETECTED: %d bytes > threshold %d ***\n", delta, LEAK_THRESHOLD);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak detected in Storage operations");
}

void test_storage_namespace_switching() {
    HeapTracker tracker;
    TestStorage storage;
    
    tracker.checkpoint("baseline");
    
    // Switch namespaces many times
    const int ITERATIONS = 10;
    for (int i = 0; i < ITERATIONS; i++) {
        String ns = "namespace" + String(i);
        storage.setNamespace(ns);
        storage.putString("key", "value");
        storage.getString("key");
        yield();
    }
    
    tracker.checkpoint("after_ns");
    
    int32_t delta = tracker.getDelta("baseline", "after_ns");
    
    Serial.printf("\n[NAMESPACE SWITCHING LEAK TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  Heap delta: %d bytes\n", delta);
    
    const int32_t LEAK_THRESHOLD = 128;  // Namespaces may use more memory
    
    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak detected in namespace switching");
}

// ============================================================================
// Test Runner
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n========================================");
    Serial.println("Storage ESP8266 Memory Leak Tests");
    Serial.println("========================================\n");
    
    UNITY_BEGIN();
    
    RUN_TEST(test_storage_heap_baseline);
    RUN_TEST(test_storage_repeated_operations);
    RUN_TEST(test_storage_namespace_switching);
    
    UNITY_END();
}

void loop() {}
