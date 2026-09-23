/**
 * @file test_ha_component.cpp
 * @brief Native unit tests for HomeAssistant component
 *
 * Tests cover:
 * - Events (HAEvents)
 * - Component creation and configuration
 * - Config get/set
 * - Entity management (sensors, switches, buttons, lights)
 * - Statistics
 * - Lifecycle (begin, loop, shutdown)
 * - Non-blocking behavior
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/HAEvents.h>
#include <DomoticsCore/ArduinoJsonString.h>  // String converters for ArduinoJson 7
#include <DomoticsCore/Testing/HeapTracker.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;
using namespace DomoticsCore::Testing;

// ============================================================================
// Event Tests
// ============================================================================

void test_ha_events_constants_defined() {
    // Verify event constants are defined and have expected values
    TEST_ASSERT_NOT_NULL(HAEvents::EVENT_DISCOVERY_PUBLISHED);
    TEST_ASSERT_NOT_NULL(HAEvents::EVENT_ENTITY_ADDED);

    TEST_ASSERT_EQUAL_STRING("ha/discovery_published", HAEvents::EVENT_DISCOVERY_PUBLISHED);
    TEST_ASSERT_EQUAL_STRING("ha/entity_added", HAEvents::EVENT_ENTITY_ADDED);
}

// ============================================================================
// Component Creation Tests
// ============================================================================

void test_ha_component_creation_default() {
    HomeAssistantComponent ha;

    TEST_ASSERT_EQUAL_STRING("HomeAssistant", ha.metadata.name);
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", ha.metadata.author);
    // The exact version is enforced by tools/check_versions.py, which compares
    // library.json against every metadata.version in the component's sources.
    // Repeating the literal here only makes the test stale at the next release.
    TEST_ASSERT_NOT_NULL(ha.metadata.version);
    TEST_ASSERT_NOT_EQUAL('\0', ha.metadata.version[0]);
}

void test_ha_component_creation_with_config() {
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    HA::setField(config.deviceName, "Test Device", sizeof(config.deviceName));
    HA::setField(config.manufacturer, "TestMfg", sizeof(config.manufacturer));
    HA::setField(config.model, "TestModel", sizeof(config.model));
    HA::setField(config.swVersion, "2.0.0", sizeof(config.swVersion));

    HomeAssistantComponent ha(config);

    TEST_ASSERT_EQUAL_STRING("HomeAssistant", ha.metadata.name);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("test_node", cfg.nodeId);
    TEST_ASSERT_EQUAL_STRING("Test Device", cfg.deviceName);
    TEST_ASSERT_EQUAL_STRING("TestMfg", cfg.manufacturer);
    TEST_ASSERT_EQUAL_STRING("TestModel", cfg.model);
    TEST_ASSERT_EQUAL_STRING("2.0.0", cfg.swVersion);
}

// ============================================================================
// Config Tests
// ============================================================================

void test_ha_config_defaults() {
    HAConfig config;

    // Default values from HAConfig struct in HomeAssistant.h
    TEST_ASSERT_EQUAL_STRING("myDeviceId", config.nodeId);
    TEST_ASSERT_EQUAL_STRING("My Device", config.deviceName);
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.manufacturer);
    TEST_ASSERT_EQUAL_STRING("MyDeviceModel", config.model);
    TEST_ASSERT_EQUAL_STRING("1.0.0", config.swVersion);
    TEST_ASSERT_TRUE(config.retainDiscovery);
    TEST_ASSERT_EQUAL_STRING("homeassistant", config.discoveryPrefix);
    TEST_ASSERT_EQUAL_STRING("", config.availabilityTopic);
    TEST_ASSERT_EQUAL_STRING("", config.configUrl);
    TEST_ASSERT_EQUAL_STRING("", config.suggestedArea);
}

void test_ha_config_get_set() {
    HomeAssistantComponent ha;

    HAConfig newConfig;
    HA::setField(newConfig.nodeId, "new_node", sizeof(newConfig.nodeId));
    HA::setField(newConfig.deviceName, "New Device", sizeof(newConfig.deviceName));
    HA::setField(newConfig.discoveryPrefix, "custom_prefix", sizeof(newConfig.discoveryPrefix));
    newConfig.retainDiscovery = false;

    ha.setConfig(newConfig);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("new_node", cfg.nodeId);
    TEST_ASSERT_EQUAL_STRING("New Device", cfg.deviceName);
    TEST_ASSERT_EQUAL_STRING("custom_prefix", cfg.discoveryPrefix);
    TEST_ASSERT_FALSE(cfg.retainDiscovery);
}

void test_ha_availability_topic_auto_generated() {
    HAConfig config;
    HA::setField(config.nodeId, "test_device", sizeof(config.nodeId));
    HA::setField(config.discoveryPrefix, "homeassistant", sizeof(config.discoveryPrefix));
    // Leave availabilityTopic empty

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("homeassistant/test_device/availability", cfg.availabilityTopic);
}

void test_ha_availability_topic_custom() {
    HAConfig config;
    HA::setField(config.nodeId, "test_device", sizeof(config.nodeId));
    HA::setField(config.availabilityTopic, "custom/availability/topic", sizeof(config.availabilityTopic));

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("custom/availability/topic", cfg.availabilityTopic);
}

void test_ha_config_url_and_area() {
    HAConfig config;
    HA::setField(config.configUrl, "http://192.168.1.100", sizeof(config.configUrl));
    HA::setField(config.suggestedArea, "Living Room", sizeof(config.suggestedArea));

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.100", cfg.configUrl);
    TEST_ASSERT_EQUAL_STRING("Living Room", cfg.suggestedArea);
}

// ============================================================================
// Entity Management Tests - Sensors
// ============================================================================

void test_ha_add_sensor_basic() {
    HomeAssistantComponent ha;

    ha.addSensor("temp", "Temperature");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_sensor_with_all_params() {
    HomeAssistantComponent ha;

    ha.addSensor("temperature", "Temperature", "°C", "temperature", "mdi:thermometer", "measurement");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_multiple_sensors() {
    HomeAssistantComponent ha;

    ha.addSensor("temp", "Temperature", "°C");
    ha.addSensor("humidity", "Humidity", "%");
    ha.addSensor("pressure", "Pressure", "hPa");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(3, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Binary Sensors
// ============================================================================

void test_ha_add_binary_sensor_basic() {
    HomeAssistantComponent ha;

    ha.addBinarySensor("motion", "Motion Sensor");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_binary_sensor_with_class() {
    HomeAssistantComponent ha;

    ha.addBinarySensor("door", "Door Sensor", "door", "mdi:door");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Switches
// ============================================================================

void test_ha_add_switch() {
    HomeAssistantComponent ha;

    ha.addSwitch("relay", "Relay Switch", "mdi:electric-switch");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

void test_ha_add_switch_entity_registered() {
    HomeAssistantComponent ha;

    ha.addSwitch("test_switch", "Test Switch");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Lights
// ============================================================================

void test_ha_add_light() {
    HomeAssistantComponent ha;

    ha.addLight("light1", "Main Light");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Buttons
// ============================================================================

void test_ha_add_button() {
    HomeAssistantComponent ha;

    ha.addButton("restart", "Restart", "mdi:restart");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(1, stats.entityCount);
}

// ============================================================================
// Entity Management Tests - Mixed Entities
// ============================================================================

void test_ha_add_multiple_entity_types() {
    HomeAssistantComponent ha;

    ha.addSensor("temp", "Temperature", "°C");
    ha.addBinarySensor("door", "Door", "door");
    ha.addSwitch("relay", "Relay");
    ha.addButton("restart", "Restart");
    ha.addLight("light", "Light");

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(5, stats.entityCount);
}

// ============================================================================
// Statistics Tests
// ============================================================================

void test_ha_statistics_initial() {
    HomeAssistantComponent ha;

    const auto& stats = ha.getStatistics();

    TEST_ASSERT_EQUAL_UINT32(0, stats.entityCount);
    TEST_ASSERT_EQUAL_UINT32(0, stats.discoveryCount);
    TEST_ASSERT_EQUAL_UINT32(0, stats.stateUpdates);
    TEST_ASSERT_EQUAL_UINT32(0, stats.commandsReceived);
}

void test_ha_statistics_after_adding_entities() {
    HomeAssistantComponent ha;

    ha.addSensor("s1", "Sensor 1");
    ha.addSensor("s2", "Sensor 2");
    ha.addSwitch("sw1", "Switch 1");

    const auto& stats = ha.getStatistics();

    TEST_ASSERT_EQUAL_UINT32(3, stats.entityCount);
    TEST_ASSERT_EQUAL_UINT32(0, stats.discoveryCount);  // Not published yet
    TEST_ASSERT_EQUAL_UINT32(0, stats.stateUpdates);    // No states published
}

// ============================================================================
// Connection Status Tests
// ============================================================================

void test_ha_mqtt_not_connected_initial() {
    HomeAssistantComponent ha;

    TEST_ASSERT_FALSE(ha.isMQTTConnected());
}

void test_ha_not_ready_without_mqtt() {
    HomeAssistantComponent ha;

    TEST_ASSERT_FALSE(ha.isReady());
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

void test_ha_begin_returns_success() {
    HomeAssistantComponent ha;

    ComponentStatus status = ha.begin();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);

    ha.shutdown();
}

void test_ha_shutdown_returns_success() {
    HomeAssistantComponent ha;
    ha.begin();

    ComponentStatus status = ha.shutdown();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);
}

void test_ha_full_lifecycle() {
    Core core;

    HAConfig config;
    HA::setField(config.nodeId, "test_lifecycle", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSensor("test_sensor", "Test Sensor");

    core.addComponent(std::move(ha));

    bool beginResult = core.begin();
    TEST_ASSERT_TRUE(beginResult);

    // Run a few loops
    for (int i = 0; i < 10; i++) {
        core.loop();
    }

    core.shutdown();
}

// ============================================================================
// Non-blocking Tests
// ============================================================================

void test_ha_loop_non_blocking() {
    Core core;

    auto ha = std::make_unique<HomeAssistantComponent>();

    core.addComponent(std::move(ha));
    core.begin();

    // Run several loop iterations to verify non-blocking
    unsigned long start = HAL::Platform::getMillis();
    int loopCount = 0;
    while (HAL::Platform::getMillis() - start < 100) {
        core.loop();
        loopCount++;
        HAL::Platform::delayMs(1);
    }

    // Should have run many loops (non-blocking)
    // HA loop() is empty - all via EventBus, so should be very fast
    TEST_ASSERT_GREATER_THAN(50, loopCount);

    core.shutdown();
}

// ============================================================================
// Device Info Tests
// ============================================================================

void test_ha_set_device_info() {
    HomeAssistantComponent ha;

    ha.setDeviceInfo("Custom Name", "Custom Model", "Custom Manufacturer", "3.0.0");

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("Custom Name", cfg.deviceName);
    TEST_ASSERT_EQUAL_STRING("Custom Model", cfg.model);
    TEST_ASSERT_EQUAL_STRING("Custom Manufacturer", cfg.manufacturer);
    TEST_ASSERT_EQUAL_STRING("3.0.0", cfg.swVersion);
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_ha_no_entities() {
    HomeAssistantComponent ha;

    ComponentStatus status = ha.begin();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);

    const auto& stats = ha.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(0, stats.entityCount);

    ha.shutdown();
}

void test_ha_component_no_dependencies() {
    HomeAssistantComponent ha;

    auto deps = ha.getDependencies();
    // HA component has no explicit dependencies (communicates via EventBus)
    TEST_ASSERT_EQUAL(0, deps.size());
}

void test_ha_empty_config_fields() {
    HAConfig config;
    config.configUrl[0] = '\0';
    config.suggestedArea[0] = '\0';

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("", cfg.configUrl);
    TEST_ASSERT_EQUAL_STRING("", cfg.suggestedArea);
}

void test_ha_special_characters_in_node_id() {
    HAConfig config;
    HA::setField(config.nodeId, "device-with_mixed-chars123", sizeof(config.nodeId));

    HomeAssistantComponent ha(config);

    const HAConfig& cfg = ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("device-with_mixed-chars123", cfg.nodeId);
}

// ============================================================================
// HASwitch autoPublishState - Unit Tests (Task 3)
// ============================================================================

void test_switch_auto_publish_default_true() {
    HASwitch sw("test", "Test Switch");
    TEST_ASSERT_TRUE(sw.autoPublishState);
}

void test_switch_auto_publish_set_false() {
    HASwitch sw("test", "Test Switch");
    sw.autoPublishState = false;
    TEST_ASSERT_FALSE(sw.autoPublishState);
}

void test_switch_handle_command_updates_state() {
    HASwitch sw("test", "Test Switch");

    bool result = sw.handleCommand("ON");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(sw.state);

    result = sw.handleCommand("OFF");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(sw.state);
}

void test_switch_handle_command_no_crash() {
    HASwitch sw("test", "Test Switch");
    // Should not crash and should update state
    sw.handleCommand("ON");
    TEST_ASSERT_TRUE(sw.state);
    sw.handleCommand("OFF");
    TEST_ASSERT_FALSE(sw.state);
}

// ============================================================================
// HASwitch autoPublishState - Integration Tests (Task 4)
// ============================================================================

// Helper: emit mqtt/connected and drain the event queue
static void simulateMqttConnect(Core& core) {
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    // Drain all queued events (connect -> availability -> discovery -> subscribe)
    for (int i = 0; i < 5; i++) core.loop();
}

// Helper: emit mqtt/message to simulate a switch command and drain
static void simulateSwitchCommand(Core& core, const char* nodeId,
                                   const char* entityId, const char* payload) {
    MQTTMessageEvent msg{};
    String topic = String("homeassistant/switch/") + nodeId + "/" + entityId + "/set";
    strncpy(msg.topic, topic.c_str(), MQTT_EVENT_TOPIC_SIZE - 1);
    msg.topic[MQTT_EVENT_TOPIC_SIZE - 1] = '\0';
    strncpy(msg.payload, payload, MQTT_EVENT_PAYLOAD_SIZE - 1);
    msg.payload[MQTT_EVENT_PAYLOAD_SIZE - 1] = '\0';
    core.emit<MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, msg);
    // Drain: message -> handleCommand -> possible publishState
    for (int i = 0; i < 5; i++) core.loop();
}


// ============================================================================
// The availability topic and the Last Will
// ============================================================================

// Home Assistant watches exactly one topic per device, the one `avty_t` names.
// If the broker's Last Will is set on a different topic, nothing ever writes
// `offline` where Home Assistant is looking and a device that drops off stays
// green for ever. Measured on a production broker on 2026-09-18:
// `ESP32-.../status offline` and `homeassistant/<node>/availability online`,
// both retained, side by side, 21 days apart from each other.
//
// The invariant these tests hold is one topic, not two: whatever
// `avty_t` advertises is the topic the broker corrects.

// Capture the first discovery document published for `objectId`, as JSON.
// The subscription outlives this function — the EventBus keeps it for the life
// of the Core — so the lambda must not capture anything on this frame, and the
// id is released before returning rather than left pointing at dead storage.
static String g_capturedDiscovery;
static String g_captureNeedle;

static String captureDiscoveryPayload(Core& core, const char* objectId) {
    g_capturedDiscovery = "";
    g_captureNeedle = String("/") + objectId + "/config";
    uint32_t sub = core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (g_capturedDiscovery.isEmpty() && topic.indexOf(g_captureNeedle) >= 0) {
                g_capturedDiscovery = ev.payload;
            }
        });
    simulateMqttConnect(core);
    core.getEventBus().unsubscribe(sub);
    return g_capturedDiscovery;
}

// The stub only marks itself connected when a broker is configured, so a
// fixture that wants the will as the BROKER received it has to give it one.
static void addConnectableMqtt(Core& core, const char* clientId) {
    HAL::WiFiImpl::setConnectedForTest(true);   // connect() refuses without a link
    MQTTConfig mcfg;
    mcfg.clientId = clientId;
    mcfg.broker = "192.0.2.1";
    core.addComponent(std::make_unique<MQTTComponent>(mcfg));
}

static const std::string& willTopicTheBrokerGot(Core& core) {
    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    auto* stub = static_cast<HAL::MQTT::MQTTClientImpl*>(mqtt->getClientForTest());
    return stub->getLWTTopic();
}

void test_discovery_availability_topic_is_the_one_the_will_corrects() {
    Core core;

    MQTTConfig mcfg;
    mcfg.clientId = "ESP32-bug43";
    core.addComponent(std::make_unique<MQTTComponent>(mcfg));

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    auto ha = std::make_unique<HomeAssistantComponent>(hcfg);
    ha->addSensor("s1", "Sensor 1");
    core.addComponent(std::move(ha));
    core.begin();

    String payload = captureDiscoveryPayload(core, "s1");
    TEST_ASSERT_FALSE_MESSAGE(payload.isEmpty(), "no discovery document was published");

    JsonDocument doc;
    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, deserializeJson(doc, payload).code(),
                              "the discovery document is not valid JSON");
    const char* avty = doc["avty_t"];
    TEST_ASSERT_NOT_NULL_MESSAGE(avty, "the discovery document advertises no availability topic");

    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    TEST_ASSERT_NOT_NULL(mqtt);
    const String& will = mqtt->getConfig().lwtTopic;
    TEST_ASSERT_FALSE_MESSAGE(will.isEmpty(), "the MQTT component has no Last Will topic");

    TEST_ASSERT_EQUAL_STRING_MESSAGE(will.c_str(), avty,
        "Home Assistant is told to watch a topic the broker will never correct: the "
        "Last Will is on another one, so a device that drops off stays available");

    core.shutdown();
}

// The other direction, and the half that keeps the invariant from being
// satisfied by accident: a user who names the availability topic moves the
// will with it, or naming the topic re-opens the defect.
void test_a_named_availability_topic_moves_the_will() {
    Core core;

    MQTTConfig mcfg;
    mcfg.clientId = "ESP32-bug43b";
    core.addComponent(std::make_unique<MQTTComponent>(mcfg));

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    HA::setField(hcfg.availabilityTopic, "site/panel/alive", sizeof(hcfg.availabilityTopic));
    auto ha = std::make_unique<HomeAssistantComponent>(hcfg);
    ha->addSensor("s1", "Sensor 1");
    core.addComponent(std::move(ha));
    core.begin();

    String payload = captureDiscoveryPayload(core, "s1");
    JsonDocument doc;
    deserializeJson(doc, payload);
    const char* avty = doc["avty_t"];
    TEST_ASSERT_NOT_NULL(avty);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("site/panel/alive", avty,
                                     "the configured availability topic was overwritten");

    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("site/panel/alive", mqtt->getConfig().lwtTopic.c_str(),
        "the will stayed on its default while Home Assistant was pointed elsewhere");

    core.shutdown();
}

// The assertion that matters on a live device: not what the config field says,
// but what the broker was handed in CONNECT. Without this, the reconnect half of
// the fix is dead code in every test — both the cases above leave `broker` empty,
// so the session never opens and `if (isConnected())` never runs.
void test_the_broker_receives_the_will_on_the_advertised_topic() {
    Core core;
    addConnectableMqtt(core, "ESP32-bug43c");

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    HA::setField(hcfg.availabilityTopic, "site/panel/alive", sizeof(hcfg.availabilityTopic));
    auto ha = std::make_unique<HomeAssistantComponent>(hcfg);
    ha->addSensor("s1", "Sensor 1");
    core.addComponent(std::move(ha));
    core.begin();

    String payload = captureDiscoveryPayload(core, "s1");
    JsonDocument doc;
    deserializeJson(doc, payload);
    const char* avty = doc["avty_t"];
    TEST_ASSERT_NOT_NULL(avty);

    TEST_ASSERT_EQUAL_STRING_MESSAGE(avty, willTopicTheBrokerGot(core).c_str(),
        "the broker holds a will on a different topic from the one the discovery "
        "document advertises: the session was never reopened after the move");

    core.shutdown();
}

// MQTT connects inside its own begin(), so when it is registered first the
// session is already open when HomeAssistant reconciles. That is the ordering
// the reconnect exists for, and it is the common one.
void test_a_session_opened_before_the_move_is_reopened_with_the_new_will() {
    Core core;
    addConnectableMqtt(core, "ESP32-bug43d");

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    HA::setField(hcfg.availabilityTopic, "site/panel/alive", sizeof(hcfg.availabilityTopic));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();

    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    TEST_ASSERT_TRUE_MESSAGE(mqtt->isConnected(),
                             "the fixture never connected: this test proves nothing");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("site/panel/alive", willTopicTheBrokerGot(core).c_str(),
        "the live session kept the old will for its whole lifetime");

    core.shutdown();
}

// autoReconnect is a public field, and with it off MQTTComponent::loop() never
// retries. Dropping the session to re-send the will must not be the last thing
// that ever happens to it. This also drives the runtime path: the topic is named
// through setConfig() after begin(), not in the constructor.
void test_moving_the_will_does_not_strand_a_component_that_never_reconnects() {
    HAL::WiFiImpl::setConnectedForTest(true);
    Core core;
    MQTTConfig mcfg;
    mcfg.clientId = "ESP32-bug43e";
    mcfg.broker = "192.0.2.1";
    mcfg.autoReconnect = false;        // nothing will reconnect this on its own
    core.addComponent(std::make_unique<MQTTComponent>(mcfg));

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();

    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    TEST_ASSERT_TRUE_MESSAGE(mqtt->connect(), "the fixture never connected");

    auto* ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
    HAConfig later = ha->getConfig();
    HA::setField(later.availabilityTopic, "site/panel/alive", sizeof(later.availabilityTopic));
    ha->setConfig(later);

    TEST_ASSERT_TRUE_MESSAGE(mqtt->isConnected(),
        "the device was taken off the broker and nothing reconnects it: with "
        "autoReconnect off, MQTTComponent::loop() never retries");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("site/panel/alive", willTopicTheBrokerGot(core).c_str(),
        "the reopened session carries the old will");

    core.shutdown();
}

// A will with no topic must not blank the availability topic: an empty avty_t
// is dropped from every document, which is the same symptom by another road.
void test_an_empty_will_topic_does_not_blank_availability() {
    Core core;
    MQTTConfig mcfg;
    mcfg.clientId = "ESP32-bug43f";
    mcfg.enableLWT = false;          // so the constructor does not fill lwtTopic
    core.addComponent(std::make_unique<MQTTComponent>(mcfg));
    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    MQTTConfig live = mqtt->getConfig();
    live.enableLWT = true;           // ... and now ask for a will with no topic
    live.lwtTopic = "";
    mqtt->setConfig(live);

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();

    auto* ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("homeassistant/test_node/availability",
                                     ha->getConfig().availabilityTopic,
        "an empty will topic was adopted: avty_t is now absent from every document");

    core.shutdown();
}

// With no will at all there is nothing to agree with. The generated topic stays
// — Home Assistant keeps reading "online" and nothing contradicts it, which the
// component says aloud rather than pretending otherwise.
void test_without_a_will_the_generated_topic_is_kept() {
    Core core;
    MQTTConfig mcfg;
    mcfg.clientId = "ESP32-bug43g";
    mcfg.enableLWT = false;
    core.addComponent(std::make_unique<MQTTComponent>(mcfg));

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();

    auto* ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
    TEST_ASSERT_EQUAL_STRING("homeassistant/test_node/availability",
                             ha->getConfig().availabilityTopic);

    core.shutdown();
}

// setConfig() is the runtime entry point: SystemPersistence calls it after
// Core::begin() and the WebUI calls it then republishes discovery. The symmetry
// has to hold there too, or the fix only covers configuration supplied at boot.
void test_naming_the_topic_after_begin_moves_the_will_too() {
    Core core;
    addConnectableMqtt(core, "ESP32-bug43h");

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();

    auto* ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
    HAConfig later = ha->getConfig();
    HA::setField(later.availabilityTopic, "site/panel/alive", sizeof(later.availabilityTopic));
    ha->setConfig(later);

    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("site/panel/alive", mqtt->getConfig().lwtTopic.c_str(),
        "a topic named after begin() left the will where it was: the two drift "
        "apart again by the runtime path");

    core.shutdown();
}

// The flag that decides which of the two topics wins cannot be "the field is
// non-empty": begin() fills the field, and SystemPersistence round-trips
// getConfig() through setConfig() on every boot of a FullStack application. A
// component that then believes it was named pushes the topic it ADOPTED back
// over a will the user has since moved — option (a), the layering inversion
// the layering inversion, reached without the application naming anything.
void test_an_adopted_topic_is_not_mistaken_for_a_named_one() {
    HAL::WiFiImpl::setConnectedForTest(true);
    Core core;
    MQTTConfig mcfg; mcfg.clientId = "ESP32-probe"; mcfg.broker = "192.0.2.1";
    core.addComponent(std::make_unique<MQTTComponent>(mcfg));
    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();
    auto* ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    // what SystemPersistence does on every boot
    ha->setConfig(ha->getConfig());
    // now the user moves the will from MQTT's side
    MQTTConfig m2 = mqtt->getConfig(); m2.lwtTopic = "site/user/chosen"; mqtt->setConfig(m2);
    ha->setConfig(ha->getConfig());
    TEST_ASSERT_EQUAL_STRING_MESSAGE("site/user/chosen", mqtt->getConfig().lwtTopic.c_str(),
        "HomeAssistant reverted the user's will to the topic it had adopted");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("site/user/chosen", ha->getConfig().availabilityTopic,
        "the discovery documents still advertise the old adopted topic");
    core.shutdown();
}

// A will that is not retained is transient: whoever is not subscribed at that
// instant never sees "offline", while HA's own retained "online" stays on the
// broker for ever. That is the production capture again, one field away.
void test_a_will_that_is_not_retained_is_made_retained() {
    HAL::WiFiImpl::setConnectedForTest(true);
    Core core;
    MQTTConfig mcfg;
    mcfg.clientId = "ESP32-bug43i";
    mcfg.broker = "192.0.2.1";
    mcfg.lwtRetain = false;
    core.addComponent(std::make_unique<MQTTComponent>(mcfg));

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();

    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    TEST_ASSERT_TRUE_MESSAGE(mqtt->getConfig().lwtRetain,
        "the will is transient while availability is retained: the 'online' on the "
        "availability topic outlives the device");
    core.shutdown();
}

// The WebUI clears availabilityTopic so it follows a new nodeId. setConfig then
// generates one — and must not mistake its own generated topic for an
// application's choice, or it pushes it onto the will and drops a live session
// on every settings save.
void test_a_regenerated_topic_does_not_push_itself_onto_the_will() {
    HAL::WiFiImpl::setConnectedForTest(true);
    Core core;
    addConnectableMqtt(core, "ESP32-bug43j");

    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();

    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    auto* ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
    const uint32_t connectsBefore = mqtt->getStatistics().connectCount;

    // exactly what HomeAssistantWebUI does for a node_id change
    HAConfig renamed = ha->getConfig();
    HA::setField(renamed.nodeId, "lab01", sizeof(renamed.nodeId));
    renamed.availabilityTopic[0] = '\0';
    ha->setConfig(renamed);

    TEST_ASSERT_EQUAL_STRING_MESSAGE("ESP32-bug43j/status", mqtt->getConfig().lwtTopic.c_str(),
        "a settings save pushed a generated topic onto the will");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(connectsBefore, mqtt->getStatistics().connectCount,
        "a settings save bounced the live MQTT session");
    core.shutdown();
}

void test_switch_command_auto_publishes_state() {
    // AC 1: Default switch auto-publishes state after command
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1");
    core.addComponent(std::move(ha));
    core.begin();

    // Subscribe to publish events
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

    // Connect MQTT (drains availability + discovery noise)
    simulateMqttConnect(core);
    statePublished = false;  // Reset after connect noise

    // Send switch command
    simulateSwitchCommand(core, "test_node", "sw1", "ON");

    TEST_ASSERT_TRUE(statePublished);
    TEST_ASSERT_EQUAL_STRING("ON", capturedPayload.c_str());

    // And then OFF, which is the half that makes this test discriminating.
    // publishState is overloaded on String, const char* and bool, and the bug-008
    // misresolution sends every payload through the bool overload — which prints
    // "ON" for any non-null pointer. A suite that only ever commands ON passes
    // just as well against that bug: it was demonstrated here, by replacing the
    // call site in HomeAssistant.h with publishState(entity->id, (bool)payload),
    // and all 91 native cases stayed green.
    statePublished = false;
    capturedPayload = "";
    simulateSwitchCommand(core, "test_node", "sw1", "OFF");

    TEST_ASSERT_TRUE(statePublished);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("OFF", capturedPayload.c_str(),
        "the auto-publish resolved to the bool overload: it publishes \"ON\" for "
        "every command, and a switch turned off in Home Assistant reappears on");

    core.shutdown();
}

void test_switch_command_no_auto_publish_when_disabled() {
    // AC 2: autoPublishState=false -> no auto-publish (RED until Phase 3 fix)
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1", "", false);  // autoPublishState = false
    core.addComponent(std::move(ha));
    core.begin();

    // Subscribe before connect
    bool statePublished = false;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
            }
        });

    simulateMqttConnect(core);
    statePublished = false;  // Reset after connect noise

    simulateSwitchCommand(core, "test_node", "sw1", "ON");

    TEST_ASSERT_FALSE(statePublished);  // Should NOT auto-publish

    core.shutdown();
}

void test_switch_optimistic_overrides_auto_publish() {
    // AC 4: optimistic=true suppresses auto-publish regardless of autoPublishState
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1", "", true, true);  // autoPublishState=true, optimistic=true
    core.addComponent(std::move(ha));
    core.begin();

    bool statePublished = false;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
            }
        });

    simulateMqttConnect(core);
    statePublished = false;

    simulateSwitchCommand(core, "test_node", "sw1", "ON");

    TEST_ASSERT_FALSE(statePublished);  // Optimistic suppresses publish

    core.shutdown();
}

void test_switch_manual_publish_after_auto_disabled() {
    // AC 3: Manual publishState() works even when autoPublishState=false
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    HomeAssistantComponent* haPtr = ha.get();
    ha->addSwitch("sw1", "Switch 1", "", false);  // autoPublishState = false
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

    // Command arrives, no auto-publish
    simulateSwitchCommand(core, "test_node", "sw1", "ON");
    TEST_ASSERT_FALSE(statePublished);

    // Consumer manually publishes state
    haPtr->publishState("sw1", true);
    for (int i = 0; i < 5; i++) core.loop();  // Drain
    TEST_ASSERT_TRUE(statePublished);
    TEST_ASSERT_EQUAL_STRING("ON", capturedPayload.c_str());

    core.shutdown();
}

void test_switch_optimistic_true_auto_publish_false() {
    // Interaction matrix: optimistic=true, autoPublishState=false -> no publish
    Core core;
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));

    auto ha = std::make_unique<HomeAssistantComponent>(config);
    ha->addSwitch("sw1", "Switch 1", "", false, true);  // autoPublishState=false, optimistic=true
    core.addComponent(std::move(ha));
    core.begin();

    bool statePublished = false;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/state") >= 0) {
                statePublished = true;
            }
        });

    simulateMqttConnect(core);
    statePublished = false;

    simulateSwitchCommand(core, "test_node", "sw1", "ON");

    TEST_ASSERT_FALSE(statePublished);  // Both flags suppress publish

    core.shutdown();
}

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
// R6 — char[] field tests
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
// MEM-2 — the command parse, pinned where the char* rewrite could drop it
// ============================================================================
//
// These five cases are behaviour, not cost. They cannot show an allocation was
// avoided: the native String is std::string (Platform_Stub.h:27), and every id
// and payload below fits its own small-string buffer or is copied into a char[]
// before it is measured. What they hold in place is what the rewrite could
// silently change while still passing everything else — the two truncation
// points, the two malformed-topic refusals, and the fact that an id longer than
// the event field still finds its entity.

// Log capture. The rules are the OTA suite's (test_ota_component.cpp:723): the
// buffers are static, not stack Strings captured by reference, and the callback
// is removed before the first assertion. A failed TEST_ASSERT longjmps out of
// the test, and a callback still holding a reference to a dead stack object
// would then be written through by every later DLOG in the suite.
//
// Removing it before the asserts is necessary and not sufficient: the longjmp
// can also come from an assertion that runs *before* the removal, which is why
// tearDown() below removes it again. The callback list is a process-wide
// singleton, so a probe left installed by one test appends to the buffers of
// every test after it.
static String g_capturedWarn;
static String g_capturedError;
static bool g_captureActive = false;
static LoggerCallbacks::CallbackId g_captureId = 0;

static void startLogCapture() {
    if (g_captureActive) LoggerCallbacks::removeCallback(g_captureId);
    g_capturedWarn = "";
    g_capturedError = "";
    g_captureId = LoggerCallbacks::addCallback(
        [](LogLevel level, const char* tag, const char* message) {
            if (strcmp(tag, LOG_HA) != 0) return;
            if (level == LOG_LEVEL_WARN) {
                g_capturedWarn += message;
                g_capturedWarn += "\n";
            } else if (level == LOG_LEVEL_ERROR) {
                g_capturedError += message;
                g_capturedError += "\n";
            }
        });
    g_captureActive = true;
}

static void stopLogCapture() {
    if (!g_captureActive) return;
    LoggerCallbacks::removeCallback(g_captureId);
    g_captureActive = false;
}

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

void test_ha_command_topic_without_slash_is_refused() {
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
    simulateRawMessage(core, "homeassistant", "ON");
    stopLogCapture();
    String err = g_capturedError;

    TEST_ASSERT_FALSE_MESSAGE(eventFired, "a topic with no slash at all produced a command event");
    TEST_ASSERT_TRUE_MESSAGE(err.indexOf("Invalid topic format - no trailing slash") >= 0,
        "the topic was discarded silently");

    core.shutdown();
}

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


// ============================================================================
// Test Runner
// ============================================================================

void setUp() {}

// The log probe is the only state in this file that outlives a test: the
// LoggerCallbacks list is a process-wide singleton, and a Unity assertion that
// fails before stopLogCapture() longjmps straight past it. Left installed, it
// would append every later test's HA warnings to the capture buffers and let one
// failure be read as several.
void tearDown() {
    stopLogCapture();
    // A failed assertion longjmps past whatever a test meant to restore, and the
    // stub is process-wide: leave it down so the next test states its own needs.
    HAL::WiFiImpl::setConnectedForTest(false);
}

// ============================================================================
// Baselines pinned before the discovery payload gains fields (OBS-5)
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
    // The bytes today's code sends for a sensor with a unit and an icon. The
    // OBS-5 fields (entity_category, value_template, json_attributes_topic, a
    // state_topic override, the availability opt-out) must be appended after
    // these keys and emitted only when set, so this string does not move.
    // Keys are Home Assistant's abbreviations since BUG-38; the pin is the same.
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
        "\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
        "\"unit_of_meas\":\"bytes\",\"stat_cla\":\"measurement\"}",
        payload.c_str());
    core.shutdown();
}

// How many sensors a device can declare before the connect burst overflows the
// EventBus. A platform figure: derived from QueueCost below, not written down.
// The eight system entities come out of the same budget.
static void connectWithSensors(Core& core, int n, int& configs) {
    HAConfig config;
    HA::setField(config.nodeId, "test_node", sizeof(config.nodeId));
    auto ha = std::make_unique<HomeAssistantComponent>(config);
    for (int i = 0; i < n; i++) ha->addSensor(String("s") + i, String("Sensor ") + i);
    core.addComponent(std::move(ha));
    core.begin();
    configs = 0;
    core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [&](const MQTTPublishEvent& ev) {
            if (strstr(ev.topic, "/config") != nullptr) configs++;
        });
    simulateMqttConnect(core);
    for (int i = 0; i < 10; i++) core.loop();
}

// The budget holds this many events the size of an MQTTPublishEvent; the connect
// handler spends two of them on its own account (availability, and the command
// subscription), and the rest carry configs.
static size_t sensorsThatFitAtConnect() {
    using DomoticsCore::Utils::QueueCost;
    const size_t refEvents = QueueCost::kBudgetBytes /
        QueueCost::of(sizeof(MQTTPublishEvent), strlen(DomoticsCore::MQTTEvents::EVENT_PUBLISH));
    return refEvents - 2;
}

void test_every_sensor_the_budget_holds_reaches_the_bus_at_connect_without_a_drop() {
    const size_t N = sensorsThatFitAtConnect();
    Core core;
    int configs = 0;
    connectWithSensors(core, (int)N, configs);
    TEST_ASSERT_EQUAL_INT((int)N, configs);
    TEST_ASSERT_EQUAL_UINT32(0, core.getEventBus().getDroppedCount());
    core.shutdown();
}

void test_one_sensor_past_the_budget_costs_a_dropped_event_at_connect() {
    const size_t N = sensorsThatFitAtConnect();
    Core core;
    int configs = 0;
    connectWithSensors(core, (int)N + 1, configs);
    TEST_ASSERT_EQUAL_UINT32(1, core.getEventBus().getDroppedCount());
    core.shutdown();
}


void test_discovery_config_with_the_diagnostic_fields_emits_exactly_them() {
    // OBS-5: the fields a system diagnostic entity needs — a category, a shared
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
// ============================================================================
// A dropped link, at the seam between MQTT and HomeAssistant
// ============================================================================

// isReady() gates every publish this component makes, and it can only fall if
// MQTT says the link is gone. The real component is driven here, not
// simulateMqttConnect(): the whole point is that the event comes from a loop()
// noticing a client that went down on its own.
struct DroppedLinkFixture {
    Core core;
    Components::MQTTComponent* mqtt = nullptr;
    HomeAssistantComponent* ha = nullptr;

    DroppedLinkFixture() {
        HAL::WiFiImpl::setConnectedForTest(true);
        Components::MQTTConfig mcfg;
        mcfg.clientId = "ESP32-bug44";
        mcfg.broker = "192.0.2.1";
        mcfg.autoReconnect = false;        // no reconnection to muddy the reading
        core.addComponent(std::make_unique<Components::MQTTComponent>(mcfg));

        HAConfig hcfg;
        HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
        core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
        core.begin();

        mqtt = core.getComponent<Components::MQTTComponent>("MQTT");
        ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
        TEST_ASSERT_NOT_NULL_MESSAGE(mqtt, "the fixture has no MQTT component");
        TEST_ASSERT_NOT_NULL_MESSAGE(ha, "the fixture has no HomeAssistant component");
        TEST_ASSERT_TRUE_MESSAGE(mqtt->connect(), "the fixture never connected");
        drain();
        TEST_ASSERT_TRUE_MESSAGE(ha->isReady(), "the fixture never came up ready");
    }

    ~DroppedLinkFixture() {
        core.shutdown();
        HAL::WiFiImpl::setConnectedForTest(false);
    }

    void drain() { for (int i = 0; i < 5; i++) core.loop(); }

    // The broker drops the link: the client goes down under the component.
    void dropTheLink() {
        auto* client = mqtt->getClientForTest();
        TEST_ASSERT_NOT_NULL_MESSAGE(client, "nothing to drop: no client was built");
        client->disconnect();
        drain();
    }
};

void test_a_dropped_link_takes_home_assistant_out_of_ready() {
    DroppedLinkFixture f;
    f.dropTheLink();

    TEST_ASSERT_FALSE_MESSAGE(f.ha->isReady(),
        "isReady() answers true over a dead link, so every publish guard in this "
        "component stops guarding");
}

// isReady() is the conjunction of two members, so it falls if either does. The
// guards the entry is about read mqttConnected alone: pin that one through a
// publish, or an edit that clears the other member keeps the test above green
// while every guard stays open.
void test_a_state_published_during_an_outage_reaches_nothing() {
    DroppedLinkFixture f;
    f.ha->addSensor("uptime", "Uptime", "s");
    f.drain();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, (uint32_t)f.mqtt->getQueuedMessageCount(),
        "the fixture starts with a drained queue or this proves nothing");

    f.dropTheLink();
    f.ha->publishState("uptime", String(42));
    f.drain();

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, (uint32_t)f.mqtt->getQueuedMessageCount(),
        "the state was handed to MQTT during the outage: the mqttConnected guard "
        "is still open");
}

// The fall must be transient. mqtt/disconnected now fires where it never did,
// so a component that does not re-arm on the reconnection would go mute for the
// life of the process — a worse failure than the one being fixed.
void test_home_assistant_is_ready_again_after_the_link_returns() {
    DroppedLinkFixture f;
    f.dropTheLink();
    TEST_ASSERT_FALSE(f.ha->isReady());

    TEST_ASSERT_TRUE_MESSAGE(f.mqtt->connect(), "the fixture never reconnected");
    f.drain();

    TEST_ASSERT_TRUE_MESSAGE(f.ha->isReady(),
        "the link came back and HomeAssistant stayed mute");
}

int runAllTests() {
    UNITY_BEGIN();

    // Event tests
    RUN_TEST(test_ha_events_constants_defined);

    // Component creation tests
    RUN_TEST(test_ha_component_creation_default);
    RUN_TEST(test_ha_component_creation_with_config);

    // Config tests
    RUN_TEST(test_ha_config_defaults);
    RUN_TEST(test_ha_config_get_set);
    RUN_TEST(test_ha_availability_topic_auto_generated);
    RUN_TEST(test_ha_availability_topic_custom);
    RUN_TEST(test_ha_config_url_and_area);

    // Entity management tests - Sensors
    RUN_TEST(test_ha_add_sensor_basic);
    RUN_TEST(test_ha_add_sensor_with_all_params);
    RUN_TEST(test_ha_add_multiple_sensors);

    // Entity management tests - Binary Sensors
    RUN_TEST(test_ha_add_binary_sensor_basic);
    RUN_TEST(test_ha_add_binary_sensor_with_class);

    // Entity management tests - Switches
    RUN_TEST(test_ha_add_switch);
    RUN_TEST(test_ha_add_switch_entity_registered);

    // Entity management tests - Lights
    RUN_TEST(test_ha_add_light);

    // Entity management tests - Buttons
    RUN_TEST(test_ha_add_button);

    // Entity management tests - Mixed
    RUN_TEST(test_ha_add_multiple_entity_types);

    // Statistics tests
    RUN_TEST(test_ha_statistics_initial);
    RUN_TEST(test_ha_statistics_after_adding_entities);

    // Connection status tests
    RUN_TEST(test_ha_mqtt_not_connected_initial);
    RUN_TEST(test_ha_not_ready_without_mqtt);

    // Lifecycle tests
    RUN_TEST(test_ha_begin_returns_success);
    RUN_TEST(test_ha_shutdown_returns_success);
    RUN_TEST(test_ha_full_lifecycle);

    // Non-blocking tests
    RUN_TEST(test_ha_loop_non_blocking);

    // Device info tests
    RUN_TEST(test_ha_set_device_info);

    // Edge cases
    RUN_TEST(test_ha_no_entities);
    RUN_TEST(test_ha_component_no_dependencies);
    RUN_TEST(test_ha_empty_config_fields);
    RUN_TEST(test_ha_special_characters_in_node_id);

    // HASwitch autoPublishState - Unit tests
    RUN_TEST(test_switch_auto_publish_default_true);
    RUN_TEST(test_switch_auto_publish_set_false);
    RUN_TEST(test_switch_handle_command_updates_state);
    RUN_TEST(test_switch_handle_command_no_crash);

    // HASwitch autoPublishState - Integration tests
    RUN_TEST(test_switch_command_auto_publishes_state);
    RUN_TEST(test_switch_command_no_auto_publish_when_disabled);
    RUN_TEST(test_switch_optimistic_overrides_auto_publish);
    RUN_TEST(test_switch_manual_publish_after_auto_disabled);
    RUN_TEST(test_switch_optimistic_true_auto_publish_false);

    // publishState() overload resolution tests (bug 008)
    RUN_TEST(test_publish_state_const_char_ptr);
    RUN_TEST(test_publish_state_constexpr_char_ptr);
    RUN_TEST(test_publish_state_bool_still_works);

    // Baselines pinned before OBS-5 changes the discovery payload
    RUN_TEST(test_discovery_config_for_a_sensor_is_this_exact_document);
    RUN_TEST(test_every_sensor_the_budget_holds_reaches_the_bus_at_connect_without_a_drop);
    RUN_TEST(test_one_sensor_past_the_budget_costs_a_dropped_event_at_connect);

    // OBS-5 — the discovery fields and the duplicate-id warning
    RUN_TEST(test_discovery_config_with_the_diagnostic_fields_emits_exactly_them);
    RUN_TEST(test_a_duplicate_entity_id_warns_and_registers_both);
    RUN_TEST(test_a_state_published_through_the_component_lands_on_the_overridden_topic);
    RUN_TEST(test_a_discovery_config_over_the_event_field_is_refused_aloud);
    RUN_TEST(test_a_topic_over_the_event_field_is_refused_and_counted);
    RUN_TEST(test_shutdown_removes_the_discovery_it_published);
    RUN_TEST(test_an_invalid_entity_category_is_left_out_and_warned);
    RUN_TEST(test_publish_state_string_still_works);
    RUN_TEST(test_publish_state_string_literal);

    // MEM-2 — command parse behaviours the char* rewrite could drop
    RUN_TEST(test_ha_command_entity_id_over_63_truncates_and_warns);
    RUN_TEST(test_ha_command_payload_over_127_truncates_and_warns);
    RUN_TEST(test_ha_command_topic_without_slash_is_refused);
    RUN_TEST(test_ha_command_topic_with_one_slash_is_refused);
    RUN_TEST(test_ha_commands_received_counts_only_known_entities);

    // R6 — char[] field tests
    RUN_TEST(test_ha_set_field_truncation);
    RUN_TEST(test_ha_set_field_null_input);
    RUN_TEST(test_ha_node_id_processing);
    RUN_TEST(test_ha_config_no_heap_allocation);
    RUN_TEST(test_discovery_availability_topic_is_the_one_the_will_corrects);
    RUN_TEST(test_an_adopted_topic_is_not_mistaken_for_a_named_one);
    RUN_TEST(test_a_will_that_is_not_retained_is_made_retained);
    RUN_TEST(test_a_regenerated_topic_does_not_push_itself_onto_the_will);
    RUN_TEST(test_a_named_availability_topic_moves_the_will);
    RUN_TEST(test_the_broker_receives_the_will_on_the_advertised_topic);
    RUN_TEST(test_a_session_opened_before_the_move_is_reopened_with_the_new_will);
    RUN_TEST(test_moving_the_will_does_not_strand_a_component_that_never_reconnects);
    RUN_TEST(test_an_empty_will_topic_does_not_blank_availability);
    RUN_TEST(test_without_a_will_the_generated_topic_is_kept);
    RUN_TEST(test_naming_the_topic_after_begin_moves_the_will_too);

    // A dropped link, at the seam between MQTT and HomeAssistant
    RUN_TEST(test_a_dropped_link_takes_home_assistant_out_of_ready);
    RUN_TEST(test_a_state_published_during_an_outage_reaches_nothing);
    RUN_TEST(test_home_assistant_is_ready_again_after_the_link_returns);

    return UNITY_END();
}

#ifdef ARDUINO
void setup() { runAllTests(); }
void loop() {}
#else

int main(int argc, char** argv) { return runAllTests(); }
#endif
