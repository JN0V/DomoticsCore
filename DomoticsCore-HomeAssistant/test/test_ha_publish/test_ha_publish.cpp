/**
 * @file test_ha_publish.cpp
 * @brief HomeAssistant component: publishState() overloads, char[] fields and the command parse.
 */

#include <unity.h>
#include "../ha_test_support.h"

// ============================================================================
// publishState() overload resolution tests (bug 008)
// ============================================================================

void test_publish_state_const_char_ptr() {
    // Bug: publishState(id, const char*) was resolving to bool overload,
    // publishing "ON" instead of the actual string value.
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("alarm", "Alarm State");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    // Pass a const char* — must NOT resolve to bool overload
    haPtr->publishState("alarm", AlarmPanelState::Arming);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("arming", capturedPayload.c_str());

    core.shutdown();
}

void test_publish_state_constexpr_char_ptr() {
    // Verify constexpr const char* values work correctly
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("alarm", "Alarm State");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    haPtr->publishState("alarm", AlarmPanelState::ArmedAway);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("armed_away", capturedPayload.c_str());

    core.shutdown();
}

void test_publish_state_bool_still_works() {
    // Ensure the bool overload is not broken by the new const char* overload
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addBinarySensor("fault", "Fault");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    haPtr->publishState("fault", true);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("ON", capturedPayload.c_str());

    capturedPayload = "";
    haPtr->publishState("fault", false);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("OFF", capturedPayload.c_str());

    core.shutdown();
}

void test_publish_state_string_still_works() {
    // Ensure the String overload still works
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("status", "Status");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    haPtr->publishState("status", String("custom_value"));
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("custom_value", capturedPayload.c_str());

    core.shutdown();
}

void test_publish_state_string_literal() {
    // String literals are const char[], which decay to const char*
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("alarm", "Alarm");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    haPtr->publishState("alarm", "triggered");
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("triggered", capturedPayload.c_str());

    core.shutdown();
}

// ============================================================================
// char[] field tests
// ============================================================================

void test_ha_set_field_truncation() {
    char buf[10];
    HA::setField(buf, "a_very_long_string_exceeding_buffer", sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("a_very_lo", buf);
    TEST_ASSERT_EQUAL(9, strlen(buf));
    TEST_ASSERT_EQUAL('\0', buf[9]);
}

void test_ha_set_field_null_input() {
    char buf[10];
    buf[0] = 'x'; // ensure it gets cleared
    HA::setField(buf, nullptr, sizeof(buf));
    TEST_ASSERT_EQUAL('\0', buf[0]);
}

void test_ha_node_id_processing() {
    // Simulate System.h nodeId processing: lowercase + space→underscore
    HAConfig config;
    HA::setField(config.nodeId, "My Device Name", sizeof(config.nodeId));
    for (size_t i = 0; config.nodeId[i]; i++) {
        if (config.nodeId[i] == ' ') config.nodeId[i] = '_';
        else config.nodeId[i] = tolower((unsigned char)config.nodeId[i]);
    }
    TEST_ASSERT_EQUAL_STRING("my_device_name", config.nodeId);
}

// ============================================================================
// The command parse, pinned where the char* rewrite could drop it
// ============================================================================
//
// These five cases are behaviour, not cost. They cannot show an allocation was
// avoided: the native String is std::string (Platform_Stub.h:27), and every id
// and payload below fits its own small-string buffer or is copied into a char[]
// before it is measured. What they hold in place is what the rewrite could
// silently change while still passing everything else — the two truncation
// points, the two malformed-topic refusals, and the fact that an id longer than
// the event field still finds its entity.


// The helpers above build the topic from its parts, which cannot express a
// malformed one. This sends the topic verbatim.
static void simulateRawMessage(Core& core, const char* topic, const char* payload) {
    // The event's buffers are what MQTT delivers through, and silently cutting a
    // fixture to fit them would leave the test measuring a different topic from
    // the one it reads in the source. Refuse instead.
    TEST_ASSERT_TRUE_MESSAGE(strlen(topic) < MQTT_EVENT_TOPIC_SIZE,
        "the test's topic does not fit MQTTMessageEvent::topic — it would be "
        "truncated, and the case below would not be the case that was written");
    TEST_ASSERT_TRUE_MESSAGE(strlen(payload) < MQTT_EVENT_PAYLOAD_SIZE,
        "the test's payload does not fit MQTTMessageEvent::payload — it would be "
        "truncated, and the case below would not be the case that was written");

    MQTTMessageEvent msg{};
    strncpy(msg.topic, topic, MQTT_EVENT_TOPIC_SIZE - 1);
    msg.topic[MQTT_EVENT_TOPIC_SIZE - 1] = '\0';
    strncpy(msg.payload, payload, MQTT_EVENT_PAYLOAD_SIZE - 1);
    msg.payload[MQTT_EVENT_PAYLOAD_SIZE - 1] = '\0';
    core.emit<MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, msg);
    for (int i = 0; i < 5; i++) core.loop();
}

void test_ha_command_entity_id_over_63_truncates_and_warns() {
    // 70 characters, against the 64-byte HACommandEvent::entityId. The entity is
    // registered under the full id, so this also pins the half of the rewrite
    // that is easiest to lose: the lookup compares the whole extracted id, not
    // the copy that has already been cut to fit the event.
    char longId[71];
    memset(longId, 'e', 70);
    longId[70] = '\0';

    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch(longId, "Overlong Switch");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    char evEntityId[64] = {};
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        auto& ev = *reinterpret_cast<const HAEvents::HACommandEvent*>(data);
        eventFired = true;
        strncpy(evEntityId, ev.entityId, sizeof(evEntityId) - 1);
    }, nullptr);

    simulateMqttConnect(core);

    String topic = String("homeassistant/switch/test_node/") + longId + "/set";
    startLogCapture();
    simulateRawMessage(core, topic.c_str(), "ON");
    stopLogCapture();
    String warn = g_capturedWarn;

    // Non-vacuity: everything below is about what the event carried, and there
    // is no event if the 70-character id failed to match its entity.
    TEST_ASSERT_TRUE_MESSAGE(eventFired,
        "no ha/command event: the overlong id never matched its entity, so the "
        "truncation this test measures never happened");

    char expected[64];
    memcpy(expected, longId, 63);
    expected[63] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, evEntityId);
    TEST_ASSERT_EQUAL_MESSAGE(63, strlen(evEntityId), "the id was not cut at the field's 63 characters");

    TEST_ASSERT_TRUE_MESSAGE(warn.indexOf("Entity ID truncated") >= 0,
        "the id was truncated with no warning at all");
    TEST_ASSERT_TRUE_MESSAGE(warn.indexOf("(70 > 63)") >= 0,
        "the warning does not name the overflow it dropped");

    core.shutdown();
}

void test_ha_command_payload_over_127_truncates_and_warns() {
    // 200 characters against the 128-byte HACommandEvent::command.
    char longPayload[201];
    memset(longPayload, 'p', 200);
    longPayload[200] = '\0';

    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    char evCommand[128] = {};
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        auto& ev = *reinterpret_cast<const HAEvents::HACommandEvent*>(data);
        eventFired = true;
        strncpy(evCommand, ev.command, sizeof(evCommand) - 1);
    }, nullptr);

    simulateMqttConnect(core);

    startLogCapture();
    simulateRawMessage(core, "homeassistant/switch/test_node/sw1/set", longPayload);
    stopLogCapture();
    String warn = g_capturedWarn;

    TEST_ASSERT_TRUE_MESSAGE(eventFired,
        "no ha/command event: the oversized payload never reached the event, so "
        "the truncation this test measures never happened");
    TEST_ASSERT_EQUAL_MESSAGE(127, strlen(evCommand),
        "the payload was not cut at the field's 127 characters");

    TEST_ASSERT_TRUE_MESSAGE(warn.indexOf("Command payload truncated") >= 0,
        "the payload was truncated with no warning at all");
    TEST_ASSERT_TRUE_MESSAGE(warn.indexOf("(200 > 127)") >= 0,
        "the warning does not name the overflow it dropped");

    core.shutdown();
}

// Every message the shared client receives arrives here, most of them the host
// application's own. A topic outside the discovery prefix is not this
// component's business: it costs no parse and produces no line, at any level.
void test_a_topic_outside_the_discovery_prefix_says_nothing() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void*) {
        eventFired = true;
    }, nullptr);

    simulateMqttConnect(core);

    startLogCapture();
    simulateRawMessage(core, "alarm/command", "arm");            // one slash, an application's own
    simulateRawMessage(core, "sensors/kitchen/temperature", "21.5");  // two, so the parse would reach the lookup
    simulateRawMessage(core, "homeassistant_other/switch/test_node/sw1/set", "ON");  // the prefix as a prefix of itself
    stopLogCapture();
    String err = g_capturedError;
    String warn = g_capturedWarn;
    String info = g_capturedInfo;

    TEST_ASSERT_FALSE_MESSAGE(eventFired, "a foreign topic produced a command event");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, haPtr->getStatistics().commandsReceived,
        "a foreign message was counted as a command");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", err.c_str(),
        "an application's own MQTT traffic is reported at error level");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", warn.c_str(),
        "a foreign topic with two slashes reaches the unknown-entity warning");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", info.c_str(),
        "a foreign topic is announced as a received command");

    core.shutdown();
}

// The escape hatch, pinned in the direction it actually goes: with no prefix
// configured there is nothing to recognise the component's own traffic by, so
// every message is parsed as before and a malformed one is reported.
void test_an_empty_discovery_prefix_parses_everything_again() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    config.discoveryPrefix[0] = '\0';

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();
    simulateMqttConnect(core);

    startLogCapture();
    simulateRawMessage(core, "no_slash_at_all", "ON");
    stopLogCapture();

    TEST_ASSERT_TRUE_MESSAGE(g_capturedError.indexOf("no trailing slash") >= 0,
        "with no prefix to filter on, a malformed topic is no longer reported at all");

    core.shutdown();
}

// The filter is built from the prefix at connect time, so a prefix changed
// afterwards has to move it: otherwise the device listens where commands no
// longer arrive, and the prefix check drops the ones that do.
void test_a_prefix_changed_at_runtime_moves_the_command_filter() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();
    simulateMqttConnect(core);

    String lastFilter;
    core.on<MQTTSubscribeEvent>(DomoticsCore::MQTTEvents::EVENT_SUBSCRIBE,
        [&](const MQTTSubscribeEvent& ev) { lastFilter = ev.topic; });

    HAConfig moved = haPtr->getConfig();
    HA::setField(moved.discoveryPrefix, "hass", sizeof(moved.discoveryPrefix));
    haPtr->setConfig(moved);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING_MESSAGE("hass/+/test_node/+/set", lastFilter.c_str(),
        "the subscription still names the old prefix, so no command can arrive");

    simulateRawMessage(core, "hass/switch/test_node/sw1/set", "ON");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, haPtr->getStatistics().commandsReceived,
        "a command on the new prefix was dropped");

    core.shutdown();
}

// Nothing looked at what shutdown publishes, and it publishes an empty retained
// payload per entity — the message that deletes it from Home Assistant. The
// component's own shutdown is called here rather than the Core's: the Core
// stops dispatching before these reach the bus, which is filed separately.
void test_shutdown_removes_the_discovery_it_published() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSwitch("sw1", "Switch 1");
    ha->addSensor("temp", "Temperature");
    core.addComponent(std::move(ha));
    core.begin();
    simulateMqttConnect(core);

    int removals = 0;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            if (strstr(ev.topic, "/config") && ev.payload[0] == '\0') removals++;
        });

    haPtr->shutdown();
    for (int i = 0; i < 5; i++) core.loop();   // the removals cross the bus like any publish
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, removals,
        "shutdown left the entities in Home Assistant");
}

// The other half of the prefix check: a malformed topic *under* the component's
// own prefix is a genuine defect and keeps its error line.
void test_ha_command_topic_with_one_slash_is_refused() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void*) {
        eventFired = true;
    }, nullptr);

    simulateMqttConnect(core);

    startLogCapture();
    simulateRawMessage(core, "homeassistant/set", "ON");
    stopLogCapture();
    String err = g_capturedError;

    TEST_ASSERT_FALSE_MESSAGE(eventFired, "a topic with one slash produced a command event");
    TEST_ASSERT_TRUE_MESSAGE(err.indexOf("Invalid topic format - missing entity ID") >= 0,
        "the topic was discarded silently");

    core.shutdown();
}

void test_ha_commands_received_counts_only_known_entities() {
    // stats.commandsReceived increments after the unknown-entity return and
    // before the payload is validated. Both halves of that position are load
    // bearing and neither is visible in any other test: a discarded message must
    // not count, and a command an entity rejects must.
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addButton("btn1", "Button");
    core.addComponent(std::move(ha));
    core.begin();

    simulateMqttConnect(core);
    TEST_ASSERT_EQUAL_UINT32(0, haPtr->getStatistics().commandsReceived);

    simulateRawMessage(core, "homeassistant/button/test_node/nobody_here/set", "PRESS");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, haPtr->getStatistics().commandsReceived,
        "a command for an unregistered entity was counted as received");

    // HAButton::handleCommand returns false for anything but PRESS, so this one
    // is counted and then dropped without an event.
    simulateRawMessage(core, "homeassistant/button/test_node/btn1/set", "NONSENSE");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, haPtr->getStatistics().commandsReceived,
        "the counter moved to after validation: a rejected command is no longer counted");

    core.shutdown();
}

void test_ha_config_no_heap_allocation() {
    // Verify HAConfig uses no heap (all stack/struct storage)
    size_t heapBefore = HAL::Platform::getFreeHeap();
    {
        HAConfig configs[10];
        // Access fields to prevent optimization
        for (int i = 0; i < 10; i++) {
            volatile char c = configs[i].nodeId[0];
            (void)c;
        }
    }
    size_t heapAfter = HAL::Platform::getFreeHeap();
    // Allow small variance for allocator bookkeeping
    TEST_ASSERT_INT_WITHIN(64, 0, (int)(heapBefore - heapAfter));
}

static int runAllTests() {
    UNITY_BEGIN();

    // publishState() overload resolution tests (bug 008)
    RUN_TEST(test_publish_state_const_char_ptr);
    RUN_TEST(test_publish_state_constexpr_char_ptr);
    RUN_TEST(test_publish_state_bool_still_works);

    // The discovery fields and the duplicate-id warning
    RUN_TEST(test_publish_state_string_still_works);
    RUN_TEST(test_publish_state_string_literal);

    // Command parse behaviours the char* rewrite could drop
    RUN_TEST(test_ha_command_entity_id_over_63_truncates_and_warns);
    RUN_TEST(test_ha_command_payload_over_127_truncates_and_warns);
    RUN_TEST(test_a_topic_outside_the_discovery_prefix_says_nothing);
    RUN_TEST(test_an_empty_discovery_prefix_parses_everything_again);
    RUN_TEST(test_a_prefix_changed_at_runtime_moves_the_command_filter);
    RUN_TEST(test_shutdown_removes_the_discovery_it_published);
    RUN_TEST(test_ha_command_topic_with_one_slash_is_refused);
    RUN_TEST(test_ha_commands_received_counts_only_known_entities);

    // char[] field tests
    RUN_TEST(test_ha_set_field_truncation);
    RUN_TEST(test_ha_set_field_null_input);
    RUN_TEST(test_ha_node_id_processing);
    RUN_TEST(test_ha_config_no_heap_allocation);

    return UNITY_END();
}

int main(int argc, char** argv) { return runAllTests(); }
