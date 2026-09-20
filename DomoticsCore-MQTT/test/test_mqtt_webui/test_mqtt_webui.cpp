/**
 * @file test_mqtt_webui.cpp
 * @brief The MQTTWebUI provider — the settings page a browser posts to.
 *
 * Alone among the providers it registers no route, so everything it does goes
 * through handleWebUIRequest: the field dispatch, the password field that means
 * "unchanged" when empty, and the Last Will topic, which is the same field a
 * HomeAssistant component reconciles from the other side.
 */

#include <unity.h>

#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/MQTTWebUI.h>

#include <cstring>
#include <map>

using namespace DomoticsCore::Components;

namespace {

bool contains(const String& haystack, const char* needle) {
    return strstr(haystack.c_str(), needle) != nullptr;
}

bool succeeded(const String& response) { return contains(response, "\"success\":true"); }

std::map<String, String> fieldValue(const char* field, const char* value) {
    std::map<String, String> params;
    params[String("field")] = String(field);
    params[String("value")] = String(value);
    return params;
}

/// One broker client and its provider. No WebUI component is needed: this
/// provider has no init() and registers nothing.
struct Fixture {
    MQTTComponent mqtt;
    WebUI::MQTTWebUI ui;

    Fixture() : mqtt(), ui(&mqtt) { mqtt.begin(); }

    String post(const char* field, const char* value) {
        return ui.handleWebUIRequest(String("mqtt_settings"), String("/api/ui/action"),
                                     String("POST"), fieldValue(field, value));
    }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_the_provider_registers_no_route() {
    Fixture f;
    // It is the one provider with no init(), which is why its WebUI.h include
    // was the only one that could be removed outright.
    TEST_ASSERT_NULL(AsyncWebServer::findRouteAnywhere("/api/mqtt/settings"));
}

void test_the_broker_and_client_id_are_stored_verbatim() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("broker", "broker.example.test")));
    TEST_ASSERT_TRUE(succeeded(f.post("client_id", "device-42")));

    const MQTTConfig cfg = f.mqtt.getConfig();
    TEST_ASSERT_EQUAL_STRING("broker.example.test", cfg.broker.c_str());
    TEST_ASSERT_EQUAL_STRING("device-42", cfg.clientId.c_str());
}

void test_an_empty_password_means_unchanged() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("password", "secret")));
    TEST_ASSERT_EQUAL_STRING("secret", f.mqtt.getConfig().password.c_str());

    // The settings page sends an empty password to mean "leave it alone", so an
    // empty value must not clear a stored one.
    TEST_ASSERT_TRUE(succeeded(f.post("password", "")));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("secret", f.mqtt.getConfig().password.c_str(),
                                     "an empty password field cleared the stored one");
}

void test_a_username_is_cleared_by_an_empty_value() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("username", "device")));
    TEST_ASSERT_TRUE(succeeded(f.post("username", "")));
    // Deliberately unlike the password: an empty username does clear it.
    TEST_ASSERT_EQUAL_STRING("", f.mqtt.getConfig().username.c_str());
}

void test_the_port_is_taken_without_any_range_check() {
    Fixture f;
    // Pinned as it is, not as it should be: RemoteConsoleWebUI refuses a port
    // outside 1..65535 and this one does not, so 0 lands in the config and a
    // value past 65535 wraps through uint16_t.
    TEST_ASSERT_TRUE(succeeded(f.post("port", "0")));
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, f.mqtt.getConfig().port,
                                     "a range check appeared — update this test with the fix");

    TEST_ASSERT_TRUE(succeeded(f.post("port", "65536")));
    TEST_ASSERT_EQUAL_UINT16(0, f.mqtt.getConfig().port);

    TEST_ASSERT_TRUE(succeeded(f.post("port", "8883")));
    TEST_ASSERT_EQUAL_UINT16(8883, f.mqtt.getConfig().port);
}

void test_the_boolean_fields_accept_true_and_one_and_nothing_else() {
    Fixture f;
    for (const char* field : {"use_tls", "lwt_enabled"}) {
        TEST_ASSERT_TRUE(succeeded(f.post(field, "true")));
        TEST_ASSERT_TRUE(succeeded(f.post(field, "0")));
    }
    const MQTTConfig cfg = f.mqtt.getConfig();
    TEST_ASSERT_FALSE(cfg.useTLS);
    TEST_ASSERT_FALSE(cfg.enableLWT);

    TEST_ASSERT_TRUE(succeeded(f.post("use_tls", "yes")));
    TEST_ASSERT_FALSE_MESSAGE(f.mqtt.getConfig().useTLS, "\"yes\" enabled TLS");
}

void test_the_last_will_topic_is_settable_from_this_page() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("lwt_topic", "devices/42/status")));
    TEST_ASSERT_EQUAL_STRING("devices/42/status", f.mqtt.getConfig().lwtTopic.c_str());

    // This is the live half of a deferred item: a HomeAssistant component
    // reconciles its availability topic with this field at begin() and through
    // setConfig(), but nothing tells it when the move comes from here.
    TEST_ASSERT_TRUE(succeeded(f.post("lwt_message", "gone")));
    TEST_ASSERT_EQUAL_STRING("gone", f.mqtt.getConfig().lwtMessage.c_str());
}

void test_an_unknown_field_reports_success_and_changes_nothing() {
    Fixture f;
    const MQTTConfig before = f.mqtt.getConfig();
    // Pinned as it is: the dispatch has no else, so an unknown field falls
    // through to setConfig() with an unmodified copy and answers success.
    // NTPWebUI refuses the same request with "Unknown field".
    TEST_ASSERT_TRUE_MESSAGE(succeeded(f.post("keep_alive", "120")),
                             "a refusal appeared — update this test with the fix");
    TEST_ASSERT_EQUAL_UINT16(before.keepAlive, f.mqtt.getConfig().keepAlive);
}

void test_a_method_other_than_get_or_post_is_refused() {
    Fixture f;
    const String response = f.ui.handleWebUIRequest(String("mqtt_settings"),
                                                     String("/api/ui/action"), String("PUT"),
                                                     fieldValue("broker", "x"));
    TEST_ASSERT_FALSE(succeeded(response));
    TEST_ASSERT_TRUE_MESSAGE(contains(response, "Method not allowed"), response.c_str());
}

void test_a_post_without_field_or_value_is_refused() {
    Fixture f;
    std::map<String, String> empty;
    const String response = f.ui.handleWebUIRequest(String("mqtt_settings"),
                                                     String("/api/ui/action"), String("POST"),
                                                     empty);
    TEST_ASSERT_FALSE(succeeded(response));
    TEST_ASSERT_TRUE_MESSAGE(contains(response, "Unknown request"), response.c_str());
}

void test_the_settings_context_reports_what_was_saved() {
    Fixture f;
    f.post("broker", "broker.example.test");
    f.post("port", "8883");

    const String data = f.ui.getWebUIData(String("mqtt_settings"));
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "broker.example.test"), data.c_str());
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "8883"), data.c_str());
}

void test_the_status_context_reports_a_disconnected_client() {
    Fixture f;
    const String data = f.ui.getWebUIData(String("mqtt_status"));
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "disconnected") || contains(data, "Disconnected"),
                             data.c_str());
}

void test_an_unknown_context_serializes_an_empty_document_as_null() {
    Fixture f;
    TEST_ASSERT_EQUAL_STRING("null", f.ui.getWebUIData(String("not_a_context")).c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_provider_registers_no_route);
    RUN_TEST(test_the_broker_and_client_id_are_stored_verbatim);
    RUN_TEST(test_an_empty_password_means_unchanged);
    RUN_TEST(test_a_username_is_cleared_by_an_empty_value);
    RUN_TEST(test_the_port_is_taken_without_any_range_check);
    RUN_TEST(test_the_boolean_fields_accept_true_and_one_and_nothing_else);
    RUN_TEST(test_the_last_will_topic_is_settable_from_this_page);
    RUN_TEST(test_an_unknown_field_reports_success_and_changes_nothing);
    RUN_TEST(test_a_method_other_than_get_or_post_is_refused);
    RUN_TEST(test_a_post_without_field_or_value_is_refused);
    RUN_TEST(test_the_settings_context_reports_what_was_saved);
    RUN_TEST(test_the_status_context_reports_a_disconnected_client);
    RUN_TEST(test_an_unknown_context_serializes_an_empty_document_as_null);
    return UNITY_END();
}
