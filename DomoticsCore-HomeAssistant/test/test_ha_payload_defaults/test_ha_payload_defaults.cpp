/**
 * @file test_ha_payload_defaults.cpp
 * @brief A discovery document states a payload only where it differs from the
 *        value Home Assistant assumes for an absent key.
 *
 * Each key restating a default costs characters against the event field a
 * document crosses, and a document over that field is refused whole. The
 * defaults themselves are pinned here, since nothing on the wire states them.
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

void setUp() {}
void tearDown() {}

static const char* kAvailability = "homeassistant/node1/availability";

static void build(const HAEntity& entity, JsonDocument& doc) {
    JsonDocument deviceDoc;
    JsonObject device = deviceDoc.to<JsonObject>();
    device["name"] = "TestDevice";
    entity.buildDiscoveryPayload(doc, "node1", "homeassistant", device, kAvailability);
}

static void assertAvailabilityWithoutPayloads(const HAEntity& entity) {
    JsonDocument doc;
    build(entity, doc);
    TEST_ASSERT_EQUAL_STRING(kAvailability, doc["avty_t"].as<String>().c_str());
    TEST_ASSERT_TRUE_MESSAGE(doc["pl_avail"].isNull(), "pl_avail restates Home Assistant's default");
    TEST_ASSERT_TRUE_MESSAGE(doc["pl_not_avail"].isNull(), "pl_not_avail restates Home Assistant's default");
}

void test_the_defaults_are_home_assistants_own() {
    TEST_ASSERT_EQUAL_STRING("online", HAPayload::AVAILABLE);
    TEST_ASSERT_EQUAL_STRING("offline", HAPayload::NOT_AVAILABLE);
    TEST_ASSERT_EQUAL_STRING("ON", HAPayload::ON);
    TEST_ASSERT_EQUAL_STRING("OFF", HAPayload::OFF);
    TEST_ASSERT_EQUAL_STRING("PRESS", HAPayload::PRESS);
}

// A document no longer names its availability payloads, so the broker's will and
// setAvailable() are the only places they exist; both must say Home Assistant's.
void test_the_will_and_the_availability_publish_the_defaults() {
    TEST_ASSERT_EQUAL_STRING(HAPayload::NOT_AVAILABLE, MQTTConfig().lwtMessage.c_str());

    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "node1", sizeof(config.nodeId));
    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    core.addComponent(std::move(ha));
    core.begin();

    String seen;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            if (strstr(ev.topic, "/availability")) { seen += ev.payload; seen += ";"; }
        });
    haPtr->setAvailable(true);
    haPtr->setAvailable(false);
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING((String(HAPayload::AVAILABLE) + ";" + HAPayload::NOT_AVAILABLE + ";").c_str(),
                             seen.c_str());
    core.shutdown();
}

void test_no_entity_states_the_availability_payloads() {
    assertAvailabilityWithoutPayloads(HASensor("t", "Temperature"));
    assertAvailabilityWithoutPayloads(HASwitch("r", "Relay"));
    assertAvailabilityWithoutPayloads(HABinarySensor("m", "Motion"));
    assertAvailabilityWithoutPayloads(HAButton("b", "Restart"));
    assertAvailabilityWithoutPayloads(HALight("l", "Lamp"));
}

// The button builds its own document and must honour the opt-out like every
// other entity.
void test_a_button_honours_the_availability_opt_out() {
    HAButton button("b", "Restart");
    button.useAvailability = false;
    JsonDocument doc;
    build(button, doc);
    TEST_ASSERT_TRUE(doc["avty_t"].isNull());
}

void test_a_default_switch_states_no_payload() {
    JsonDocument doc;
    build(HASwitch("r", "Relay"), doc);
    TEST_ASSERT_TRUE(doc["pl_on"].isNull());
    TEST_ASSERT_TRUE(doc["pl_off"].isNull());
    TEST_ASSERT_TRUE(doc["stat_on"].isNull());
    TEST_ASSERT_TRUE(doc["stat_off"].isNull());
    TEST_ASSERT_FALSE(doc["cmd_t"].isNull());
}

// Home Assistant reads state_on as payload_on when it is absent, so an override
// is stated once, as the command payload.
void test_a_switch_states_only_the_payload_its_caller_changed() {
    HASwitch relay("r", "Relay");
    relay.payloadOn = "1";
    JsonDocument doc;
    build(relay, doc);
    TEST_ASSERT_EQUAL_STRING("1", doc["pl_on"].as<String>().c_str());
    TEST_ASSERT_TRUE(doc["pl_off"].isNull());
    TEST_ASSERT_TRUE(doc["stat_on"].isNull());
    TEST_ASSERT_TRUE(doc["stat_off"].isNull());
}

void test_a_binary_sensor_states_only_the_payload_its_caller_changed() {
    JsonDocument plain;
    build(HABinarySensor("m", "Motion"), plain);
    TEST_ASSERT_TRUE(plain["pl_on"].isNull());
    TEST_ASSERT_TRUE(plain["pl_off"].isNull());

    HABinarySensor door("d", "Door");
    door.payloadOff = "closed";
    JsonDocument doc;
    build(door, doc);
    TEST_ASSERT_TRUE(doc["pl_on"].isNull());
    TEST_ASSERT_EQUAL_STRING("closed", doc["pl_off"].as<String>().c_str());
}

void test_a_button_states_its_press_payload_only_when_changed() {
    JsonDocument plain;
    build(HAButton("b", "Restart"), plain);
    TEST_ASSERT_TRUE(plain["pl_prs"].isNull());
    TEST_ASSERT_FALSE(plain["cmd_t"].isNull());

    HAButton custom("c", "Calibrate");
    custom.payloadPress = "GO";
    JsonDocument doc;
    build(custom, doc);
    TEST_ASSERT_EQUAL_STRING("GO", doc["pl_prs"].as<String>().c_str());
}

// brightness_scale defaults to 255, and "brightness" is a key of the JSON schema
// only: the default schema this light declares strips it before reading.
void test_a_light_states_no_default_and_no_foreign_key() {
    JsonDocument doc;
    build(HALight("l", "Lamp"), doc);
    TEST_ASSERT_TRUE(doc["pl_on"].isNull());
    TEST_ASSERT_TRUE(doc["pl_off"].isNull());
    TEST_ASSERT_TRUE(doc["bri_scl"].isNull());
    TEST_ASSERT_TRUE(doc["brightness"].isNull());
    TEST_ASSERT_FALSE(doc["bri_cmd_t"].isNull());
    TEST_ASSERT_FALSE(doc["bri_stat_t"].isNull());
    TEST_ASSERT_EQUAL_STRING("brightness", doc["on_cmd_type"].as<String>().c_str());
}

int runAllTests() {
    UNITY_BEGIN();
    RUN_TEST(test_the_defaults_are_home_assistants_own);
    RUN_TEST(test_the_will_and_the_availability_publish_the_defaults);
    RUN_TEST(test_no_entity_states_the_availability_payloads);
    RUN_TEST(test_a_button_honours_the_availability_opt_out);
    RUN_TEST(test_a_default_switch_states_no_payload);
    RUN_TEST(test_a_switch_states_only_the_payload_its_caller_changed);
    RUN_TEST(test_a_binary_sensor_states_only_the_payload_its_caller_changed);
    RUN_TEST(test_a_button_states_its_press_payload_only_when_changed);
    RUN_TEST(test_a_light_states_no_default_and_no_foreign_key);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() { runAllTests(); }
void loop() {}
#else
int main(int argc, char** argv) { return runAllTests(); }
#endif
