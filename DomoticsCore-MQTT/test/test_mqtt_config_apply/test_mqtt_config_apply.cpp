/**
 * @file test_mqtt_config_apply.cpp
 * @brief What setConfig() hands the next connection.
 *
 * Persistence reaches the component through setConfig() after begin(), so every
 * field the connection uses must be taken from the config at connect time, and
 * an empty client id must mean "generated", as it does at construction.
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

HAL::MQTT::MQTTClientImpl* stubOf(MQTTComponent& mqtt) {
    return static_cast<HAL::MQTT::MQTTClientImpl*>(mqtt.getClientForTest());
}

void connectNow(MQTTComponent& mqtt) {
    HAL::WiFiImpl::setConnectedForTest(true);
    TEST_ASSERT_TRUE(mqtt.connect());
    mqtt.loop();
    TEST_ASSERT_TRUE(mqtt.isConnected());
}

}  // namespace

void setUp() {}
void tearDown() { HAL::WiFiImpl::setConnectedForTest(false); }

void test_an_empty_client_id_from_set_config_connects_with_the_generated_one() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    MQTTComponent mqtt(cfg);
    const String generated = mqtt.getConfig().clientId;
    TEST_ASSERT_FALSE(generated.isEmpty());
    mqtt.begin();

    MQTTConfig loaded = mqtt.getConfig();
    loaded.clientId = "";  // what a stored empty key hands back
    mqtt.setConfig(loaded);
    TEST_ASSERT_EQUAL_STRING(generated.c_str(), mqtt.getConfig().clientId.c_str());

    connectNow(mqtt);
    TEST_ASSERT_EQUAL_STRING(generated.c_str(), stubOf(mqtt)->getClientId().c_str());
    mqtt.shutdown();
}

void test_a_client_id_set_after_begin_reaches_the_connection() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    MQTTComponent mqtt(cfg);
    mqtt.begin();

    MQTTConfig loaded = mqtt.getConfig();
    loaded.clientId = "kitchen-panel";
    mqtt.setConfig(loaded);

    connectNow(mqtt);
    TEST_ASSERT_EQUAL_STRING("kitchen-panel", stubOf(mqtt)->getClientId().c_str());
    mqtt.shutdown();
}

void test_the_keep_alive_loaded_after_begin_reaches_the_connection() {
    // No broker at begin(): the shape of a device whose broker lives in storage.
    MQTTComponent mqtt;
    mqtt.begin();

    MQTTConfig loaded = mqtt.getConfig();
    loaded.broker = "test.local";
    loaded.keepAlive = 60;
    mqtt.setConfig(loaded);

    connectNow(mqtt);
    TEST_ASSERT_EQUAL_UINT16(60, stubOf(mqtt)->getKeepAlive());
    mqtt.shutdown();
}

void test_a_keep_alive_changed_after_begin_reaches_the_next_connection() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    cfg.keepAlive = 60;
    MQTTComponent mqtt(cfg);
    mqtt.begin();

    MQTTConfig changed = mqtt.getConfig();
    changed.keepAlive = 30;
    mqtt.setConfig(changed);

    connectNow(mqtt);
    TEST_ASSERT_EQUAL_UINT16(30, stubOf(mqtt)->getKeepAlive());
    mqtt.shutdown();
}

void test_the_default_will_topic_follows_a_new_client_id() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    cfg.clientId = "first";
    MQTTComponent mqtt(cfg);
    mqtt.begin();
    TEST_ASSERT_EQUAL_STRING("first/status", mqtt.getConfig().lwtTopic.c_str());

    MQTTConfig loaded = mqtt.getConfig();
    loaded.clientId = "second";
    mqtt.setConfig(loaded);
    TEST_ASSERT_EQUAL_STRING("second/status", mqtt.getConfig().lwtTopic.c_str());

    connectNow(mqtt);
    TEST_ASSERT_EQUAL_STRING("second/status", stubOf(mqtt)->getLWTTopic().c_str());
    mqtt.shutdown();
}

void test_a_named_will_topic_stays_when_the_client_id_changes() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    cfg.clientId = "first";
    cfg.lwtTopic = "house/panel/availability";
    MQTTComponent mqtt(cfg);
    mqtt.begin();

    MQTTConfig loaded = mqtt.getConfig();
    loaded.clientId = "second";
    mqtt.setConfig(loaded);
    TEST_ASSERT_EQUAL_STRING("house/panel/availability", mqtt.getConfig().lwtTopic.c_str());
    mqtt.shutdown();
}

void test_an_empty_will_topic_means_the_default() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    cfg.clientId = "panel";
    MQTTComponent mqtt(cfg);
    mqtt.begin();

    MQTTConfig loaded = mqtt.getConfig();
    loaded.lwtTopic = "";
    mqtt.setConfig(loaded);

    connectNow(mqtt);
    TEST_ASSERT_EQUAL_STRING("panel/status", stubOf(mqtt)->getLWTTopic().c_str());
    mqtt.shutdown();
}

void test_a_will_qos_over_two_is_clamped_by_set_config() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    MQTTComponent mqtt(cfg);
    mqtt.begin();

    MQTTConfig loaded = mqtt.getConfig();
    loaded.lwtQoS = 3;
    mqtt.setConfig(loaded);
    TEST_ASSERT_EQUAL_UINT8(2, mqtt.getConfig().lwtQoS);

    connectNow(mqtt);
    TEST_ASSERT_EQUAL_UINT8(2, stubOf(mqtt)->getLWTQoS());
    mqtt.shutdown();
}

void test_a_session_setting_changed_while_connected_reopens_the_session() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    cfg.clientId = "first";
    MQTTComponent mqtt(cfg);
    mqtt.begin();
    connectNow(mqtt);
    const uint32_t connects = mqtt.getStatistics().connectCount;

    MQTTConfig changed = mqtt.getConfig();
    changed.clientId = "second";
    mqtt.setConfig(changed);
    mqtt.loop();

    TEST_ASSERT_TRUE(mqtt.isConnected());
    TEST_ASSERT_EQUAL_UINT32(connects + 1, mqtt.getStatistics().connectCount);
    TEST_ASSERT_EQUAL_STRING("second", stubOf(mqtt)->getClientId().c_str());
    mqtt.shutdown();
}

void test_a_setting_outside_the_session_does_not_reopen_it() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    MQTTComponent mqtt(cfg);
    mqtt.begin();
    connectNow(mqtt);
    const uint32_t connects = mqtt.getStatistics().connectCount;

    MQTTConfig changed = mqtt.getConfig();
    changed.reconnectDelay += 1000;
    mqtt.setConfig(changed);
    mqtt.loop();
    mqtt.loop();

    TEST_ASSERT_EQUAL_UINT32(connects, mqtt.getStatistics().connectCount);
    mqtt.shutdown();
}

void test_tls_turned_on_reaches_the_next_session() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    MQTTComponent mqtt(cfg);
    mqtt.begin();
    connectNow(mqtt);
    TEST_ASSERT_FALSE(stubOf(mqtt)->usesTLS());

    MQTTConfig changed = mqtt.getConfig();
    changed.useTLS = true;
    mqtt.setConfig(changed);
    mqtt.loop();

    TEST_ASSERT_TRUE(mqtt.isConnected());
    TEST_ASSERT_TRUE_MESSAGE(stubOf(mqtt)->usesTLS(), "the session still runs on the plaintext client");
    mqtt.shutdown();
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_an_empty_client_id_from_set_config_connects_with_the_generated_one);
    RUN_TEST(test_a_client_id_set_after_begin_reaches_the_connection);
    RUN_TEST(test_the_keep_alive_loaded_after_begin_reaches_the_connection);
    RUN_TEST(test_a_keep_alive_changed_after_begin_reaches_the_next_connection);
    RUN_TEST(test_the_default_will_topic_follows_a_new_client_id);
    RUN_TEST(test_a_named_will_topic_stays_when_the_client_id_changes);
    RUN_TEST(test_an_empty_will_topic_means_the_default);
    RUN_TEST(test_a_will_qos_over_two_is_clamped_by_set_config);
    RUN_TEST(test_a_session_setting_changed_while_connected_reopens_the_session);
    RUN_TEST(test_a_setting_outside_the_session_does_not_reopen_it);
    RUN_TEST(test_tls_turned_on_reaches_the_next_session);
    return UNITY_END();
}
