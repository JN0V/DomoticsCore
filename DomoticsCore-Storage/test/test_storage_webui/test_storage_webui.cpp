/**
 * @file test_storage_webui.cpp
 * @brief TEST-6: coverage for StorageWebUI, which has no input surface.
 *
 * StorageWebUI's handler ignores its arguments and always returns
 * {"success":false} — there is nothing to validate and nothing to fix here.
 * These tests are recorded as coverage, not evidence: none of them can fail
 * against an "unfixed" version, because there is no fix. They pin the read shape
 * of both contexts, the inert handler, and the null-component guard the provider
 * already carries.
 */

#include <unity.h>
#include <map>

#include <DomoticsCore/Logger.h>       // LOG_STORAGE + DLOG_* used by Storage.h
#include <DomoticsCore/Storage_HAL.h>
#include <DomoticsCore/StorageWebUI.h>

using namespace DomoticsCore::Components::WebUI;
using DomoticsCore::Components::StorageComponent;
using DomoticsCore::Components::StorageConfig;

void setUp() {}
void tearDown() {}

// --- a null component is inert, not a crash ------------------------------------
void test_a_null_component_is_inert(void) {
    StorageWebUI provider(nullptr);
    TEST_ASSERT_EQUAL_STRING("{}", provider.getWebUIData("storage_component").c_str());
    TEST_ASSERT_EQUAL_STRING("{}", provider.getWebUIData("storage_settings").c_str());
    String r = provider.handleWebUIRequest("storage_component", "/", "POST", {});
    TEST_ASSERT_TRUE(r.indexOf("\"success\":false") >= 0);
    TEST_ASSERT_EQUAL_STRING("Storage", provider.getWebUIName().c_str());
}

// --- the component context reports the stats -----------------------------------
void test_component_context_reports_the_stats(void) {
    StorageConfig cfg; cfg.namespace_name = "teststore";
    StorageComponent sc(cfg);
    sc.begin();
    StorageWebUI provider(&sc);
    String json = provider.getWebUIData("storage_component");
    TEST_ASSERT_TRUE(json.indexOf("\"namespace\":") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"entries\":") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"free_entries\":") >= 0);
}

// --- the settings context reports the namespace --------------------------------
void test_settings_context_reports_the_namespace(void) {
    StorageConfig cfg; cfg.namespace_name = "teststore";
    StorageComponent sc(cfg);
    sc.begin();
    StorageWebUI provider(&sc);
    String json = provider.getWebUIData("storage_settings");
    TEST_ASSERT_TRUE(json.indexOf("\"namespace\":") >= 0);
    // The settings context carries only the namespace, not the entry counts.
    TEST_ASSERT_TRUE(json.indexOf("\"entries\":") < 0);
}

// --- the handler has no input surface ------------------------------------------
void test_the_handler_is_inert(void) {
    StorageConfig cfg; cfg.namespace_name = "teststore";
    StorageComponent sc(cfg);
    sc.begin();
    StorageWebUI provider(&sc);
    std::map<String, String> p;
    p["field"] = "namespace";
    p["value"] = "hijack";
    String r = provider.handleWebUIRequest("storage_settings", "/", "POST", p);
    TEST_ASSERT_TRUE(r.indexOf("\"success\":false") >= 0);
    // It changed nothing.
    TEST_ASSERT_EQUAL_STRING("teststore", sc.getNamespace().c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_null_component_is_inert);
    RUN_TEST(test_component_context_reports_the_stats);
    RUN_TEST(test_settings_context_reports_the_namespace);
    RUN_TEST(test_the_handler_is_inert);
    return UNITY_END();
}
