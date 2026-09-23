/**
 * @file test_ha_state_republish.cpp
 * @brief What a broker outage holds, and what comes back when the link returns.
 *
 * Tests cover:
 * - a state published while the broker is unreachable reaches it after the
 *   reconnection, behind that reconnection's discovery documents
 * - two states for one entity in one outage produce one publish, the last
 * - the JSON state path and the attributes path are held on the same terms
 * - the flush is paced, so a reconnection never overruns the event queue
 * - a payload larger than the event field is refused rather than held
 * - an outage during the flush keeps what the flush had not sent
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/HAEvents.h>
#include <DomoticsCore/ArduinoJsonString.h>
#include <DomoticsCore/MQTT.h>

#include <vector>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

// ============================================================================
// Harness
// ============================================================================

// Every mqtt/publish the component emits, in order. The EventBus hands a raw
// pointer to a handler with no state of its own, so the log is a file static.
static std::vector<String> g_topics;
static std::vector<String> g_payloads;

static void recordPublishes(Core& core) {
    g_topics.clear();
    g_payloads.clear();
    core.getEventBus().subscribe(String(DomoticsCore::MQTTEvents::EVENT_PUBLISH),
        [](const void* data) {
            auto& ev = *reinterpret_cast<const MQTTPublishEvent*>(data);
            g_topics.push_back(String(ev.topic));
            g_payloads.push_back(String(ev.payload));
        }, nullptr);
}

static void pump(Core& core, int loops = 12) {
    for (int i = 0; i < loops; i++) core.loop();
}

static void connect(Core& core) {
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    pump(core);
}

static void disconnect(Core& core) {
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_DISCONNECTED, true);
    pump(core, 3);
}

// First index whose topic ends with `suffix`, or -1.
static int indexOfTopicEnding(const char* suffix) {
    String needle(suffix);
    for (size_t i = 0; i < g_topics.size(); ++i) {
        const String& t = g_topics[i];
        if (t.length() >= needle.length() &&
            t.substring(t.length() - needle.length()) == needle) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static int countTopicsEnding(const char* suffix) {
    String needle(suffix);
    int n = 0;
    for (size_t i = 0; i < g_topics.size(); ++i) {
        const String& t = g_topics[i];
        if (t.length() >= needle.length() &&
            t.substring(t.length() - needle.length()) == needle) {
            ++n;
        }
    }
    return n;
}

// ============================================================================
// The state a disconnected broker never received
// ============================================================================

void test_state_published_offline_arrives_after_the_reconnection() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    haPtr->addBinarySensor("door", "Front Door", "door");
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    haPtr->publishState("door", "ON");
    pump(core);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countTopicsEnding("/door/state"),
                                  "a state left the device while the broker was down");

    connect(core);
    pump(core);

    const int state = indexOfTopicEnding("/door/state");
    const int discovery = indexOfTopicEnding("/door/config");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, state, "the held state was never republished");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, discovery, "no discovery document was published");
    TEST_ASSERT_GREATER_THAN_MESSAGE(discovery, state,
                                     "the state was published before its discovery document");
    TEST_ASSERT_EQUAL_STRING("ON", g_payloads[state].c_str());
}

void test_two_states_in_one_outage_publish_once_with_the_last_value() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    haPtr->addBinarySensor("door", "Front Door", "door");
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    haPtr->publishState("door", "ON");
    haPtr->publishState("door", "OFF");
    pump(core);

    connect(core);
    pump(core);

    // The discriminator against a plain queue: a queue would deliver both.
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countTopicsEnding("/door/state"),
                                  "the outage's states were queued, not coalesced");
    const int state = indexOfTopicEnding("/door/state");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("OFF", g_payloads[state].c_str(),
                                     "the first value of the outage was republished, not the last");
}

void test_the_json_state_path_is_held_on_the_same_terms() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    haPtr->addLight("lamp", "Lamp");
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    JsonDocument doc;
    doc["state"] = "ON";
    doc["brightness"] = 128;
    haPtr->publishStateJson("lamp", doc);
    pump(core);

    connect(core);
    pump(core);

    const int state = indexOfTopicEnding("/lamp/state");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, state, "the light's held JSON state was never republished");
    TEST_ASSERT_TRUE_MESSAGE(g_payloads[state].indexOf("\"brightness\":128") >= 0,
                             "the republished payload is not the one that was held");
}

void test_attributes_are_held_too() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    haPtr->addSensor("temp", "Temperature", "°C", "temperature");
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    JsonDocument attrs;
    attrs["calibrated"] = true;
    haPtr->publishAttributes("temp", attrs);
    pump(core);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countTopicsEnding("/temp/attributes"),
                                  "attributes left the device while the broker was down");

    connect(core);
    pump(core);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countTopicsEnding("/temp/attributes"),
                                  "the held attributes were never republished");
}

void test_state_and_attributes_hold_separate_slots() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    haPtr->addSensor("temp", "Temperature", "°C", "temperature");
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    haPtr->publishState("temp", "21.50");
    JsonDocument attrs;
    attrs["calibrated"] = true;
    haPtr->publishAttributes("temp", attrs);
    pump(core);

    connect(core);
    pump(core);

    // One entity, two topics: coalescing by entity alone would lose one.
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countTopicsEnding("/temp/state"), "the state slot was lost");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countTopicsEnding("/temp/attributes"),
                                  "the attributes slot was lost");
}

// ============================================================================
// What the flush must not do to the event queue
// ============================================================================

void test_the_flush_never_overruns_the_event_queue() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    // Twenty entities: one discovery document each fits the budget, one
    // document plus one state each does not — unless the flush is paced. The
    // drop counter is read from the start: the first connect's own burst of
    // twenty-one events has to stay inside the budget too.
    for (int i = 0; i < 20; ++i) {
        char id[8];
        snprintf(id, sizeof(id), "s%d", i);
        haPtr->addBinarySensor(id, id, "door");
    }
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    for (int i = 0; i < 20; ++i) {
        char id[8];
        snprintf(id, sizeof(id), "s%d", i);
        haPtr->publishState(id, "ON");
    }
    pump(core);

    connect(core);
    pump(core, 40);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, core.getEventBus().getDroppedCount(),
                                     "the reconnection's burst evicted events from the queue");
    TEST_ASSERT_EQUAL_INT_MESSAGE(20, countTopicsEnding("/state"),
                                  "the flush did not deliver every held state");
}

void test_a_payload_over_the_event_field_is_refused_not_held() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    haPtr->addSensor("temp", "Temperature", "°C", "temperature");
    haPtr->addSensor("hum", "Humidity", "%", "humidity");
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    String oversized;
    for (size_t i = 0; i < MQTT_EVENT_PAYLOAD_SIZE + 64; ++i) oversized += 'x';
    haPtr->publishState("temp", oversized);
    haPtr->publishState("hum", "60.00");
    pump(core);

    // Two entities, not one: a single entity would hide the refusal, because
    // the second state would overwrite the oversized slot either way. The store
    // is where this is visible — on the wire both are refused, one here and one
    // at the event field.
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, (uint32_t)haPtr->getPendingPublishCount(),
                                     "a payload that can never leave was held anyway");

    connect(core);
    pump(core);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countTopicsEnding("/temp/state"),
                                  "the oversized payload reached the broker");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countTopicsEnding("/hum/state"),
                                  "the publishable state beside it was lost");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, (uint32_t)haPtr->getPendingPublishCount(),
                                     "the store was not released once the flush had drained it");
}

void test_an_outage_during_the_flush_keeps_what_it_had_not_sent() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    for (int i = 0; i < 12; ++i) {
        char id[8];
        snprintf(id, sizeof(id), "s%d", i);
        haPtr->addBinarySensor(id, id, "door");
    }
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    for (int i = 0; i < 12; ++i) {
        char id[8];
        snprintf(id, sizeof(id), "s%d", i);
        haPtr->publishState(id, "ON");
    }
    pump(core);

    // Reconnect, let the flush start, then lose the link again before it ends.
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    core.loop();
    core.loop();
    // The store, not the topics the bus has dispatched: twelve discovery
    // documents sit ahead of the states either way, so counting publishes here
    // reads the same number whether the flush is paced or not.
    const uint32_t stillHeld = (uint32_t)haPtr->getPendingPublishCount();
    disconnect(core);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, stillHeld,
                                     "the flush emptied the store in one pass; the pacing is gone");

    connect(core);
    pump(core, 30);

    TEST_ASSERT_EQUAL_INT_MESSAGE(12, countTopicsEnding("/state"),
                                  "the states the first flush had not reached were dropped");
}

// Pinning, not discriminating: it passes with the whole store removed, and is
// here so a later change that holds every publish is caught.
void test_nothing_is_held_while_the_link_is_up() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    haPtr->addBinarySensor("door", "Front Door", "door");
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);

    recordPublishes(core);
    haPtr->publishState("door", "ON");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, (uint32_t)haPtr->getPendingPublishCount(),
                                     "a state was held while the link was up");
    pump(core);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countTopicsEnding("/door/state"),
                                  "a state published over a live link did not go out");

    // A later reconnection must not replay it: it was never held.
    disconnect(core);
    connect(core);
    pump(core);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countTopicsEnding("/door/state"),
                                  "a state that had already gone out was republished");
}

void test_a_live_publish_supersedes_what_the_outage_held() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    for (int i = 0; i < 12; ++i) {
        char id[8];
        snprintf(id, sizeof(id), "s%d", i);
        haPtr->addBinarySensor(id, id, "door");
    }
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    recordPublishes(core);
    for (int i = 0; i < 12; ++i) {
        char id[8];
        snprintf(id, sizeof(id), "s%d", i);
        haPtr->publishState(id, "ON");
    }
    pump(core);

    // Reconnect and let the drain start, but not reach the last slot: this is
    // the window in which an application republishes its own state, which every
    // example does on isReady().
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    core.loop();
    core.loop();
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, (uint32_t)haPtr->getPendingPublishCount(),
                                     "the drain finished in one pass; this test measures nothing");
    haPtr->publishState("s11", "OFF");
    pump(core, 30);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countTopicsEnding("/s11/state"),
                                  "the outage's value was republished over the live one");
    const int state = indexOfTopicEnding("/s11/state");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("OFF", g_payloads[state].c_str(),
                                     "the stale held value won over the one published live");
}

void test_the_store_has_a_byte_budget_not_only_a_slot_count() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    for (int i = 0; i < 6; ++i) {
        char id[8];
        snprintf(id, sizeof(id), "s%d", i);
        haPtr->addSensor(id, id);
    }
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    // Six payloads of 600 bytes are 3 600, well over the budget: a slot count
    // alone would take all six.
    String big;
    for (int i = 0; i < 600; ++i) big += 'x';
    for (int i = 0; i < 6; ++i) {
        char id[8];
        snprintf(id, sizeof(id), "s%d", i);
        haPtr->publishState(id, big);
    }
    pump(core);

    TEST_ASSERT_LESS_THAN_MESSAGE(6, (uint32_t)haPtr->getPendingPublishCount(),
                                  "the store took every slot regardless of what they weigh");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, haPtr->getStatistics().statesRefused,
                                     "payloads were dropped without being counted");
}

void test_shutdown_releases_what_the_outage_was_holding() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    auto* haPtr = ha.get();
    haPtr->addBinarySensor("door", "Front Door", "door");
    haPtr->addSensor("temp", "Temperature", "°C", "temperature");
    core.addComponent(std::move(ha));
    core.begin();

    connect(core);
    disconnect(core);

    haPtr->publishState("door", "ON");
    haPtr->publishState("temp", "21.50");
    pump(core);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2, (uint32_t)haPtr->getPendingPublishCount(),
                                     "the outage held nothing to release");

    haPtr->shutdown();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, (uint32_t)haPtr->getPendingPublishCount(),
                                     "shutdown left the store holding payloads nothing will drain");
}

// ============================================================================

void setUp() {}
void tearDown() {}

int runAllTests() {
    UNITY_BEGIN();

    RUN_TEST(test_state_published_offline_arrives_after_the_reconnection);
    RUN_TEST(test_two_states_in_one_outage_publish_once_with_the_last_value);
    RUN_TEST(test_the_json_state_path_is_held_on_the_same_terms);
    RUN_TEST(test_attributes_are_held_too);
    RUN_TEST(test_state_and_attributes_hold_separate_slots);
    RUN_TEST(test_the_flush_never_overruns_the_event_queue);
    RUN_TEST(test_a_payload_over_the_event_field_is_refused_not_held);
    RUN_TEST(test_an_outage_during_the_flush_keeps_what_it_had_not_sent);
    RUN_TEST(test_nothing_is_held_while_the_link_is_up);
    RUN_TEST(test_a_live_publish_supersedes_what_the_outage_held);
    RUN_TEST(test_the_store_has_a_byte_budget_not_only_a_slot_count);
    RUN_TEST(test_shutdown_releases_what_the_outage_was_holding);

    return UNITY_END();
}

#ifdef ARDUINO
void setup() { runAllTests(); }
void loop() {}
#else
int main(int argc, char** argv) { return runAllTests(); }
#endif
