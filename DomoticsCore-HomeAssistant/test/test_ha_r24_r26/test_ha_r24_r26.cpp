/**
 * @file test_ha_r24_r26.cpp
 * @brief Tests for R24 (virtual dispatch) and R26 (EventBus command events)
 *
 * Tests cover:
 * - R24: Virtual dispatch via HAEntity* base pointer
 * - R26: ha/command EventBus emission for all entity types
 * - R26: Auto-publish after command for switches
 * - R26: Heap stability for command event cycle
 * - R26: Negative/edge-case tests (empty payload, invalid button, unknown entity)
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/HAEvents.h>
#include <DomoticsCore/ArduinoJsonString.h>
#include <DomoticsCore/Testing/HeapTracker.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;
using namespace DomoticsCore::Testing;

// ============================================================================
// Helpers
// ============================================================================

static void simulateMqttConnect(Core& core) {
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    for (int i = 0; i < 5; i++) core.loop();
}

static void simulateEntityCommand(Core& core, const char* component,
                                   const char* nodeId, const char* entityId,
                                   const char* payload) {
    MQTTMessageEvent msg{};
    String topic = String("homeassistant/") + component + "/" + nodeId + "/" + entityId + "/set";
    strncpy(msg.topic, topic.c_str(), MQTT_EVENT_TOPIC_SIZE - 1);
    msg.topic[MQTT_EVENT_TOPIC_SIZE - 1] = '\0';
    strncpy(msg.payload, payload, MQTT_EVENT_PAYLOAD_SIZE - 1);
    msg.payload[MQTT_EVENT_PAYLOAD_SIZE - 1] = '\0';
    core.emit<MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, msg);
    for (int i = 0; i < 5; i++) core.loop();
}

// ============================================================================
// R24 -- Virtual Dispatch Tests (Task 16)
// ============================================================================

void test_virtual_dispatch_via_base_pointer() {
    HASwitch sw("test", "Test");
    HAEntity* base = &sw;
    base->handleCommand("ON");
    TEST_ASSERT_TRUE(sw.state);
    base->handleCommand("OFF");
    TEST_ASSERT_FALSE(sw.state);
}

void test_virtual_dispatch_light_via_base_pointer() {
    HALight light("test", "Test Light");
    HAEntity* base = &light;
    bool result = base->handleCommand("ON");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(light.state);
    TEST_ASSERT_EQUAL_UINT8(255, light.brightness);
}

void test_virtual_dispatch_button_via_base_pointer() {
    HAButton btn("test", "Test Button");
    HAEntity* base = &btn;
    bool result = base->handleCommand("PRESS");
    TEST_ASSERT_TRUE(result);
    // Invalid payload returns false
    result = base->handleCommand("INVALID");
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// R26 -- EventBus Emission Tests (Task 17)
// ============================================================================

void test_switch_command_emits_ha_command_event() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    char evEntityId[64] = {};
    char evComponent[32] = {};
    char evCommand[128] = {};
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        auto& ev = *reinterpret_cast<const HAEvents::HACommandEvent*>(data);
        eventFired = true;
        strncpy(evEntityId, ev.entityId, sizeof(evEntityId) - 1);
        strncpy(evComponent, ev.component, sizeof(evComponent) - 1);
        strncpy(evCommand, ev.command, sizeof(evCommand) - 1);
    }, nullptr);

    simulateMqttConnect(core);
    simulateEntityCommand(core, "switch", "test_node", "sw1", "ON");

    TEST_ASSERT_TRUE(eventFired);
    TEST_ASSERT_EQUAL_STRING("sw1", evEntityId);
    TEST_ASSERT_EQUAL_STRING("switch", evComponent);
    TEST_ASSERT_EQUAL_STRING("ON", evCommand);

    core.shutdown();
}

void test_light_command_emits_ha_command_event() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addLight("led1", "LED");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    char evComponent[32] = {};
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        auto& ev = *reinterpret_cast<const HAEvents::HACommandEvent*>(data);
        eventFired = true;
        strncpy(evComponent, ev.component, sizeof(evComponent) - 1);
    }, nullptr);

    simulateMqttConnect(core);
    simulateEntityCommand(core, "light", "test_node", "led1", "ON");

    TEST_ASSERT_TRUE(eventFired);
    TEST_ASSERT_EQUAL_STRING("light", evComponent);

    core.shutdown();
}

void test_button_command_emits_ha_command_event() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addButton("btn1", "Button");
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
    simulateEntityCommand(core, "button", "test_node", "btn1", "PRESS");

    TEST_ASSERT_TRUE(eventFired);
    TEST_ASSERT_EQUAL_STRING("PRESS", evCommand);

    core.shutdown();
}

void test_alarm_command_emits_ha_command_event_with_code() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addAlarmControlPanel("alarm1", "Alarm");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    char evCommand[128] = {};
    char evCode[32] = {};
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        auto& ev = *reinterpret_cast<const HAEvents::HACommandEvent*>(data);
        eventFired = true;
        strncpy(evCommand, ev.command, sizeof(evCommand) - 1);
        strncpy(evCode, ev.code, sizeof(evCode) - 1);
    }, nullptr);

    simulateMqttConnect(core);
    simulateEntityCommand(core, "alarm_control_panel", "test_node", "alarm1", "ARM_AWAY 1234");

    TEST_ASSERT_TRUE(eventFired);
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", evCommand);
    TEST_ASSERT_EQUAL_STRING("1234", evCode);

    core.shutdown();
}

void test_switch_auto_publish_after_command_event() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");  // autoPublishState=true by default
    core.addComponent(std::move(ha));
    core.begin();

    bool statePublished = false;
    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);
    statePublished = false;

    simulateEntityCommand(core, "switch", "test_node", "sw1", "ON");

    TEST_ASSERT_TRUE(statePublished);
    TEST_ASSERT_EQUAL_STRING("ON", capturedPayload.c_str());

    core.shutdown();
}

// ============================================================================
// R26 -- HeapTracker Test (Task 18)
// ============================================================================

void test_command_event_heap_stability() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();

    simulateMqttConnect(core);

    // Warm-up pass, deliberately outside the measured window. What this test
    // means to assert is that handling a command leaks nothing per command --
    // not that the very first command allocates nothing, which is a different
    // and much weaker claim. Buffers sized on first use and lazily built lookup
    // tables are paid once and never again, and measuring from a cold start
    // charges them to the loop.
    //
    // On the host that distinction is the difference between passing and
    // failing: on glibc 2.43 a cold measurement came to zero bytes, while the
    // ubuntu-latest runner's allocator put it at 384, over the 256 tolerance.
    // The loop itself is clean either way -- with the tolerance forced to zero,
    // 10, 100 and 1000 iterations all measured exactly zero bytes of growth.
    for (int i = 0; i < 10; i++) {
        simulateEntityCommand(core, "switch", "test_node", "sw1", (i % 2 == 0) ? "ON" : "OFF");
    }

    HeapTracker tracker;
    HEAP_CHECKPOINT(tracker, "before");

    // Simulate 10 switch commands
    for (int i = 0; i < 10; i++) {
        simulateEntityCommand(core, "switch", "test_node", "sw1", (i % 2 == 0) ? "ON" : "OFF");
    }

    HEAP_CHECKPOINT(tracker, "after");
    HEAP_ASSERT_STABLE(tracker, "before", "after", 256);

    core.shutdown();
}

// ============================================================================
// R26 -- Negative/Edge-Case Tests (Task 18b)
// ============================================================================

void test_empty_payload_does_not_crash() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        eventFired = true;
    }, nullptr);

    simulateMqttConnect(core);
    simulateEntityCommand(core, "switch", "test_node", "sw1", "");

    // Empty payload is still valid for switches (parsed as not-ON)
    TEST_ASSERT_TRUE(eventFired);

    core.shutdown();
}

void test_button_invalid_payload_no_event() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addButton("btn1", "Button");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        eventFired = true;
    }, nullptr);

    simulateMqttConnect(core);
    simulateEntityCommand(core, "button", "test_node", "btn1", "INVALID");

    TEST_ASSERT_FALSE(eventFired);  // Invalid payload should NOT emit event

    core.shutdown();
}

void test_alarm_command_without_code() {
    HAAlarmControlPanel panel("alarm", "Alarm");
    panel.handleCommand("ARM_AWAY");
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", panel.lastCommand);
    TEST_ASSERT_EQUAL_STRING("", panel.lastCode);
}

void test_alarm_command_with_multiple_spaces() {
    HAAlarmControlPanel panel("alarm", "Alarm");
    panel.handleCommand("ARM_AWAY  1234");
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", panel.lastCommand);
    TEST_ASSERT_EQUAL_STRING("1234", panel.lastCode);
}

void test_command_to_unknown_entity_no_crash() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();

    bool eventFired = false;
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        eventFired = true;
    }, nullptr);

    simulateMqttConnect(core);
    // Send command to non-existent entity
    simulateEntityCommand(core, "switch", "test_node", "unknown_entity", "ON");

    TEST_ASSERT_FALSE(eventFired);  // Unknown entity should not emit event

    core.shutdown();
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp() {}
void tearDown() {}

int runAllTests() {
    UNITY_BEGIN();

    // R24 -- Virtual dispatch tests
    RUN_TEST(test_virtual_dispatch_via_base_pointer);
    RUN_TEST(test_virtual_dispatch_light_via_base_pointer);
    RUN_TEST(test_virtual_dispatch_button_via_base_pointer);

    // R26 -- EventBus emission tests
    RUN_TEST(test_switch_command_emits_ha_command_event);
    RUN_TEST(test_light_command_emits_ha_command_event);
    RUN_TEST(test_button_command_emits_ha_command_event);
    RUN_TEST(test_alarm_command_emits_ha_command_event_with_code);
    RUN_TEST(test_switch_auto_publish_after_command_event);

    // R26 -- Heap stability test
    RUN_TEST(test_command_event_heap_stability);

    // R26 -- Negative/edge-case tests
    RUN_TEST(test_empty_payload_does_not_crash);
    RUN_TEST(test_button_invalid_payload_no_event);
    RUN_TEST(test_alarm_command_without_code);
    RUN_TEST(test_alarm_command_with_multiple_spaces);
    RUN_TEST(test_command_to_unknown_entity_no_crash);

    return UNITY_END();
}

#ifdef ARDUINO
void setup() { runAllTests(); }
void loop() {}
#else
int main(int argc, char** argv) { return runAllTests(); }
#endif
