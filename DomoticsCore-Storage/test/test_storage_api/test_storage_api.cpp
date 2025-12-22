// Unit tests for Storage API (IStorage interface)
// Uses RAMOnlyStorage stub for native testing

#include <DomoticsCore/Storage_HAL.h>
#include <unity.h>

using namespace DomoticsCore::HAL;

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

    return UNITY_END();
}
