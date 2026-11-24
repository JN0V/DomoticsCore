/**
 * @file test_ha_core_shutdown.cpp
 * @brief HomeAssistant component: what Core::shutdown() delivers to MQTT.
 */

#include <unity.h>
#include "../ha_test_support.h"

namespace {

struct CoreShutdownFixture {
    Core core;
    Components::MQTTComponent* mqtt = nullptr;
    HomeAssistantComponent* ha = nullptr;

    CoreShutdownFixture() {
        HAL::WiFiImpl::setConnectedForTest(true);
        Components::MQTTConfig mcfg;
        mcfg.clientId = "ESP32-core-shutdown";
        mcfg.broker = "192.0.2.1";
        mcfg.autoReconnect = false;
        core.addComponent(std::make_unique<Components::MQTTComponent>(mcfg));

        HAConfig hcfg;
        HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
        core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
        core.begin();

        mqtt = core.getComponent<Components::MQTTComponent>("MQTT");
        ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
        TEST_ASSERT_NOT_NULL(mqtt);
        TEST_ASSERT_NOT_NULL(ha);
        TEST_ASSERT_TRUE_MESSAGE(mqtt->connect(), "the fixture never connected");
        drain();
        TEST_ASSERT_TRUE_MESSAGE(ha->isReady(), "the fixture never came up ready");
    }

    ~CoreShutdownFixture() {
        core.shutdown();
        HAL::WiFiImpl::setConnectedForTest(false);
    }

    void drain() { for (int i = 0; i < 5; i++) core.loop(); }

    void addSensors(int n) {
        for (int i = 0; i < n; i++) {
            String id = String("s") + String(i);
            ha->addSensor(id, id, "u");
        }
        drain();
    }
};

struct Seen {
    int removals = 0;
    int offline = 0;
    int publishes = 0;
};

void watch(Core& core, Seen& seen) {
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&seen](const MQTTPublishEvent& ev) {
            seen.publishes++;
            if (strstr(ev.topic, "/config") && ev.payload[0] == '\0') seen.removals++;
            if (strcmp(ev.payload, "offline") == 0) seen.offline++;
        });
}

} // namespace

void test_core_shutdown_withdraws_two_entities_and_goes_offline() {
    Seen seen;
    CoreShutdownFixture f;
    f.addSensors(2);
    watch(f.core, seen);

    f.core.shutdown();

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, seen.removals, "Core::shutdown() left entities in Home Assistant");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, seen.offline, "Core::shutdown() did not publish offline");
}

// One poll() handles 8 events: a device with more entities must not lose the
// withdrawals past the eighth.
void test_core_shutdown_withdraws_more_entities_than_one_poll_handles() {
    Seen seen;
    CoreShutdownFixture f;
    f.addSensors(20);
    watch(f.core, seen);

    f.core.shutdown();

    TEST_ASSERT_EQUAL_INT_MESSAGE(20, seen.removals, "withdrawals were lost past one poll()");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, seen.offline, "offline was lost");
}

void test_core_shutdown_over_a_dead_link_publishes_nothing() {
    Seen seen;
    CoreShutdownFixture f;
    f.addSensors(3);
    auto* client = f.mqtt->getClientForTest();
    TEST_ASSERT_NOT_NULL(client);
    client->disconnect();
    f.drain();
    TEST_ASSERT_FALSE(f.ha->isReady());
    watch(f.core, seen);

    f.core.shutdown();

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, seen.publishes, "Core::shutdown() published over a dead link");
}

static int runAllTests() {
    UNITY_BEGIN();
    RUN_TEST(test_core_shutdown_withdraws_two_entities_and_goes_offline);
    RUN_TEST(test_core_shutdown_withdraws_more_entities_than_one_poll_handles);
    RUN_TEST(test_core_shutdown_over_a_dead_link_publishes_nothing);
    return UNITY_END();
}

int main(int argc, char** argv) { return runAllTests(); }
