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
                             doc["cmd_t"].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("homeassistant/alarm_control_panel/node1/alarm/state",
                             doc["stat_t"].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("1234", doc["code"].as<String>().c_str());
    TEST_ASSERT_TRUE(doc["cod_dis_req"].as<bool>());
    TEST_ASSERT_FALSE(doc["cod_arm_req"].as<bool>());
    TEST_ASSERT_FALSE(doc["cod_trig_req"].as<bool>());

    // Command template must be present when code config is active
    TEST_ASSERT_FALSE(doc["cmd_tpl"].isNull());
    TEST_ASSERT_EQUAL_STRING("{{ action }}{% if code %} {{ code }}{% endif %}",
                             doc["cmd_tpl"].as<String>().c_str());

    // Command payload constants (only for supported features + always disarm)
    TEST_ASSERT_EQUAL_STRING("ARM_HOME", doc["pl_arm_home"].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("ARM_AWAY", doc["pl_arm_away"].as<String>().c_str());
    TEST_ASSERT_TRUE(doc["pl_arm_nite"].isNull());       // Not in supportedFeatures
    TEST_ASSERT_TRUE(doc["pl_arm_vacation"].isNull());    // Not in supportedFeatures
    TEST_ASSERT_TRUE(doc["pl_arm_custom_b"].isNull()); // Not in supportedFeatures
    TEST_ASSERT_EQUAL_STRING("DISARM", doc["pl_disarm"].as<String>().c_str());  // Always present
    TEST_ASSERT_EQUAL_STRING("TRIGGER", doc["pl_trig"].as<String>().c_str());

    // Supported features array
    JsonArray features = doc["sup_feat"].as<JsonArray>();
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
        JsonArray f = doc["sup_feat"].as<JsonArray>();
        TEST_ASSERT_EQUAL(1, f.size());
        TEST_ASSERT_EQUAL_STRING("arm_night", f[0].as<String>().c_str());
    }

    // Multiple flags
    {
        JsonDocument doc = buildFeatures(AlarmFeature::ArmAway | AlarmFeature::ArmHome);
        JsonArray f = doc["sup_feat"].as<JsonArray>();
        TEST_ASSERT_EQUAL(2, f.size());
        TEST_ASSERT_EQUAL_STRING("arm_home", f[0].as<String>().c_str());
        TEST_ASSERT_EQUAL_STRING("arm_away", f[1].as<String>().c_str());
    }

    // All flags
    {
        AlarmFeature all = AlarmFeature::ArmHome | AlarmFeature::ArmAway | AlarmFeature::ArmNight
                         | AlarmFeature::ArmVacation | AlarmFeature::ArmCustomBypass | AlarmFeature::Trigger;
        JsonDocument doc = buildFeatures(all);
        JsonArray f = doc["sup_feat"].as<JsonArray>();
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
        TEST_ASSERT_TRUE(doc["cmd_tpl"].isNull());
        TEST_ASSERT_TRUE(doc["cod_arm_req"].isNull());
        TEST_ASSERT_TRUE(doc["cod_dis_req"].isNull());
        TEST_ASSERT_TRUE(doc["cod_trig_req"].isNull());
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
        TEST_ASSERT_TRUE(doc["cod_arm_req"].as<bool>());
        TEST_ASSERT_TRUE(doc["cod_dis_req"].as<bool>());
        TEST_ASSERT_FALSE(doc["cmd_tpl"].isNull());
        TEST_ASSERT_EQUAL_STRING("{{ action }}{% if code %} {{ code }}{% endif %}",
                                 doc["cmd_tpl"].as<String>().c_str());
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

    // BUG-38: with Home Assistant's long key names this panel's discovery
    // document was 774 characters, over the 699-character event field. It was
    // cut mid-JSON and published anyway for months — this test passed because
    // ArduinoJson yields the keys parsed before the cut, and it asserted only
    // those — then refused aloud since OBS Lot D. With the abbreviated keys it
    // is 638 characters and reaches the bus whole.
    static char warn[160]; warn[0] = '\0';
    auto cb = LoggerCallbacks::addCallback([](LogLevel level, const char*, const char* msg) {
        if (level == LOG_LEVEL_WARN && strstr(msg, "not published")) snprintf(warn, sizeof(warn), "%s", msg);
    });
    String published;
    HomeAssistantComponent* haPtr = ha.get();
    HAEntity* panel = ha->entity("alarm");
    TEST_ASSERT_NOT_NULL(panel);
    {
        JsonDocument doc, deviceDoc;
        JsonObject device = deviceDoc.to<JsonObject>();
        panel->buildDiscoveryPayload(doc, "test_node", "homeassistant", device, "homeassistant/test_node/availability");
        TEST_ASSERT_EQUAL_STRING("5678", doc["code"].as<String>().c_str());
        TEST_ASSERT_TRUE(doc["cod_arm_req"].as<bool>());
        TEST_ASSERT_TRUE(doc["cod_dis_req"].as<bool>());
        TEST_ASSERT_FALSE(doc["cod_trig_req"].as<bool>());
        TEST_ASSERT_FALSE(doc["cmd_tpl"].isNull());
    }
    core.addComponent(std::move(ha));
    core.begin();

    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("alarm_control_panel") >= 0 && topic.indexOf("/config") >= 0 && ev.payload[0] != '\0') {
                published = ev.payload;
            }
        });

    simulateMqttConnect(core);
    TEST_ASSERT_EQUAL_MESSAGE(638, published.length(), "BUG-38: the abbreviated panel config reaches the bus whole");
    TEST_ASSERT_EQUAL_STRING("", warn);
    TEST_ASSERT_EQUAL_UINT32(0, haPtr->getStatistics().discoveryRefused);
    LoggerCallbacks::removeCallback(cb);

    core.shutdown();
}

// ============================================================================
// Test 11: Command routing
// ============================================================================

void test_alarm_panel_over_the_event_field_is_refused_and_counted() {
    // The shape the abbreviations do not rescue: six arm modes, a 32-character
    // node id, a configuration URL and an area — 974 characters against the
    // 699-character field (and a 1060-byte packet against PubSubClient's 768 on
    // ESP8266). Refused before the bus, named in one warning, counted, and not
    // announced as queued (BUG-38).
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "abcdefghijklmnopqrstuvwxyz012345", sizeof(config.nodeId));
    HA::setField(config.configUrl, "http://10.0.0.100:80/", sizeof(config.configUrl));
    HA::setField(config.suggestedArea, "Living Room", sizeof(config.suggestedArea));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addAlarmControlPanel("alarm", "Alarm Panel", "mdi:shield-lock",
        AlarmFeature::ArmAway | AlarmFeature::ArmHome | AlarmFeature::ArmNight |
        AlarmFeature::ArmVacation | AlarmFeature::ArmCustomBypass | AlarmFeature::Trigger,
        "5678", true, true, true);
    HomeAssistantComponent* haPtr = ha.get();

    static char warn[200]; warn[0] = '\0';
    static int queuedInfo; queuedInfo = 0;
    auto cb = LoggerCallbacks::addCallback([](LogLevel level, const char*, const char* msg) {
        if (level == LOG_LEVEL_WARN && strstr(msg, "not published")) snprintf(warn, sizeof(warn), "%s", msg);
        if (level == LOG_LEVEL_INFO && strstr(msg, "Discovery queued")) queuedInfo++;
    });
    bool discoveryPublished = false;
    core.addComponent(std::move(ha));
    core.begin();
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            if (strstr(ev.topic, "alarm_control_panel") && strstr(ev.topic, "/config") && ev.payload[0] != '\0') {
                discoveryPublished = true;
            }
        });

    simulateMqttConnect(core);
    TEST_ASSERT_FALSE_MESSAGE(discoveryPublished, "BUG-38: a document over the event field must be refused, not cut");
    TEST_ASSERT_EQUAL_STRING("Payload for 'homeassistant/alarm_control_panel/abcdefghijklmnopqrstuvwxyz012345/alarm/config' is 974 bytes, over the 699-byte event field: not published", warn);
    TEST_ASSERT_EQUAL_UINT32(1, haPtr->getStatistics().discoveryRefused);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, queuedInfo, "a refused config must not be announced as queued");
    LoggerCallbacks::removeCallback(cb);

    core.shutdown();
}

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
    RUN_TEST(test_alarm_panel_over_the_event_field_is_refused_and_counted);
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
