/**
 * @file test_mqtt_link_loss_listener.cpp
 * @brief A subscriber already listening, whose mqtt/disconnected the full
 *        EventBus queue evicted, is told again (HomeAssistant's mqttConnected).
 */

#include <unity.h>
#include <memory>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/MQTTEvents.h>
using namespace DomoticsCore;
using namespace DomoticsCore::Components;

void setUp() {}
void tearDown() {
    HAL::WiFiImpl::setConnectedForTest(false);
    HAL::Platform::resetMillisForTest();
}

void test_a_listener_that_lost_the_disconnect_to_a_full_queue_is_told_again() {
    Core core;
    MQTTConfig cfg;
    cfg.broker = "test.broker.com";
    cfg.port = 1883;
    cfg.enabled = true;
    cfg.autoReconnect = false;
    cfg.reconnectDelay = 0;
    core.addComponent(std::make_unique<MQTTComponent>(cfg));
    core.begin();
    HAL::WiFiImpl::setConnectedForTest(true);
    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    TEST_ASSERT_NOT_NULL(mqtt);

    uint32_t disconnects = 0;
    core.getEventBus().subscribe(String(MQTTEvents::EVENT_DISCONNECTED),
                                 [&disconnects](const void*) { disconnects++; }, nullptr);
    mqtt->connect();
    core.getEventBus().poll();
    TEST_ASSERT_TRUE(mqtt->isConnected());

    mqtt->getClientForTest()->disconnect();
    mqtt->loop();  // announces the loss

    // The queue fills: the announcement is evicted before any dispatch.
    static const uint8_t blob[800] = {0};
    for (int i = 0; i < 64; i++) {
        core.getEventBus().publish("test/filler", static_cast<const void*>(blob), sizeof(blob));
    }
    core.getEventBus().poll(1000);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, disconnects, "the loss was not evicted: the test proves nothing");

    for (int i = 0; i < 5; i++) {
        mqtt->loop();
        core.getEventBus().poll(1000);
    }
    TEST_ASSERT_TRUE_MESSAGE(disconnects >= 1, "the listening subscriber was never told");
    mqtt->shutdown();
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_listener_that_lost_the_disconnect_to_a_full_queue_is_told_again);
    return UNITY_END();
}
