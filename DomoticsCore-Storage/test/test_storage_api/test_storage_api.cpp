// Unit tests for Storage API (IStorage interface)
// Uses RAMOnlyStorage stub for native testing
// Includes HeapTracker integration for memory leak detection

#include <DomoticsCore/Storage_HAL.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/Testing/HeapTracker.h>
#include <unity.h>

using namespace DomoticsCore::HAL;
using namespace DomoticsCore::Testing;

// Global storage instance for tests
PlatformStorage storage;

void setUp(void) {
    storage.begin("test_ns");
    storage.clear();
}

void tearDown(void) {
    storage.end();
}

// ============================================================================
// String Tests
// ============================================================================

void test_put_get_string(void) {
    TEST_ASSERT_TRUE(storage.putString("key1", "value1"));
    TEST_ASSERT_EQUAL_STRING("value1", storage.getString("key1").c_str());
}

void test_get_string_default(void) {
    TEST_ASSERT_EQUAL_STRING("default", storage.getString("nonexistent", "default").c_str());
}

void test_overwrite_string(void) {
    storage.putString("key1", "value1");
    storage.putString("key1", "value2");
    TEST_ASSERT_EQUAL_STRING("value2", storage.getString("key1").c_str());
}

// ============================================================================
// Integer Tests
// ============================================================================

void test_put_get_int(void) {
    TEST_ASSERT_TRUE(storage.putInt("int_key", 42));
    TEST_ASSERT_EQUAL_INT32(42, storage.getInt("int_key"));
}

void test_get_int_default(void) {
    TEST_ASSERT_EQUAL_INT32(-1, storage.getInt("nonexistent", -1));
}

void test_put_get_negative_int(void) {
    storage.putInt("neg", -123);
    TEST_ASSERT_EQUAL_INT32(-123, storage.getInt("neg"));
}

// ============================================================================
// Boolean Tests
// ============================================================================

void test_put_get_bool_true(void) {
    TEST_ASSERT_TRUE(storage.putBool("bool_key", true));
    TEST_ASSERT_TRUE(storage.getBool("bool_key"));
}

void test_put_get_bool_false(void) {
    storage.putBool("bool_key", false);
    TEST_ASSERT_FALSE(storage.getBool("bool_key"));
}

void test_get_bool_default(void) {
    TEST_ASSERT_TRUE(storage.getBool("nonexistent", true));
    TEST_ASSERT_FALSE(storage.getBool("nonexistent", false));
}

// ============================================================================
// Float Tests
// ============================================================================

void test_put_get_float(void) {
    TEST_ASSERT_TRUE(storage.putFloat("float_key", 3.14159f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14159f, storage.getFloat("float_key"));
}

void test_get_float_default(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, storage.getFloat("nonexistent", 1.5f));
}

// ============================================================================
// Key Management Tests
// ============================================================================

void test_is_key_exists(void) {
    storage.putString("exists", "value");
    TEST_ASSERT_TRUE(storage.isKey("exists"));
}

void test_is_key_not_exists(void) {
    TEST_ASSERT_FALSE(storage.isKey("not_exists"));
}

void test_remove_key(void) {
    storage.putString("to_remove", "value");
    TEST_ASSERT_TRUE(storage.isKey("to_remove"));
    TEST_ASSERT_TRUE(storage.remove("to_remove"));
    TEST_ASSERT_FALSE(storage.isKey("to_remove"));
}

void test_remove_nonexistent(void) {
    TEST_ASSERT_FALSE(storage.remove("nonexistent"));
}

void test_clear(void) {
    storage.putString("key1", "v1");
    storage.putString("key2", "v2");
    storage.putInt("key3", 3);
    TEST_ASSERT_TRUE(storage.clear());
    TEST_ASSERT_FALSE(storage.isKey("key1"));
    TEST_ASSERT_FALSE(storage.isKey("key2"));
    TEST_ASSERT_FALSE(storage.isKey("key3"));
}

// ============================================================================
// Multiple Types Test
// ============================================================================

void test_multiple_types_same_namespace(void) {
    storage.putString("str", "hello");
    storage.putInt("num", 42);
    storage.putBool("flag", true);
    storage.putFloat("pi", 3.14f);

    TEST_ASSERT_EQUAL_STRING("hello", storage.getString("str").c_str());
    TEST_ASSERT_EQUAL_INT32(42, storage.getInt("num"));
    TEST_ASSERT_TRUE(storage.getBool("flag"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.14f, storage.getFloat("pi"));
}

// ============================================================================
// Namespace Isolation Tests
// ============================================================================

void test_namespace_isolation(void) {
    // Use a separate storage instance for ns1
    PlatformStorage storage1;
    storage1.begin("namespace_alpha");
    storage1.putString("shared_key", "value_from_alpha");

    // Use another instance for ns2
    PlatformStorage storage2;
    storage2.begin("namespace_beta");
    storage2.putString("shared_key", "value_from_beta");

    // Read back - should get namespace-specific values
    TEST_ASSERT_EQUAL_STRING("value_from_alpha", storage1.getString("shared_key").c_str());
    TEST_ASSERT_EQUAL_STRING("value_from_beta", storage2.getString("shared_key").c_str());

    storage1.end();
    storage2.end();
}

void test_namespace_switch(void) {
    // Same instance, switch namespace
    PlatformStorage s;
    s.begin("ns_first");
    s.putString("key", "first_value");
    s.end();

    s.begin("ns_second");
    s.putString("key", "second_value");

    // Should get ns_second value
    TEST_ASSERT_EQUAL_STRING("second_value", s.getString("key").c_str());
    s.end();

    // Switch back to ns_first
    s.begin("ns_first");
    TEST_ASSERT_EQUAL_STRING("first_value", s.getString("key").c_str());
    s.end();
}

// ============================================================================
// Memory Leak Detection Tests (HeapTracker Integration)
// ============================================================================

void test_storage_memory_stability_basic_ops(void) {
    HeapTracker tracker;
    
    // Checkpoint before operations
    tracker.checkpoint("before_ops");
    
    // Perform multiple storage operations
    for (int i = 0; i < 10; i++) {
        String key = "key" + String(i);
        String value = "value" + String(i);
        storage.putString(key.c_str(), value);
        storage.getString(key.c_str());
        storage.remove(key.c_str());
    }
    
    // Checkpoint after operations
    tracker.checkpoint("after_ops");
    
    // Assert heap is stable (within 512 bytes tolerance for native simulation)
    MemoryTestResult result = tracker.assertStable("before_ops", "after_ops", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

void test_storage_memory_stability_namespace_lifecycle(void) {
    HeapTracker tracker;
    
    tracker.checkpoint("before");
    
    // Create and destroy multiple storage instances
    for (int i = 0; i < 5; i++) {
        PlatformStorage temp;
        String ns = "temp_ns_" + String(i);
        String data = "test_data_" + String(i);
        temp.begin(ns.c_str());
        temp.putString("data", data);
        temp.clear();
        temp.end();
    }
    
    tracker.checkpoint("after");
    
    MemoryTestResult result = tracker.assertStable("before", "after", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

void test_storage_memory_no_growth_repeated_reads(void) {
    HeapTracker tracker;
    
    // Setup: store some data
    storage.putString("persistent", "some_value");
    storage.putInt("number", 42);
    
    tracker.checkpoint("baseline");
    
    // Perform many read operations
    for (int i = 0; i < 100; i++) {
        String val = storage.getString("persistent");
        int num = storage.getInt("number");
        (void)val; (void)num; // Suppress unused warnings
    }
    
    // Verify no heap growth
    MemoryTestResult result = tracker.assertNoGrowth("baseline", 256);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

// ============================================================================
// UInt64 Round-Trip Tests (BUG-16)
// ============================================================================

void test_put_get_uint64_large_value(void) {
    // BUG-16: Values > 2^32 must round-trip correctly
    uint64_t bigValue = 0xFFFFFFFF00000001ULL;
    TEST_ASSERT_TRUE(storage.putULong64("u64_key", bigValue));
    uint64_t result = storage.getULong64("u64_key");
    TEST_ASSERT_EQUAL_UINT64(bigValue, result);
}

void test_put_get_uint64_zero(void) {
    TEST_ASSERT_TRUE(storage.putULong64("u64_zero", 0));
    TEST_ASSERT_EQUAL_UINT64(0, storage.getULong64("u64_zero"));
}

void test_put_get_uint64_max(void) {
    uint64_t maxVal = 0xFFFFFFFFFFFFFFFFULL;
    TEST_ASSERT_TRUE(storage.putULong64("u64_max", maxVal));
    TEST_ASSERT_EQUAL_UINT64(maxVal, storage.getULong64("u64_max"));
}

void test_get_uint64_default(void) {
    TEST_ASSERT_EQUAL_UINT64(999, storage.getULong64("nonexistent", 999));
}

// ============================================================================
// Bytes Round-Trip Tests (BUG-17)
// ============================================================================

void test_put_get_bytes_roundtrip(void) {
    // BUG-17: putBytes/getBytes must store and retrieve actual data
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    size_t written = storage.putBytes("bytes_key", data, sizeof(data));
    TEST_ASSERT_EQUAL(sizeof(data), written);

    uint8_t buffer[16] = {0};
    size_t read = storage.getBytes("bytes_key", buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(sizeof(data), read);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, buffer, sizeof(data));
}

void test_get_bytes_length(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    storage.putBytes("bl_key", data, sizeof(data));
    TEST_ASSERT_EQUAL(sizeof(data), storage.getBytesLength("bl_key"));
}

void test_get_bytes_nonexistent(void) {
    uint8_t buffer[16];
    TEST_ASSERT_EQUAL(0, storage.getBytes("no_such_key", buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL(0, storage.getBytesLength("no_such_key"));
}

void test_put_bytes_single_byte(void) {
    uint8_t data[] = {0xFF};
    TEST_ASSERT_EQUAL(1, storage.putBytes("single", data, 1));
    uint8_t out[1] = {0};
    TEST_ASSERT_EQUAL(1, storage.getBytes("single", out, 1));
    TEST_ASSERT_EQUAL_UINT8(0xFF, out[0]);
}

// ============================================================================
// Cache-First Behavior Tests (BUG-15 via StorageComponent)
// ============================================================================

// These tests use StorageComponent to verify cache-first read behavior

void test_cache_getString_hit(void) {
    // Use StorageComponent to test cache behavior
    DomoticsCore::Components::StorageComponent sc;
    sc.begin();
    sc.putString("ck", "cached_value");
    // First read populates cache (or returns cached from put)
    TEST_ASSERT_EQUAL_STRING("cached_value", sc.getString("ck").c_str());
    // Second read should hit cache
    TEST_ASSERT_EQUAL_STRING("cached_value", sc.getString("ck").c_str());
    sc.shutdown();
}

void test_cache_getInt_hit(void) {
    DomoticsCore::Components::StorageComponent sc;
    sc.begin();
    sc.putInt("ik", 42);
    TEST_ASSERT_EQUAL_INT32(42, sc.getInt("ik"));
    TEST_ASSERT_EQUAL_INT32(42, sc.getInt("ik"));
    sc.shutdown();
}

void test_cache_invalidation_on_remove(void) {
    DomoticsCore::Components::StorageComponent sc;
    sc.begin();
    sc.putString("rk", "value");
    TEST_ASSERT_EQUAL_STRING("value", sc.getString("rk").c_str());
    sc.remove("rk");
    // After remove, should return default
    TEST_ASSERT_EQUAL_STRING("default", sc.getString("rk", "default").c_str());
    sc.shutdown();
}

void test_cache_invalidation_on_clear(void) {
    DomoticsCore::Components::StorageComponent sc;
    sc.begin();
    sc.putString("a", "va");
    sc.putInt("b", 10);
    sc.clear();
    // After clear, all getters should return defaults
    TEST_ASSERT_EQUAL_STRING("def", sc.getString("a", "def").c_str());
    TEST_ASSERT_EQUAL_INT32(-1, sc.getInt("b", -1));
    sc.shutdown();
}

void test_getEntryCount_delegates_to_HAL(void) {
    DomoticsCore::Components::StorageComponent sc;
    sc.begin();
    // F6: Assert exact count after F1 fix (maxEntries queried from HAL)
    // RAMOnlyStorage MAX_ENTRIES = 32, initially 0 used
    size_t countBefore = sc.getEntryCount();
    TEST_ASSERT_EQUAL(0, countBefore);
    sc.putString("ec1", "v1");
    sc.putString("ec2", "v2");
    size_t count = sc.getEntryCount();
    // Should reflect exactly 2 entries in HAL
    TEST_ASSERT_EQUAL(2, count);
    sc.shutdown();
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // String tests
    RUN_TEST(test_put_get_string);
    RUN_TEST(test_get_string_default);
    RUN_TEST(test_overwrite_string);

    // Integer tests
    RUN_TEST(test_put_get_int);
    RUN_TEST(test_get_int_default);
    RUN_TEST(test_put_get_negative_int);

    // Boolean tests
    RUN_TEST(test_put_get_bool_true);
    RUN_TEST(test_put_get_bool_false);
    RUN_TEST(test_get_bool_default);

    // Float tests
    RUN_TEST(test_put_get_float);
    RUN_TEST(test_get_float_default);

    // Key management tests
    RUN_TEST(test_is_key_exists);
    RUN_TEST(test_is_key_not_exists);
    RUN_TEST(test_remove_key);
    RUN_TEST(test_remove_nonexistent);
    RUN_TEST(test_clear);

    // Multiple types test
    RUN_TEST(test_multiple_types_same_namespace);

    // Namespace isolation tests
    RUN_TEST(test_namespace_isolation);
    RUN_TEST(test_namespace_switch);

    // UInt64 round-trip tests (BUG-16)
    RUN_TEST(test_put_get_uint64_large_value);
    RUN_TEST(test_put_get_uint64_zero);
    RUN_TEST(test_put_get_uint64_max);
    RUN_TEST(test_get_uint64_default);

    // Bytes round-trip tests (BUG-17)
    RUN_TEST(test_put_get_bytes_roundtrip);
    RUN_TEST(test_get_bytes_length);
    RUN_TEST(test_get_bytes_nonexistent);
    RUN_TEST(test_put_bytes_single_byte);

    // Cache-first behavior tests (BUG-15)
    RUN_TEST(test_cache_getString_hit);
    RUN_TEST(test_cache_getInt_hit);
    RUN_TEST(test_cache_invalidation_on_remove);
    RUN_TEST(test_cache_invalidation_on_clear);
    RUN_TEST(test_getEntryCount_delegates_to_HAL);

    // Memory leak detection tests (HeapTracker)
    RUN_TEST(test_storage_memory_stability_basic_ops);
    RUN_TEST(test_storage_memory_stability_namespace_lifecycle);
    RUN_TEST(test_storage_memory_no_growth_repeated_reads);

    return UNITY_END();
}
