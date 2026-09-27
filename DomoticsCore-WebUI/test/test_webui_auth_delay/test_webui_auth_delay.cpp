/**
 * @file test_webui_auth_delay.cpp
 * @brief A wrong password makes its address wait before its next attempt is read.
 *
 * The wait is 1 s after the first failure, doubling to authDelayMaxMs. Inside
 * it a request is answered 401 without its credentials being evaluated, so a
 * guess costs the wait; nothing is blocked and no connection is closed.
 */

#include <unity.h>

#include <DomoticsCore/WebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

constexpr uint32_t ATTACKER = 0x0A00002A;
constexpr uint32_t NEIGHBOUR = 0x0A00002B;

WebUIConfig authConfig(uint32_t delayMaxMs = 8000) {
    WebUIConfig cfg;
    cfg.enableAuth = true;
    cfg.setPassword("s3cret");
    cfg.authDelayMaxMs = delayMaxMs;
    return cfg;
}

/** Sends one GET /api/system/info from `ip`; empty `pass` sends no credentials. */
int infoFrom(uint32_t ip, const char* pass) {
    RecordedRoute* route = AsyncWebServer::findRouteAnywhere("/api/system/info");
    TEST_ASSERT_NOT_NULL_MESSAGE(route, "the info route was not registered");
    AsyncWebServerRequest request;
    request.client()->remoteAddress = ip;
    if (pass[0] != '\0') request.setCredentials("admin", pass);
    route->handler(&request);
    return request.sentCode;
}

}  // namespace

void setUp() {
    AsyncWebServer::instances().clear();
    HAL::Platform::setMillisForTest(100000);
}
void tearDown() { HAL::Platform::resetMillisForTest(); }

void test_the_right_password_is_read_at_once() {
    WebUIComponent webui(authConfig());
    webui.begin();
    TEST_ASSERT_EQUAL_INT(200, infoFrom(ATTACKER, "s3cret"));
}

void test_a_wrong_password_holds_the_next_attempt_for_one_second() {
    WebUIComponent webui(authConfig());
    webui.begin();
    TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, "wrong"));
    TEST_ASSERT_EQUAL_INT_MESSAGE(401, infoFrom(ATTACKER, "s3cret"), "the right password inside the wait is not read");
    HAL::Platform::advanceMillisForTest(999);
    TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, "s3cret"));
    HAL::Platform::advanceMillisForTest(1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, infoFrom(ATTACKER, "s3cret"), "read once the wait has passed");
}

// Only a read failure moves the count: 1, 2, 4, 8 s, then the cap.
void test_the_wait_doubles_and_caps() {
    WebUIComponent webui(authConfig(8000));
    webui.begin();
    TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, "wrong"));
    const unsigned long expected[] = {1000, 2000, 4000, 8000, 8000};
    for (unsigned long wait : expected) {
        HAL::Platform::advanceMillisForTest(wait - 1);
        TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, "wrong"));   // unread, does not count
        TEST_ASSERT_EQUAL_INT_MESSAGE(401, infoFrom(ATTACKER, "s3cret"), "not read one millisecond early");
        HAL::Platform::advanceMillisForTest(1);
        TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, "wrong"));   // read: the next failure
    }
    HAL::Platform::advanceMillisForTest(8000);
    TEST_ASSERT_EQUAL_INT(200, infoFrom(ATTACKER, "s3cret"));
}

void test_another_address_is_not_waiting() {
    WebUIComponent webui(authConfig());
    webui.begin();
    TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, "wrong"));
    TEST_ASSERT_EQUAL_INT(200, infoFrom(NEIGHBOUR, "s3cret"));
}

// A browser's first request carries no credentials; that is the challenge, not a guess.
void test_a_request_without_credentials_is_not_a_failure() {
    WebUIComponent webui(authConfig());
    webui.begin();
    TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, ""));
    TEST_ASSERT_EQUAL_INT(200, infoFrom(ATTACKER, "s3cret"));
}

void test_a_success_clears_the_count() {
    WebUIComponent webui(authConfig());
    webui.begin();
    infoFrom(ATTACKER, "wrong");
    HAL::Platform::advanceMillisForTest(1000);
    infoFrom(ATTACKER, "wrong");                       // second failure: 2 s
    HAL::Platform::advanceMillisForTest(2000);
    TEST_ASSERT_EQUAL_INT(200, infoFrom(ATTACKER, "s3cret"));
    TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, "wrong"));
    HAL::Platform::advanceMillisForTest(1000);
    TEST_ASSERT_EQUAL_INT_MESSAGE(200, infoFrom(ATTACKER, "s3cret"), "back to a 1 s wait, not 4 s");
}

void test_a_zero_cap_restores_the_old_behaviour() {
    WebUIComponent webui(authConfig(0));
    webui.begin();
    TEST_ASSERT_EQUAL_INT(401, infoFrom(ATTACKER, "wrong"));
    TEST_ASSERT_EQUAL_INT(200, infoFrom(ATTACKER, "s3cret"));
}

void test_auth_off_ignores_the_wait() {
    WebUIComponent webui(authConfig());
    webui.begin();
    infoFrom(ATTACKER, "wrong");
    WebUIConfig off = webui.getConfig();
    off.enableAuth = false;
    webui.setConfig(off);
    TEST_ASSERT_EQUAL_INT(200, infoFrom(ATTACKER, ""));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_right_password_is_read_at_once);
    RUN_TEST(test_a_wrong_password_holds_the_next_attempt_for_one_second);
    RUN_TEST(test_the_wait_doubles_and_caps);
    RUN_TEST(test_another_address_is_not_waiting);
    RUN_TEST(test_a_request_without_credentials_is_not_a_failure);
    RUN_TEST(test_a_success_clears_the_count);
    RUN_TEST(test_a_zero_cap_restores_the_old_behaviour);
    RUN_TEST(test_auth_off_ignores_the_wait);
    return UNITY_END();
}
