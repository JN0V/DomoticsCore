/**
 * @file test_ha_switch_will.cpp
 * @brief HomeAssistant component: HASwitch state publishing, and the availability topic against the Last Will.
 */

#include <unity.h>
#include "../ha_test_support.h"

// ============================================================================
// HASwitch autoPublishState - Unit Tests
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
// HASwitch autoPublishState - Integration Tests
// ============================================================================


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

// A client id changed at runtime moves the default will with it; availability
// has to follow, or avty_t names a topic the broker never writes offline to.
void test_availability_follows_a_will_moved_by_a_new_client_id() {
    Core core;
    addConnectableMqtt(core, "ESP32-first");
    HAConfig hcfg;
    HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    core.addComponent(std::make_unique<HomeAssistantComponent>(hcfg));
    core.begin();
    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    auto* ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
    TEST_ASSERT_TRUE_MESSAGE(mqtt->isConnected(), "the fixture never connected");
    TEST_ASSERT_EQUAL_STRING("ESP32-first/status", ha->getConfig().availabilityTopic);

    MQTTConfig renamed = mqtt->getConfig();
    renamed.clientId = "kitchen-panel";
    mqtt->setConfig(renamed);
    core.loop();
    core.loop();

    TEST_ASSERT_TRUE(mqtt->isConnected());
    TEST_ASSERT_EQUAL_STRING("kitchen-panel/status", willTopicTheBrokerGot(core).c_str());
    TEST_ASSERT_EQUAL_STRING_MESSAGE("kitchen-panel/status", ha->getConfig().availabilityTopic,
        "availability stayed on a topic the broker no longer corrects");
    core.shutdown();
}

// A will asked for with no topic gets the default one, and availability follows
// it: an empty avty_t would be dropped from every document.
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
    TEST_ASSERT_EQUAL_STRING("ESP32-bug43f/status", mqtt->getConfig().lwtTopic.c_str());
    TEST_ASSERT_EQUAL_STRING_MESSAGE("ESP32-bug43f/status",
                                     ha->getConfig().availabilityTopic,
        "availability does not follow the will the broker holds");

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

static int runAllTests() {
    UNITY_BEGIN();

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

    // char[] field tests
    RUN_TEST(test_discovery_availability_topic_is_the_one_the_will_corrects);
    RUN_TEST(test_an_adopted_topic_is_not_mistaken_for_a_named_one);
    RUN_TEST(test_a_will_that_is_not_retained_is_made_retained);
    RUN_TEST(test_a_regenerated_topic_does_not_push_itself_onto_the_will);
    RUN_TEST(test_a_named_availability_topic_moves_the_will);
    RUN_TEST(test_the_broker_receives_the_will_on_the_advertised_topic);
    RUN_TEST(test_a_session_opened_before_the_move_is_reopened_with_the_new_will);
    RUN_TEST(test_moving_the_will_does_not_strand_a_component_that_never_reconnects);
    RUN_TEST(test_an_empty_will_topic_does_not_blank_availability);
    RUN_TEST(test_availability_follows_a_will_moved_by_a_new_client_id);
    RUN_TEST(test_without_a_will_the_generated_topic_is_kept);
    RUN_TEST(test_naming_the_topic_after_begin_moves_the_will_too);

    return UNITY_END();
}

int main(int argc, char** argv) { return runAllTests(); }
