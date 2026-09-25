/**
 * @file test_ntp_client_gate.cpp
 * @brief The SNTP client starts only once the platform can open a socket.
 *
 * On ESP32 lwIP exists from the first network interface on; starting the
 * client before that aborts the firmware. The component asks
 * HAL::canOpenServer() in begin() and starts from loop() when the answer
 * turns, never after shutdown().
 */

#include <unity.h>

#include <DomoticsCore/Core.h>
#include <DomoticsCore/NTP.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

bool clientRunning() { return HAL::NTPImpl::clientRunning(); }

}  // namespace

void setUp() {
    HAL::setCanOpenServerForTest(true);
    HAL::NTPImpl::clientRunning() = false;
    HAL::NTPImpl::forceSyncCalls() = 0;
    HAL::NTPImpl::stopCalls() = 0;
}
void tearDown() { HAL::setCanOpenServerForTest(true); }

void test_the_client_starts_in_begin_when_a_socket_can_be_opened() {
    NTPComponent ntp;
    TEST_ASSERT_EQUAL(ComponentStatus::Success, ntp.begin());
    TEST_ASSERT_TRUE(clientRunning());
}

void test_begin_defers_the_client_while_no_socket_can_be_opened() {
    HAL::setCanOpenServerForTest(false);
    NTPComponent ntp;
    TEST_ASSERT_EQUAL(ComponentStatus::Success, ntp.begin());
    TEST_ASSERT_FALSE(clientRunning());

    ntp.loop();
    TEST_ASSERT_FALSE_MESSAGE(clientRunning(), "loop() started it with no interface");
}

void test_loop_starts_the_deferred_client_once_an_interface_appears() {
    HAL::setCanOpenServerForTest(false);
    NTPComponent ntp;
    ntp.begin();

    HAL::setCanOpenServerForTest(true);
    ntp.loop();
    TEST_ASSERT_TRUE(clientRunning());
}

void test_sync_now_is_refused_before_the_client_starts() {
    HAL::setCanOpenServerForTest(false);
    NTPComponent ntp;
    ntp.begin();

    TEST_ASSERT_FALSE(ntp.syncNow());
    TEST_ASSERT_EQUAL_UINT32(0, HAL::NTPImpl::forceSyncCalls());
}

void test_a_deferred_client_stays_stopped_after_shutdown() {
    HAL::setCanOpenServerForTest(false);
    NTPComponent ntp;
    ntp.begin();
    ntp.shutdown();

    HAL::setCanOpenServerForTest(true);
    ntp.loop();
    TEST_ASSERT_FALSE_MESSAGE(clientRunning(), "loop() restarted a client shutdown() stopped");
}

void test_a_running_client_stays_stopped_after_shutdown() {
    NTPComponent ntp;
    ntp.begin();
    ntp.shutdown();
    TEST_ASSERT_FALSE(clientRunning());

    ntp.loop();
    TEST_ASSERT_FALSE_MESSAGE(clientRunning(), "loop() restarted a client shutdown() stopped");
}

void test_a_config_change_while_deferred_does_not_start_the_client() {
    HAL::setCanOpenServerForTest(false);
    NTPComponent ntp;
    ntp.begin();

    NTPConfig cfg = ntp.getConfig();
    cfg.syncInterval = 7200;
    ntp.setConfig(cfg);
    TEST_ASSERT_FALSE(clientRunning());

    HAL::setCanOpenServerForTest(true);
    ntp.loop();
    TEST_ASSERT_TRUE(clientRunning());
    TEST_ASSERT_EQUAL_UINT32(7200u * 1000u, HAL::NTPImpl::lastSyncIntervalMs());
}

void test_a_config_change_restarts_a_running_client() {
    NTPComponent ntp;
    ntp.begin();

    NTPConfig cfg = ntp.getConfig();
    cfg.syncInterval = 7200;
    ntp.setConfig(cfg);
    TEST_ASSERT_TRUE_MESSAGE(clientRunning(), "the restart left the client stopped");
    TEST_ASSERT_EQUAL_UINT32(7200u * 1000u, HAL::NTPImpl::lastSyncIntervalMs());
}

void test_a_client_never_started_is_never_stopped() {
    HAL::setCanOpenServerForTest(false);
    {
        NTPComponent ntp;
        ntp.begin();
        NTPConfig cfg = ntp.getConfig();
        cfg.syncInterval = 7200;
        ntp.setConfig(cfg);
        ntp.shutdown();
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, HAL::NTPImpl::stopCalls(), "stop() reached a client that never started");
}

void test_disabling_through_set_config_stops_the_client() {
    NTPComponent ntp;
    ntp.begin();
    TEST_ASSERT_TRUE(clientRunning());

    NTPConfig off = ntp.getConfig();
    off.enabled = false;
    ntp.setConfig(off);
    TEST_ASSERT_FALSE_MESSAGE(clientRunning(), "the clock keeps syncing with NTP turned off");

    NTPConfig on = ntp.getConfig();
    on.enabled = true;
    ntp.setConfig(on);
    TEST_ASSERT_TRUE(clientRunning());
}

void test_enabling_a_component_begun_disabled_starts_the_client() {
    NTPConfig cfg;
    cfg.enabled = false;
    NTPComponent ntp(cfg);
    ntp.begin();
    TEST_ASSERT_FALSE(clientRunning());

    NTPConfig on = ntp.getConfig();
    on.enabled = true;
    ntp.setConfig(on);
    TEST_ASSERT_TRUE(clientRunning());
}

void test_set_config_does_not_restart_a_shut_down_component() {
    NTPComponent ntp;
    ntp.begin();
    ntp.shutdown();

    NTPConfig changed = ntp.getConfig();
    changed.syncInterval = 7200;
    ntp.setConfig(changed);
    TEST_ASSERT_FALSE_MESSAGE(clientRunning(), "setConfig() undid shutdown()");
}

void test_set_config_before_begin_starts_nothing() {
    NTPComponent ntp;
    NTPConfig changed = ntp.getConfig();
    changed.syncInterval = 7200;
    ntp.setConfig(changed);
    TEST_ASSERT_FALSE(clientRunning());

    ntp.begin();
    TEST_ASSERT_TRUE(clientRunning());
    TEST_ASSERT_EQUAL_UINT32(7200u * 1000u, HAL::NTPImpl::lastSyncIntervalMs());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_client_starts_in_begin_when_a_socket_can_be_opened);
    RUN_TEST(test_begin_defers_the_client_while_no_socket_can_be_opened);
    RUN_TEST(test_loop_starts_the_deferred_client_once_an_interface_appears);
    RUN_TEST(test_sync_now_is_refused_before_the_client_starts);
    RUN_TEST(test_a_deferred_client_stays_stopped_after_shutdown);
    RUN_TEST(test_a_running_client_stays_stopped_after_shutdown);
    RUN_TEST(test_a_config_change_while_deferred_does_not_start_the_client);
    RUN_TEST(test_a_config_change_restarts_a_running_client);
    RUN_TEST(test_a_client_never_started_is_never_stopped);
    RUN_TEST(test_disabling_through_set_config_stops_the_client);
    RUN_TEST(test_enabling_a_component_begun_disabled_starts_the_client);
    RUN_TEST(test_set_config_does_not_restart_a_shut_down_component);
    RUN_TEST(test_set_config_before_begin_starts_nothing);
    return UNITY_END();
}
