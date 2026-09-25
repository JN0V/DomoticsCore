/**
 * @file test_webui_server_gate.cpp
 * @brief The HTTP server opens only once the platform can open a socket.
 *
 * On ESP32 lwIP exists from the first network interface on; a server begun
 * before that aborts the firmware. The component asks HAL::canOpenServer() in
 * begin() and opens from loop() when the answer turns, never after shutdown().
 */

#include <unity.h>

#include <DomoticsCore/WebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

bool serverRunning() {
    AsyncWebServer* server = AsyncWebServer::last();
    TEST_ASSERT_NOT_NULL_MESSAGE(server, "the component built no server");
    return server->running;
}

}  // namespace

void setUp() { HAL::setCanOpenServerForTest(true); }
void tearDown() { HAL::setCanOpenServerForTest(true); }

void test_a_server_opens_in_begin_when_a_socket_can_be_opened() {
    WebUIComponent webui;
    TEST_ASSERT_EQUAL(ComponentStatus::Success, webui.begin());
    TEST_ASSERT_TRUE(serverRunning());
}

void test_begin_defers_the_server_while_no_socket_can_be_opened() {
    HAL::setCanOpenServerForTest(false);
    WebUIComponent webui;
    TEST_ASSERT_EQUAL(ComponentStatus::Success, webui.begin());
    TEST_ASSERT_FALSE(serverRunning());

    webui.loop();
    TEST_ASSERT_FALSE_MESSAGE(serverRunning(), "loop() opened it with no interface");
}

void test_loop_opens_the_deferred_server_once_an_interface_appears() {
    HAL::setCanOpenServerForTest(false);
    WebUIComponent webui;
    webui.begin();

    HAL::setCanOpenServerForTest(true);
    webui.loop();
    TEST_ASSERT_TRUE(serverRunning());
}

void test_a_deferred_server_stays_closed_after_shutdown() {
    HAL::setCanOpenServerForTest(false);
    WebUIComponent webui;
    webui.begin();
    webui.shutdown();

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, AsyncWebServer::last()->endCalls, "end() reached a server never begun");

    HAL::setCanOpenServerForTest(true);
    webui.loop();
    TEST_ASSERT_FALSE_MESSAGE(serverRunning(), "loop() reopened a server shutdown() closed");
}

void test_an_open_server_stays_closed_after_shutdown() {
    WebUIComponent webui;
    webui.begin();
    webui.shutdown();
    TEST_ASSERT_FALSE(serverRunning());

    webui.loop();
    TEST_ASSERT_FALSE_MESSAGE(serverRunning(), "loop() reopened a server shutdown() closed");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_server_opens_in_begin_when_a_socket_can_be_opened);
    RUN_TEST(test_begin_defers_the_server_while_no_socket_can_be_opened);
    RUN_TEST(test_loop_opens_the_deferred_server_once_an_interface_appears);
    RUN_TEST(test_a_deferred_server_stays_closed_after_shutdown);
    RUN_TEST(test_an_open_server_stays_closed_after_shutdown);
    return UNITY_END();
}
