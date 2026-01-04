/**
 * @file test_heap_tracker.cpp
 * @brief Unit tests for HeapTracker memory leak detection
 * 
 * Tests:
 * - Checkpoint creation and retrieval
 * - Delta calculation between checkpoints
 * - Leak rate calculation
 * - Stability assertions
 * - No-growth assertions
 * - JSON output generation
 */

#include <unity.h>
#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Testing/HeapTracker.h>
#include <cmath>
#include <thread>
#include <chrono>

using namespace DomoticsCore::Testing;

// Test fixture
static HeapTracker* tracker = nullptr;

void setUp() {
    tracker = new HeapTracker();
}

void tearDown() {
    delete tracker;
    tracker = nullptr;
}

// ============================================================================
// Checkpoint Tests
// ============================================================================

void test_checkpoint_creation() {
    tracker->checkpoint("test1");
    TEST_ASSERT_TRUE(tracker->hasCheckpoint("test1"));
    TEST_ASSERT_EQUAL(1, tracker->getCheckpointCount());
}

void test_checkpoint_retrieval() {
    tracker->checkpoint("before");
    HeapSnapshot snap = tracker->getCheckpoint("before");
    
    TEST_ASSERT_TRUE(snap.freeHeap > 0);
    TEST_ASSERT_TRUE(snap.timestamp > 0);
}

void test_multiple_checkpoints() {
    tracker->checkpoint("cp1");
    tracker->checkpoint("cp2");
    tracker->checkpoint("cp3");
    
    TEST_ASSERT_EQUAL(3, tracker->getCheckpointCount());
    TEST_ASSERT_TRUE(tracker->hasCheckpoint("cp1"));
    TEST_ASSERT_TRUE(tracker->hasCheckpoint("cp2"));
    TEST_ASSERT_TRUE(tracker->hasCheckpoint("cp3"));
}

void test_nonexistent_checkpoint() {
    TEST_ASSERT_FALSE(tracker->hasCheckpoint("nonexistent"));
    HeapSnapshot snap = tracker->getCheckpoint("nonexistent");
    TEST_ASSERT_EQUAL(0, snap.freeHeap);
}

void test_clear_checkpoints() {
    tracker->checkpoint("cp1");
    tracker->checkpoint("cp2");
    TEST_ASSERT_EQUAL(2, tracker->getCheckpointCount());
    
    tracker->clear();
    TEST_ASSERT_EQUAL(0, tracker->getCheckpointCount());
}

// ============================================================================
// Delta Calculation Tests
// ============================================================================

void test_delta_same_checkpoint() {
    tracker->checkpoint("same");
    int32_t delta = tracker->getDelta("same", "same");
    TEST_ASSERT_EQUAL(0, delta);
}

void test_delta_calculation() {
    tracker->checkpoint("start");
    tracker->checkpoint("end");
    
    int32_t delta = tracker->getDelta("start", "end");
    // Delta should be small (timestamps differ but heap is simulated same)
    TEST_ASSERT_TRUE(delta >= -100 && delta <= 100);
}

// ============================================================================
// Stability Assertion Tests
// ============================================================================

void test_assert_stable_pass() {
    tracker->checkpoint("before");
    tracker->checkpoint("after");
    
    MemoryTestResult result = tracker->assertStable("before", "after", 100);
    TEST_ASSERT_TRUE(result.passed);
}

void test_assert_stable_with_tolerance() {
    tracker->checkpoint("start");
    tracker->checkpoint("end");
    
    MemoryTestResult result = tracker->assertStable("start", "end", 1000);
    TEST_ASSERT_TRUE(result.passed);
    TEST_ASSERT_TRUE(result.message.length() > 0);
}

void test_assert_stable_missing_start() {
    tracker->checkpoint("end");
    
    MemoryTestResult result = tracker->assertStable("missing", "end", 100);
    TEST_ASSERT_FALSE(result.passed);
    TEST_ASSERT_TRUE(result.message.indexOf("not found") >= 0);
}

void test_assert_stable_missing_end() {
    tracker->checkpoint("start");
    
    MemoryTestResult result = tracker->assertStable("start", "missing", 100);
    TEST_ASSERT_FALSE(result.passed);
    TEST_ASSERT_TRUE(result.message.indexOf("not found") >= 0);
}

// ============================================================================
// No-Growth Assertion Tests
// ============================================================================

void test_assert_no_growth_pass() {
    tracker->checkpoint("baseline");
    
    MemoryTestResult result = tracker->assertNoGrowth("baseline", 100);
    TEST_ASSERT_TRUE(result.passed);
}

void test_assert_no_growth_missing_checkpoint() {
    MemoryTestResult result = tracker->assertNoGrowth("nonexistent", 100);
    TEST_ASSERT_FALSE(result.passed);
}

// ============================================================================
// Leak Rate Tests
// ============================================================================

void test_leak_rate_zero_duration() {
    tracker->checkpoint("instant");
    float rate = tracker->getLeakRate("instant", "instant");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rate);
}

void test_leak_rate_calculation() {
    tracker->checkpoint("t0");
    // Small delay to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    tracker->checkpoint("t1");
    
    float rate = tracker->getLeakRate("t0", "t1");
    // Rate should be a finite number
    TEST_ASSERT_FALSE(std::isnan(rate));
    TEST_ASSERT_FALSE(std::isinf(rate));
}

// ============================================================================
// Heap Snapshot Tests
// ============================================================================

void test_snapshot_fragmentation_zero() {
    HeapSnapshot snap;
    snap.freeHeap = 1000;
    snap.largestFreeBlock = 1000;  // No fragmentation
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, snap.getFragmentation());
}

void test_snapshot_fragmentation_fifty_percent() {
    HeapSnapshot snap;
    snap.freeHeap = 1000;
    snap.largestFreeBlock = 500;  // 50% fragmentation
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, snap.getFragmentation());
}

void test_snapshot_fragmentation_empty() {
    HeapSnapshot snap;
    snap.freeHeap = 0;
    snap.largestFreeBlock = 0;
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, snap.getFragmentation());
}

// ============================================================================
// JSON Output Tests
// ============================================================================

void test_json_empty() {
    String json = tracker->toJson();
    TEST_ASSERT_TRUE(json.indexOf("checkpoints") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("[]") >= 0);
}

void test_json_with_checkpoints() {
    tracker->checkpoint("test1");
    tracker->checkpoint("test2");
    
    String json = tracker->toJson();
    TEST_ASSERT_TRUE(json.indexOf("test1") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("test2") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("freeHeap") >= 0);
}

// ============================================================================
// Convenience Method Tests
// ============================================================================

void test_get_free_heap() {
    uint32_t freeHeap = tracker->getFreeHeap();
    TEST_ASSERT_TRUE(freeHeap > 0);
}

// ============================================================================
// REAL Heap Tracking Verification Tests
// ============================================================================

/**
 * Test that verifies REAL memory tracking works.
 * Allocates memory and checks that the heap delta reflects actual allocation.
 */
void test_real_heap_tracking_detects_allocation() {
    HeapTracker localTracker;
    
    localTracker.checkpoint("before_alloc");
    
    // Allocate a significant chunk of memory
    const size_t ALLOC_SIZE = 64 * 1024;  // 64KB
    char* leak = new char[ALLOC_SIZE];
    memset(leak, 'X', ALLOC_SIZE);  // Touch memory to ensure it's allocated
    
    localTracker.checkpoint("after_alloc");
    
    int32_t delta = localTracker.getDelta("before_alloc", "after_alloc");
    
    // Print for debugging - this shows the REAL heap tracking
    printf("\n[REAL HEAP TEST] Allocated %zu bytes\n", ALLOC_SIZE);
    printf("  Heap delta detected: %d bytes\n", delta);
    printf("  Expected: ~%zu bytes (negative = heap grew)\n", ALLOC_SIZE);
    
    // Delta should be significant (negative means heap used increased)
    // On Linux with mallinfo, we should see the allocation
    // Note: delta is (before.freeHeap - after.freeHeap), so positive = leak
    TEST_ASSERT_TRUE(delta != 0);  // Must detect SOMETHING
    
    // Clean up
    delete[] leak;
    
    localTracker.checkpoint("after_free");
    int32_t deltaAfterFree = localTracker.getDelta("before_alloc", "after_free");
    printf("  Delta after free: %d bytes\n", deltaAfterFree);
}

/**
 * Test that detects a REAL memory leak (intentionally don't free)
 */
void test_real_heap_tracking_detects_leak() {
    HeapTracker localTracker;
    
    localTracker.checkpoint("baseline");
    
    // Create an intentional leak
    std::vector<char*> leaks;
    for (int i = 0; i < 10; i++) {
        char* leak = new char[1024];  // 1KB each
        memset(leak, 'L', 1024);
        leaks.push_back(leak);
    }
    
    localTracker.checkpoint("after_leaks");
    
    int32_t delta = localTracker.getDelta("baseline", "after_leaks");
    
    printf("\n[LEAK DETECTION TEST] Created 10KB of intentional leaks\n");
    printf("  Heap delta: %d bytes\n", delta);
    
    // Should detect the ~10KB leak
    TEST_ASSERT_TRUE(delta > 5000);  // At least 5KB detected
    
    // Clean up (in real code, this would be the leak)
    for (auto ptr : leaks) {
        delete[] ptr;
    }
}

// ============================================================================
// Memory Test Result Tests
// ============================================================================

void test_memory_result_bool_conversion() {
    MemoryTestResult passed;
    passed.passed = true;
    TEST_ASSERT_TRUE(passed);
    
    MemoryTestResult failed;
    failed.passed = false;
    TEST_ASSERT_FALSE(failed);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Checkpoint tests
    RUN_TEST(test_checkpoint_creation);
    RUN_TEST(test_checkpoint_retrieval);
    RUN_TEST(test_multiple_checkpoints);
    RUN_TEST(test_nonexistent_checkpoint);
    RUN_TEST(test_clear_checkpoints);
    
    // Delta tests
    RUN_TEST(test_delta_same_checkpoint);
    RUN_TEST(test_delta_calculation);
    
    // Stability assertion tests
    RUN_TEST(test_assert_stable_pass);
    RUN_TEST(test_assert_stable_with_tolerance);
    RUN_TEST(test_assert_stable_missing_start);
    RUN_TEST(test_assert_stable_missing_end);
    
    // No-growth tests
    RUN_TEST(test_assert_no_growth_pass);
    RUN_TEST(test_assert_no_growth_missing_checkpoint);
    
    // Leak rate tests
    RUN_TEST(test_leak_rate_zero_duration);
    RUN_TEST(test_leak_rate_calculation);
    
    // Snapshot tests
    RUN_TEST(test_snapshot_fragmentation_zero);
    RUN_TEST(test_snapshot_fragmentation_fifty_percent);
    RUN_TEST(test_snapshot_fragmentation_empty);
    
    // JSON tests
    RUN_TEST(test_json_empty);
    RUN_TEST(test_json_with_checkpoints);
    
    // Convenience tests
    RUN_TEST(test_get_free_heap);
    
    // REAL heap tracking verification tests
    RUN_TEST(test_real_heap_tracking_detects_allocation);
    RUN_TEST(test_real_heap_tracking_detects_leak);
    
    // Result tests
    RUN_TEST(test_memory_result_bool_conversion);
    
    return UNITY_END();
}
