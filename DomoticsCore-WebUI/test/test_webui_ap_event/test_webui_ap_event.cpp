/**
 * @file test_webui_ap_event.cpp
 * @brief LO-26: the WebUI closes its connections when the AP-disabled event is
 * published on the shared topic (Events::EVENT_WIFI_AP_ENABLED).
 */

#include <unity.h>

#include <DomoticsCore/ComponentRegistry.h>
#include <DomoticsCore/Events.h>
#include <DomoticsCore/WebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

struct Fixture {
    ComponentRegistry registry;
    AsyncEventSource* sse = nullptr;

    Fixture() {
        HAL::setCanOpenServerForTest(true);
        TEST_ASSERT_TRUE(registry.registerComponent(std::unique_ptr<IComponent>(new WebUIComponent())));
        TEST_ASSERT_EQUAL(ComponentStatus::Success, registry.initializeAll());
        for (auto* h : AsyncWebServer::last()->handlers) {
            if (auto* s = dynamic_cast<AsyncEventSource*>(h)) sse = s;
        }
        TEST_ASSERT_NOT_NULL_MESSAGE(sse, "no event source: the WebUI did not enable SSE");
    }

    void publishAp(bool enabled) {
        registry.getEventBus().publish(Events::EVENT_WIFI_AP_ENABLED, enabled);
        registry.getEventBus().poll();
    }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_the_topic_is_the_one_wifi_publishes() {
    TEST_ASSERT_EQUAL_STRING("wifi/ap/enabled", Events::EVENT_WIFI_AP_ENABLED);
}

void test_ap_disabled_closes_the_connections() {
    Fixture f;
    f.sse->connectClient();
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)f.sse->count());
    f.publishAp(false);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)f.sse->count());
}

void test_ap_enabled_keeps_the_connections() {
    Fixture f;
    f.sse->connectClient();
    f.publishAp(true);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)f.sse->count());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_topic_is_the_one_wifi_publishes);
    RUN_TEST(test_ap_disabled_closes_the_connections);
    RUN_TEST(test_ap_enabled_keeps_the_connections);
    return UNITY_END();
}
