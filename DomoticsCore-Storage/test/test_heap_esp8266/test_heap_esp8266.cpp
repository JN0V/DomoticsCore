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
 *
 * The measuring loops call core.loop(), and that is not incidental. Storage
 * emits storage/changed on every put and remove; EventBus queues each one at a
 * cost of roughly 122 bytes and releases it only when poll() dispatches it,
 * which firmware reaches through Core::loop() on every pass. A loop that omits
 * the call measures queue occupancy and reports it as a Storage leak — the
 * error that produced STOR-ESP-1, where 3,904 bytes over 20 undrained cycles
 * were read as 195 bytes leaked per operation. The last two tests hold that
 * behaviour in place deliberately: growth without a drain must plateau at the
 * queue cap, and draining must give the memory back.
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

        // Drain the EventBus, as firmware does every pass. Storage emits
        // storage/changed on every put and remove; without this call the queue
        // fills and its occupancy is charged to Storage as a leak.
        s.core.loop();
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

    // PlatformIO filters the serial stream down to Unity lines, so the figures
    // above never reach the test report. Carry them in the message instead: a
    // failure that names the number is comparable between candidates, and
    // "Expected TRUE Was FALSE" is not.
    char msg[160];
    snprintf(msg, sizeof(msg),
             "Memory leak in Storage operations: delta=%ld B over %d put/get/remove cycles "
             "(%ld B/cycle), threshold=%ld B",
             (long)delta, ITERATIONS, (long)perOp, (long)LEAK_THRESHOLD);

    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, msg);
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
        s.core.loop();  // drain the bus, as firmware does
        yield();
    }

    tracker.checkpoint("after_churn");

    int32_t delta = tracker.getDelta("baseline", "after_churn");

    Serial.printf("\n[SINGLE KEY CHURN TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  Heap delta: %d bytes\n", delta);
    Serial.printf("  Per operation: %d bytes\n", delta / ITERATIONS);

    const int32_t LEAK_THRESHOLD = 64;

    char msg[160];
    snprintf(msg, sizeof(msg),
             "Single-key churn leaks: delta=%ld B over %d cycles (%ld B/cycle), threshold=%ld B "
             "-- growth is per operation, not per key",
             (long)delta, ITERATIONS, (long)(delta / ITERATIONS), (long)LEAK_THRESHOLD);

    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, msg);
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
        s.core.loop();  // drain the bus, as firmware does
        yield();
    }

    tracker.checkpoint("after_writes");

    int32_t delta = tracker.getDelta("baseline", "after_writes");

    Serial.printf("\n[OVERWRITE SAME KEY TEST]\n");
    Serial.printf("  Iterations: %d\n", ITERATIONS);
    Serial.printf("  Heap delta: %d bytes\n", delta);
    Serial.printf("  Per write: %d bytes\n", delta / ITERATIONS);

    const int32_t LEAK_THRESHOLD = 64;

    char msg[160];
    snprintf(msg, sizeof(msg),
             "Overwriting one key leaks: delta=%ld B over %d writes (%ld B/write), threshold=%ld B "
             "-- the fault is in the put path",
             (long)delta, ITERATIONS, (long)(delta / ITERATIONS), (long)LEAK_THRESHOLD);

    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, msg);
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

    char msg[160];
    snprintf(msg, sizeof(msg),
             "Memory leak in namespace lifecycle: delta=%ld B over %d cycles (%ld B/cycle), threshold=%ld B",
             (long)delta, ITERATIONS, (long)(delta / ITERATIONS), (long)LEAK_THRESHOLD);

    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, msg);
}

// ============================================================================
// The undrained bus — what this suite once mistook for a Storage leak
// ============================================================================

// Writing without ever draining the bus DOES cost heap: every put queues a
// storage/changed event. What matters is that the cost stops. EventBus caps the
// queue at 32 and drops the oldest to make room, so occupancy reaches a ceiling
// and stays there.
//
// This is the test the suite was missing. Measuring 20 undrained writes and
// calling the result a per-operation leak is what produced STOR-ESP-1: 20 never
// reached the 32-entry ceiling, so bounded growth was indistinguishable from
// unbounded. Here the second half of the run is compared against the first — if
// the cap is ever removed, the plateau disappears and this fails.
void test_storage_undrained_writes_plateau() {
    HeapTracker tracker;
    OpenStorage s("undrained");

    TEST_ASSERT_TRUE_MESSAGE(s.storage->putString("counter", "0"),
                             "Storage did not open — the rest would measure nothing");

    // Start the window on an empty queue. begin() and the warm-up put have
    // already queued events; letting the measured half free them would offset
    // real growth by roughly the amount being measured.
    s.core.loop();
    yield();

    const int HALF = 40;  // 40 > 32, so the queue is already full at the midpoint

    tracker.checkpoint("baseline");
    for (int i = 0; i < HALF; i++) {
        s.storage->putString("counter", String(i));
        yield();
    }
    tracker.checkpoint("half");
    for (int i = 0; i < HALF; i++) {
        s.storage->putString("counter", String(i));
        yield();
    }
    tracker.checkpoint("full");

    int32_t firstHalf = tracker.getDelta("baseline", "half");
    int32_t secondHalf = tracker.getDelta("half", "full");

    Serial.printf("\n[UNDRAINED PLATEAU TEST]\n");
    Serial.printf("  First %d writes:  %d bytes\n", HALF, firstHalf);
    Serial.printf("  Second %d writes: %d bytes\n", HALF, secondHalf);

    // A plateau proves nothing unless the queue actually filled. Assert the
    // floor first: if events ever stop being queued -- say enqueue learns to
    // skip topics with no subscriber, and this suite subscribes to none -- both
    // halves fall to zero and the plateau assert below would pass while
    // measuring nothing. That is the failure this file exists to refuse.
    const int32_t OCCUPANCY_FLOOR = 1000;

    char floorMsg[192];
    snprintf(floorMsg, sizeof(floorMsg),
             "Queue never filled: first %d writes cost only %ld B, floor=%ld B "
             "-- nothing was queued, so this test measured nothing",
             HALF, (long)firstHalf, (long)OCCUPANCY_FLOOR);

    TEST_ASSERT_TRUE_MESSAGE(firstHalf >= OCCUPANCY_FLOOR, floorMsg);

    const int32_t PLATEAU_THRESHOLD = 64;

    char msg[192];
    snprintf(msg, sizeof(msg),
             "Undrained queue did not plateau: first %d writes cost %ld B, next %d cost %ld B "
             "(threshold=%ld B) -- the queue cap is gone and growth is now unbounded",
             HALF, (long)firstHalf, HALF, (long)secondHalf, (long)PLATEAU_THRESHOLD);

    TEST_ASSERT_TRUE_MESSAGE(secondHalf <= PLATEAU_THRESHOLD, msg);
}

// The other half of the same claim: queue occupancy is held memory, not lost
// memory. If this fails while the plateau test passes, the events really are
// leaking.
//
// Measured differentially, over two identical fill-and-drain cycles. A single
// cycle does not come back to zero — it leaves ~128 B behind, and that residue
// is one-time rather than per-event: EventBus keeps the topic's entry in its
// pending-by-topic map (poll() decrements the counter but never erases the
// entry), and HeapTracker charges each checkpoint's own map node to the window
// that follows it. Widening the threshold to swallow that would blind the test
// to a real 128 B leak. Comparing two cycles does not: a genuine loss recurs
// every cycle, a one-time allocation is already paid by the first.
void test_storage_drain_reclaims_queue_memory() {
    HeapTracker tracker;
    OpenStorage s("reclaim");

    TEST_ASSERT_TRUE_MESSAGE(s.storage->putString("counter", "0"),
                             "Storage did not open — the rest would measure nothing");

    const int WRITES = 40;  // 40 > the queue cap, so the queue reaches its ceiling

    // Cycle 1 — absorbs every one-time cost, and is not measured.
    for (int i = 0; i < WRITES; i++) {
        s.storage->putString("counter", String(i));
        yield();
    }
    for (int i = 0; i < 10; i++) {
        s.core.loop();
        yield();
    }

    tracker.checkpoint("baseline");
    const uint32_t heapAtBaseline = ESP.getFreeHeap();

    // Cycle 2 — identical, and measured.
    for (int i = 0; i < WRITES; i++) {
        s.storage->putString("counter", String(i));
        yield();
    }

    // Read the queued occupancy straight from the heap rather than through a
    // checkpoint: a third checkpoint would add a tracker map node to the very
    // window being measured.
    const uint32_t heapWhileQueued = ESP.getFreeHeap();
    const int32_t held = (int32_t)heapAtBaseline - (int32_t)heapWhileQueued;

    // 40 writes were issued but the cap holds fewer, so at most a capful of
    // events is waiting. poll() dispatches 8 per call; ten passes is headroom.
    for (int i = 0; i < 10; i++) {
        s.core.loop();
        yield();
    }
    tracker.checkpoint("drained");

    const int32_t after = tracker.getDelta("baseline", "drained");

    Serial.printf("\n[DRAIN RECLAIM TEST]\n");
    Serial.printf("  Held while queued (cycle 2): %d bytes\n", held);
    Serial.printf("  Left after draining:         %d bytes\n", after);

    // Floor first: reclaiming nothing from an empty queue is not evidence of
    // reclamation. If events ever stop being queued, `held` collapses and the
    // assertion below would pass while measuring nothing.
    const int32_t OCCUPANCY_FLOOR = 1000;

    char floorMsg[192];
    snprintf(floorMsg, sizeof(floorMsg),
             "Nothing to reclaim: %d writes held only %ld B, floor=%ld B "
             "-- the queue never filled, so this test measured nothing",
             WRITES, (long)held, (long)OCCUPANCY_FLOOR);

    TEST_ASSERT_TRUE_MESSAGE(held >= OCCUPANCY_FLOOR, floorMsg);

    const int32_t LEAK_THRESHOLD = 64;

    char msg[224];
    snprintf(msg, sizeof(msg),
             "Draining did not reclaim the queue: a second identical cycle held %ld B and gave "
             "back all but %ld B (threshold=%ld B) -- the loss recurs per cycle, so it is a leak, "
             "not a one-time allocation",
             (long)held, (long)after, (long)LEAK_THRESHOLD);

    TEST_ASSERT_TRUE_MESSAGE(after <= LEAK_THRESHOLD, msg);
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
    RUN_TEST(test_storage_undrained_writes_plateau);
    RUN_TEST(test_storage_drain_reclaims_queue_memory);

    UNITY_END();
}

void loop() {}
