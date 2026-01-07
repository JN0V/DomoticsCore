// Unit tests for WifiWebUI provider
// Tests: context building, badge data, STA activation flow
// Reproduces bugs: badge not showing, STA crash without SSID

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/WifiWebUI.h>
#include <DomoticsCore/IWebUIProvider.h>
#include <ArduinoJson.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// Test fixtures
static Core* testCore = nullptr;
static WifiComponent* wifi = nullptr;
static WifiWebUI* wifiWebUI = nullptr;

void setUp(void) {
    testCore = new Core();
    
    // Create WiFi component in AP-only mode (empty SSID)
    auto wifiComp = std::make_unique<WifiComponent>("", "");
    wifi = wifiComp.get();
    testCore->addComponent(std::move(wifiComp));
    
    // Initialize core to trigger component init() - this enables AP mode
    CoreConfig cfg;
    cfg.deviceName = "TestDevice";
    cfg.logLevel = 0; // Quiet for tests
    testCore->begin(cfg);
    
    // Create WifiWebUI provider
    wifiWebUI = new WifiWebUI(wifi);
}

void tearDown(void) {
    delete wifiWebUI;
    wifiWebUI = nullptr;
    wifi = nullptr;
    
    if (testCore) {
        testCore->shutdown();
        delete testCore;
        testCore = nullptr;
    }
}

// ============================================================================
// BUG REPRODUCTION: Contexts not being built
// ============================================================================

void test_wifi_webui_builds_contexts(void) {
    // Bug: wifi_status context was not being registered
    // Expected: buildContexts should create at least wifi_status, wifi_component, wifi_settings
    
    std::vector<WebUIContext> contexts;
    int contextCount = 0;
    
    wifiWebUI->forEachContext([&contextCount, &contexts](const WebUIContext& ctx) {
        contextCount++;
        contexts.push_back(ctx);
        return true;
    });
    
    // Must have at least 3 contexts
    TEST_ASSERT_GREATER_OR_EQUAL(3, contextCount);
    
    // Verify wifi_status exists (the badge)
    bool hasWifiStatus = false;
    bool hasWifiComponent = false;
    bool hasWifiSettings = false;
    
    for (const auto& ctx : contexts) {
        if (ctx.contextId == "wifi_status") hasWifiStatus = true;
        if (ctx.contextId == "wifi_component") hasWifiComponent = true;
        if (ctx.contextId == "wifi_settings") hasWifiSettings = true;
    }
    
    TEST_ASSERT_TRUE_MESSAGE(hasWifiStatus, "wifi_status context missing - badge won't work");
    TEST_ASSERT_TRUE_MESSAGE(hasWifiComponent, "wifi_component context missing");
    TEST_ASSERT_TRUE_MESSAGE(hasWifiSettings, "wifi_settings context missing");
}

void test_wifi_status_context_is_header_badge(void) {
    // Bug: Badge wasn't showing because context might have wrong location
    
    WebUIContext* statusCtx = nullptr;
    wifiWebUI->forEachContext([&statusCtx](const WebUIContext& ctx) {
        if (ctx.contextId == "wifi_status") {
            statusCtx = new WebUIContext(ctx);
            return false; // stop iteration
        }
        return true;
    });
    
    TEST_ASSERT_NOT_NULL_MESSAGE(statusCtx, "wifi_status context not found");
    TEST_ASSERT_EQUAL_MESSAGE(WebUILocation::HeaderStatus, statusCtx->location, 
                              "wifi_status must be HeaderStatus for badge");
    
    delete statusCtx;
}

// ============================================================================
// BUG REPRODUCTION: Badge data format
// ============================================================================

void test_wifi_status_data_contains_required_fields(void) {
    // Bug: Frontend expects 'icon' and 'tooltip' fields for badge
    
    String data = wifiWebUI->getWebUIData("wifi_status");
    TEST_ASSERT_FALSE_MESSAGE(data.isEmpty(), "wifi_status data is empty");
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);
    TEST_ASSERT_FALSE_MESSAGE(err, "wifi_status data is not valid JSON");
    
    // Required fields for badge (ArduinoJson 7: use is<T>() instead of containsKey)
    TEST_ASSERT_TRUE_MESSAGE(doc["state"].is<const char*>(), "Missing 'state' field");
    TEST_ASSERT_TRUE_MESSAGE(doc["icon"].is<const char*>(), "Missing 'icon' field for badge");
    TEST_ASSERT_TRUE_MESSAGE(doc["tooltip"].is<const char*>(), "Missing 'tooltip' field for badge");
}

void test_wifi_status_ap_mode_icon(void) {
    // In AP-only mode, icon should be dc-wifi-ap
    
    String data = wifiWebUI->getWebUIData("wifi_status");
    JsonDocument doc;
    deserializeJson(doc, data);
    
    const char* icon = doc["icon"];
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("dc-wifi-ap", icon, 
                                     "AP mode should use dc-wifi-ap icon");
}

void test_wifi_status_state_on_when_ap_enabled(void) {
    // When AP is enabled, state should be ON
    
    String data = wifiWebUI->getWebUIData("wifi_status");
    JsonDocument doc;
    deserializeJson(doc, data);
    
    const char* state = doc["state"];
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("ON", state, 
                                     "State should be ON when AP is enabled");
}

void test_wifi_status_tooltip_shows_ap_ssid(void) {
    // Tooltip should show AP SSID when in AP mode
    
    String data = wifiWebUI->getWebUIData("wifi_status");
    JsonDocument doc;
    deserializeJson(doc, data);
    
    const char* tooltip = doc["tooltip"];
    TEST_ASSERT_NOT_NULL_MESSAGE(tooltip, "Tooltip is null");
    // Should contain the AP SSID (DomoticsCore-XXXXXX pattern)
    TEST_ASSERT_TRUE_MESSAGE(strlen(tooltip) > 0, "Tooltip should not be empty");
}

// ============================================================================
// BUG REPRODUCTION: STA activation crash
// ============================================================================

// Helper to create params map for handleWebUIRequest
static std::map<String, String> makeParams(const String& field, const String& value) {
    std::map<String, String> params;
    params["field"] = field;
    params["value"] = value;
    return params;
}

void test_sta_enable_without_ssid_returns_error(void) {
    // Bug: Enabling STA without SSID caused crash
    // Fix: Should return error JSON without crashing
    
    auto params = makeParams("wifi_enabled", "true");
    String result = wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", params);
    
    TEST_ASSERT_FALSE_MESSAGE(result.isEmpty(), "Response should not be empty");
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, result);
    TEST_ASSERT_FALSE_MESSAGE(err, "Response is not valid JSON");
    
    // Should indicate failure
    bool success = doc["success"] | true;  // default to true if missing
    TEST_ASSERT_FALSE_MESSAGE(success, "Should return success:false when SSID missing");
    
    // Should have error message
    const char* error = doc["error"];
    TEST_ASSERT_NOT_NULL_MESSAGE(error, "Should have error message");
}

void test_sta_enable_with_ssid_succeeds(void) {
    // After setting SSID, enabling STA should work
    
    // First set SSID
    auto ssidParams = makeParams("ssid", "TestNetwork");
    String ssidResult = wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", ssidParams);
    JsonDocument ssidDoc;
    deserializeJson(ssidDoc, ssidResult);
    TEST_ASSERT_TRUE_MESSAGE(ssidDoc["success"] | false, "Setting SSID should succeed");
    
    // Now enable STA
    auto enableParams = makeParams("wifi_enabled", "true");
    String enableResult = wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", enableParams);
    JsonDocument enableDoc;
    deserializeJson(enableDoc, enableResult);
    
    // Should succeed (even if connection fails, the operation itself should succeed)
    bool success = enableDoc["success"] | false;
    TEST_ASSERT_TRUE_MESSAGE(success, "Enabling STA with SSID should succeed");
}

// ============================================================================
// BUG REPRODUCTION: Crash when disabling AP from WebSocket callback
// ============================================================================

void test_ap_disable_triggers_mode_change(void) {
    // Bug: Disabling AP from WebSocket callback crashes ESP8266
    // This test simulates what happens when user disables AP in WebUI
    
    // Verify we start in AP mode
    TEST_ASSERT_TRUE_MESSAGE(wifi->isAPEnabled(), "Should start in AP mode");
    
    // First set STA credentials (like user would in WebUI)
    auto ssidParams = makeParams("ssid", "TestNetwork");
    wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", ssidParams);
    
    auto passParams = makeParams("sta_password", "testpass123");
    wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", passParams);
    
    // Now disable AP - this is what triggers the crash on ESP8266
    auto params = makeParams("ap_enabled", "false");
    String result = wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", params);
    
    // Should return success without crashing
    TEST_ASSERT_FALSE_MESSAGE(result.isEmpty(), "Response should not be empty");
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, result);
    TEST_ASSERT_FALSE_MESSAGE(err, "Response is not valid JSON");
    
    bool success = doc["success"] | false;
    TEST_ASSERT_TRUE_MESSAGE(success, "Disabling AP should succeed");
}

void test_ap_disable_applies_sta_credentials(void) {
    // Bug: STA credentials were not applied when disabling AP
    
    // Set STA credentials first
    auto ssidParams = makeParams("ssid", "MyNetwork");
    wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", ssidParams);
    
    auto passParams = makeParams("sta_password", "mypassword");
    wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", passParams);
    
    // Disable AP - should apply pending STA credentials
    auto params = makeParams("ap_enabled", "false");
    wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", params);
    
    // Check that WiFi component has the SSID configured
    String configuredSSID = wifi->getConfiguredSSID();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("MyNetwork", configuredSSID.c_str(), 
                                     "STA SSID should be applied when disabling AP");
}

void test_mode_change_sequence_ap_to_sta(void) {
    // Reproduce the exact sequence that crashes on ESP8266
    // 1. Start in AP mode
    // 2. User enters SSID
    // 3. User enters password  
    // 4. User disables AP (triggers mode change to STA)
    
    TEST_ASSERT_TRUE(wifi->isAPEnabled());
    TEST_ASSERT_FALSE(wifi->isWifiEnabled());
    
    // Step 1: Enter SSID
    auto p1 = makeParams("ssid", "CrashTestNetwork");
    String r1 = wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", p1);
    TEST_ASSERT_TRUE(r1.indexOf("success") > 0);
    
    // Step 2: Enter password
    auto p2 = makeParams("sta_password", "crashtest123");
    String r2 = wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", p2);
    TEST_ASSERT_TRUE(r2.indexOf("success") > 0);
    
    // Step 3: Disable AP (this is where ESP8266 crashes)
    auto p3 = makeParams("ap_enabled", "false");
    String r3 = wifiWebUI->handleWebUIRequest("wifi_settings", "/api/wifi", "POST", p3);
    
    // If we get here without crash, the fix is working
    TEST_ASSERT_FALSE_MESSAGE(r3.isEmpty(), "Mode change should complete without crash");
    
    // Verify state after mode change
    // Note: In native test, WiFi HAL is mocked, so actual connection won't happen
    // But we can verify the config was applied
    TEST_ASSERT_EQUAL_STRING("CrashTestNetwork", wifi->getConfiguredSSID().c_str());
}

// ============================================================================
// Data update flow tests
// ============================================================================

void test_wifi_settings_data_reflects_current_state(void) {
    // Settings data should show current configuration
    
    String data = wifiWebUI->getWebUIData("wifi_settings");
    TEST_ASSERT_FALSE_MESSAGE(data.isEmpty(), "wifi_settings data is empty");
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);
    TEST_ASSERT_FALSE_MESSAGE(err, "wifi_settings data is not valid JSON");
    
    // Should have AP enabled by default (we started in AP mode)
    const char* apEnabled = doc["ap_enabled"];
    TEST_ASSERT_NOT_NULL(apEnabled);
    TEST_ASSERT_EQUAL_STRING("true", apEnabled);
}

void test_has_data_changed_returns_false_when_unchanged(void) {
    // First call might return true (initial state)
    wifiWebUI->hasDataChanged("wifi_status");
    
    // Second call without any changes should return false
    bool changed = wifiWebUI->hasDataChanged("wifi_status");
    TEST_ASSERT_FALSE_MESSAGE(changed, "Data should not change when state is stable");
}

// ============================================================================
// Test runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Context building tests (bug: contexts not registered)
    RUN_TEST(test_wifi_webui_builds_contexts);
    RUN_TEST(test_wifi_status_context_is_header_badge);
    
    // Badge data tests (bug: badge not working)
    RUN_TEST(test_wifi_status_data_contains_required_fields);
    RUN_TEST(test_wifi_status_ap_mode_icon);
    RUN_TEST(test_wifi_status_state_on_when_ap_enabled);
    RUN_TEST(test_wifi_status_tooltip_shows_ap_ssid);
    
    // STA activation tests (bug: crash without SSID)
    RUN_TEST(test_sta_enable_without_ssid_returns_error);
    RUN_TEST(test_sta_enable_with_ssid_succeeds);
    
    // AP disable crash tests (bug: crash when disabling AP from WebSocket)
    RUN_TEST(test_ap_disable_triggers_mode_change);
    RUN_TEST(test_ap_disable_applies_sta_credentials);
    RUN_TEST(test_mode_change_sequence_ap_to_sta);
    
    // Data flow tests
    RUN_TEST(test_wifi_settings_data_reflects_current_state);
    RUN_TEST(test_has_data_changed_returns_false_when_unchanged);
    
    return UNITY_END();
}
