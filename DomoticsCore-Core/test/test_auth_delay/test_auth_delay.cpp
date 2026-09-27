#include <unity.h>
#include <DomoticsCore/AuthDelay.h>

using DomoticsCore::Utils::AuthDelay;

void setUp(void) {}
void tearDown(void) {}

void test_wait_doubles_from_one_second_and_caps(void) {
    TEST_ASSERT_EQUAL_UINT32(0, AuthDelay::waitMs(0, 8000));
    TEST_ASSERT_EQUAL_UINT32(1000, AuthDelay::waitMs(1, 8000));
    TEST_ASSERT_EQUAL_UINT32(2000, AuthDelay::waitMs(2, 8000));
    TEST_ASSERT_EQUAL_UINT32(4000, AuthDelay::waitMs(3, 8000));
    TEST_ASSERT_EQUAL_UINT32(8000, AuthDelay::waitMs(4, 8000));
    TEST_ASSERT_EQUAL_UINT32(8000, AuthDelay::waitMs(255, 8000));
    TEST_ASSERT_EQUAL_UINT32(0, AuthDelay::waitMs(3, 0));
}

void test_an_unknown_address_is_not_waiting(void) {
    AuthDelay d;
    TEST_ASSERT_EQUAL_UINT32(5000, d.notBefore(0x0A000001, 5000, 8000));
    TEST_ASSERT_FALSE(d.isWaiting(0x0A000001, 5000, 8000));
}

void test_a_failure_sets_the_wait_for_that_address_only(void) {
    AuthDelay d;
    TEST_ASSERT_EQUAL_UINT8(1, d.noteFailure(1, 10000));
    TEST_ASSERT_TRUE(d.isWaiting(1, 10999, 8000));
    TEST_ASSERT_FALSE(d.isWaiting(1, 11000, 8000));
    TEST_ASSERT_FALSE(d.isWaiting(2, 10000, 8000));
    TEST_ASSERT_EQUAL_UINT8(2, d.noteFailure(1, 11000));
    TEST_ASSERT_EQUAL_UINT32(13000, d.notBefore(1, 11000, 8000));
}

void test_forget_clears_the_address(void) {
    AuthDelay d;
    d.noteFailure(1, 1000);
    d.noteFailure(1, 2000);
    d.forget(1);
    TEST_ASSERT_FALSE(d.isWaiting(1, 2000, 8000));
    TEST_ASSERT_EQUAL_UINT8(1, d.noteFailure(1, 2000));
}

void test_a_quiet_minute_resets_the_count(void) {
    AuthDelay d;
    d.noteFailure(1, 1000);
    d.noteFailure(1, 2000);
    TEST_ASSERT_FALSE(d.isWaiting(1, 2000 + AuthDelay::FORGET_MS, 8000));
    TEST_ASSERT_EQUAL_UINT8(1, d.noteFailure(1, 2000 + AuthDelay::FORGET_MS));
}

void test_a_full_table_evicts_the_oldest_failure(void) {
    AuthDelay d;
    for (uint32_t ip = 1; ip <= AuthDelay::ENTRIES; ++ip) d.noteFailure(ip, 1000 + ip);
    d.noteFailure(99, 2000);
    TEST_ASSERT_FALSE_MESSAGE(d.isWaiting(1, 2000, 8000), "the oldest address was evicted");
    TEST_ASSERT_TRUE(d.isWaiting(2, 2000, 8000));
    TEST_ASSERT_TRUE(d.isWaiting(99, 2000, 8000));
}

// Eviction compares ages, so a failure logged just before millis() wraps is still the older one.
void test_eviction_survives_the_millis_wrap(void) {
    AuthDelay d;
    const unsigned long nearWrap = static_cast<unsigned long>(-1) - 500;
    d.noteFailure(1, nearWrap);
    for (uint32_t ip = 2; ip <= AuthDelay::ENTRIES; ++ip) d.noteFailure(ip, 100 + ip);
    d.noteFailure(99, 200);
    TEST_ASSERT_FALSE_MESSAGE(d.isWaiting(1, 200, 8000), "the pre-wrap failure was the oldest");
    TEST_ASSERT_TRUE(d.isWaiting(2, 200, 8000));
}

void test_the_wait_is_measured_across_the_millis_wrap(void) {
    AuthDelay d;
    const unsigned long nearWrap = static_cast<unsigned long>(-1) - 500;
    d.noteFailure(1, nearWrap);
    TEST_ASSERT_TRUE(d.isWaiting(1, nearWrap + 999, 8000));
    TEST_ASSERT_FALSE(d.isWaiting(1, nearWrap + 1000, 8000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_wait_doubles_from_one_second_and_caps);
    RUN_TEST(test_an_unknown_address_is_not_waiting);
    RUN_TEST(test_a_failure_sets_the_wait_for_that_address_only);
    RUN_TEST(test_forget_clears_the_address);
    RUN_TEST(test_a_quiet_minute_resets_the_count);
    RUN_TEST(test_a_full_table_evicts_the_oldest_failure);
    RUN_TEST(test_eviction_survives_the_millis_wrap);
    RUN_TEST(test_the_wait_is_measured_across_the_millis_wrap);
    return UNITY_END();
}
