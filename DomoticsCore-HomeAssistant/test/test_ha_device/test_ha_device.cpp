/**
 * @file test_ha_device.cpp
 * @brief The discovery documents a board actually builds, measured on the board.
 *
 * The host suites measure these documents against a String and an ArduinoJson
 * the host provides; a panel that fits by fourteen characters is a claim about
 * the board's. No broker is needed: the component publishes on the connect event
 * and the documents cross the EventBus, where the field that refuses them lives.
 */

#include <Arduino.h>
#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

static Core* testCore = nullptr;
static String capturedConfig;
static size_t capturedLength = 0;

void setUp(void) {
    testCore = new Core();
    capturedConfig = "";
    capturedLength = 0;
}

void tearDown(void) {
    if (testCore) {
        testCore->shutdown();
        delete testCore;
        testCore = nullptr;
    }
}

/** @brief Register a panel, connect, and keep the config document it publishes.
 *
 * The shapes mirror the host suites exactly — same node id length, same entity
 * id, same icon — so a figure that differs here is the board's String or JSON
 * differing, and nothing else.
 */
static HomeAssistantComponent* publishPanel(const char* nodeId, const char* entityId,
                                            const char* entityName, const char* icon,
                                            AlarmFeature features,
                                            const char* code = nullptr) {
    HAConfig config;
    HA::setField(config.nodeId, nodeId, sizeof(config.nodeId));
    if (code) {
        HA::setField(config.configUrl, "http://10.0.0.100:80/", sizeof(config.configUrl));
        HA::setField(config.suggestedArea, "Living Room", sizeof(config.suggestedArea));
    }

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    if (code) {
        ha->addAlarmControlPanel(entityId, entityName, icon, features, code, true, true, true);
    } else {
        ha->addAlarmControlPanel(entityId, entityName, icon, features);
    }
    testCore->addComponent(std::move(ha));
    testCore->begin();

    testCore->on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [](const MQTTPublishEvent& ev) {
            if (strstr(ev.topic, "alarm_control_panel") && strstr(ev.topic, "/config")) {
                capturedLength = strlen(ev.payload);
                capturedConfig = ev.payload;
            }
        });

    testCore->emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    for (int i = 0; i < 8; i++) testCore->loop();
    return haPtr;
}

/** @brief The two-arm-mode code-less panel the host suites measure at 541. */
static HomeAssistantComponent* publishConsumerPanel() {
    return publishPanel("device_seventeen1", "alarm_control", "Alarm Control",
                        "mdi:shield-home", AlarmFeature::ArmAway | AlarmFeature::ArmNight);
}

// The shape a consumer runs: no code, two arm modes. Its length is what decides
// whether the entity exists at all, and it is measured here on the board's own
// String and JSON rather than on the host's.
void test_a_code_less_panel_fits_the_event_field_on_the_board() {
    HomeAssistantComponent* ha = publishConsumerPanel();

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(541, (uint32_t)capturedLength,
        "the board builds a different document from the host suites");
    TEST_ASSERT_EQUAL_UINT32(0, ha->getStatistics().discoveryRefused);
}

// Home Assistant reads an absent code_*_required as true, which is the reverse of
// this component's default, so both keys must be on the wire and false.
void test_the_requirement_keys_are_on_the_wire() {
    publishConsumerPanel();

    TEST_ASSERT_TRUE_MESSAGE(capturedConfig.indexOf("\"cod_arm_req\":false") >= 0,
        "a code-less panel published no cod_arm_req and cannot be armed");
    TEST_ASSERT_TRUE_MESSAGE(capturedConfig.indexOf("\"cod_dis_req\":false") >= 0,
        "a code-less panel published no cod_dis_req");
    TEST_ASSERT_TRUE_MESSAGE(capturedConfig.indexOf("pl_arm") < 0,
        "a payload key restating Home Assistant's default is back in the document");
}

// The widest shape a panel can take, and the one the payload keys used to push
// over the field: every arm mode, no code.
void test_every_arm_mode_still_fits() {
    HomeAssistantComponent* ha = publishPanel("test_node", "alarm", "Alarm Panel",
        "mdi:shield-home",
        AlarmFeature::ArmHome | AlarmFeature::ArmAway | AlarmFeature::ArmNight |
        AlarmFeature::ArmVacation | AlarmFeature::ArmCustomBypass | AlarmFeature::Trigger);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(552, (uint32_t)capturedLength,
        "the widest panel moved: re-derive the figures the reference states");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, ha->getStatistics().discoveryRefused,
        "the widest code-less panel is over the field again");
}

// And the shape nothing rescues: the same panel with a code, a long node id.
// Refused whole rather than published cut, and counted.
void test_a_document_over_the_field_is_refused_on_the_board() {
    HomeAssistantComponent* ha = publishPanel("abcdefghijklmnopqrstuvwxyz012345",
        "alarm", "Alarm Panel", "mdi:shield-lock",
        AlarmFeature::ArmHome | AlarmFeature::ArmAway | AlarmFeature::ArmNight |
        AlarmFeature::ArmVacation | AlarmFeature::ArmCustomBypass | AlarmFeature::Trigger,
        "5678");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, (uint32_t)capturedLength,
        "a document over the event field reached the bus, cut");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, ha->getStatistics().discoveryRefused,
        "the refusal was not counted");
}

// Publishing discovery is not free, but it must not cost anything that stays:
// the component rebuilds the same documents on every reconnection.
void test_republishing_discovery_leaves_the_heap_where_it_was() {
    publishConsumerPanel();

    const uint32_t before = HAL::Platform::getFreeHeap();
    for (int round = 0; round < 5; ++round) {
        testCore->emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
        for (int i = 0; i < 8; i++) testCore->loop();
    }
    const uint32_t after = HAL::Platform::getFreeHeap();

    char note[80];
    snprintf(note, sizeof(note), "five republishes cost %d bytes", (int)(before - after));
    TEST_ASSERT_TRUE_MESSAGE(after + 64 >= before, note);
}

int runAllTests() {
    UNITY_BEGIN();
    RUN_TEST(test_a_code_less_panel_fits_the_event_field_on_the_board);
    RUN_TEST(test_the_requirement_keys_are_on_the_wire);
    RUN_TEST(test_every_arm_mode_still_fits);
    RUN_TEST(test_a_document_over_the_field_is_refused_on_the_board);
    RUN_TEST(test_republishing_discovery_leaves_the_heap_where_it_was);
    return UNITY_END();
}

void setup() {
    delay(2000);
    runAllTests();
}

void loop() {}
