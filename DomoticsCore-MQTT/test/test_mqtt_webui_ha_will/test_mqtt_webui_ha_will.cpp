/**
 * @file test_mqtt_webui_ha_will.cpp
 * @brief The MQTT settings page and a HomeAssistant component sharing one Last Will topic.
 *
 * Home Assistant watches the one topic `avty_t` names, and only the broker's
 * Last Will can write `offline` there. An operator who edits the Last Will
 * topic on the MQTT settings page must therefore either move what the discovery
 * documents advertise, or be told no: a device whose will sits on another
 * topic stays green in Home Assistant for ever.
 *
 * This suite lives in the MQTT project because only it has the async web
 * server mock the settings page needs; the HomeAssistant header is reached by
 * its path.
 */

#include <unity.h>

#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/MQTTEvents.h>
#include <DomoticsCore/MQTTWebUI.h>
#include "../../../DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h"

#include <cstring>
#include <map>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

bool contains(const String& haystack, const char* needle) {
    return strstr(haystack.c_str(), needle) != nullptr;
}

// The newest discovery document published for the sensor "s1". The
// subscription outlives the function that makes it, so the lambda captures
// nothing from this frame and the id is released before returning.
String g_lastDiscovery;

void emitConnected(Core& core) {
    core.emit<bool>(DomoticsCore::MQTTEvents::EVENT_CONNECTED, true);
    for (int i = 0; i < 5; i++) core.loop();
}

void drain(Core& core) {
    for (int i = 0; i < 10; i++) core.loop();
}

// What the device would tell Home Assistant on its next connection: a
// connection event, then the discovery documents it publishes.
String discoveryAfterConnect(Core& core) {
    g_lastDiscovery = "";
    uint32_t sub = core.on<MQTTPublishEvent>(DomoticsCore::MQTTEvents::EVENT_PUBLISH,
        [](const MQTTPublishEvent& ev) {
            String topic(ev.topic);
            if (topic.indexOf("/s1/config") >= 0) g_lastDiscovery = ev.payload;
        });
    emitConnected(core);
    core.getEventBus().unsubscribe(sub);
    return g_lastDiscovery;
}

std::map<String, String> fieldValue(const char* field, const char* value) {
    std::map<String, String> params;
    params[String("field")] = String(field);
    params[String("value")] = String(value);
    return params;
}

}  // namespace

void setUp() {}
void tearDown() { HAL::WiFiImpl::setConnectedForTest(false); }

// Given a running device with Home Assistant discovery published,
// When the operator saves a new Last Will topic on the MQTT settings page,
// Then the availability topic in the discovery documents is the new topic,
//      or the edit is refused with a stated reason.
void test_a_last_will_topic_saved_from_the_page_reaches_the_discovery_documents() {
    HAL::WiFiImpl::setConnectedForTest(true);
    Core core;

    MQTTConfig mcfg;
    mcfg.clientId = "ESP32-lwtedit";
    mcfg.broker = "192.0.2.1";
    auto mqttOwned = std::make_unique<MQTTComponent>(mcfg);
    core.addComponent(std::move(mqttOwned));

    // The application named its availability topic, so the will follows it.
    HomeAssistant::HAConfig hcfg;
    HomeAssistant::HA::setField(hcfg.nodeId, "test_node", sizeof(hcfg.nodeId));
    HomeAssistant::HA::setField(hcfg.availabilityTopic, "site/panel/alive",
                                sizeof(hcfg.availabilityTopic));
    auto ha = std::make_unique<HomeAssistant::HomeAssistantComponent>(hcfg);
    ha->addSensor("s1", "Sensor 1");
    core.addComponent(std::move(ha));
    core.begin();
    drain(core);

    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    TEST_ASSERT_NOT_NULL(mqtt);
    TEST_ASSERT_TRUE_MESSAGE(mqtt->isConnected(), "the fixture never connected: this test proves nothing");

    // Discovery is out on the old topic before the operator touches anything.
    const String before = discoveryAfterConnect(core);
    TEST_ASSERT_FALSE_MESSAGE(before.isEmpty(), "no discovery document was published");

    // The operator saves a new Last Will topic.
    WebUI::MQTTWebUI ui(mqtt);
    const String response = ui.handleWebUIRequest(String("mqtt_settings"), String("/api/ui/action"),
                                                   String("POST"), fieldValue("lwt_topic", "site/new/will"));
    drain(core);

    // What Home Assistant is told after the edit has settled.
    const String payload = discoveryAfterConnect(core);
    TEST_ASSERT_FALSE_MESSAGE(payload.isEmpty(), "no discovery document was published after the edit");
    JsonDocument doc;
    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, deserializeJson(doc, payload.c_str()).code(),
                              "the discovery document is not valid JSON");
    const char* avty = doc["avty_t"];
    TEST_ASSERT_NOT_NULL_MESSAGE(avty, "the discovery document advertises no availability topic");

    auto* stub = static_cast<HAL::MQTT::MQTTClientImpl*>(mqtt->getClientForTest());
    const bool accepted = contains(response, "\"success\":true");

    if (accepted) {
        TEST_ASSERT_EQUAL_STRING_MESSAGE("site/new/will", avty,
            "the edit was accepted but the discovery documents still advertise the old topic");
        TEST_ASSERT_EQUAL_STRING_MESSAGE("site/new/will", mqtt->getConfig().lwtTopic.c_str(),
            "the edit was accepted and then reverted on the MQTT side");
    } else {
        TEST_ASSERT_TRUE_MESSAGE(contains(response, "\"error\":\""), "the edit was refused with no reason");
        TEST_ASSERT_FALSE_MESSAGE(contains(response, "\"error\":\"\""), "the edit was refused with an empty reason");
    }
    // Either way, one topic and not two: what Home Assistant watches is what
    // the broker corrects, on the stored will and on the session itself.
    TEST_ASSERT_EQUAL_STRING_MESSAGE(avty, mqtt->getConfig().lwtTopic.c_str(),
        "Home Assistant watches a topic the stored Last Will is not on");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(avty, stub->getLWTTopic().c_str(),
        "Home Assistant watches a topic the broker's will is not on");

    core.shutdown();
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_last_will_topic_saved_from_the_page_reaches_the_discovery_documents);
    return UNITY_END();
}
