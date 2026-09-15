// TEST-7: the native String must parse and print as the Arduino cores do,
// or a native suite pins the stub's behaviour and calls it the board's.
// toInt()/toFloat() are atol()/atof() there; String(float) prints two decimals.
#include <unity.h>
#include <DomoticsCore/Platform_HAL.h>
#include <cmath>
#include <type_traits>

void setUp(void) {}
void tearDown(void) {}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-6f; }

void test_to_int_reads_a_numeric_prefix_and_gives_zero_on_garbage(void) {
    TEST_ASSERT_EQUAL(42, String("42").toInt());
    TEST_ASSERT_EQUAL(-7, String("-7").toInt());
    TEST_ASSERT_EQUAL(42, String(" 42x").toInt());   // atol skips blanks, stops at 'x'
    TEST_ASSERT_EQUAL(0, String("abc").toInt());
    TEST_ASSERT_EQUAL(0, String("").toInt());
}

void test_to_int_saturates_at_the_boards_32_bit_long(void) {
    TEST_ASSERT_EQUAL(2147483647L, String("2147483648").toInt());
    TEST_ASSERT_EQUAL(2147483647L, String("4294967295").toInt());
    TEST_ASSERT_EQUAL(-2147483647L - 1L, String("-2147483649").toInt());
}

void test_to_int_returns_a_long_as_on_the_boards(void) {
    static_assert(std::is_same<decltype(String().toInt()), long>::value,
                  "Arduino's String::toInt() returns long");
    TEST_PASS();
}

void test_to_float_reads_a_numeric_prefix_and_gives_zero_on_garbage(void) {
    TEST_ASSERT_TRUE(near(1.5f, String("1.5").toFloat()));
    TEST_ASSERT_TRUE(near(1.5f, String("1.5abc").toFloat()));
    TEST_ASSERT_TRUE(near(0.0f, String("abc").toFloat()));
}

void test_a_string_from_a_float_prints_two_decimals_like_the_cores(void) {
    TEST_ASSERT_EQUAL_STRING("1.50", String(1.5f).c_str());
    TEST_ASSERT_EQUAL_STRING("3.00", String(3.0f).c_str());
    TEST_ASSERT_EQUAL_STRING("2.25", String(2.25).c_str());
    TEST_ASSERT_EQUAL_STRING("1.500", String(1.5f, 3).c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_to_int_reads_a_numeric_prefix_and_gives_zero_on_garbage);
    RUN_TEST(test_to_int_saturates_at_the_boards_32_bit_long);
    RUN_TEST(test_to_int_returns_a_long_as_on_the_boards);
    RUN_TEST(test_to_float_reads_a_numeric_prefix_and_gives_zero_on_garbage);
    RUN_TEST(test_a_string_from_a_float_prints_two_decimals_like_the_cores);
    return UNITY_END();
}
