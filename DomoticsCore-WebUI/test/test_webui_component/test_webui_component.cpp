/**
 * @file test_webui_component.cpp
 * @brief Native unit tests for WebUI component structures
 *
 * Tests cover:
 * - WebUIConfig defaults and configuration
 * - WebUIField creation and fluent interface
 * - WebUIContext creation and factory methods
 * - LazyState change tracking
 * - ProviderRegistry registration and lookup
 * - CachingWebUIProvider memory leak prevention
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IWebUIProvider.h>
#include <DomoticsCore/WebUI/WebUIConfig.h>
#include <DomoticsCore/WebUI/ProviderRegistry.h>
#include <DomoticsCore/Testing/HeapTracker.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;
using namespace DomoticsCore::Testing;

// ============================================================================
// WebUIConfig Tests
// ============================================================================

void test_webui_config_defaults() {
    WebUIConfig config;

    TEST_ASSERT_EQUAL_STRING("DomoticsCore Device", config.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("auto", config.theme.c_str());
    TEST_ASSERT_EQUAL_UINT16(80, config.port);
    TEST_ASSERT_TRUE(config.enableWebSocket);
    TEST_ASSERT_EQUAL_INT(5000, config.wsUpdateInterval);
    TEST_ASSERT_FALSE(config.useFileSystem);
    TEST_ASSERT_EQUAL_STRING("/webui", config.staticPath.c_str());
    TEST_ASSERT_EQUAL_STRING("#007acc", config.primaryColor.c_str());
    TEST_ASSERT_FALSE(config.enableAuth);
    TEST_ASSERT_EQUAL_STRING("admin", config.username.c_str());
    TEST_ASSERT_TRUE(config.password.isEmpty());
    TEST_ASSERT_EQUAL_INT(3, config.maxWebSocketClients);
    TEST_ASSERT_EQUAL_INT(5000, config.apiTimeout);
    TEST_ASSERT_TRUE(config.enableCompression);
    TEST_ASSERT_TRUE(config.enableCaching);
    TEST_ASSERT_FALSE(config.enableCORS);
}

void test_webui_config_custom_values() {
    WebUIConfig config;
    config.deviceName = "Custom Device";
    config.theme = "dark";
    config.port = 8080;
    config.enableWebSocket = false;
    config.wsUpdateInterval = 1000;
    config.maxWebSocketClients = 5;
    config.enableAuth = true;
    config.username = "user";
    config.password = "secret";

    TEST_ASSERT_EQUAL_STRING("Custom Device", config.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("dark", config.theme.c_str());
    TEST_ASSERT_EQUAL_UINT16(8080, config.port);
    TEST_ASSERT_FALSE(config.enableWebSocket);
    TEST_ASSERT_EQUAL_INT(1000, config.wsUpdateInterval);
    TEST_ASSERT_EQUAL_INT(5, config.maxWebSocketClients);
    TEST_ASSERT_TRUE(config.enableAuth);
    TEST_ASSERT_EQUAL_STRING("user", config.username.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", config.password.c_str());
}

// ============================================================================
// WebUIField Tests
// ============================================================================

void test_webui_field_basic_construction() {
    WebUIField field("temp", "Temperature", WebUIFieldType::Number, "25.5", "°C", true);

    TEST_ASSERT_EQUAL_STRING("temp", field.name.c_str());
    TEST_ASSERT_EQUAL_STRING("Temperature", field.label.c_str());
    TEST_ASSERT_EQUAL(WebUIFieldType::Number, field.type);
    TEST_ASSERT_EQUAL_STRING("25.5", field.value.c_str());
    TEST_ASSERT_EQUAL_STRING("°C", field.unit.c_str());
    TEST_ASSERT_TRUE(field.readOnly);
}

void test_webui_field_default_values() {
    WebUIField field("status", "Status", WebUIFieldType::Text);

    TEST_ASSERT_EQUAL_STRING("status", field.name.c_str());
    TEST_ASSERT_EQUAL_STRING("Status", field.label.c_str());
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

    TEST_ASSERT_EQUAL_STRING("/api/power/set", field.endpoint.c_str());
}

void test_webui_field_copy_constructor() {
    WebUIField original("test", "Test", WebUIFieldType::Number, "42", "units", false);
    original.range(0, 100);
    original.addOption("a", "Option A");

    WebUIField copy(original);

    TEST_ASSERT_EQUAL_STRING("test", copy.name.c_str());
    TEST_ASSERT_EQUAL_STRING("Test", copy.label.c_str());
    TEST_ASSERT_EQUAL_STRING("42", copy.value.c_str());
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

    TEST_ASSERT_EQUAL(WebUIFieldType::Text, f1.type);
    TEST_ASSERT_EQUAL(WebUIFieldType::Number, f2.type);
    TEST_ASSERT_EQUAL(WebUIFieldType::File, f14.type);
}

// ============================================================================
// WebUIContext Tests
// ============================================================================

void test_webui_context_basic_construction() {
    WebUIContext ctx("test_ctx", "Test Context", "dc-test", WebUILocation::Dashboard, WebUIPresentation::Card);

    TEST_ASSERT_EQUAL_STRING("test_ctx", ctx.contextId.c_str());
    TEST_ASSERT_EQUAL_STRING("Test Context", ctx.title.c_str());
    TEST_ASSERT_EQUAL_STRING("dc-test", ctx.icon.c_str());
    TEST_ASSERT_EQUAL(WebUILocation::Dashboard, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Card, ctx.presentation);
    TEST_ASSERT_EQUAL_INT(0, ctx.priority);
    TEST_ASSERT_FALSE(ctx.realTime);
    TEST_ASSERT_EQUAL_INT(5000, ctx.updateInterval);
}

void test_webui_context_factory_dashboard() {
    auto ctx = WebUIContext::dashboard("dash_id", "Dashboard Card", "dc-dashboard");

    TEST_ASSERT_EQUAL_STRING("dash_id", ctx.contextId.c_str());
    TEST_ASSERT_EQUAL_STRING("Dashboard Card", ctx.title.c_str());
    TEST_ASSERT_EQUAL_STRING("dc-dashboard", ctx.icon.c_str());
    TEST_ASSERT_EQUAL(WebUILocation::Dashboard, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Card, ctx.presentation);
}

void test_webui_context_factory_gauge() {
    auto ctx = WebUIContext::gauge("gauge_id", "Gauge Title");

    TEST_ASSERT_EQUAL_STRING("gauge_id", ctx.contextId.c_str());
    TEST_ASSERT_EQUAL(WebUILocation::Dashboard, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Gauge, ctx.presentation);
}

void test_webui_context_factory_status_badge() {
    auto ctx = WebUIContext::statusBadge("status_id", "Status", "dc-wifi");

    TEST_ASSERT_EQUAL_STRING("status_id", ctx.contextId.c_str());
    TEST_ASSERT_EQUAL(WebUILocation::HeaderStatus, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::StatusBadge, ctx.presentation);
    // Should have custom HTML for SVG icon
    TEST_ASSERT_TRUE(ctx.customHtml.indexOf("svg") >= 0);
}

void test_webui_context_factory_header_info() {
    auto ctx = WebUIContext::headerInfo("time_id", "Time", "dc-clock");

    TEST_ASSERT_EQUAL_STRING("time_id", ctx.contextId.c_str());
    TEST_ASSERT_EQUAL(WebUILocation::HeaderInfo, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Text, ctx.presentation);
}

void test_webui_context_factory_settings() {
    auto ctx = WebUIContext::settings("settings_id", "Settings");

    TEST_ASSERT_EQUAL_STRING("settings_id", ctx.contextId.c_str());
    TEST_ASSERT_EQUAL(WebUILocation::Settings, ctx.location);
    TEST_ASSERT_EQUAL(WebUIPresentation::Card, ctx.presentation);
}

void test_webui_context_fluent_with_field() {
    auto ctx = WebUIContext::dashboard("test", "Test")
        .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number));

    TEST_ASSERT_EQUAL(1, ctx.fields.size());
    TEST_ASSERT_EQUAL_STRING("temp", ctx.fields[0].name.c_str());
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

    TEST_ASSERT_EQUAL_STRING("/api/test", ctx.apiEndpoint.c_str());
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

    TEST_ASSERT_TRUE(ctx.customHtml.indexOf("custom") >= 0);
    TEST_ASSERT_TRUE(ctx.customCss.indexOf("color") >= 0);
    TEST_ASSERT_TRUE(ctx.customJs.indexOf("console") >= 0);
}

void test_webui_context_copy_constructor() {
    auto original = WebUIContext::dashboard("orig", "Original")
        .withField(WebUIField("f1", "Field", WebUIFieldType::Text))
        .withRealTime(2000);

    WebUIContext copy(original);

    TEST_ASSERT_EQUAL_STRING("orig", copy.contextId.c_str());
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

// ============================================================================
// Mock Provider for ProviderRegistry Tests
// ============================================================================

class MockWebUIProvider : public IWebUIProvider {
private:
    String name;
    String version;
    std::vector<WebUIContext> contexts;
    bool enabled = true;

public:
    MockWebUIProvider(const String& n, const String& v) : name(n), version(v) {}

    void addContext(const WebUIContext& ctx) {
        contexts.push_back(ctx);
    }

    String getWebUIName() const override { return name; }
    String getWebUIVersion() const override { return version; }

    std::vector<WebUIContext> getWebUIContexts() override {
        return contexts;
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override {
        return "{\"success\":true}";
    }

    String getWebUIData(const String& contextId) override {
        return "{}";
    }

    bool isWebUIEnabled() override { return enabled; }
    void setEnabled(bool e) { enabled = e; }
};

// ============================================================================
// ProviderRegistry Tests
// ============================================================================

void test_provider_registry_empty() {
    ProviderRegistry registry;

    auto* provider = registry.getProviderForContext("nonexistent");
    TEST_ASSERT_NULL(provider);
}

void test_provider_registry_register_provider() {
    ProviderRegistry registry;
    MockWebUIProvider provider("TestProvider", "1.0.0");
    provider.addContext(WebUIContext::dashboard("test_ctx", "Test"));

    registry.registerProvider(&provider);

    auto* found = registry.getProviderForContext("test_ctx");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("TestProvider", found->getWebUIName().c_str());
}

void test_provider_registry_register_multiple_contexts() {
    ProviderRegistry registry;
    MockWebUIProvider provider("MultiContext", "1.0.0");
    provider.addContext(WebUIContext::dashboard("ctx1", "Context 1"));
    provider.addContext(WebUIContext::settings("ctx2", "Context 2"));
    provider.addContext(WebUIContext::statusBadge("ctx3", "Context 3", "dc-test"));

    registry.registerProvider(&provider);

    TEST_ASSERT_NOT_NULL(registry.getProviderForContext("ctx1"));
    TEST_ASSERT_NOT_NULL(registry.getProviderForContext("ctx2"));
    TEST_ASSERT_NOT_NULL(registry.getProviderForContext("ctx3"));

    // All should point to same provider
    TEST_ASSERT_EQUAL_PTR(registry.getProviderForContext("ctx1"),
                          registry.getProviderForContext("ctx2"));
}

void test_provider_registry_unregister_provider() {
    ProviderRegistry registry;
    MockWebUIProvider provider("ToRemove", "1.0.0");
    provider.addContext(WebUIContext::dashboard("remove_ctx", "Remove"));

    registry.registerProvider(&provider);
    TEST_ASSERT_NOT_NULL(registry.getProviderForContext("remove_ctx"));

    registry.unregisterProvider(&provider);
    TEST_ASSERT_NULL(registry.getProviderForContext("remove_ctx"));
}

void test_provider_registry_register_factory() {
    ProviderRegistry registry;

    bool factoryCalled = false;
    registry.registerProviderFactory("test_type", [&factoryCalled](IComponent* comp) -> IWebUIProvider* {
        factoryCalled = true;
        return nullptr;  // Just testing factory registration
    });

    // Factory stored but not called until discovery
    TEST_ASSERT_FALSE(factoryCalled);
}

void test_provider_registry_get_components_list() {
    ProviderRegistry registry;
    MockWebUIProvider provider1("Provider1", "1.0.0");
    provider1.addContext(WebUIContext::dashboard("p1_ctx", "P1"));
    MockWebUIProvider provider2("Provider2", "2.0.0");
    provider2.addContext(WebUIContext::settings("p2_ctx", "P2"));

    registry.registerProvider(&provider1);
    registry.registerProvider(&provider2);

    JsonDocument doc;
    registry.getComponentsList(doc);

    TEST_ASSERT_TRUE(doc["components"].is<JsonArray>());
    JsonArray components = doc["components"].as<JsonArray>();
    TEST_ASSERT_EQUAL(2, components.size());
}

void test_provider_registry_enable_disable() {
    ProviderRegistry registry;
    MockWebUIProvider provider("Toggleable", "1.0.0");
    provider.addContext(WebUIContext::dashboard("toggle_ctx", "Toggle"));

    registry.registerProvider(&provider);

    // Disable
    auto result = registry.enableComponent("Toggleable", false);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_FALSE(result.enabled);

    // Context should be removed
    TEST_ASSERT_NULL(registry.getProviderForContext("toggle_ctx"));

    // Re-enable
    result = registry.enableComponent("Toggleable", true);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_TRUE(result.enabled);

    // Context should be back
    TEST_ASSERT_NOT_NULL(registry.getProviderForContext("toggle_ctx"));
}

void test_provider_registry_cannot_disable_webui() {
    ProviderRegistry registry;
    MockWebUIProvider webuiProvider("WebUI", "1.0.0");
    webuiProvider.addContext(WebUIContext::dashboard("webui_ctx", "WebUI"));

    registry.registerProvider(&webuiProvider);

    auto result = registry.enableComponent("WebUI", false);

    // Should return warning without disabling
    TEST_ASSERT_FALSE(result.warning.isEmpty());
    TEST_ASSERT_FALSE(result.success);
}

void test_provider_registry_enable_nonexistent() {
    ProviderRegistry registry;

    auto result = registry.enableComponent("NonExistent", true);

    TEST_ASSERT_FALSE(result.found);
    TEST_ASSERT_FALSE(result.success);
}

void test_provider_registry_context_providers_accessor() {
    ProviderRegistry registry;
    MockWebUIProvider provider("Accessor", "1.0.0");
    provider.addContext(WebUIContext::dashboard("acc_ctx", "Accessor"));

    registry.registerProvider(&provider);

    const auto& contextProviders = registry.getContextProviders();
    TEST_ASSERT_EQUAL(1, contextProviders.size());
    TEST_ASSERT_TRUE(contextProviders.find("acc_ctx") != contextProviders.end());
}

void test_provider_registry_prepare_schema_generation() {
    ProviderRegistry registry;
    MockWebUIProvider provider("Schema", "1.0.0");
    provider.addContext(WebUIContext::dashboard("schema_ctx", "Schema"));

    registry.registerProvider(&provider);

    auto state = registry.prepareSchemaGeneration();
    TEST_ASSERT_NOT_NULL(state.get());
    TEST_ASSERT_FALSE(state->finished);
    TEST_ASSERT_EQUAL(1, state->providers.size());
}

void test_provider_registry_get_next_context() {
    ProviderRegistry registry;
    MockWebUIProvider provider("NextCtx", "1.0.0");
    provider.addContext(WebUIContext::dashboard("ctx_a", "A"));
    provider.addContext(WebUIContext::settings("ctx_b", "B"));

    registry.registerProvider(&provider);

    auto state = registry.prepareSchemaGeneration();
    WebUIContext ctx;

    bool hasFirst = registry.getNextContext(state, ctx);
    TEST_ASSERT_TRUE(hasFirst);
    TEST_ASSERT_EQUAL_STRING("ctx_a", ctx.contextId.c_str());

    bool hasSecond = registry.getNextContext(state, ctx);
    TEST_ASSERT_TRUE(hasSecond);
    TEST_ASSERT_EQUAL_STRING("ctx_b", ctx.contextId.c_str());

    bool hasThird = registry.getNextContext(state, ctx);
    TEST_ASSERT_FALSE(hasThird);
    TEST_ASSERT_TRUE(state->finished);
}

// ============================================================================
// Memory Leak DETECTION Tests - Test CURRENT behavior before fixes
// ============================================================================

/**
 * This test DETECTS memory behavior of the standard IWebUIProvider.
 * MockWebUIProvider inherits from IWebUIProvider (NOT CachingWebUIProvider),
 * so it recreates contexts on every getWebUIContexts() call.
 * 
 * PURPOSE: Measure actual memory impact of repeated context creation
 * to understand WHERE memory leaks originate.
 */
void test_detect_memory_behavior_repeated_context_creation() {
    HeapTracker tracker;
    
    // Create a provider with substantial context data (simulating real WebUI)
    MockWebUIProvider provider("LeakTest", "1.0.0");
    provider.addContext(WebUIContext::dashboard("dash", "Dashboard")
        .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number, "25.5", "°C", true))
        .withField(WebUIField("humid", "Humidity", WebUIFieldType::Number, "60", "%", true))
        .withCustomHtml("<div class=\"widget\"><span class=\"value\">Custom HTML content here for testing memory allocation patterns in WebUI contexts</span></div>")
        .withCustomCss(".widget { background: #fff; padding: 1rem; } .value { font-size: 2rem; color: #007acc; }"));
    
    provider.addContext(WebUIContext::settings("settings", "Settings")
        .withField(WebUIField("name", "Device Name", WebUIFieldType::Text, "DomoticsCore"))
        .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true")));
    
    // Warm up - first call
    auto warmup = provider.getWebUIContexts();
    (void)warmup;
    
    // Checkpoint after warmup
    tracker.checkpoint("after_warmup");
    
    // Call getWebUIContexts() 50 times - this is what happens during WebSocket broadcasts
    for (int i = 0; i < 50; i++) {
        auto contexts = provider.getWebUIContexts();
        // Simulate what WebUI does: iterate over contexts
        for (const auto& ctx : contexts) {
            (void)ctx.contextId;
        }
    }
    
    tracker.checkpoint("after_50_calls");
    
    // Calculate delta
    int32_t delta = tracker.getDelta("after_warmup", "after_50_calls");
    
    // Report the actual memory behavior
    printf("\n[MEMORY DETECTION] IWebUIProvider::getWebUIContexts() x50:\n");
    printf("  Heap delta: %d bytes\n", delta);
    printf("  Per call: ~%d bytes\n", delta / 50);
    
    // FAIL if significant memory leak detected
    // Native uses real mallinfo tracking, so we can detect real leaks
    const int32_t LEAK_THRESHOLD = 1024;  // 1KB tolerance for native (allocator overhead)
    
    if (delta > LEAK_THRESHOLD) {
        printf("  *** MEMORY LEAK DETECTED: %d bytes > threshold %d ***\n", delta, LEAK_THRESHOLD);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak detected in getWebUIContexts()");
}

/**
 * Test memory behavior when contexts contain large customHtml/Css/Js strings
 * These are the suspected source of memory leaks on ESP8266.
 */
void test_detect_memory_large_custom_content() {
    HeapTracker tracker;
    
    MockWebUIProvider provider("LargeContent", "1.0.0");
    
    // Create context with large custom content (simulating real chart/complex UI)
    String largeHtml = "<div class=\"chart-container\">";
    for (int i = 0; i < 20; i++) {
        largeHtml += "<div class=\"data-point\" data-value=\"" + String(i * 10) + "\"></div>";
    }
    largeHtml += "</div>";
    
    provider.addContext(WebUIContext::dashboard("chart", "Chart")
        .withCustomHtml(largeHtml)
        .withCustomCss(".chart-container { display: flex; } .data-point { width: 20px; height: var(--value); }")
        .withCustomJs("function updateChart(data) { /* chart update logic */ }"));
    
    tracker.checkpoint("before");
    
    // Call multiple times
    for (int i = 0; i < 20; i++) {
        auto contexts = provider.getWebUIContexts();
        for (const auto& ctx : contexts) {
            // Access custom content (triggers String copy)
            String html = ctx.customHtml;
            String css = ctx.customCss;
            String js = ctx.customJs;
            (void)html; (void)css; (void)js;
        }
    }
    
    tracker.checkpoint("after");
    
    int32_t delta = tracker.getDelta("before", "after");
    printf("\n[MEMORY DETECTION] Large customHtml/Css/Js x20:\n");
    printf("  Heap delta: %d bytes\n", delta);
    printf("  Content size: ~%zu bytes\n", largeHtml.length());
    
    // FAIL if significant memory leak detected
    const int32_t LEAK_THRESHOLD = 512;
    
    if (delta > LEAK_THRESHOLD) {
        printf("  *** MEMORY LEAK DETECTED: %d bytes > threshold %d ***\n", delta, LEAK_THRESHOLD);
    }
    
    TEST_ASSERT_TRUE_MESSAGE(delta <= LEAK_THRESHOLD, "Memory leak in large custom content");
}

// ============================================================================
// CachingWebUIProvider Memory Tests (HeapTracker Integration)
// ============================================================================

// Test implementation of CachingWebUIProvider for memory testing
class TestCachingProvider : public CachingWebUIProvider {
public:
    int buildCount = 0;
    
protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        buildCount++;
        // Create contexts with substantial data to detect leaks
        contexts.push_back(WebUIContext::dashboard("test_dash", "Dashboard")
            .withField(WebUIField("field1", "Field 1", WebUIFieldType::Text, "value1"))
            .withField(WebUIField("field2", "Field 2", WebUIFieldType::Number, "42"))
            .withCustomHtml("<div class='test'>Custom HTML Content</div>")
            .withCustomCss(".test { color: red; }"));
        
        contexts.push_back(WebUIContext::settings("test_settings", "Settings")
            .withField(WebUIField("setting1", "Setting", WebUIFieldType::Boolean, "true")));
    }
    
public:
    String getWebUIName() const override { return "TestProvider"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    
    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override {
        return "{}";
    }
};

void test_caching_provider_builds_once() {
    TestCachingProvider provider;
    
    // First call should trigger build
    auto contexts1 = provider.getWebUIContexts();
    TEST_ASSERT_EQUAL(1, provider.buildCount);
    TEST_ASSERT_EQUAL(2, contexts1.size());
    
    // Second call should use cache
    auto contexts2 = provider.getWebUIContexts();
    TEST_ASSERT_EQUAL(1, provider.buildCount); // Still 1, not rebuilt
    TEST_ASSERT_EQUAL(2, contexts2.size());
    
    // Third call - still cached
    auto contexts3 = provider.getWebUIContexts();
    TEST_ASSERT_EQUAL(1, provider.buildCount);
}

void test_caching_provider_memory_stable_100_calls() {
    HeapTracker tracker;
    TestCachingProvider provider;
    
    // First call builds cache
    provider.getWebUIContexts();
    
    // Checkpoint after cache is built
    tracker.checkpoint("after_cache");
    
    // Call 100 times - should not allocate new memory
    for (int i = 0; i < 100; i++) {
        auto contexts = provider.getWebUIContexts();
        TEST_ASSERT_EQUAL(2, contexts.size());
    }
    
    tracker.checkpoint("after_100_calls");
    
    // Assert no heap growth (tolerance for internal std::vector copies)
    MemoryTestResult result = tracker.assertStable("after_cache", "after_100_calls", 1024);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

void test_caching_provider_invalidate_rebuilds() {
    TestCachingProvider provider;
    
    // Build cache
    provider.getWebUIContexts();
    TEST_ASSERT_EQUAL(1, provider.buildCount);
    
    // Invalidate
    provider.invalidateContextCache();
    
    // Next call should rebuild
    provider.getWebUIContexts();
    TEST_ASSERT_EQUAL(2, provider.buildCount);
}

void test_caching_provider_foreach_no_rebuild() {
    TestCachingProvider provider;
    
    // Build via getWebUIContexts
    provider.getWebUIContexts();
    TEST_ASSERT_EQUAL(1, provider.buildCount);
    
    // forEachContext should use cache
    int contextCount = 0;
    provider.forEachContext([&contextCount](const WebUIContext& ctx) {
        contextCount++;
        return true;
    });
    
    TEST_ASSERT_EQUAL(2, contextCount);
    TEST_ASSERT_EQUAL(1, provider.buildCount); // No rebuild
}

void test_caching_provider_get_context_at() {
    TestCachingProvider provider;
    
    WebUIContext ctx;
    bool found = provider.getContextAt(0, ctx);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("test_dash", ctx.contextId.c_str());
    
    found = provider.getContextAt(1, ctx);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("test_settings", ctx.contextId.c_str());
    
    found = provider.getContextAt(2, ctx);
    TEST_ASSERT_FALSE(found);
    
    // All via cache - only 1 build
    TEST_ASSERT_EQUAL(1, provider.buildCount);
}

void test_caching_provider_memory_lifecycle() {
    HeapTracker tracker;
    
    tracker.checkpoint("before");
    
    // Create and destroy multiple providers
    for (int i = 0; i < 10; i++) {
        TestCachingProvider provider;
        provider.getWebUIContexts();
        // Provider destroyed at end of scope
    }
    
    tracker.checkpoint("after");
    
    MemoryTestResult result = tracker.assertStable("before", "after", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();

    // WebUIConfig tests
    RUN_TEST(test_webui_config_defaults);
    RUN_TEST(test_webui_config_custom_values);

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

    // ProviderRegistry tests
    RUN_TEST(test_provider_registry_empty);
    RUN_TEST(test_provider_registry_register_provider);
    RUN_TEST(test_provider_registry_register_multiple_contexts);
    RUN_TEST(test_provider_registry_unregister_provider);
    RUN_TEST(test_provider_registry_register_factory);
    RUN_TEST(test_provider_registry_get_components_list);
    RUN_TEST(test_provider_registry_enable_disable);
    RUN_TEST(test_provider_registry_cannot_disable_webui);
    RUN_TEST(test_provider_registry_enable_nonexistent);
    RUN_TEST(test_provider_registry_context_providers_accessor);
    RUN_TEST(test_provider_registry_prepare_schema_generation);
    RUN_TEST(test_provider_registry_get_next_context);

    // Memory leak DETECTION tests (measure current behavior)
    RUN_TEST(test_detect_memory_behavior_repeated_context_creation);
    RUN_TEST(test_detect_memory_large_custom_content);

    // CachingWebUIProvider memory tests (HeapTracker)
    RUN_TEST(test_caching_provider_builds_once);
    RUN_TEST(test_caching_provider_memory_stable_100_calls);
    RUN_TEST(test_caching_provider_invalidate_rebuilds);
    RUN_TEST(test_caching_provider_foreach_no_rebuild);
    RUN_TEST(test_caching_provider_get_context_at);
    RUN_TEST(test_caching_provider_memory_lifecycle);

    return UNITY_END();
}
