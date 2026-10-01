/**
 * @file test_ha_discovery.cpp
 * @brief HomeAssistant component: discovery documents and the connect burst.
 */

#include <unity.h>
#include "../ha_test_support.h"

// ============================================================================
// Baselines pinned before the discovery payload gained fields
// ============================================================================

// Captures the config document HomeAssistant publishes for one entity id.
// The caller owns `captured`: the subscription outlives this call (shutdown()
// publishes to the same topic), so the referent must outlive the core.
static void captureDiscoveryConfig(Core& core, const char* configTopic, String& captured) {
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&captured, configTopic](const MQTTPublishEvent& ev) {
            if (strcmp(ev.topic, configTopic) == 0) captured = ev.payload;
        });
    simulateMqttConnect(core);
}

void test_discovery_config_for_a_sensor_is_this_exact_document() {
    // The bytes sent for a sensor with a unit and an icon. Optional fields are
    // emitted only when set, and a payload equal to Home Assistant's default is
    // not stated, so nothing else belongs in this string.
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    HA::setField(config.deviceName, "Test Device", sizeof(config.deviceName));
    HA::setField(config.manufacturer, "TestMfg", sizeof(config.manufacturer));
    HA::setField(config.model, "TestModel", sizeof(config.model));
    HA::setField(config.swVersion, "2.0.0", sizeof(config.swVersion));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSensor("free_heap", "Free Heap", "bytes", "", "mdi:memory");
    core.addComponent(std::move(ha));
    core.begin();

    String payload;
    captureDiscoveryConfig(core, "homeassistant/sensor/test_node/free_heap/config", payload);
    TEST_ASSERT_EQUAL_STRING(
        "{\"name\":\"Free Heap\",\"uniq_id\":\"test_node_free_heap\","
        "\"stat_t\":\"homeassistant/sensor/test_node/free_heap/state\","
        "\"ic\":\"mdi:memory\","
        "\"dev\":{\"ids\":[\"test_node\"],\"name\":\"Test Device\","
        "\"mdl\":\"TestModel\",\"mf\":\"TestMfg\",\"sw\":\"2.0.0\"},"
        "\"avty_t\":\"homeassistant/test_node/availability\","
        "\"unit_of_meas\":\"bytes\",\"stat_cla\":\"measurement\"}",
        payload.c_str());
    core.shutdown();
}

// Discovery leaves one document per loop, so the connect burst never holds more
// than a few events in the queue, whatever the number of entities.
struct ConnectCapture {
    int configs = 0;
    int states = 0;
    int removals = 0;  // config topics with an empty payload
    int discoveryDone = 0;
};

static HomeAssistantComponent* addHaWithSensors(Core& core, int n, ConnectCapture& cap) {
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    auto ha = std::make_unique<HomeAssistantComponent>(config);
    for (int i = 0; i < n; i++) ha->addSensor(String("s") + i, String("Sensor ") + i);
    HomeAssistantComponent* raw = ha.get();
    core.addComponent(std::move(ha));
    core.begin();
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&cap](const MQTTPublishEvent& ev) {
            if (strstr(ev.topic, "/config") != nullptr) {
                if (ev.payload[0] == '\0') cap.removals++;
                else cap.configs++;
            }
            else if (strstr(ev.topic, "/state") != nullptr) cap.states++;
        });
    core.on<int>(DomoticsCore::HAEvents::EVENT_DISCOVERY_PUBLISHED,
        [&cap](const int&) { cap.discoveryDone++; });
    return raw;
}

// Well past what the queue could hold at once: the old burst dropped here.
static int manySensors() {
    using DomoticsCore::Utils::QueueCost;
    return (int)(3 * QueueCost::kBudgetBytes /
                 QueueCost::of(sizeof(MQTTPublishEvent), strlen(DomoticsCore::MQTTEvents::EVENT_PUBLISH)));
}

void test_every_sensor_reaches_the_bus_at_connect_without_a_drop() {
    const int n = manySensors();
    Core core;
    ConnectCapture cap;
    addHaWithSensors(core, n, cap);
    simulateMqttConnect(core);
    for (int i = 0; i < n + 5; i++) core.loop();
    TEST_ASSERT_EQUAL_INT(n, cap.configs);
    TEST_ASSERT_EQUAL_INT(1, cap.discoveryDone);
    TEST_ASSERT_EQUAL_UINT32(0, core.getEventBus().getDroppedCount());
    core.shutdown();
}

void test_the_connect_burst_holds_a_few_events_not_one_per_entity() {
    using DomoticsCore::Utils::QueueCost;
    const int n = manySensors();
    Core core;
    ConnectCapture cap;
    addHaWithSensors(core, n, cap);
    simulateMqttConnect(core);
    for (int i = 0; i < n + 5; i++) core.loop();
    // The connect handler's availability and subscription, plus one document.
    const size_t oneDoc = QueueCost::of(sizeof(MQTTPublishEvent), strlen(DomoticsCore::MQTTEvents::EVENT_PUBLISH));
    const unsigned boundPct = (unsigned)((3 * oneDoc * 100 + QueueCost::kBudgetBytes - 1) / QueueCost::kBudgetBytes);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(boundPct, core.getEventBus().getQueueHighWaterPct());
    core.shutdown();
}

void test_a_state_published_during_discovery_is_sent_not_held() {
    const int n = 12;
    Core core;
    ConnectCapture cap;
    HomeAssistantComponent* ha = addHaWithSensors(core, n, cap);
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    core.loop();
    core.loop();
    TEST_ASSERT_TRUE(ha->isDiscoveryPending());
    ha->publishState("s0", "42");
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)ha->getPendingPublishCount());
    core.loop();
    TEST_ASSERT_EQUAL_INT(1, cap.states);
    for (int i = 0; i < n + 10; i++) core.loop();
    TEST_ASSERT_EQUAL_INT(n, cap.configs);
    core.shutdown();
}

void test_remove_discovery_cancels_a_pass() {
    const int n = 12;
    Core core;
    ConnectCapture cap;
    HomeAssistantComponent* ha = addHaWithSensors(core, n, cap);
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    for (int i = 0; i < 3; i++) core.loop();
    const int sentBefore = cap.configs;
    TEST_ASSERT_TRUE(sentBefore > 0 && sentBefore < n);
    ha->removeDiscovery();
    for (int i = 0; i < n + 5; i++) core.loop();
    TEST_ASSERT_FALSE(ha->isDiscoveryPending());
    TEST_ASSERT_EQUAL_INT(n, cap.removals);
    // At most the document the loop dispatching the removal had already queued.
    TEST_ASSERT_LESS_OR_EQUAL_INT(sentBefore + 1, cap.configs);
    TEST_ASSERT_EQUAL_INT(0, cap.discoveryDone);
    core.shutdown();
}

void test_a_disconnect_pauses_discovery_and_a_reconnect_restarts_it() {
    const int n = 12;
    Core core;
    ConnectCapture cap;
    HomeAssistantComponent* ha = addHaWithSensors(core, n, cap);
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    for (int i = 0; i < 4; i++) core.loop();
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_DISCONNECTED, false);
    core.loop();  // the loop that dispatches the drop may still send one
    const int sentBeforeDrop = cap.configs;
    TEST_ASSERT_TRUE(sentBeforeDrop > 0 && sentBeforeDrop < n);
    for (int i = 0; i < n + 5; i++) core.loop();
    TEST_ASSERT_EQUAL_INT(sentBeforeDrop, cap.configs);
    TEST_ASSERT_EQUAL_INT(0, cap.discoveryDone);
    simulateMqttConnect(core);
    for (int i = 0; i < n + 5; i++) core.loop();
    TEST_ASSERT_EQUAL_INT(sentBeforeDrop + n, cap.configs);
    TEST_ASSERT_EQUAL_INT(1, cap.discoveryDone);
    TEST_ASSERT_FALSE(ha->isDiscoveryPending());
    core.shutdown();
}


void test_discovery_config_with_the_diagnostic_fields_emits_exactly_them() {
    // The fields a system diagnostic entity needs — a category, a shared
    // state topic with a template, an attributes topic — and, for the entity
    // that must stay readable while the device is down, no availability block.
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    HA::setField(config.deviceName, "Test Device", sizeof(config.deviceName));
    HA::setField(config.manufacturer, "TestMfg", sizeof(config.manufacturer));
    HA::setField(config.model, "TestModel", sizeof(config.model));
    HA::setField(config.swVersion, "2.0.0", sizeof(config.swVersion));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSensor("sys_last_death", "Last Death");
    HAEntity* e = ha->entity("sys_last_death");
    TEST_ASSERT_NOT_NULL(e);
    e->entityCategory = "diagnostic";
    e->stateTopicOverride = "dev/crash";
    e->valueTemplate = "{{ value_json.promotion }}";
    e->jsonAttributesTopic = "dev/crash";
    e->useAvailability = false;
    TEST_ASSERT_NULL(ha->entity("nobody"));
    core.addComponent(std::move(ha));
    core.begin();

    String payload;
    captureDiscoveryConfig(core, "homeassistant/sensor/test_node/sys_last_death/config", payload);
    TEST_ASSERT_EQUAL_STRING(
        "{\"name\":\"Last Death\",\"uniq_id\":\"test_node_sys_last_death\","
        "\"stat_t\":\"dev/crash\","
        "\"dev\":{\"ids\":[\"test_node\"],\"name\":\"Test Device\","
        "\"mdl\":\"TestModel\",\"mf\":\"TestMfg\",\"sw\":\"2.0.0\"},"
        "\"ent_cat\":\"diagnostic\","
        "\"val_tpl\":\"{{ value_json.promotion }}\","
        "\"json_attr_t\":\"dev/crash\"}",
        payload.c_str());
    core.shutdown();
}

void test_a_duplicate_entity_id_warns_and_registers_both() {
    // Nothing refuses the second registration — that is the behaviour being
    // pinned, not endorsed — but it is no longer silent, through every add*().
    static int warns; warns = 0;
    auto cb = LoggerCallbacks::addCallback([](LogLevel level, const char*, const char* msg) {
        if (level == LOG_LEVEL_WARN && strstr(msg, "already registered")) warns++;
    });
    HomeAssistantComponent ha;
    ha.addSensor("uptime", "Uptime");
    TEST_ASSERT_EQUAL_INT(0, warns);
    ha.addSensor("uptime", "Uptime again");
    TEST_ASSERT_EQUAL_INT(1, warns);
    ha.addSwitch("uptime", "As a switch");
    ha.addButton("uptime", "As a button");
    ha.addBinarySensor("uptime", "As a binary sensor");
    ha.addLight("uptime", "As a light");
    TEST_ASSERT_EQUAL_INT(5, warns);
    TEST_ASSERT_EQUAL_UINT32(6, ha.getStatistics().entityCount);
    LoggerCallbacks::removeCallback(cb);
}

void test_a_state_published_through_the_component_lands_on_the_overridden_topic() {
    // Discovery told Home Assistant to read the override; publishState must
    // write there too, or the entity never updates, silently.
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSensor("free_heap", "Free Heap", "B");
    ha->entity("free_heap")->stateTopicOverride = "dev/telemetry";
    ha->entity("free_heap")->jsonAttributesTopic = "dev/attrs";
    core.addComponent(std::move(ha));
    core.begin();
    String stateTopic, attrTopic;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            if (strcmp(ev.payload, "42") == 0) stateTopic = ev.topic;
            if (strstr(ev.payload, "\"k\":1")) attrTopic = ev.topic;
        });
    simulateMqttConnect(core);
    haPtr->publishState("free_heap", "42");
    JsonDocument attrs; attrs["k"] = 1;
    haPtr->publishAttributes("free_heap", attrs);
    for (int i = 0; i < 5; i++) core.loop();
    TEST_ASSERT_EQUAL_STRING("dev/telemetry", stateTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("dev/attrs", attrTopic.c_str());
    core.shutdown();
}

void test_a_discovery_config_over_the_event_field_is_refused_aloud() {
    // 700 bytes and up would be cut mid-JSON and sit retained on the broker,
    // rejected by Home Assistant at every restart: refused instead, with a WARN.
    static int warns; warns = 0;
    auto cb = LoggerCallbacks::addCallback([](LogLevel level, const char*, const char* msg) {
        if (level == LOG_LEVEL_WARN && strstr(msg, "not published")) warns++;
    });
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    auto ha = std::make_unique<HomeAssistantComponent>(config);
    String longName;
    for (int i = 0; i < 40; i++) longName += "0123456789";   // 400 chars of name
    ha->addSensor("big", longName, "B");
    ha->entity("big")->valueTemplate = "{{ value_json.a_rather_long_key_name_to_push_the_document_past_the_cap }}";
    core.addComponent(std::move(ha));
    core.begin();
    String payload;
    captureDiscoveryConfig(core, "homeassistant/sensor/test_node/big/config", payload);
    TEST_ASSERT_EQUAL_INT(1, warns);
    TEST_ASSERT_EQUAL_STRING("", payload.c_str());
    LoggerCallbacks::removeCallback(cb);
    core.shutdown();
}

// A topic the event field cannot carry is refused, not cut. The shape matters:
// a long prefix and node on a SENSOR keeps the document well under the payload
// ceiling, so the payload guard cannot fire and take the credit — the discovery
// is refused for its topic or not at all.
void test_a_topic_over_the_event_field_is_refused_and_counted() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "abcdefghijklmnopqrstuvwxyz012345", sizeof(config.nodeId));
    HA::setField(config.discoveryPrefix, "homeassistant_with_a_long_prefix", sizeof(config.discoveryPrefix));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    // 50 characters: prefix 32 + "/sensor/" + node 32 + the id + "/config" = 130.
    ha->addSensor("a_sensor_id_of_exactly_fifty_characters_0123456789", "Sensor");
    core.addComponent(std::move(ha));
    core.begin();

    bool publishedAnything = false;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            // The cut loses "/config", so matching on it would be vacuous.
            if (strstr(ev.topic, "a_sensor_id_of_exactly_fifty")) publishedAnything = true;
        });

    startLogCapture();
    simulateMqttConnect(core);
    stopLogCapture();
    String warn = g_capturedWarn;

    TEST_ASSERT_FALSE_MESSAGE(publishedAnything,
        "a document whose topic does not fit the event field was published on a cut topic");
    TEST_ASSERT_TRUE_MESSAGE(warn.indexOf("Topic for 'a_sensor_id_of_exactly_fifty_characters_0123456789' needs 130 chars, over the 127-char event field: not published") >= 0,
        "the topic was cut in silence");
    TEST_ASSERT_TRUE_MESSAGE(warn.indexOf("event field: not published") == warn.lastIndexOf("event field: not published"),
        "the payload guard fired too: this shape no longer isolates the topic");
    TEST_ASSERT_EQUAL_UINT32(1, haPtr->getStatistics().discoveryRefused);

    core.shutdown();
}

void test_an_invalid_entity_category_is_left_out_and_warned() {
    static int warns; warns = 0;
    auto cb = LoggerCallbacks::addCallback([](LogLevel level, const char*, const char* msg) {
        if (level == LOG_LEVEL_WARN && strstr(msg, "entity_category")) warns++;
    });
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSensor("s", "S");
    ha->entity("s")->entityCategory = "diagnostics";   // the plural is not a category
    core.addComponent(std::move(ha));
    core.begin();
    String payload;
    captureDiscoveryConfig(core, "homeassistant/sensor/test_node/s/config", payload);
    TEST_ASSERT_EQUAL_INT(1, warns);
    TEST_ASSERT_TRUE(payload.indexOf("entity_category") < 0);
    LoggerCallbacks::removeCallback(cb);
    core.shutdown();
}

static int runAllTests() {
    UNITY_BEGIN();

    // Baselines pinned before the discovery payload gained fields
    RUN_TEST(test_discovery_config_for_a_sensor_is_this_exact_document);
    RUN_TEST(test_every_sensor_reaches_the_bus_at_connect_without_a_drop);
    RUN_TEST(test_the_connect_burst_holds_a_few_events_not_one_per_entity);
    RUN_TEST(test_a_state_published_during_discovery_is_sent_not_held);
    RUN_TEST(test_remove_discovery_cancels_a_pass);
    RUN_TEST(test_a_disconnect_pauses_discovery_and_a_reconnect_restarts_it);

    // The discovery fields and the duplicate-id warning
    RUN_TEST(test_discovery_config_with_the_diagnostic_fields_emits_exactly_them);
    RUN_TEST(test_a_duplicate_entity_id_warns_and_registers_both);
    RUN_TEST(test_a_state_published_through_the_component_lands_on_the_overridden_topic);
    RUN_TEST(test_a_discovery_config_over_the_event_field_is_refused_aloud);
    RUN_TEST(test_a_topic_over_the_event_field_is_refused_and_counted);
    RUN_TEST(test_an_invalid_entity_category_is_left_out_and_warned);

    return UNITY_END();
}

int main(int argc, char** argv) { return runAllTests(); }
