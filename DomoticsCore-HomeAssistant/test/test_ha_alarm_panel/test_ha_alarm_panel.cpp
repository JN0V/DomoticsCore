/**
 * @file test_ha_alarm_panel.cpp
 * @brief Native unit tests for HAAlarmControlPanel entity
 *
 * Tests cover:
 * - Discovery payload generation (JSON fields, supported_features, code config, command_template)
 * - Command handling (basic, with code, no callback, edge cases)
 * - State publishing (correct topic, no auto-publish)
 * - Entity registration (addAlarmControlPanel with code parameter passthrough)
 * - Command routing in HomeAssistantComponent
 * - Heap stability (Constitution XIV)
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
// setUp / tearDown (Unity lifecycle)
// ============================================================================

void setUp() {}
void tearDown() {}

// ============================================================================
// Helpers
// ============================================================================

static void simulateMqttConnect(Core& core) {
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    for (int i = 0; i < 5; i++) core.loop();
}

static void simulateAlarmCommand(Core& core, const char* nodeId,
                                  const char* entityId, const char* payload) {
    MQTTMessageEvent msg{};
    String topic = String("homeassistant/alarm_control_panel/") + nodeId + "/" + entityId + "/set";
    strncpy(msg.topic, topic.c_str(), MQTT_EVENT_TOPIC_SIZE - 1);
    msg.topic[MQTT_EVENT_TOPIC_SIZE - 1] = '\0';
    strncpy(msg.payload, payload, MQTT_EVENT_PAYLOAD_SIZE - 1);
    msg.payload[MQTT_EVENT_PAYLOAD_SIZE - 1] = '\0';
    core.emit<MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, msg);
    for (int i = 0; i < 5; i++) core.loop();
}

// ============================================================================
// Test 1: Discovery payload
// ============================================================================

void test_alarm_panel_discovery_payload() {
    HAAlarmControlPanel panel("alarm", "Alarm Panel", "mdi:shield-home");
    panel.supportedFeatures = AlarmFeature::ArmAway | AlarmFeature::ArmHome | AlarmFeature::Trigger;
    panel.code = "1234";
    panel.codeDisarmRequired = true;

    JsonDocument doc;
    JsonDocument deviceDoc;
    JsonObject device = deviceDoc.to<JsonObject>();
    device["name"] = "TestDevice";

    panel.buildDiscoveryPayload(doc, "node1", "homeassistant", device, "homeassistant/node1/availability");

    TEST_ASSERT_EQUAL_STRING("homeassistant/alarm_control_panel/node1/alarm/set",
                             doc["command_topic"].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("homeassistant/alarm_control_panel/node1/alarm/state",
                             doc["state_topic"].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("1234", doc["code"].as<String>().c_str());
    TEST_ASSERT_TRUE(doc["code_disarm_required"].as<bool>());
    TEST_ASSERT_FALSE(doc["code_arm_required"].as<bool>());
    TEST_ASSERT_FALSE(doc["code_trigger_required"].as<bool>());

    // Command template must be present when code config is active
    TEST_ASSERT_FALSE(doc["command_template"].isNull());
    TEST_ASSERT_EQUAL_STRING("{{ action }}{% if code %} {{ code }}{% endif %}",
                             doc["command_template"].as<String>().c_str());

    // Command payload constants (only for supported features + always disarm)
    TEST_ASSERT_EQUAL_STRING("ARM_HOME", doc["payload_arm_home"].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", doc["payload_arm_away"].as<String>().c_str());
    TEST_ASSERT_TRUE(doc["payload_arm_night"].isNull());       // Not in supportedFeatures
    TEST_ASSERT_TRUE(doc["payload_arm_vacation"].isNull());    // Not in supportedFeatures
    TEST_ASSERT_TRUE(doc["payload_arm_custom_bypass"].isNull()); // Not in supportedFeatures
    TEST_ASSERT_EQUAL_STRING("DISARM", doc["payload_disarm"].as<String>().c_str());  // Always present
    TEST_ASSERT_EQUAL_STRING("TRIGGER", doc["payload_trigger"].as<String>().c_str());

    // Supported features array
    JsonArray features = doc["supported_features"].as<JsonArray>();
    TEST_ASSERT_EQUAL(3, features.size());
    TEST_ASSERT_EQUAL_STRING("arm_home", features[0].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("arm_away", features[1].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("trigger", features[2].as<String>().c_str());
}

// ============================================================================
// Test 2: Supported features bitmask to array
// ============================================================================

void test_alarm_panel_discovery_supported_features() {
    auto buildFeatures = [](AlarmFeature mask) -> JsonDocument {
        HAAlarmControlPanel panel("alarm", "Alarm");
        panel.supportedFeatures = mask;
        JsonDocument doc;
        JsonDocument deviceDoc;
        JsonObject device = deviceDoc.to<JsonObject>();
        panel.buildDiscoveryPayload(doc, "n", "ha", device, "");
        return doc;
    };

    // Single flag
    {
        JsonDocument doc = buildFeatures(AlarmFeature::ArmNight);
        JsonArray f = doc["supported_features"].as<JsonArray>();
        TEST_ASSERT_EQUAL(1, f.size());
        TEST_ASSERT_EQUAL_STRING("arm_night", f[0].as<String>().c_str());
    }

    // Multiple flags
    {
        JsonDocument doc = buildFeatures(AlarmFeature::ArmAway | AlarmFeature::ArmHome);
        JsonArray f = doc["supported_features"].as<JsonArray>();
        TEST_ASSERT_EQUAL(2, f.size());
        TEST_ASSERT_EQUAL_STRING("arm_home", f[0].as<String>().c_str());
        TEST_ASSERT_EQUAL_STRING("arm_away", f[1].as<String>().c_str());
    }

    // All flags
    {
        AlarmFeature all = AlarmFeature::ArmHome | AlarmFeature::ArmAway | AlarmFeature::ArmNight
                         | AlarmFeature::ArmVacation | AlarmFeature::ArmCustomBypass | AlarmFeature::Trigger;
        JsonDocument doc = buildFeatures(all);
        JsonArray f = doc["supported_features"].as<JsonArray>();
        TEST_ASSERT_EQUAL(6, f.size());
    }
}

// ============================================================================
// Test 3: Code fields and command_template conditional
// ============================================================================

void test_alarm_panel_discovery_code_fields() {
    // No code config -> no code fields at all
    {
        HAAlarmControlPanel panel("alarm", "Alarm");
        JsonDocument doc;
        JsonDocument deviceDoc;
        JsonObject device = deviceDoc.to<JsonObject>();
        panel.buildDiscoveryPayload(doc, "n", "ha", device, "");

        TEST_ASSERT_TRUE(doc["code"].isNull());
        TEST_ASSERT_TRUE(doc["command_template"].isNull());
        TEST_ASSERT_TRUE(doc["code_arm_required"].isNull());
        TEST_ASSERT_TRUE(doc["code_disarm_required"].isNull());
        TEST_ASSERT_TRUE(doc["code_trigger_required"].isNull());
    }

    // Code set -> command_template present
    {
        HAAlarmControlPanel panel("alarm", "Alarm");
        panel.code = "5678";
        panel.codeArmRequired = true;
        panel.codeDisarmRequired = true;
        JsonDocument doc;
        JsonDocument deviceDoc;
        JsonObject device = deviceDoc.to<JsonObject>();
        panel.buildDiscoveryPayload(doc, "n", "ha", device, "");

        TEST_ASSERT_EQUAL_STRING("5678", doc["code"].as<String>().c_str());
        TEST_ASSERT_TRUE(doc["code_arm_required"].as<bool>());
        TEST_ASSERT_TRUE(doc["code_disarm_required"].as<bool>());
        TEST_ASSERT_FALSE(doc["command_template"].isNull());
        TEST_ASSERT_EQUAL_STRING("{{ action }}{% if code %} {{ code }}{% endif %}",
                                 doc["command_template"].as<String>().c_str());
    }
}

// ============================================================================
// Test 4: handleCommand basic
// ============================================================================

void test_alarm_panel_handle_command_basic() {
    HAAlarmControlPanel panel("alarm", "Alarm");

    bool result = panel.handleCommand("ARM_AWAY");

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", panel.lastCommand);
    TEST_ASSERT_EQUAL_STRING("", panel.lastCode);
}

// ============================================================================
// Test 5: handleCommand with code
// ============================================================================

void test_alarm_panel_handle_command_with_code() {
    HAAlarmControlPanel panel("alarm", "Alarm");

    bool result = panel.handleCommand("DISARM 1234");

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("DISARM", panel.lastCommand);
    TEST_ASSERT_EQUAL_STRING("1234", panel.lastCode);
}

// ============================================================================
// Test 6: handleCommand no callback
// ============================================================================

void test_alarm_panel_handle_command_no_callback() {
    HAAlarmControlPanel panel("alarm", "Alarm");
    // Should not crash and should store results
    panel.handleCommand("ARM_AWAY");
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", panel.lastCommand);
    panel.handleCommand("DISARM 1234");
    TEST_ASSERT_EQUAL_STRING("DISARM", panel.lastCommand);
    TEST_ASSERT_EQUAL_STRING("1234", panel.lastCode);
}

// ============================================================================
// Test 7: handleCommand edge cases
// ============================================================================

void test_alarm_panel_handle_command_edge_cases() {
    HAAlarmControlPanel panel("alarm", "Alarm");

    // Empty payload -> lastCommand stays empty
    panel.handleCommand("");
    TEST_ASSERT_EQUAL_STRING("", panel.lastCommand);

    // Whitespace only -> lastCommand stays empty
    panel.handleCommand("   ");
    TEST_ASSERT_EQUAL_STRING("", panel.lastCommand);

    // Trailing space -> command parsed, code empty
    panel.handleCommand("ARM_AWAY ");
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", panel.lastCommand);
    TEST_ASSERT_EQUAL_STRING("", panel.lastCode);
}

// ============================================================================
// Test 8: State publish (integration)
// ============================================================================

void test_alarm_panel_state_publish() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addAlarmControlPanel("alarm", "Alarm Panel");
    core.addComponent(std::move(ha));
    core.begin();

    String capturedTopic;
    String capturedPayload;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/alarm/state") >= 0) {
                capturedTopic = ev.topic;
                capturedPayload = ev.payload;
            }
        });

    simulateMqttConnect(core);

    // Get component pointer and publish state
    auto* haComp = static_cast<HomeAssistantComponent*>(core.getComponent("HomeAssistant"));
    TEST_ASSERT_NOT_NULL(haComp);
    haComp->publishState("alarm", String("arming"));
    for (int i = 0; i < 5; i++) core.loop();

    TEST_ASSERT_EQUAL_STRING("homeassistant/alarm_control_panel/test_node/alarm/state",
                             capturedTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("arming", capturedPayload.c_str());

    core.shutdown();
}

// ============================================================================
// Test 9: No auto-publish on command
// ============================================================================

void test_alarm_panel_no_auto_publish() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addAlarmControlPanel("alarm", "Alarm Panel");
    core.addComponent(std::move(ha));
    core.begin();

    simulateMqttConnect(core);

    // Register listener AFTER connect+discovery to avoid capture of discovery publishes
    bool statePublished = false;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/alarm/state") >= 0) {
                statePublished = true;
            }
        });

    simulateAlarmCommand(core, "test_node", "alarm", "ARM_AWAY");

    TEST_ASSERT_FALSE(statePublished);  // No auto-publish for alarm panel

    core.shutdown();
}

// ============================================================================
// Test 10: addAlarmControlPanel registration with code params
// ============================================================================

void test_alarm_panel_add_method() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addAlarmControlPanel("alarm", "Alarm Panel",
        "mdi:shield-lock",
        AlarmFeature::ArmAway | AlarmFeature::ArmHome,
        "5678", true, true, false);

    const auto& stats = ha->getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);

    // Verify discovery is published when connected
    bool discoveryPublished = false;
    core.addComponent(std::move(ha));
    core.begin();

    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("alarm_control_panel") >= 0 && topic.indexOf("/config") >= 0) {
                discoveryPublished = true;
                // Verify code params passed through in discovery payload
                JsonDocument doc;
                deserializeJson(doc, ev.payload);
                TEST_ASSERT_EQUAL_STRING("5678", doc["code"].as<String>().c_str());
                TEST_ASSERT_TRUE(doc["code_arm_required"].as<bool>());
                TEST_ASSERT_TRUE(doc["code_disarm_required"].as<bool>());
                TEST_ASSERT_FALSE(doc["code_trigger_required"].as<bool>());
                TEST_ASSERT_FALSE(doc["command_template"].isNull());
            }
        });

    simulateMqttConnect(core);
    TEST_ASSERT_TRUE(discoveryPublished);

    core.shutdown();
}

// ============================================================================
// Test 11: Command routing
// ============================================================================

void test_alarm_panel_command_routing() {
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addAlarmControlPanel("alarm", "Alarm Panel");
    core.addComponent(std::move(ha));
    core.begin();

    // Subscribe to ha/command EventBus events
    bool eventFired = false;
    char receivedCommand[128] = {};
    core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
        auto& ev = *reinterpret_cast<const HAEvents::HACommandEvent*>(data);
        if (strcmp(ev.component, "alarm_control_panel") == 0) {
            eventFired = true;
            strncpy(receivedCommand, ev.command, sizeof(receivedCommand) - 1);
        }
    }, nullptr);

    simulateMqttConnect(core);
    simulateAlarmCommand(core, "test_node", "alarm", "ARM_AWAY");

    TEST_ASSERT_TRUE(eventFired);
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", receivedCommand);

    auto* haComp = static_cast<HomeAssistantComponent*>(core.getComponent("HomeAssistant"));
    TEST_ASSERT_EQUAL_UINT32(1, haComp->getStatistics().commandsReceived);

    core.shutdown();
}

// ============================================================================
// Test 12: Polymorphic dispatch through base pointer (AC 8)
// ============================================================================

void test_alarm_panel_polymorphic_dispatch() {
    HAAlarmControlPanel panel("alarm", "Alarm");

    // Call through HAEntity base pointer — verifies virtual override works
    HAEntity* base = &panel;
    bool result = base->handleCommand("ARM_AWAY");

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", panel.lastCommand);
}

// ============================================================================
// Test 13: Heap stability (Constitution XIV)
// ============================================================================

void test_alarm_panel_heap_stability() {
    HeapTracker tracker;
    HEAP_CHECKPOINT(tracker, "before");

    for (int i = 0; i < 10; i++) {
        HAAlarmControlPanel panel("alarm", "Alarm", "mdi:shield-home");
        panel.supportedFeatures = AlarmFeature::ArmAway | AlarmFeature::ArmHome;
        panel.code = "1234";
        panel.codeDisarmRequired = true;

        // Build discovery payload
        JsonDocument doc;
        JsonDocument deviceDoc;
        JsonObject device = deviceDoc.to<JsonObject>();
        panel.buildDiscoveryPayload(doc, "node1", "homeassistant", device, "");

        // Handle command
        panel.handleCommand("ARM_AWAY");
        panel.handleCommand("DISARM 1234");
    }

    HEAP_CHECKPOINT(tracker, "after");
    HEAP_ASSERT_STABLE(tracker, "before", "after", 100);
}

// ============================================================================
// Test 14: AlarmFeature type safety (R25)
// ============================================================================

void test_alarm_feature_type_safety() {
    // operator| returns AlarmFeature (not uint8_t)
    AlarmFeature combined = AlarmFeature::ArmAway | AlarmFeature::ArmHome;
    (void)combined; // Compiles = type is AlarmFeature

    // operator& returns bool (usable in conditionals)
    AlarmFeature features = AlarmFeature::ArmAway | AlarmFeature::ArmHome;
    TEST_ASSERT_TRUE(features & AlarmFeature::ArmAway);
    TEST_ASSERT_TRUE(features & AlarmFeature::ArmHome);
    TEST_ASSERT_FALSE(features & AlarmFeature::ArmNight);

    // operator|= works for compound assignment
    AlarmFeature f = AlarmFeature::ArmAway;
    f |= AlarmFeature::ArmHome;
    TEST_ASSERT_TRUE(f & AlarmFeature::ArmHome);
    TEST_ASSERT_TRUE(f & AlarmFeature::ArmAway);
}

// ============================================================================
// Main
// ============================================================================

int runAllTests() {
    UNITY_BEGIN();

    // Unit tests (entity only)
    RUN_TEST(test_alarm_panel_discovery_payload);
    RUN_TEST(test_alarm_panel_discovery_supported_features);
    RUN_TEST(test_alarm_panel_discovery_code_fields);
    RUN_TEST(test_alarm_panel_handle_command_basic);
    RUN_TEST(test_alarm_panel_handle_command_with_code);
    RUN_TEST(test_alarm_panel_handle_command_no_callback);
    RUN_TEST(test_alarm_panel_handle_command_edge_cases);

    // Integration tests (require Core + HomeAssistantComponent)
    RUN_TEST(test_alarm_panel_state_publish);
    RUN_TEST(test_alarm_panel_no_auto_publish);
    RUN_TEST(test_alarm_panel_add_method);
    RUN_TEST(test_alarm_panel_command_routing);
    RUN_TEST(test_alarm_panel_polymorphic_dispatch);
    RUN_TEST(test_alarm_panel_heap_stability);
    RUN_TEST(test_alarm_feature_type_safety);

    return UNITY_END();
}

#ifdef ARDUINO
void setup() { runAllTests(); }
void loop() {}
#else
int main(int argc, char** argv) { return runAllTests(); }
#endif
