/**
 * @file test_ha_command_path.cpp
 * @brief HomeAssistant component: what the command path refuses.
 */

#include <unity.h>
#include "../ha_test_support.h"

// Sends a topic verbatim, as MQTT delivers it, and lets the bus drain.
static void sendRawMessage(Core& core, const char* topic, const char* payload) {
    MQTTMessageEvent msg{};
    strncpy(msg.topic, topic, MQTT_EVENT_TOPIC_SIZE - 1);
    strncpy(msg.payload, payload, MQTT_EVENT_PAYLOAD_SIZE - 1);
    core.emit<MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, msg);
    for (int i = 0; i < 5; i++) core.loop();
}

// Scenario: Empty id refused
// Given an entity registered with an empty id,
// When a command arrives on a topic with two adjacent slashes
//      (homeassistant/switch/node//set),
// Then no entity matches, nothing is emitted and a warning is logged.
void test_ha_command_with_an_empty_entity_id_is_refused() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSwitch("", "Nameless");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void*) {
        eventFired = true;
    }, nullptr);

    simulateMqttConnect(core);

    startLogCapture();
    sendRawMessage(core, "homeassistant/switch/node//set", "ON");
    stopLogCapture();
    String warn = g_capturedWarn;

    TEST_ASSERT_FALSE_MESSAGE(eventFired,
        "an empty id matched the entity registered with an empty id and a command event was emitted");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, haPtr->getStatistics().commandsReceived,
        "a command for an empty id was counted as received");
    TEST_ASSERT_TRUE_MESSAGE(warn.length() > 0,
        "the refused empty id was dropped without a warning");

    core.shutdown();
}

static int runAllTests() {
    UNITY_BEGIN();
    RUN_TEST(test_ha_command_with_an_empty_entity_id_is_refused);
    return UNITY_END();
}

int main(int argc, char** argv) { return runAllTests(); }
