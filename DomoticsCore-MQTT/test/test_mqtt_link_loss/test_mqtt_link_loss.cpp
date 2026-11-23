/**
 * @file test_mqtt_link_loss.cpp
 * @brief MQTT component: what a dropped link still has to say about itself.
 *
 * resetReconnect() leaves the connected state, a link lost twice is counted
 * twice, uptime stops over an outage, and a loss the EventBus could not hold
 * is not forgotten.
 */

#include <unity.h>
#include <memory>
#include <string>
#include <vector>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/MQTTEvents.h>
#include <DomoticsCore/Testing/HeapTracker.h>
using namespace DomoticsCore;
using namespace DomoticsCore::Components;

void setUp() {}
void tearDown() {
    HAL::WiFiImpl::setConnectedForTest(false);
    HAL::Platform::resetMillisForTest();
}

namespace {

/// Brings a component up inside a Core, connected to the stub broker.
struct ConnectedBus {
    Core core;
    MQTTComponent* mqtt = nullptr;
    uint32_t connects = 0;
    uint32_t disconnects = 0;
    std::vector<const char*> order;

    explicit ConnectedBus(bool autoReconnect) {
        MQTTConfig cfg;
        cfg.broker = "test.broker.com";
        cfg.port = 1883;
        cfg.enabled = true;
        cfg.autoReconnect = autoReconnect;
        cfg.reconnectDelay = 0;  // Eliminate the timer as a variable
        core.addComponent(std::make_unique<MQTTComponent>(cfg));
        core.begin();
        HAL::WiFiImpl::setConnectedForTest(true);
        mqtt = core.getComponent<MQTTComponent>("MQTT");
        TEST_ASSERT_NOT_NULL_MESSAGE(mqtt, "the fixture has no component to drive");
        core.getEventBus().subscribe(String(MQTTEvents::EVENT_CONNECTED), [this](const void*) {
            connects++;
            order.push_back(MQTTEvents::EVENT_CONNECTED);
        }, nullptr);
        core.getEventBus().subscribe(String(MQTTEvents::EVENT_DISCONNECTED), [this](const void*) {
            disconnects++;
            order.push_back(MQTTEvents::EVENT_DISCONNECTED);
        }, nullptr);
        mqtt->connect();
        core.getEventBus().poll();
    }

    ~ConnectedBus() {
        mqtt->shutdown();
        HAL::WiFiImpl::setConnectedForTest(false);
    }

    /// The broker or the network drops the link: the client goes down under
    /// the component, which is never told.
    void dropTheLink() {
        auto* client = mqtt->getClientForTest();
        TEST_ASSERT_NOT_NULL_MESSAGE(client, "nothing to drop: the fixture never built a client");
        client->disconnect();
    }

    void loopAndPoll(int times = 1) {
        for (int i = 0; i < times; i++) mqtt->loop();
        core.getEventBus().poll();
    }

    /// Events of nobody's interest, large enough that a few of them fill the queue.
    void floodTheQueue(int count) {
        static const uint8_t blob[800] = {0};
        for (int i = 0; i < count; i++) {
            core.getEventBus().publish("test/filler", static_cast<const void*>(blob), sizeof(blob));
        }
    }
};

}  // namespace

// Given a connected client, When resetReconnect() is called,
// Then subscribers receive mqtt/disconnected before any later mqtt/connected.
void test_reset_reconnect_announces_the_loss_before_the_next_connection() {
    ConnectedBus bus(/*autoReconnect=*/false);
    TEST_ASSERT_TRUE(bus.mqtt->isConnected());
    TEST_ASSERT_EQUAL_UINT32(1, bus.connects);

    bus.mqtt->resetReconnect();
    bus.core.getEventBus().poll();
    TEST_ASSERT_TRUE(bus.mqtt->connect());
    bus.core.getEventBus().poll();

    TEST_ASSERT_EQUAL_UINT32(1, bus.disconnects);
    TEST_ASSERT_EQUAL_UINT32(2, bus.connects);
    TEST_ASSERT_EQUAL_size_t(3, bus.order.size());
    TEST_ASSERT_EQUAL_STRING(MQTTEvents::EVENT_CONNECTED, bus.order[0]);
    TEST_ASSERT_EQUAL_STRING(MQTTEvents::EVENT_DISCONNECTED, bus.order[1]);
    TEST_ASSERT_EQUAL_STRING(MQTTEvents::EVENT_CONNECTED, bus.order[2]);
}

// Given a link that drops twice, When statistics are read,
// Then they show two losses.
void test_statistics_count_each_lost_link() {
    ConnectedBus bus(/*autoReconnect=*/true);

    bus.dropTheLink();
    bus.loopAndPoll();
    TEST_ASSERT_TRUE(bus.mqtt->isConnected());

    bus.dropTheLink();
    bus.loopAndPoll();
    TEST_ASSERT_TRUE(bus.mqtt->isConnected());

    TEST_ASSERT_EQUAL_UINT32(2, bus.mqtt->getStatistics().lossCount);
}

// Given a dropped link, When statistics are read during the outage,
// Then uptime is 0 (it neither reads as live nor keeps its last value).
void test_uptime_is_zero_during_an_outage() {
    HAL::Platform::setMillisForTest(1000);
    ConnectedBus bus(/*autoReconnect=*/false);

    HAL::Platform::advanceMillisForTest(5000);
    bus.loopAndPoll();
    TEST_ASSERT_EQUAL_UINT32(5, bus.mqtt->getStatistics().uptime);

    bus.dropTheLink();
    bus.loopAndPoll();
    HAL::Platform::advanceMillisForTest(10000);
    bus.loopAndPoll();

    TEST_ASSERT_FALSE(bus.mqtt->isConnected());
    TEST_ASSERT_EQUAL_UINT32(0, bus.mqtt->getStatistics().uptime);
}

// Given a full EventBus queue at the moment of a drop, When the queue frees up,
// Then subscribers still learn that the link is down: the one subscribed
// before the drop is told again, or one subscribing afterwards replays it.
void test_a_loss_the_full_queue_dropped_is_not_forgotten() {
    uint32_t lateDisconnects = 0;
    ConnectedBus bus(/*autoReconnect=*/false);

    bus.floodTheQueue(64);
    TEST_ASSERT_TRUE_MESSAGE(bus.core.getEventBus().getDroppedCount() > 0,
                             "the queue was never full");

    bus.dropTheLink();
    bus.mqtt->loop();
    // The queue stays full: what was queued first is evicted first.
    bus.floodTheQueue(64);
    bus.core.getEventBus().poll(1000);
    TEST_ASSERT_FALSE(bus.mqtt->isConnected());

    bus.core.getEventBus().subscribe(String(MQTTEvents::EVENT_DISCONNECTED),
                                     [&lateDisconnects](const void*) { lateDisconnects++; },
                                     nullptr, /*replayLast=*/true);
    for (int i = 0; i < 5; i++) {
        bus.mqtt->loop();
        bus.core.getEventBus().poll(1000);
    }

    TEST_ASSERT_TRUE_MESSAGE(bus.disconnects + lateDisconnects >= 1,
                             "no subscriber ever learned that the link is down");
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_reset_reconnect_announces_the_loss_before_the_next_connection);
    RUN_TEST(test_statistics_count_each_lost_link);
    RUN_TEST(test_uptime_is_zero_during_an_outage);
    RUN_TEST(test_a_loss_the_full_queue_dropped_is_not_forgotten);

    return UNITY_END();
}
