/**
 * @file test_systeminfo_webui.cpp
 * @brief BUG-32 / TEST-6: SystemInfoWebUI validates the device name it accepts.
 *
 * The device-name handler had no length cap, no emptiness check and no
 * null-component guard — unlike getWebUIData and hasDataChanged, which both guard.
 * The escaping fix (test_system_header) stops a bad name corrupting the payload;
 * this suite stops the bad name being stored in the first place, and stops the
 * handler crashing on a null component.
 *
 * Removal checks: the cap and empty-refusal tests fail against the pre-fix handler
 * (which stored the value whole and accepted an empty one). The null test is
 * different in kind — without the guard it SEGFAULTS rather than failing red,
 * taking the Unity binary down with it, so it runs last and its removal check is
 * run in isolation, expecting a crash and no summary.
 */

#include <unity.h>
#include <map>
#include <string>

#include <DomoticsCore/Logger.h>  // LOG_SYSTEM + DLOG_* used by SystemInfo.h
#include <DomoticsCore/SystemInfoWebUI.h>

using namespace DomoticsCore::Components::WebUI;
using DomoticsCore::Components::SystemInfoComponent;
using DomoticsCore::Components::SystemInfoConfig;

static std::map<String, String> action(const char* field, const char* value) {
    std::map<String, String> p;
    p["field"] = field;
    p["value"] = value;
    return p;
}

static bool ok(const String& r) { return r.indexOf("\"success\":true") >= 0; }

void setUp() {}
void tearDown() {}

// --- a normal name is accepted and stored ---------------------------------------
void test_a_normal_name_is_accepted(void) {
    SystemInfoComponent sys;
    SystemInfoWebUI provider(&sys);
    String r = provider.handleWebUIRequest("system_settings", "/", "POST", action("device_name", "Kitchen"));
    TEST_ASSERT_TRUE(ok(r));
    TEST_ASSERT_EQUAL_STRING("Kitchen", sys.getConfig().deviceName.c_str());
}

// --- an over-long name is capped at 31 (fails vs unfixed: stored whole) ----------
void test_an_over_long_name_is_capped_at_31(void) {
    SystemInfoComponent sys;
    SystemInfoWebUI provider(&sys);
    // 40 characters.
    const char* longName = "0123456789012345678901234567890123456789";
    String r = provider.handleWebUIRequest("system_settings", "/", "POST", action("device_name", longName));
    TEST_ASSERT_TRUE(ok(r));
    TEST_ASSERT_EQUAL_UINT(31u, sys.getConfig().deviceName.length());
    TEST_ASSERT_EQUAL_STRING("0123456789012345678901234567890", sys.getConfig().deviceName.c_str());
}

// --- an empty name is refused, and changes nothing (fails vs unfixed: blanks it) -
void test_an_empty_name_is_refused(void) {
    SystemInfoComponent sys;
    SystemInfoWebUI provider(&sys);
    const String before = sys.getConfig().deviceName;
    String r = provider.handleWebUIRequest("system_settings", "/", "POST", action("device_name", ""));
    TEST_ASSERT_FALSE(ok(r));
    TEST_ASSERT_EQUAL_STRING(before.c_str(), sys.getConfig().deviceName.c_str());
    // A refusal carries no error key (app.js would pop a modal on data.error).
    TEST_ASSERT_TRUE(r.indexOf("error") < 0);
}

// --- guards that already held: recorded as coverage, not evidence ---------------
void test_an_unknown_field_is_refused(void) {
    SystemInfoComponent sys;
    SystemInfoWebUI provider(&sys);
    TEST_ASSERT_FALSE(ok(provider.handleWebUIRequest("system_settings", "/", "POST", action("nope", "x"))));
}
void test_a_non_post_is_refused(void) {
    SystemInfoComponent sys;
    SystemInfoWebUI provider(&sys);
    TEST_ASSERT_FALSE(ok(provider.handleWebUIRequest("system_settings", "/", "GET", action("device_name", "x"))));
}

// --- getWebUIData shape (coverage) ----------------------------------------------
void test_getwebuidata_carries_the_device_name(void) {
    SystemInfoComponent sys;
    SystemInfoWebUI provider(&sys);
    provider.handleWebUIRequest("system_settings", "/", "POST", action("device_name", "Garage"));
    String json = provider.getWebUIData("system_settings");
    TEST_ASSERT_TRUE(json.indexOf("\"device_name\":\"Garage\"") >= 0);
}

// --- a null component is refused rather than dereferenced ------------------------
// Runs LAST: without the guard this segfaults on sys->getConfig() rather than
// failing red. Its removal check is run in isolation, expecting a crash and no
// Unity summary.
void test_a_null_component_is_refused_rather_than_dereferenced(void) {
    SystemInfoWebUI provider(nullptr);
    String r = provider.handleWebUIRequest("system_settings", "/", "POST", action("device_name", "x"));
    TEST_ASSERT_FALSE(ok(r));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_normal_name_is_accepted);
    RUN_TEST(test_an_over_long_name_is_capped_at_31);
    RUN_TEST(test_an_empty_name_is_refused);
    RUN_TEST(test_an_unknown_field_is_refused);
    RUN_TEST(test_a_non_post_is_refused);
    RUN_TEST(test_getwebuidata_carries_the_device_name);
    RUN_TEST(test_a_null_component_is_refused_rather_than_dereferenced);
    return UNITY_END();
}
