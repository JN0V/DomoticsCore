/**
 * @file test_ntp_local_time.cpp
 * @brief Local time across DST transitions and zones, and the servers handed to
 *        the SNTP client. The host's C library applies the POSIX zone rules, as
 *        the boards' does; the clock is the stub's, set per test.
 */

#include <unity.h>
#include <cstring>
#include <string>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/NTP.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

static const char* const kParis = "CET-1CEST,M3.5.0,M10.5.0/3";

// 2026: CEST starts 29 March 01:00 UTC and ends 25 October 01:00 UTC.
static const time_t kBeforeSpring = 1774745999;  // 2026-03-29 00:59:59 UTC
static const time_t kAtSpring     = 1774746000;  // 2026-03-29 01:00:00 UTC
static const time_t kBeforeAutumn = 1792889999;  // 2026-10-25 00:59:59 UTC
static const time_t kAtAutumn     = 1792890000;  // 2026-10-25 01:00:00 UTC
static const time_t kSummer       = 1785924000;  // 2026-08-05 10:00:00 UTC
static const time_t kWinter       = 1768384800;  // 2026-01-14 10:00:00 UTC

static std::string g_warnings;
static LoggerCallbacks::CallbackId g_logId = 0;
static bool g_logging = false;

void setUp() {
    HAL::NTPImpl::clockForTest() = 0;
}

void tearDown() {
    HAL::NTPImpl::clockForTest() = 0;
    if (g_logging) {
        LoggerCallbacks::removeCallback(g_logId);
        g_logging = false;
    }
}

// A component whose clock reads `at` and that has seen the client sync.
struct Synced {
    NTPComponent ntp;
    explicit Synced(const char* tz, time_t at) : ntp(config(tz)) {
        HAL::NTPImpl::clockForTest() = at;
        ntp.begin();
        ntp.loop();
        TEST_ASSERT_TRUE_MESSAGE(ntp.isSynced(), "the fixture's component never synced");
    }
    static NTPConfig config(const char* tz) {
        NTPConfig cfg;
        cfg.timezone = tz;
        return cfg;
    }
    void at(time_t t) { HAL::NTPImpl::clockForTest() = t; }
    ~Synced() { ntp.shutdown(); }
};

void test_spring_forward_skips_from_two_to_three() {
    Synced s(kParis, kBeforeSpring);
    TEST_ASSERT_EQUAL_STRING("01:59:59", s.ntp.getFormattedTime("%H:%M:%S").c_str());
    TEST_ASSERT_FALSE(s.ntp.isDST());
    TEST_ASSERT_EQUAL_INT(3600, s.ntp.getGMTOffset());
    s.at(kAtSpring);
    TEST_ASSERT_EQUAL_STRING("03:00:00", s.ntp.getFormattedTime("%H:%M:%S").c_str());
    TEST_ASSERT_TRUE(s.ntp.isDST());
    TEST_ASSERT_EQUAL_INT(7200, s.ntp.getGMTOffset());
}

void test_fall_back_repeats_two_oclock() {
    Synced s(kParis, kBeforeAutumn);
    TEST_ASSERT_EQUAL_STRING("02:59:59", s.ntp.getFormattedTime("%H:%M:%S").c_str());
    TEST_ASSERT_TRUE(s.ntp.isDST());
    TEST_ASSERT_EQUAL_INT(7200, s.ntp.getGMTOffset());
    s.at(kAtAutumn);
    TEST_ASSERT_EQUAL_STRING("02:00:00", s.ntp.getFormattedTime("%H:%M:%S").c_str());
    TEST_ASSERT_FALSE(s.ntp.isDST());
    TEST_ASSERT_EQUAL_INT(3600, s.ntp.getGMTOffset());
}

void test_iso8601_names_the_summer_offset() {
    Synced s(kParis, kSummer);
    TEST_ASSERT_EQUAL_STRING("2026-08-05T12:00:00+02:00", s.ntp.getISO8601().c_str());
    s.at(kWinter);
    TEST_ASSERT_EQUAL_STRING("2026-01-14T11:00:00+01:00", s.ntp.getISO8601().c_str());
}

void test_the_southern_hemisphere_is_in_dst_in_january() {
    Synced s("AEST-10AEDT,M10.1.0,M4.1.0/3", kWinter);
    TEST_ASSERT_TRUE(s.ntp.isDST());
    TEST_ASSERT_EQUAL_INT(11 * 3600, s.ntp.getGMTOffset());
    s.at(kSummer);
    TEST_ASSERT_FALSE(s.ntp.isDST());
    TEST_ASSERT_EQUAL_INT(10 * 3600, s.ntp.getGMTOffset());
}

void test_a_negative_half_hour_zone_keeps_its_minutes() {
    Synced s("NST3:30NDT,M3.2.0,M11.1.0", kWinter);
    TEST_ASSERT_EQUAL_INT(-(3 * 3600 + 1800), s.ntp.getGMTOffset());
    TEST_ASSERT_EQUAL_STRING("2026-01-14T06:30:00-03:30", s.ntp.getISO8601().c_str());
    s.at(kSummer);
    TEST_ASSERT_EQUAL_STRING("2026-08-05T07:30:00-02:30", s.ntp.getISO8601().c_str());
}

void test_an_offset_under_an_hour_west_keeps_its_sign() {
    Synced s("XXX0:30", kWinter);
    TEST_ASSERT_EQUAL_INT(-1800, s.ntp.getGMTOffset());
    TEST_ASSERT_EQUAL_STRING("2026-01-14T09:30:00-00:30", s.ntp.getISO8601().c_str());
}

static void startWithServers(std::vector<String> servers) {
    NTPConfig cfg;
    cfg.servers = servers;
    NTPComponent ntp(cfg);
    ntp.begin();
    ntp.shutdown();
}

void test_the_three_servers_reach_the_client_in_order() {
    startWithServers({"a.example", "b.example", "c.example"});
    TEST_ASSERT_EQUAL_STRING("a.example", HAL::NTPImpl::initServer(0).c_str());
    TEST_ASSERT_EQUAL_STRING("b.example", HAL::NTPImpl::initServer(1).c_str());
    TEST_ASSERT_EQUAL_STRING("c.example", HAL::NTPImpl::initServer(2).c_str());
}

void test_a_single_server_leaves_the_other_slots_empty() {
    startWithServers({"only.example"});
    TEST_ASSERT_EQUAL_STRING("only.example", HAL::NTPImpl::initServer(0).c_str());
    TEST_ASSERT_EQUAL_STRING("", HAL::NTPImpl::initServer(1).c_str());
    TEST_ASSERT_EQUAL_STRING("", HAL::NTPImpl::initServer(2).c_str());
}

void test_an_empty_list_falls_back_to_the_pool() {
    startWithServers({});
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", HAL::NTPImpl::initServer(0).c_str());
}

void test_a_fourth_server_is_ignored_aloud() {
    g_warnings.clear();
    g_logId = LoggerCallbacks::addCallback([](LogLevel level, const char* tag, const char* message) {
        if (level == LOG_LEVEL_WARN && strcmp(tag, LOG_NTP) == 0) g_warnings += message;
    });
    g_logging = true;
    startWithServers({"a.example", "b.example", "c.example", "d.example"});
    LoggerCallbacks::removeCallback(g_logId);
    g_logging = false;
    TEST_ASSERT_EQUAL_STRING("c.example", HAL::NTPImpl::initServer(2).c_str());
    TEST_ASSERT_TRUE_MESSAGE(g_warnings.find("4 NTP servers configured") != std::string::npos, g_warnings.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_spring_forward_skips_from_two_to_three);
    RUN_TEST(test_fall_back_repeats_two_oclock);
    RUN_TEST(test_iso8601_names_the_summer_offset);
    RUN_TEST(test_the_southern_hemisphere_is_in_dst_in_january);
    RUN_TEST(test_a_negative_half_hour_zone_keeps_its_minutes);
    RUN_TEST(test_an_offset_under_an_hour_west_keeps_its_sign);
    RUN_TEST(test_the_three_servers_reach_the_client_in_order);
    RUN_TEST(test_a_single_server_leaves_the_other_slots_empty);
    RUN_TEST(test_an_empty_list_falls_back_to_the_pool);
    RUN_TEST(test_a_fourth_server_is_ignored_aloud);
    return UNITY_END();
}
