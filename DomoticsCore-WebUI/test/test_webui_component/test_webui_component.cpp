/**
 * @file test_webui_component.cpp
 * @brief WebUI component: WebUIConfig, WebUIField, WebUIContext, location, presentation and LazyState.
 */

#include <unity.h>
#include "../webui_test_support.h"

void setUp() {}
void tearDown() {}

// ============================================================================
// WebUIConfig Tests
// ============================================================================

void test_webui_config_defaults() {
    WebUIConfig config;

    TEST_ASSERT_EQUAL_STRING("DomoticsCore Device", config.deviceName);
    TEST_ASSERT_EQUAL_STRING("auto", config.theme);
    TEST_ASSERT_EQUAL_UINT16(80, config.port);
    TEST_ASSERT_TRUE(config.enableWebSocket);
    TEST_ASSERT_EQUAL_INT(5000, config.wsUpdateInterval);
    TEST_ASSERT_FALSE(config.useFileSystem);
    TEST_ASSERT_EQUAL_STRING("/webui", config.staticPath);
    TEST_ASSERT_EQUAL_STRING("#007acc", config.primaryColor);
    TEST_ASSERT_FALSE(config.enableAuth);
    TEST_ASSERT_EQUAL_STRING("admin", config.username);
    TEST_ASSERT_EQUAL_STRING("", config.password);
    TEST_ASSERT_EQUAL_INT(3, config.maxWebSocketClients);
    TEST_ASSERT_EQUAL_INT(5000, config.apiTimeout);
    TEST_ASSERT_TRUE(config.enableCompression);
    TEST_ASSERT_TRUE(config.enableCaching);
    TEST_ASSERT_FALSE(config.enableCORS);
}

// SEC-14: the settings-card rules live on the config, where a native test reaches them
void test_webui_config_refuses_auth_without_a_password() {
    WebUIConfig c;
    const char* err = nullptr;
    TEST_ASSERT_FALSE(c.applySetting("enable_auth", "true", err));
    TEST_ASSERT_EQUAL_STRING("Set a password before enabling authentication", err);
    TEST_ASSERT_FALSE(c.enableAuth);
    TEST_ASSERT_TRUE(c.applySetting("password", "s3cret", err));
    TEST_ASSERT_TRUE(c.applySetting("enable_auth", "true", err));
    TEST_ASSERT_TRUE(c.enableAuth);
    TEST_ASSERT_FALSE(c.applySetting("password", "", err));
    TEST_ASSERT_EQUAL_STRING("Authentication is enabled; the password cannot be empty", err);
    TEST_ASSERT_EQUAL_STRING("s3cret", c.password);
    TEST_ASSERT_TRUE(c.enableAuth);
    TEST_ASSERT_TRUE(c.applySetting("enable_auth", "false", err));
    TEST_ASSERT_FALSE(c.applySetting("password", "", err));
    TEST_ASSERT_EQUAL_STRING("Password unchanged", err);
    TEST_ASSERT_EQUAL_STRING("s3cret", c.password);
    TEST_ASSERT_FALSE(c.applySetting("nope", "x", err));
    TEST_ASSERT_EQUAL_STRING("Unknown field", err);
}

void test_webui_config_applies_the_other_settings_fields() {
    WebUIConfig c;
    const char* err = nullptr;
    TEST_ASSERT_TRUE(c.applySetting("theme", "dark", err));
    TEST_ASSERT_TRUE(c.applySetting("primary_color", "#123456", err));
    TEST_ASSERT_TRUE(c.applySetting("username", "bench", err));
    TEST_ASSERT_FALSE(c.applySetting("enable_auth", "1", err));  // still no password
    TEST_ASSERT_EQUAL_STRING("dark", c.theme);
    TEST_ASSERT_EQUAL_STRING("#123456", c.primaryColor);
    TEST_ASSERT_EQUAL_STRING("bench", c.username);
    TEST_ASSERT_NOT_NULL(err);  // set by the refusal
}

void test_webui_config_refuses_an_empty_or_unknown_value() {
    WebUIConfig c;
    const char* err = nullptr;
    TEST_ASSERT_TRUE(c.applySetting("theme", "light", err));
    TEST_ASSERT_TRUE(c.applySetting("primary_color", "#123456", err));
    TEST_ASSERT_TRUE(c.applySetting("username", "bench", err));

    TEST_ASSERT_FALSE(c.applySetting("theme", "", err));
    TEST_ASSERT_EQUAL_STRING("Theme must be dark, light or auto", err);
    TEST_ASSERT_FALSE(c.applySetting("theme", "blue", err));
    TEST_ASSERT_FALSE(c.applySetting("primary_color", "", err));
    TEST_ASSERT_EQUAL_STRING("Primary color cannot be empty", err);
    TEST_ASSERT_FALSE(c.applySetting("username", "", err));
    TEST_ASSERT_EQUAL_STRING("Username cannot be empty", err);

    TEST_ASSERT_EQUAL_STRING("light", c.theme);
    TEST_ASSERT_EQUAL_STRING("#123456", c.primaryColor);
    TEST_ASSERT_EQUAL_STRING("bench", c.username);
}

void test_webui_config_normalize_auth_clears_an_unusable_flag() {
    WebUIConfig c;
    c.enableAuth = true;   // the stored state SEC-14 guards against: auth on, no password
    TEST_ASSERT_FALSE(c.authIsUsable());
    TEST_ASSERT_TRUE(c.normalizeAuth());
    TEST_ASSERT_FALSE(c.enableAuth);
    TEST_ASSERT_FALSE(c.normalizeAuth());
    c.setPassword("x");
    c.enableAuth = true;
    TEST_ASSERT_TRUE(c.authIsUsable());
    TEST_ASSERT_FALSE(c.normalizeAuth());
    TEST_ASSERT_TRUE(c.enableAuth);
}

void test_webui_config_custom_values() {
    WebUIConfig config;
    config.setDeviceName("Custom Device");
    config.setTheme("dark");
    config.port = 8080;
    config.enableWebSocket = false;
    config.wsUpdateInterval = 1000;
    config.maxWebSocketClients = 5;
    config.enableAuth = true;
    config.setUsername("user");
    config.setPassword("secret");

    TEST_ASSERT_EQUAL_STRING("Custom Device", config.deviceName);
    TEST_ASSERT_EQUAL_STRING("dark", config.theme);
    TEST_ASSERT_EQUAL_UINT16(8080, config.port);
    TEST_ASSERT_FALSE(config.enableWebSocket);
    TEST_ASSERT_EQUAL_INT(1000, config.wsUpdateInterval);
    TEST_ASSERT_EQUAL_INT(5, config.maxWebSocketClients);
    TEST_ASSERT_TRUE(config.enableAuth);
    TEST_ASSERT_EQUAL_STRING("user", config.username);
    TEST_ASSERT_EQUAL_STRING("secret", config.password);
}

// ============================================================================
// WebUIField Tests
// ============================================================================

void test_webui_field_basic_construction() {
    WebUIField field("temp", "Temperature", WebUIFieldType::Number, "25.5", "°C", true);

    TEST_ASSERT_EQUAL_STRING("temp", field.getNameCStr());
    TEST_ASSERT_EQUAL_STRING("Temperature", field.getLabelCStr());
    TEST_ASSERT_EQUAL(WebUIFieldType::Number, field.type);
    TEST_ASSERT_EQUAL_STRING("25.5", field.getValueCStr());
    TEST_ASSERT_EQUAL_STRING("°C", field.getUnitCStr());
    TEST_ASSERT_TRUE(field.readOnly);
}

void test_webui_field_default_values() {
    WebUIField field("status", "Status", WebUIFieldType::Text);

    TEST_ASSERT_EQUAL_STRING("status", field.getNameCStr());
    TEST_ASSERT_EQUAL_STRING("Status", field.getLabelCStr());
    TEST_ASSERT_EQUAL(WebUIFieldType::Text, field.type);
    TEST_ASSERT_TRUE(field.value.isEmpty());
    TEST_ASSERT_TRUE(field.unit.isEmpty());
    TEST_ASSERT_FALSE(field.readOnly);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, field.minValue);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 100.0, field.maxValue);
}

void test_webui_field_fluent_range() {
    WebUIField field("brightness", "Brightness", WebUIFieldType::Slider);
    field.range(0, 255);

    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, field.minValue);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 255.0, field.maxValue);
}

void test_webui_field_fluent_choices() {
    WebUIField field("mode", "Mode", WebUIFieldType::Select);
    std::vector<String> opts = {"auto", "manual", "off"};
    field.choices(opts);

    TEST_ASSERT_EQUAL(3, field.options.size());
    TEST_ASSERT_EQUAL_STRING("auto", field.options[0].c_str());
    TEST_ASSERT_EQUAL_STRING("manual", field.options[1].c_str());
    TEST_ASSERT_EQUAL_STRING("off", field.options[2].c_str());
}

void test_webui_field_fluent_add_option() {
    WebUIField field("speed", "Speed", WebUIFieldType::Select);
    field.addOption("low", "Low Speed")
         .addOption("medium", "Medium Speed")
         .addOption("high", "High Speed");

    TEST_ASSERT_EQUAL(3, field.options.size());
    TEST_ASSERT_EQUAL_STRING("low", field.options[0].c_str());
    TEST_ASSERT_EQUAL_STRING("Low Speed", field.optionLabels["low"].c_str());
    TEST_ASSERT_EQUAL_STRING("medium", field.options[1].c_str());
    TEST_ASSERT_EQUAL_STRING("Medium Speed", field.optionLabels["medium"].c_str());
}

void test_webui_field_fluent_api() {
    WebUIField field("power", "Power", WebUIFieldType::Button);
    field.api("/api/power/set");

    TEST_ASSERT_EQUAL_STRING("/api/power/set", field.getEndpointCStr());
}

void test_webui_field_copy_constructor() {
    WebUIField original("test", "Test", WebUIFieldType::Number, "42", "units", false);
    original.range(0, 100);
    original.addOption("a", "Option A");

    WebUIField copy(original);

    TEST_ASSERT_EQUAL_STRING("test", copy.getNameCStr());
    TEST_ASSERT_EQUAL_STRING("Test", copy.getLabelCStr());
    TEST_ASSERT_EQUAL_STRING("42", copy.getValueCStr());
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, copy.minValue);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 100.0, copy.maxValue);
    TEST_ASSERT_EQUAL(1, copy.options.size());
}

void test_webui_field_all_types() {
    // Verify all field types are accessible
    WebUIField f1("a", "A", WebUIFieldType::Text);
    WebUIField f2("b", "B", WebUIFieldType::Number);
    WebUIField f3("c", "C", WebUIFieldType::Float);
    WebUIField f4("d", "D", WebUIFieldType::Boolean);
    WebUIField f5("e", "E", WebUIFieldType::Select);
    WebUIField f6("f", "F", WebUIFieldType::Slider);
    WebUIField f7("g", "G", WebUIFieldType::Color);
    WebUIField f8("h", "H", WebUIFieldType::Button);
    WebUIField f9("i", "I", WebUIFieldType::Display);
    WebUIField f10("j", "J", WebUIFieldType::Chart);
    WebUIField f11("k", "K", WebUIFieldType::Status);
    WebUIField f12("l", "L", WebUIFieldType::Progress);
    WebUIField f13("m", "M", WebUIFieldType::Password);
    WebUIField f14("n", "N", WebUIFieldType::File);
    WebUIField f15("o", "O", WebUIFieldType::Multiselect);

    TEST_ASSERT_EQUAL(14, static_cast<int>(f15.type));

    TEST_ASSERT_EQUAL(WebUIFieldType::Text, f1.type);
    TEST_ASSERT_EQUAL(WebUIFieldType::Number, f2.type);
    TEST_ASSERT_EQUAL(WebUIFieldType::File, f14.type);
    TEST_ASSERT_EQUAL(WebUIFieldType::Multiselect, f15.type);
}

// ============================================================================
// WebUIContext Tests
// ============================================================================

void test_webui_context_basic_construction() {
    WebUIContext ctx("test_ctx", "Test Context", "dc-test", WebUILocation::Dashboard, WebUIPresentation::Card);

    TEST_ASSERT_EQUAL_STRING("test_ctx", ctx.getContextIdCStr());
    TEST_ASSERT_EQUAL_STRING("Test Context", ctx.getTitleCStr());
    TEST_ASSERT_EQUAL_STRING("dc-test", ctx.getIconCStr());
    TEST_ASSERT_EQUAL(WebUILocation::Dashboard, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Card, ctx.presentation);
    TEST_ASSERT_EQUAL_INT(0, ctx.priority);
    TEST_ASSERT_FALSE(ctx.realTime);
    TEST_ASSERT_EQUAL_INT(5000, ctx.updateInterval);
}

void test_webui_context_factory_dashboard() {
    auto ctx = WebUIContext::dashboard("dash_id", "Dashboard Card", "dc-dashboard");

    TEST_ASSERT_EQUAL_STRING("dash_id", ctx.getContextIdCStr());
    TEST_ASSERT_EQUAL_STRING("Dashboard Card", ctx.getTitleCStr());
    TEST_ASSERT_EQUAL_STRING("dc-dashboard", ctx.getIconCStr());
    TEST_ASSERT_EQUAL(WebUILocation::Dashboard, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Card, ctx.presentation);
}

void test_webui_context_factory_gauge() {
    auto ctx = WebUIContext::gauge("gauge_id", "Gauge Title");

    TEST_ASSERT_EQUAL_STRING("gauge_id", ctx.getContextIdCStr());
    TEST_ASSERT_EQUAL(WebUILocation::Dashboard, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Gauge, ctx.presentation);
}

void test_webui_context_factory_status_badge() {
    auto ctx = WebUIContext::statusBadge("status_id", "Status", "dc-wifi");

    TEST_ASSERT_EQUAL_STRING("status_id", ctx.getContextIdCStr());
    TEST_ASSERT_EQUAL(WebUILocation::HeaderStatus, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::StatusBadge, ctx.presentation);
    // Icon is stored in icon field, rendered by frontend JS
    TEST_ASSERT_EQUAL_STRING("dc-wifi", ctx.getIconCStr());
}

void test_webui_context_factory_header_info() {
    auto ctx = WebUIContext::headerInfo("time_id", "Time", "dc-clock");

    TEST_ASSERT_EQUAL_STRING("time_id", ctx.getContextIdCStr());
    TEST_ASSERT_EQUAL(WebUILocation::HeaderInfo, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Text, ctx.presentation);
}

void test_webui_context_factory_settings() {
    auto ctx = WebUIContext::settings("settings_id", "Settings");

    TEST_ASSERT_EQUAL_STRING("settings_id", ctx.getContextIdCStr());
    TEST_ASSERT_EQUAL(WebUILocation::Settings, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Card, ctx.presentation);
}

void test_webui_context_fluent_with_field() {
    auto ctx = WebUIContext::dashboard("test", "Test")
        .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number));

    TEST_ASSERT_EQUAL(1, ctx.fields.size());
    TEST_ASSERT_EQUAL_STRING("temp", ctx.fields[0].getNameCStr());
}

void test_webui_context_fluent_with_multiple_fields() {
    auto ctx = WebUIContext::dashboard("test", "Test")
        .withField(WebUIField("f1", "Field 1", WebUIFieldType::Text))
        .withField(WebUIField("f2", "Field 2", WebUIFieldType::Number))
        .withField(WebUIField("f3", "Field 3", WebUIFieldType::Boolean));

    TEST_ASSERT_EQUAL(3, ctx.fields.size());
}

void test_webui_context_fluent_with_api() {
    auto ctx = WebUIContext::dashboard("test", "Test")
        .withAPI("/api/test");

    TEST_ASSERT_EQUAL_STRING("/api/test", ctx.getApiEndpointCStr());
}

void test_webui_context_fluent_with_real_time() {
    auto ctx = WebUIContext::dashboard("test", "Test")
        .withRealTime(1000);

    TEST_ASSERT_TRUE(ctx.realTime);
    TEST_ASSERT_EQUAL_INT(1000, ctx.updateInterval);
}

void test_webui_context_fluent_with_priority() {
    auto ctx = WebUIContext::dashboard("test", "Test")
        .withPriority(100);

    TEST_ASSERT_EQUAL_INT(100, ctx.priority);
}

void test_webui_context_fluent_always_interactive() {
    auto ctx = WebUIContext::settings("test", "Test")
        .withAlwaysInteractive(true);

    TEST_ASSERT_TRUE(ctx.alwaysInteractive);
}

void test_webui_context_custom_html_css_js() {
    auto ctx = WebUIContext::dashboard("test", "Test")
        .withCustomHtml("<div class='custom'>Content</div>")
        .withCustomCss(".custom { color: red; }")
        .withCustomJs("console.log('test');");

    TEST_ASSERT_TRUE(ctx.hasCustomHtml());
    TEST_ASSERT_TRUE(ctx.hasCustomCss());
    TEST_ASSERT_TRUE(ctx.hasCustomJs());
    TEST_ASSERT_TRUE(strstr(ctx.getCustomHtmlCStr(), "custom") != nullptr);
    TEST_ASSERT_TRUE(strstr(ctx.getCustomCssCStr(), "color") != nullptr);
    TEST_ASSERT_TRUE(strstr(ctx.getCustomJsCStr(), "console") != nullptr);
}

void test_webui_context_copy_constructor() {
    auto original = WebUIContext::dashboard("orig", "Original")
        .withField(WebUIField("f1", "Field", WebUIFieldType::Text))
        .withRealTime(2000);

    WebUIContext copy(original);

    TEST_ASSERT_EQUAL_STRING("orig", copy.getContextIdCStr());
    TEST_ASSERT_EQUAL(1, copy.fields.size());
    TEST_ASSERT_TRUE(copy.realTime);
    TEST_ASSERT_EQUAL_INT(2000, copy.updateInterval);
}

// ============================================================================
// WebUILocation and WebUIPresentation Tests
// ============================================================================

void test_webui_locations_enum() {
    // Verify all location enum values are accessible
    WebUILocation loc1 = WebUILocation::Dashboard;
    WebUILocation loc2 = WebUILocation::ComponentDetail;
    WebUILocation loc3 = WebUILocation::HeaderStatus;
    WebUILocation loc4 = WebUILocation::QuickControls;
    WebUILocation loc5 = WebUILocation::Settings;
    WebUILocation loc6 = WebUILocation::HeaderInfo;

    TEST_ASSERT_NOT_EQUAL(loc1, loc2);
    TEST_ASSERT_NOT_EQUAL(loc3, loc6);
}

void test_webui_presentations_enum() {
    // Verify all presentation enum values are accessible
    WebUIPresentation p1 = WebUIPresentation::Card;
    WebUIPresentation p2 = WebUIPresentation::Gauge;
    WebUIPresentation p3 = WebUIPresentation::Graph;
    WebUIPresentation p4 = WebUIPresentation::StatusBadge;
    WebUIPresentation p5 = WebUIPresentation::ProgressBar;
    WebUIPresentation p6 = WebUIPresentation::Table;
    WebUIPresentation p7 = WebUIPresentation::Toggle;
    WebUIPresentation p8 = WebUIPresentation::Slider;
    WebUIPresentation p9 = WebUIPresentation::Text;
    WebUIPresentation p10 = WebUIPresentation::Button;

    TEST_ASSERT_NOT_EQUAL(p1, p2);
    TEST_ASSERT_NOT_EQUAL(p9, p10);
}

// ============================================================================
// LazyState Tests
// ============================================================================

void test_lazy_state_initial_uninitialized() {
    LazyState<int> state;

    TEST_ASSERT_FALSE(state.isInitialized());
}

void test_lazy_state_has_changed_first_call() {
    LazyState<int> state;

    bool changed = state.hasChanged(42);

    TEST_ASSERT_TRUE(changed);  // First call always returns true
    TEST_ASSERT_TRUE(state.isInitialized());
    TEST_ASSERT_EQUAL_INT(42, state.getValue());
}

void test_lazy_state_has_changed_no_change() {
    LazyState<int> state;

    state.hasChanged(42);
    bool changed = state.hasChanged(42);  // Same value

    TEST_ASSERT_FALSE(changed);
}

void test_lazy_state_has_changed_with_change() {
    LazyState<int> state;

    state.hasChanged(42);
    bool changed = state.hasChanged(100);  // Different value

    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_INT(100, state.getValue());
}

void test_lazy_state_get_with_initializer() {
    LazyState<String> state;

    String& value = state.get([]() { return String("initialized"); });

    TEST_ASSERT_TRUE(state.isInitialized());
    TEST_ASSERT_EQUAL_STRING("initialized", value.c_str());
}

void test_lazy_state_get_only_initializes_once() {
    LazyState<int> state;
    int callCount = 0;

    state.get([&callCount]() { callCount++; return 1; });
    state.get([&callCount]() { callCount++; return 2; });
    state.get([&callCount]() { callCount++; return 3; });

    TEST_ASSERT_EQUAL_INT(1, callCount);
    TEST_ASSERT_EQUAL_INT(1, state.getValue());
}

void test_lazy_state_reset() {
    LazyState<int> state;
    state.hasChanged(42);

    state.reset();

    TEST_ASSERT_FALSE(state.isInitialized());
}

void test_lazy_state_with_bool() {
    LazyState<bool> state;

    TEST_ASSERT_TRUE(state.hasChanged(false));  // First call
    TEST_ASSERT_FALSE(state.hasChanged(false)); // Same
    TEST_ASSERT_TRUE(state.hasChanged(true));   // Changed
}

void test_lazy_state_with_string() {
    LazyState<String> state;

    TEST_ASSERT_TRUE(state.hasChanged("hello"));
    TEST_ASSERT_FALSE(state.hasChanged("hello"));
    TEST_ASSERT_TRUE(state.hasChanged("world"));
    TEST_ASSERT_EQUAL_STRING("world", state.getValue().c_str());
}


void test_webui_config_truncation() {
    WebUIConfig config;

    // deviceName[32] — input of 40 chars should be truncated to 31+null
    const char* longName = "This is a very long device name!!!!!!!!";
    config.setDeviceName(longName);
    TEST_ASSERT_EQUAL(31, strlen(config.deviceName));
    TEST_ASSERT_EQUAL_STRING("This is a very long device name", config.deviceName);

    // theme[8] — "verydarktheme" is 13 chars, truncated to 7+null
    config.setTheme("verydarktheme");
    TEST_ASSERT_EQUAL(7, strlen(config.theme));

    // Getter returns correct String
    TEST_ASSERT_EQUAL_STRING(config.deviceName, config.getDeviceName().c_str());

    // Short values work normally
    config.setDeviceName("OK");
    TEST_ASSERT_EQUAL_STRING("OK", config.deviceName);
}

int main() {
    UNITY_BEGIN();

    // WebUIConfig tests
    RUN_TEST(test_webui_config_defaults);
    RUN_TEST(test_webui_config_custom_values);
    RUN_TEST(test_webui_config_refuses_auth_without_a_password);
    RUN_TEST(test_webui_config_applies_the_other_settings_fields);
    RUN_TEST(test_webui_config_refuses_an_empty_or_unknown_value);
    RUN_TEST(test_webui_config_normalize_auth_clears_an_unusable_flag);

    // WebUIField tests
    RUN_TEST(test_webui_field_basic_construction);
    RUN_TEST(test_webui_field_default_values);
    RUN_TEST(test_webui_field_fluent_range);
    RUN_TEST(test_webui_field_fluent_choices);
    RUN_TEST(test_webui_field_fluent_add_option);
    RUN_TEST(test_webui_field_fluent_api);
    RUN_TEST(test_webui_field_copy_constructor);
    RUN_TEST(test_webui_field_all_types);

    // WebUIContext tests
    RUN_TEST(test_webui_context_basic_construction);
    RUN_TEST(test_webui_context_factory_dashboard);
    RUN_TEST(test_webui_context_factory_gauge);
    RUN_TEST(test_webui_context_factory_status_badge);
    RUN_TEST(test_webui_context_factory_header_info);
    RUN_TEST(test_webui_context_factory_settings);
    RUN_TEST(test_webui_context_fluent_with_field);
    RUN_TEST(test_webui_context_fluent_with_multiple_fields);
    RUN_TEST(test_webui_context_fluent_with_api);
    RUN_TEST(test_webui_context_fluent_with_real_time);
    RUN_TEST(test_webui_context_fluent_with_priority);
    RUN_TEST(test_webui_context_fluent_always_interactive);
    RUN_TEST(test_webui_context_custom_html_css_js);
    RUN_TEST(test_webui_context_copy_constructor);

    // Enum tests
    RUN_TEST(test_webui_locations_enum);
    RUN_TEST(test_webui_presentations_enum);

    // LazyState tests
    RUN_TEST(test_lazy_state_initial_uninitialized);
    RUN_TEST(test_lazy_state_has_changed_first_call);
    RUN_TEST(test_lazy_state_has_changed_no_change);
    RUN_TEST(test_lazy_state_has_changed_with_change);
    RUN_TEST(test_lazy_state_get_with_initializer);
    RUN_TEST(test_lazy_state_get_only_initializes_once);
    RUN_TEST(test_lazy_state_reset);
    RUN_TEST(test_lazy_state_with_bool);
    RUN_TEST(test_lazy_state_with_string);

    // WebUIConfig char[] optimization tests (Phase 1)
    RUN_TEST(test_webui_config_truncation);

    return UNITY_END();
}
