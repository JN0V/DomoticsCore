/**
 * @file test_ntp_webui.cpp
 * @brief The NTPWebUI provider — the path a browser takes to the clock.
 *
 * It carries the server-list parsing, the sync-interval conversion and the
 * boolean coercion that nothing had ever exercised, plus the timezone route
 * the settings card populates itself from.
 */

#include <unity.h>

#include <DomoticsCore/NTP.h>
#include <DomoticsCore/NTPWebUI.h>

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

/// One clock and its provider, wired to a WebUI component on the host.
struct Fixture {
    WebUIConfig webConfig;
    NTPComponent ntp;
    WebUI::NTPWebUI ui;
    WebUIComponent webui;

    int saves = 0;

    Fixture() : ntp(), ui(&ntp), webui(webConfig) {
        ntp.begin();
        webui.begin();
        ui.init(&webui);
        ui.setConfigSaveCallback([this](const NTPConfig&) { ++saves; });
    }

    String post(const char* field, const char* value) {
        return ui.handleWebUIRequest(String("ntp_settings"), String("/api/ui/action"),
                                     String("POST"), fieldValue(field, value));
    }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_the_timezone_route_streams_the_lookup_table() {
    Fixture f;
    RecordedRoute* route = AsyncWebServer::findRouteAnywhere("/api/ntp/timezones");
    TEST_ASSERT_NOT_NULL_MESSAGE(route, "/api/ntp/timezones was never registered");

    AsyncWebServerRequest request;
    route->handler(&request);
    TEST_ASSERT_EQUAL_INT(200, request.sentCode);
    TEST_ASSERT_TRUE_MESSAGE(contains(request.sentBody, "UTC0"), request.sentBody.c_str());
    TEST_ASSERT_TRUE(contains(request.sentBody, "\"value\""));
    TEST_ASSERT_TRUE(contains(request.sentBody, "\"label\""));
    // A JSON array, opened and closed — the stream is built by hand.
    TEST_ASSERT_EQUAL_INT('[', request.sentBody.c_str()[0]);
    TEST_ASSERT_EQUAL_INT(']', request.sentBody.c_str()[request.sentBody.length() - 1]);
}

void test_a_comma_separated_server_list_is_split_and_trimmed() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("servers", " a.pool.ntp.org , b.pool.ntp.org ")));

    const NTPConfig cfg = f.ntp.getConfig();
    TEST_ASSERT_EQUAL_UINT32(2, cfg.servers.size());
    TEST_ASSERT_EQUAL_STRING("a.pool.ntp.org", cfg.servers[0].c_str());
    TEST_ASSERT_EQUAL_STRING("b.pool.ntp.org", cfg.servers[1].c_str());
}

void test_empty_entries_in_the_server_list_are_dropped() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("servers", "a.pool.ntp.org,,  ,b.pool.ntp.org,")));

    const NTPConfig cfg = f.ntp.getConfig();
    TEST_ASSERT_EQUAL_UINT32(2, cfg.servers.size());
    TEST_ASSERT_EQUAL_STRING("b.pool.ntp.org", cfg.servers[1].c_str());
}

void test_a_single_server_needs_no_comma() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("servers", "only.pool.ntp.org")));
    TEST_ASSERT_EQUAL_UINT32(1, f.ntp.getConfig().servers.size());
}

void test_an_all_empty_server_list_leaves_no_server_at_all() {
    Fixture f;
    // Pinned as it is: the field accepts a list that clears every server, and
    // the component falls back to pool.ntp.org at sync time rather than here.
    TEST_ASSERT_TRUE(succeeded(f.post("servers", " , , ")));
    TEST_ASSERT_EQUAL_UINT32(0, f.ntp.getConfig().servers.size());
}

void test_the_sync_interval_is_read_and_stored_in_seconds() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("sync_interval", "21600")));
    TEST_ASSERT_EQUAL_UINT32(21600, f.ntp.getConfig().syncInterval);
}

void test_a_sync_interval_below_an_hour_is_refused_by_name() {
    Fixture f;
    const uint32_t before = f.ntp.getConfig().syncInterval;
    for (const char* tooShort : {"0", "1000", "3599"}) {
        const String response = f.post("sync_interval", tooShort);
        TEST_ASSERT_FALSE_MESSAGE(succeeded(response), tooShort);
        TEST_ASSERT_TRUE_MESSAGE(contains(response, "3600"), response.c_str());
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(before, f.ntp.getConfig().syncInterval, tooShort);
    }
    TEST_ASSERT_TRUE(succeeded(f.post("sync_interval", "3600")));
    TEST_ASSERT_EQUAL_UINT32(3600, f.ntp.getConfig().syncInterval);
}

// A value the page cannot type but the public config accepts: it is shown as
// what it is, where dividing by 3600 rendered it as 0 the field then refused.
void test_an_interval_below_an_hour_is_displayed_as_it_is_stored() {
    Fixture f;
    NTPConfig cfg = f.ntp.getConfig();
    cfg.syncInterval = 1000;
    f.ntp.setConfig(cfg);

    const String data = f.ui.getWebUIData(String("ntp_settings"));
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "\"sync_interval\":1000"), data.c_str());
}

void test_a_sync_interval_that_is_not_a_number_is_refused() {
    Fixture f;
    const uint32_t before = f.ntp.getConfig().syncInterval;
    for (const char* bad : {"soon", "", "6h", " 6"}) {
        TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("sync_interval", bad)), bad);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(before, f.ntp.getConfig().syncInterval, bad);
    }
}

void test_a_sync_interval_the_sntp_client_cannot_hold_is_refused() {
    Fixture f;
    const uint32_t before = f.ntp.getConfig().syncInterval;
    // The ceiling is the SNTP client's, not this page's: begin() hands the
    // interval over as milliseconds in a uint32_t, so a second past 4 294 967
    // wraps. A value past a uint32_t is refused by the parser, not by the range.
    TEST_ASSERT_FALSE(succeeded(f.post("sync_interval", "4294968")));
    TEST_ASSERT_EQUAL_UINT32(before, f.ntp.getConfig().syncInterval);

    TEST_ASSERT_FALSE(succeeded(f.post("sync_interval", "4294967296")));
    TEST_ASSERT_EQUAL_UINT32(before, f.ntp.getConfig().syncInterval);

    TEST_ASSERT_TRUE(succeeded(f.post("sync_interval", "4294967")));
    TEST_ASSERT_EQUAL_UINT32(4294967u, f.ntp.getConfig().syncInterval);
    // Read from the config, not from the literal: an assertion over constants
    // would still hold if the handler's ceiling expression were edited wrong.
    TEST_ASSERT_TRUE_MESSAGE(
        (uint64_t)f.ntp.getConfig().syncInterval * 1000ull <= 0xFFFFFFFFull,
        "the accepted ceiling itself overflows the millisecond conversion");
}

void test_enabled_accepts_true_and_one_and_nothing_else() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("enabled", "true")));
    TEST_ASSERT_TRUE(f.ntp.getConfig().enabled);

    TEST_ASSERT_TRUE(succeeded(f.post("enabled", "false")));
    TEST_ASSERT_FALSE(f.ntp.getConfig().enabled);

    TEST_ASSERT_TRUE(succeeded(f.post("enabled", "1")));
    TEST_ASSERT_TRUE(f.ntp.getConfig().enabled);

    // Anything else reads as false rather than as a refusal.
    TEST_ASSERT_TRUE(succeeded(f.post("enabled", "yes")));
    TEST_ASSERT_FALSE_MESSAGE(f.ntp.getConfig().enabled, "\"yes\" enabled the clock");
}

void test_the_timezone_is_stored_verbatim() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("timezone", "CET-1CEST,M3.5.0,M10.5.0/3")));
    TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", f.ntp.getConfig().timezone.c_str());
}

void test_an_unknown_field_is_named_in_the_refusal() {
    Fixture f;
    const String response = f.post("stratum", "2");
    TEST_ASSERT_FALSE(succeeded(response));
    TEST_ASSERT_TRUE_MESSAGE(contains(response, "Unknown field"), response.c_str());
}

void test_every_field_the_settings_card_declares_is_accepted() {
    Fixture f;
    // A field on the card with no dispatch arm is refused on every save, with
    // "Unknown field" drawn under it. Nothing else relates the two lists.
    WebUIContext settings = f.ui.getWebUIContext(String("ntp_settings"));
    TEST_ASSERT_TRUE_MESSAGE(settings.fields.size() > 0, "the settings card declared no field");

    size_t posted = 0;
    for (const auto& field : settings.fields) {
        if (field.readOnly) continue;
        const char* name = field.getNameCStr();
        TEST_ASSERT_TRUE_MESSAGE(name && *name, "a declared field has no name");
        const String response = f.post(name, "1");
        TEST_ASSERT_FALSE_MESSAGE(contains(response, "Unknown field"), name);
        ++posted;
    }
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4, posted,
                                     "the settings card no longer declares four editable fields");
}

// The card ships a default per field. A default its own handler refuses means the
// page and the component disagree about the unit — which is what the interval did
// while the field was denominated in hours and the component stored seconds.
void test_no_field_ships_a_default_its_own_handler_refuses() {
    Fixture f;
    WebUIContext settings = f.ui.getWebUIContext(String("ntp_settings"));
    size_t checked = 0;
    for (const auto& field : settings.fields) {
        if (field.readOnly) continue;
        const char* name = field.getNameCStr();
        const String declared = String(field.getValueCStr());
        if (declared.length() == 0) continue;
        const String response = f.post(name, declared.c_str());
        TEST_ASSERT_TRUE_MESSAGE(succeeded(response), (String(name) + " = " + declared + " -> " + response).c_str());
        ++checked;
    }
    TEST_ASSERT_TRUE_MESSAGE(checked >= 3, "no declared default was checked");
}

void test_a_refused_field_reaches_neither_the_config_nor_the_flash() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("timezone", "UTC0")));
    const int afterOneAccept = f.saves;
    TEST_ASSERT_EQUAL_INT(1, afterOneAccept);

    TEST_ASSERT_FALSE(succeeded(f.post("stratum", "2")));
    TEST_ASSERT_FALSE(succeeded(f.post("sync_interval", "0")));
    TEST_ASSERT_EQUAL_INT_MESSAGE(afterOneAccept, f.saves,
                                  "a refused field still invoked the save callback");
}

void test_a_method_other_than_get_or_post_is_refused() {
    Fixture f;
    const String response = f.ui.handleWebUIRequest(String("ntp_settings"),
                                                    String("/api/ui/action"), String("DELETE"),
                                                    fieldValue("timezone", "UTC0"));
    TEST_ASSERT_FALSE(succeeded(response));
    TEST_ASSERT_TRUE_MESSAGE(contains(response, "Method not allowed"), response.c_str());
}

void test_a_post_without_field_or_value_is_refused() {
    Fixture f;
    std::map<String, String> empty;
    const String response = f.ui.handleWebUIRequest(String("ntp_settings"),
                                                     String("/api/ui/action"), String("POST"),
                                                     empty);
    TEST_ASSERT_FALSE(succeeded(response));
    TEST_ASSERT_TRUE_MESSAGE(contains(response, "Unknown request"), response.c_str());
}

void test_the_settings_context_reports_what_was_saved() {
    Fixture f;
    f.post("timezone", "UTC0");
    f.post("sync_interval", "10800");

    const String data = f.ui.getWebUIData(String("ntp_settings"));
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "UTC0"), data.c_str());
    // Emitted as a number, in seconds. Sibling providers emit the same kind of
    // field as a string; the UI reads both. Asserted with the closing brace, so
    // a prefix of a longer number cannot pass for the value.
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "\"sync_interval\":10800,"), data.c_str());
}

void test_an_unknown_context_answers_an_empty_object() {
    Fixture f;
    // An untouched JsonDocument used to serialize to "null" while
    // IWebUIProvider's own default is "{}". The serializeJson == 0 guard that
    // was meant to catch it never fired: "null" is four bytes written.
    TEST_ASSERT_EQUAL_STRING("{}", f.ui.getWebUIData(String("not_a_context")).c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_timezone_route_streams_the_lookup_table);
    RUN_TEST(test_a_comma_separated_server_list_is_split_and_trimmed);
    RUN_TEST(test_empty_entries_in_the_server_list_are_dropped);
    RUN_TEST(test_a_single_server_needs_no_comma);
    RUN_TEST(test_an_all_empty_server_list_leaves_no_server_at_all);
    RUN_TEST(test_the_sync_interval_is_read_and_stored_in_seconds);
    RUN_TEST(test_a_sync_interval_below_an_hour_is_refused_by_name);
    RUN_TEST(test_an_interval_below_an_hour_is_displayed_as_it_is_stored);
    RUN_TEST(test_a_sync_interval_that_is_not_a_number_is_refused);
    RUN_TEST(test_a_sync_interval_the_sntp_client_cannot_hold_is_refused);
    RUN_TEST(test_enabled_accepts_true_and_one_and_nothing_else);
    RUN_TEST(test_the_timezone_is_stored_verbatim);
    RUN_TEST(test_an_unknown_field_is_named_in_the_refusal);
    RUN_TEST(test_every_field_the_settings_card_declares_is_accepted);
    RUN_TEST(test_no_field_ships_a_default_its_own_handler_refuses);
    RUN_TEST(test_a_refused_field_reaches_neither_the_config_nor_the_flash);
    RUN_TEST(test_a_method_other_than_get_or_post_is_refused);
    RUN_TEST(test_a_post_without_field_or_value_is_refused);
    RUN_TEST(test_the_settings_context_reports_what_was_saved);
    RUN_TEST(test_an_unknown_context_answers_an_empty_object);
    return UNITY_END();
}
