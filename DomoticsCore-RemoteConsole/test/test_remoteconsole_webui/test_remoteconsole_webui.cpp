/**
 * @file test_remoteconsole_webui.cpp
 * @brief The RemoteConsoleWebUI provider — the path a browser takes to the console.
 *
 * It carries input validation nothing had ever exercised: the port range, the
 * log-level range, the refusal of a malformed request, and the change detector
 * that decides whether the UI is pushed an update at all.
 */

#include <unity.h>

#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/RemoteConsoleWebUI.h>

#include <cstring>
#include <map>

using namespace DomoticsCore::Components;

namespace {

bool contains(const String& haystack, const char* needle) {
    return strstr(haystack.c_str(), needle) != nullptr;
}

bool succeeded(const String& response) { return contains(response, "\"success\":true"); }

/// A refusal that names nothing is drawn nowhere: the page reports `data.error`
/// and shows it under the field.
void assertRefusedWith(const String& response, const char* reason) {
    TEST_ASSERT_FALSE_MESSAGE(succeeded(response), response.c_str());
    TEST_ASSERT_TRUE_MESSAGE(contains(response, reason), response.c_str());
}

std::map<String, String> fieldValue(const char* field, const char* value) {
    std::map<String, String> params;
    params[String("field")] = String(field);
    params[String("value")] = String(value);
    return params;
}

/// One console and its provider, wired to a WebUI component on the host.
struct Fixture {
    WebUIConfig webConfig;
    RemoteConsoleComponent console;
    WebUI::RemoteConsoleWebUI ui;
    WebUIComponent webui;

    Fixture() : console(), ui(&console), webui(webConfig) {
        console.begin();
        webui.begin();
        ui.init(&webui);
    }

    String post(const char* field, const char* value) {
        return ui.handleWebUIRequest(String("console_settings"), String("/api/ui/action"),
                                     String("POST"), fieldValue(field, value));
    }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_the_log_levels_route_lists_all_six() {
    Fixture f;
    RecordedRoute* route = AsyncWebServer::findRouteAnywhere("/api/console/loglevels");
    TEST_ASSERT_NOT_NULL_MESSAGE(route, "/api/console/loglevels was never registered");

    AsyncWebServerRequest request;
    route->handler(&request);
    TEST_ASSERT_EQUAL_INT(200, request.sentCode);
    for (const char* label : {"NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE"}) {
        TEST_ASSERT_TRUE_MESSAGE(contains(request.sentBody, label), label);
    }
}

void test_the_settings_context_reports_the_live_port_and_level() {
    Fixture f;
    f.console.setPort(2323);
    f.console.setLogLevel(LOG_LEVEL_DEBUG);

    const String data = f.ui.getWebUIData(String("console_settings"));
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "\"port\":\"2323\""), data.c_str());
    TEST_ASSERT_TRUE_MESSAGE(contains(data, "\"log_level\":\"4\""), data.c_str());
    TEST_ASSERT_TRUE(contains(data, "telnet "));
}

void test_an_unknown_context_answers_an_empty_object() {
    Fixture f;
    // An untouched JsonDocument used to serialize to "null" while the
    // component-missing guard two lines above returned "{}" and
    // IWebUIProvider's own default is "{}".
    TEST_ASSERT_EQUAL_STRING("{}", f.ui.getWebUIData(String("not_a_context")).c_str());
}

void test_a_port_outside_the_range_is_refused() {
    Fixture f;
    const uint16_t before = f.console.getPort();
    TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("port", "0")), "port 0 was accepted");
    TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("port", "65536")), "port 65536 was accepted");
    TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("port", "-1")), "a negative port was accepted");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(before, f.console.getPort(),
                                     "a refused port was applied anyway");
}

void test_a_port_that_is_not_a_number_is_refused() {
    Fixture f;
    const uint16_t before = f.console.getPort();
    // "telnet" read as 0, which the range check caught. A numeric prefix did
    // not: toInt() is atol(), so "2424x" reached setPort() as 2424.
    for (const char* bad : {"telnet", "", "2424x", " 2424"}) {
        TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("port", bad)), bad);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(before, f.console.getPort(), bad);
    }
}

void test_a_port_inside_the_range_is_applied() {
    Fixture f;
    TEST_ASSERT_TRUE_MESSAGE(succeeded(f.post("port", "2424")), "a valid port was refused");
    TEST_ASSERT_EQUAL_UINT16(2424, f.console.getPort());
}

void test_a_log_level_outside_the_range_is_refused() {
    Fixture f;
    f.console.setLogLevel(LOG_LEVEL_INFO);
    TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("log_level", "6")), "level 6 was accepted");
    TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("log_level", "-1")), "level -1 was accepted");
    TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("log_level", "3x")), "level \"3x\" was accepted");
    TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("log_level", "")), "an empty level was accepted");
    // The guard is an upper bound only, so a value that truncates to 0 would
    // pass it and silence the log while answering success.
    TEST_ASSERT_FALSE_MESSAGE(succeeded(f.post("log_level", "4294967296")),
                              "a level past 2^32 was accepted");
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(LOG_LEVEL_INFO),
                                  static_cast<int>(f.console.getLogLevel()),
                                  "a refused level was applied anyway");
}

void test_each_valid_log_level_is_applied() {
    Fixture f;
    for (int level = 0; level <= 5; ++level) {
        char value[4];
        snprintf(value, sizeof(value), "%d", level);
        TEST_ASSERT_TRUE_MESSAGE(succeeded(f.post("log_level", value)), value);
        TEST_ASSERT_EQUAL_INT(level, static_cast<int>(f.console.getLogLevel()));
    }
}

void test_a_request_missing_its_field_or_value_is_refused() {
    Fixture f;
    std::map<String, String> onlyField;
    onlyField[String("field")] = String("port");
    assertRefusedWith(f.ui.handleWebUIRequest(String("console_settings"),
                                              String("/api/ui/action"),
                                              String("POST"), onlyField), "Invalid request");
    std::map<String, String> empty;
    assertRefusedWith(f.ui.handleWebUIRequest(String("console_settings"),
                                              String("/api/ui/action"),
                                              String("POST"), empty), "Invalid request");
}

// Every refusal this provider can answer, each with the reason the page draws.
void test_every_refusal_names_its_reason() {
    Fixture f;
    assertRefusedWith(f.post("port", "telnet"), "Invalid port");
    assertRefusedWith(f.post("port", "0"), "Invalid port");
    assertRefusedWith(f.post("port", "65536"), "Invalid port");
    assertRefusedWith(f.post("log_level", "6"), "Invalid log level");
    assertRefusedWith(f.post("log_level", "3x"), "Invalid log level");
    assertRefusedWith(f.post("baud", "115200"), "Unknown field");

    WebUI::RemoteConsoleWebUI orphan(nullptr);
    assertRefusedWith(orphan.handleWebUIRequest(String("console_settings"), String("/"),
                                                String("POST"), fieldValue("port", "2424")),
                      "Component not available");
}

void test_a_get_or_a_foreign_context_is_refused() {
    Fixture f;
    assertRefusedWith(f.ui.handleWebUIRequest(String("console_settings"),
                                              String("/api/ui/action"), String("GET"),
                                              fieldValue("port", "2424")), "Method not allowed");
    assertRefusedWith(f.ui.handleWebUIRequest(String("other_context"),
                                              String("/api/ui/action"), String("POST"),
                                              fieldValue("port", "2424")), "Unknown context");
}

void test_an_unknown_field_is_refused() {
    Fixture f;
    assertRefusedWith(f.post("baud", "115200"), "Unknown field");
}

void test_the_change_detector_settles_and_wakes_on_a_change() {
    Fixture f;
    // The first comparison has no previous sample to match, so it reports a
    // change; a second with nothing touched must not, or the UI is pushed
    // an update every cycle.
    f.ui.hasDataChanged(String("console_settings"));
    TEST_ASSERT_FALSE_MESSAGE(f.ui.hasDataChanged(String("console_settings")),
                              "an idle console reported a change");

    f.console.setPort(2525);
    TEST_ASSERT_TRUE_MESSAGE(f.ui.hasDataChanged(String("console_settings")),
                             "a changed port went unnoticed");
}

void test_a_foreign_context_always_reports_a_change() {
    Fixture f;
    TEST_ASSERT_TRUE(f.ui.hasDataChanged(String("not_a_context")));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_log_levels_route_lists_all_six);
    RUN_TEST(test_the_settings_context_reports_the_live_port_and_level);
    RUN_TEST(test_an_unknown_context_answers_an_empty_object);
    RUN_TEST(test_a_port_outside_the_range_is_refused);
    RUN_TEST(test_a_port_that_is_not_a_number_is_refused);
    RUN_TEST(test_a_port_inside_the_range_is_applied);
    RUN_TEST(test_a_log_level_outside_the_range_is_refused);
    RUN_TEST(test_each_valid_log_level_is_applied);
    RUN_TEST(test_a_request_missing_its_field_or_value_is_refused);
    RUN_TEST(test_a_get_or_a_foreign_context_is_refused);
    RUN_TEST(test_an_unknown_field_is_refused);
    RUN_TEST(test_every_refusal_names_its_reason);
    RUN_TEST(test_the_change_detector_settles_and_wakes_on_a_change);
    RUN_TEST(test_a_foreign_context_always_reports_a_change);
    return UNITY_END();
}
