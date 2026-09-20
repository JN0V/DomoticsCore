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

    Fixture() : ntp(), ui(&ntp), webui(webConfig) {
        ntp.begin();
        webui.begin();
        ui.init(&webui);
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

void test_the_sync_interval_is_read_in_hours_and_stored_in_seconds() {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("sync_interval", "6")));
    TEST_ASSERT_EQUAL_UINT32(6 * 3600, f.ntp.getConfig().syncInterval);
}

void test_a_sync_interval_of_zero_is_accepted_and_discarded() {
    Fixture f;
    const uint32_t before = f.ntp.getConfig().syncInterval;
    // Pinned as it is, not as it should be: the hours > 0 guard skips the
    // assignment but the handler still answers success, so the UI reports a
    // saved value that was never stored.
    TEST_ASSERT_TRUE_MESSAGE(succeeded(f.post("sync_interval", "0")),
                             "the refusal became visible — update this test with the fix");
    TEST_ASSERT_EQUAL_UINT32(before, f.ntp.getConfig().syncInterval);
}

void test_a_sync_interval_that_is_not_a_number_is_accepted_and_discarded() {
    Fixture f;
    const uint32_t before = f.ntp.getConfig().syncInterval;
    TEST_ASSERT_TRUE(succeeded(f.post("sync_interval", "soon")));
    TEST_ASSERT_EQUAL_UINT32(before, f.ntp.getConfig().syncInterval);
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
    f.post("sync_interval", "3");

    const String data = f.ui.getWebUIData(String("ntp_settings"));
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "UTC0"), data.c_str());
    // Emitted as a number and converted back to hours. Sibling providers emit
    // the same kind of field as a string; the UI reads both.
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "\"sync_interval\":3"), data.c_str());
}

void test_an_unknown_context_serializes_an_empty_document_as_null() {
    Fixture f;
    // Same shape as the other providers: an untouched JsonDocument serializes to
    // "null" while IWebUIProvider's own default is "{}". The serializeJson == 0
    // guard below it never fires, since "null" is four bytes written.
    TEST_ASSERT_EQUAL_STRING("null", f.ui.getWebUIData(String("not_a_context")).c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_timezone_route_streams_the_lookup_table);
    RUN_TEST(test_a_comma_separated_server_list_is_split_and_trimmed);
    RUN_TEST(test_empty_entries_in_the_server_list_are_dropped);
    RUN_TEST(test_a_single_server_needs_no_comma);
    RUN_TEST(test_an_all_empty_server_list_leaves_no_server_at_all);
    RUN_TEST(test_the_sync_interval_is_read_in_hours_and_stored_in_seconds);
    RUN_TEST(test_a_sync_interval_of_zero_is_accepted_and_discarded);
    RUN_TEST(test_a_sync_interval_that_is_not_a_number_is_accepted_and_discarded);
    RUN_TEST(test_enabled_accepts_true_and_one_and_nothing_else);
    RUN_TEST(test_the_timezone_is_stored_verbatim);
    RUN_TEST(test_an_unknown_field_is_named_in_the_refusal);
    RUN_TEST(test_a_method_other_than_get_or_post_is_refused);
    RUN_TEST(test_a_post_without_field_or_value_is_refused);
    RUN_TEST(test_the_settings_context_reports_what_was_saved);
    RUN_TEST(test_an_unknown_context_serializes_an_empty_document_as_null);
    return UNITY_END();
}
