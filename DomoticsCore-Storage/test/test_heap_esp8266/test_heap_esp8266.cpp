/**
 * @file test_heap_esp8266.cpp
 * @brief ESP8266 hardware memory leak detection tests for Storage component.
 *
 * Runs on a board, against real LittleFS. The host suites cannot reach any of
 * this: the native backend is RAM-only, and a leak of a few dozen bytes per
 * operation is invisible on a machine with gigabytes.
 *
 * Every test asserts that the operation it measures actually happened before it
 * measures anything. StorageComponent rejects every put and get until it has
 * been opened, and a leak test run against rejected calls allocates nothing and
 * passes for the wrong reason — which is what this file did before it was
 * repaired.
 */

#include <unity.h>
#include <Arduino.h>
#include <DomoticsCore/Testing/HeapTracker.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Testing;
using namespace DomoticsCore::Components;

void setUp() {}
void tearDown() {}

// A Core with one open StorageComponent — the lifecycle the component ships
// with. Constructing it bare leaves it closed, and closed is inert.
struct OpenStorage {
    Core core;
    StorageComponent* storage = nullptr;

    explicit OpenStorage(const char* ns) {
        StorageConfig cfg;
        cfg.namespace_name = ns;
        auto ptr = std::make_unique<StorageComponent>(cfg);
        storage = ptr.get();
        core.addComponent(std::move(ptr));
        core.begin();
    }
};

// ============================================================================
// Baseline
// ============================================================================

void test_storage_heap_baseline() {
    HeapTracker tracker;

    uint32_t freeHeap = tracker.getFreeHeap();

    Serial.printf("\n[STORAGE HEAP BASELINE]\n");
    Serial.printf("  Free heap: %u bytes\n", freeHeap);

    TEST_ASSERT_TRUE(freeHeap > 0);
    TEST_ASSERT_TRUE(freeHeap < 82000);
}

// ============================================================================
// Repeated put / get / remove
// ============================================================================

void test_storage_repeated_operations() {
    HeapTracker tracker;
    OpenStorage s("heaptest");

    // Prove the component is open before measuring. A closed StorageComponent
    // returns false from every setter without allocating, so this assertion is
    // the difference between a leak test and a no-op.
    TEST_ASSERT_TRUE_MESSAGE(s.storage->putString("warmup", "value"),
                             "Storage did not open — the rest would measure nothing");

    // Warm up outside the measurement: the first put pulls in the LittleFS
    // buffers and the JSON document, a one-off cost that is not a leak.
    s.storage->getString("warmup");
    s.storage->remove("warmup");

    tracker.checkpoint("baseline");

    const int ITERATIONS = 20;
    for (int i = 0; i < ITERATIONS; i++) {
        String key = "key" + String(i);
        String value = "value" + String(i) + "_with_some_padding_data";

        s.storage->putString(key.c_str(), value);
        String read = s.storage->getString(key.c_str());
        s.storage->remove(key.c_str());

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

    const int32_t LEAK_THRESHOLD = 64;

    if (delta > LEAK_THRESHOLD) {
        Serial.printf("  *** MEMORY LEAK DETECTED: %d bytes > threshold %d ***\n", delta, LEAK_THRESHOLD);
    }

    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak detected in Storage operations");
}

// ============================================================================
// Single-key churn — is the growth per operation, or per distinct key?
// ============================================================================

void test_storage_single_key_churn() {
    // test_storage_repeated_operations uses 20 distinct keys and grows by about
    // 192 bytes each. That measurement alone cannot tell two very different
    // faults apart:
    //
    //   per operation   — every put/remove pair costs memory, and a device
    //                     rewriting one setting in a loop dies. Unbounded.
    //   per distinct key — the ArduinoJson pool grows to its high-water mark
    //                     and is reused afterwards. Bad, but it plateaus.
    //
    // Same operations, one key. Flat here means the second; growing means the
    // first. See STOR-ESP-1 in CODE-ROADMAP for what the answer turned out to be.
    HeapTracker tracker;
    OpenStorage s("churn");

    TEST_ASSERT_TRUE_MESSAGE(s.storage->putString("k", "warmup"),
                             "Storage did not open — the rest would measure nothing");
    s.storage->getString("k");
    s.storage->remove("k");

    tracker.checkpoint("baseline");

    const int ITERATIONS = 20;
    for (int i = 0; i < ITERATIONS; i++) {
        s.storage->putString("k", "value_with_some_padding_data");
        String read = s.storage->getString("k");
        s.storage->remove("k");
        yield();
    }

    tracker.checkpoint("after_churn");

    int32_t delta = tracker.getDelta("baseline", "after_churn");

    Serial.printf("\n[SINGLE KEY CHURN TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  Heap delta: %d bytes\n", delta);
    Serial.printf("  Per operation: %d bytes\n", delta / ITERATIONS);

    const int32_t LEAK_THRESHOLD = 64;

    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD,
                             "Rewriting a single key leaks — the growth is per operation, not per key");
}

// ============================================================================
// Overwrite without remove — the commonest real pattern of all
// ============================================================================

void test_storage_repeated_put_same_key() {
    // A device persisting a counter, a last-seen timestamp or a setpoint writes
    // the same key over and over and never removes it. If this grows, the fault
    // is in the put path; if it is flat, remove() is what accumulates. Either
    // way it halves the search for whoever fixes STOR-ESP-1.
    HeapTracker tracker;
    OpenStorage s("overwrite");

    TEST_ASSERT_TRUE_MESSAGE(s.storage->putString("counter", "0"),
                             "Storage did not open — the rest would measure nothing");

    tracker.checkpoint("baseline");

    const int ITERATIONS = 20;
    for (int i = 0; i < ITERATIONS; i++) {
        s.storage->putString("counter", String(i));
        yield();
    }

    tracker.checkpoint("after_writes");

    int32_t delta = tracker.getDelta("baseline", "after_writes");

    Serial.printf("\n[OVERWRITE SAME KEY TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  Heap delta: %d bytes\n", delta);
    Serial.printf("  Per write: %d bytes\n", delta / ITERATIONS);

    const int32_t LEAK_THRESHOLD = 64;

    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD,
                             "Overwriting one key leaks — the fault is in the put path");
}

// ============================================================================
// Namespace lifecycle
// ============================================================================

void test_storage_namespace_lifecycle() {
    // Replaces a test that called StorageComponent::setNamespace(). No such
    // method has ever existed — the file was committed against an API that was
    // never written, and nothing ever built it (see the ESP8266-suites lot).
    //
    // A namespace is fixed when the component is constructed, so the thing
    // worth measuring is not switching but the open-use-close cycle: each
    // namespace is one LittleFS file, and repeated cycles must not accumulate.
    HeapTracker tracker;

    // One cycle outside the measurement, for the same one-off costs as above.
    {
        OpenStorage warm("nswarm");
        TEST_ASSERT_TRUE_MESSAGE(warm.storage->putString("key", "value"),
                                 "Storage did not open — the rest would measure nothing");
    }

    tracker.checkpoint("baseline");

    // Five, not the ten the original used: each iteration leaves a JSON file
    // behind on LittleFS, and this runs on real flash.
    const int ITERATIONS = 5;
    for (int i = 0; i < ITERATIONS; i++) {
        String ns = "ns" + String(i);
        OpenStorage s(ns.c_str());
        s.storage->putString("key", "value");
        s.storage->getString("key");
        yield();
    }

    tracker.checkpoint("after_ns");

    int32_t delta = tracker.getDelta("baseline", "after_ns");

    Serial.printf("\n[NAMESPACE LIFECYCLE LEAK TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  Heap delta: %d bytes\n", delta);
    Serial.printf("  Per cycle: %d bytes\n", delta / ITERATIONS);

    const int32_t LEAK_THRESHOLD = 128;  // A full open/close cycle costs more than a put

    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak detected in namespace lifecycle");
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
    RUN_TEST(test_storage_single_key_churn);
    RUN_TEST(test_storage_repeated_put_same_key);
    RUN_TEST(test_storage_namespace_lifecycle);

    UNITY_END();
}

void loop() {}
