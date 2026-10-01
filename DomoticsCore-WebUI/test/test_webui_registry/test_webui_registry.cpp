/**
 * @file test_webui_registry.cpp
 * @brief WebUI component: ProviderRegistry, the streaming serializer, memory stability and schema validation.
 */

#include <unity.h>
#include "../webui_test_support.h"

void setUp() {}
void tearDown() {}

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

void test_provider_registry_iterate_contexts() {
    ProviderRegistry registry;
    MockWebUIProvider provider("IterCtx", "1.0.0");
    provider.addContext(WebUIContext::dashboard("ctx_a", "A"));
    provider.addContext(WebUIContext::settings("ctx_b", "B"));

    registry.registerProvider(&provider);

    // Iterate contexts using forEachContext
    std::vector<String> contextIds;
    provider.forEachContext([&contextIds](const WebUIContext& ctx) {
        contextIds.push_back(ctx.getContextIdCStr());
        return true;  // continue
    });

    TEST_ASSERT_EQUAL(2, contextIds.size());
    TEST_ASSERT_EQUAL_STRING("ctx_a", contextIds[0].c_str());
    TEST_ASSERT_EQUAL_STRING("ctx_b", contextIds[1].c_str());
}

void test_provider_get_context_at() {
    MockWebUIProvider provider("IndexedTest", "1.0.0");
    provider.addContext(WebUIContext::dashboard("idx_0", "First"));
    provider.addContext(WebUIContext::settings("idx_1", "Second"));
    provider.addContext(WebUIContext::statusBadge("idx_2", "Third", "dc-icon"));

    // Test getContextCount
    TEST_ASSERT_EQUAL(3, provider.getContextCount());

    // Test getContextAt with valid indices
    WebUIContext ctx;
    TEST_ASSERT_TRUE(provider.getContextAt(0, ctx));
    TEST_ASSERT_EQUAL_STRING("idx_0", ctx.getContextIdCStr());

    TEST_ASSERT_TRUE(provider.getContextAt(1, ctx));
    TEST_ASSERT_EQUAL_STRING("idx_1", ctx.getContextIdCStr());

    TEST_ASSERT_TRUE(provider.getContextAt(2, ctx));
    TEST_ASSERT_EQUAL_STRING("idx_2", ctx.getContextIdCStr());

    // Test getContextAt with invalid index
    TEST_ASSERT_FALSE(provider.getContextAt(3, ctx));
    TEST_ASSERT_FALSE(provider.getContextAt(100, ctx));
}

// ============================================================================
// StreamingContextSerializer Tests
// ============================================================================

void test_streaming_serializer_simple_context() {
    // Create a simple context
    auto ctx = WebUIContext::dashboard("test_id", "Test Title", "dc-test");

    StreamingContextSerializer serializer;
    serializer.begin(ctx);

    // Serialize to buffer
    uint8_t buffer[4096];
    size_t totalWritten = 0;

    while (!serializer.isComplete() && totalWritten < sizeof(buffer)) {
        size_t written = serializer.write(buffer + totalWritten, sizeof(buffer) - totalWritten);
        if (written == 0) break;
        totalWritten += written;
    }

    TEST_ASSERT_TRUE(serializer.isComplete());
    TEST_ASSERT_GREATER_THAN(0, totalWritten);

    // Parse as JSON to validate
    buffer[totalWritten] = '\0';
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (char*)buffer);

    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING("test_id", doc["contextId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Test Title", doc["title"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("dc-test", doc["icon"].as<const char*>());
}

void test_streaming_serializer_with_fields() {
    auto ctx = WebUIContext::settings("settings_id", "Settings")
        .withField(WebUIField("name", "Name", WebUIFieldType::Text, "test"))
        .withField(WebUIField("value", "Value", WebUIFieldType::Number, "42", "units", true));

    StreamingContextSerializer serializer;
    serializer.begin(ctx);

    uint8_t buffer[4096];
    size_t totalWritten = 0;

    while (!serializer.isComplete() && totalWritten < sizeof(buffer)) {
        size_t written = serializer.write(buffer + totalWritten, sizeof(buffer) - totalWritten);
        if (written == 0) break;
        totalWritten += written;
    }

    buffer[totalWritten] = '\0';
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (char*)buffer);

    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    TEST_ASSERT_TRUE(doc["fields"].is<JsonArray>());
    TEST_ASSERT_EQUAL(2, doc["fields"].as<JsonArray>().size());

    JsonArray fields = doc["fields"].as<JsonArray>();
    TEST_ASSERT_EQUAL_STRING("name", fields[0]["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("value", fields[1]["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("42", fields[1]["value"].as<const char*>());
    TEST_ASSERT_TRUE(fields[1]["readOnly"].as<bool>());
}

void test_streaming_serializer_with_custom_html() {
    auto ctx = WebUIContext::dashboard("custom_id", "Custom")
        .withCustomHtml("<div class=\"test\">Hello</div>")
        .withCustomCss(".test { color: red; }")
        .withCustomJs("console.log('test');");

    StreamingContextSerializer serializer;
    serializer.begin(ctx);

    uint8_t buffer[4096];
    size_t totalWritten = 0;

    while (!serializer.isComplete() && totalWritten < sizeof(buffer)) {
        size_t written = serializer.write(buffer + totalWritten, sizeof(buffer) - totalWritten);
        if (written == 0) break;
        totalWritten += written;
    }

    buffer[totalWritten] = '\0';
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (char*)buffer);

    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    TEST_ASSERT_TRUE(strstr(doc["customHtml"].as<const char*>(), "class") != nullptr);
    TEST_ASSERT_TRUE(strstr(doc["customCss"].as<const char*>(), "color") != nullptr);
    TEST_ASSERT_TRUE(strstr(doc["customJs"].as<const char*>(), "console") != nullptr);
}

void test_streaming_serializer_chunked_output() {
    // Test that serializer works with small buffer sizes (simulating chunked HTTP)
    auto ctx = WebUIContext::dashboard("chunk_test", "Chunked Test")
        .withField(WebUIField("field1", "Field 1", WebUIFieldType::Text, "value1"));

    StreamingContextSerializer serializer;
    serializer.begin(ctx);

    // Use small buffer to force multiple chunks (64 bytes)
    // Must be large enough to fit the longest atomic piece (like "\"contextId\":")
    uint8_t smallBuffer[65];  // +1 for null terminator
    String fullOutput;
    int chunkCount = 0;

    while (!serializer.isComplete() && chunkCount < 200) {
        size_t written = serializer.write(smallBuffer, 64);
        if (written > 0) {
            // Append buffer content to string
            smallBuffer[written] = '\0';
            fullOutput += (const char*)smallBuffer;
            chunkCount++;
        } else if (!serializer.isComplete()) {
            break;  // Stuck
        }
    }

    TEST_ASSERT_TRUE(serializer.isComplete());
    TEST_ASSERT_GREATER_THAN(1, chunkCount);  // Should take multiple chunks

    // Validate the combined output is valid JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, fullOutput);
    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING("chunk_test", doc["contextId"].as<const char*>());
}

void test_streaming_serializer_json_escaping() {
    // Test that special characters are properly escaped
    auto ctx = WebUIContext::dashboard("escape_test", "Test \"Quotes\" & <Tags>")
        .withField(WebUIField("field", "Field\nWith\tTabs", WebUIFieldType::Text, "value\\with\\backslash"));

    StreamingContextSerializer serializer;
    serializer.begin(ctx);

    uint8_t buffer[4096];
    size_t totalWritten = 0;

    while (!serializer.isComplete() && totalWritten < sizeof(buffer)) {
        size_t written = serializer.write(buffer + totalWritten, sizeof(buffer) - totalWritten);
        if (written == 0) break;
        totalWritten += written;
    }

    buffer[totalWritten] = '\0';
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (char*)buffer);

    // If JSON is invalid, escaping failed
    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    // Verify special chars were preserved after parsing
    TEST_ASSERT_TRUE(strstr(doc["title"].as<const char*>(), "Quotes") != nullptr);
}

void test_streaming_serializer_field_with_options() {
    WebUIField field("mode", "Mode", WebUIFieldType::Select);
    field.addOption("auto", "Automatic")
         .addOption("manual", "Manual Control")
         .addOption("off", "Disabled");

    auto ctx = WebUIContext::settings("options_test", "Options Test")
        .withField(field);

    StreamingContextSerializer serializer;
    serializer.begin(ctx);

    uint8_t buffer[4096];
    size_t totalWritten = 0;

    while (!serializer.isComplete() && totalWritten < sizeof(buffer)) {
        size_t written = serializer.write(buffer + totalWritten, sizeof(buffer) - totalWritten);
        if (written == 0) break;
        totalWritten += written;
    }

    buffer[totalWritten] = '\0';
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (char*)buffer);

    TEST_ASSERT_TRUE(err == DeserializationError::Ok);

    JsonArray fields = doc["fields"].as<JsonArray>();
    TEST_ASSERT_EQUAL(1, fields.size());

    JsonArray options = fields[0]["options"].as<JsonArray>();
    TEST_ASSERT_EQUAL(3, options.size());
    TEST_ASSERT_EQUAL_STRING("auto", options[0].as<const char*>());

    JsonObject optionLabels = fields[0]["optionLabels"].as<JsonObject>();
    TEST_ASSERT_EQUAL_STRING("Automatic", optionLabels["auto"].as<const char*>());
}

// ============================================================================
// Memory Stability Tests
// ============================================================================

void test_streaming_serializer_no_memory_leak() {
    // Test that repeated schema serialization doesn't leak memory
    // This is the critical test - simulates multiple /api/ui/schema requests

    MockWebUIProvider provider("HeapTest", "1.0.0");
    provider.addContext(WebUIContext::dashboard("heap_dash", "Dashboard")
        .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number, "25.5"))
        .withField(WebUIField("humid", "Humidity", WebUIFieldType::Number, "60"))
        .withCustomHtml("<div class=\"test\">Custom HTML content here</div>")
        .withCustomCss(".test { color: red; font-size: 14px; }"));
    provider.addContext(WebUIContext::settings("heap_settings", "Settings")
        .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true"))
        .withField(WebUIField("name", "Name", WebUIFieldType::Text, "Test Device")));

    // Warm up - first allocation
    {
        uint8_t buffer[2048];
        provider.forEachContext([&buffer](const WebUIContext& ctx) {
            StreamingContextSerializer serializer;
            serializer.begin(ctx);
            while (!serializer.isComplete()) {
                serializer.write(buffer, sizeof(buffer));
            }
            return true;
        });
    }

    // Measure baseline heap after warmup
    size_t heapBefore = HAL::Platform::getFreeHeap();

    // Run multiple iterations (simulating multiple schema requests)
    const int ITERATIONS = 10;
    for (int i = 0; i < ITERATIONS; i++) {
        String schema = "[";
        bool first = true;

        provider.forEachContext([&schema, &first](const WebUIContext& ctx) {
            StreamingContextSerializer serializer;
            serializer.begin(ctx);

            uint8_t buffer[512];
            String ctxJson;

            while (!serializer.isComplete()) {
                size_t written = serializer.write(buffer, sizeof(buffer));
                if (written > 0) {
                    buffer[written] = '\0';
                    ctxJson += (const char*)buffer;
                }
            }

            if (!first) schema += ",";
            schema += ctxJson;
            first = false;
            return true;
        });

        schema += "]";

        // Verify JSON is valid each iteration
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, schema);
        TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    }

    // Measure heap after iterations
    size_t heapAfter = HAL::Platform::getFreeHeap();

    // Calculate leak per iteration
    // Allow small tolerance for allocator overhead, but no significant leak
    int heapDiff = (int)heapBefore - (int)heapAfter;
    int leakPerIteration = heapDiff / ITERATIONS;

    // Print for debugging
    printf("Heap before: %zu, after: %zu, diff: %d, per iteration: %d\n",
           heapBefore, heapAfter, heapDiff, leakPerIteration);

    // Assert no significant leak (allow 8 bytes tolerance per iteration for allocator fragmentation)
    TEST_ASSERT_TRUE(leakPerIteration <= 8);
}

void test_provider_registry_schema_generation_no_leak() {
    // Test the full registry + provider flow for memory leaks
    ProviderRegistry registry;

    MockWebUIProvider provider1("Provider1", "1.0.0");
    provider1.addContext(WebUIContext::dashboard("p1_ctx", "Provider 1")
        .withField(WebUIField("value", "Value", WebUIFieldType::Number, "100")));

    MockWebUIProvider provider2("Provider2", "1.0.0");
    provider2.addContext(WebUIContext::settings("p2_ctx", "Provider 2")
        .withField(WebUIField("mode", "Mode", WebUIFieldType::Select, "auto")));

    registry.registerProvider(&provider1);
    registry.registerProvider(&provider2);

    // Warmup
    {
        auto state = registry.prepareSchemaGeneration();
        (void)state;
    }

    size_t heapBefore = HAL::Platform::getFreeHeap();

    // Multiple schema preparation cycles
    const int ITERATIONS = 10;
    for (int i = 0; i < ITERATIONS; i++) {
        auto state = registry.prepareSchemaGeneration();
        TEST_ASSERT_NOT_NULL(state.get());
        TEST_ASSERT_EQUAL(2, state->providers.size());
        // State goes out of scope here, should be freed
    }

    size_t heapAfter = HAL::Platform::getFreeHeap();
    int heapDiff = (int)heapBefore - (int)heapAfter;

    printf("Registry heap before: %zu, after: %zu, diff: %d\n",
           heapBefore, heapAfter, heapDiff);

    // shared_ptr should clean up properly - allow small tolerance
    TEST_ASSERT_TRUE(heapDiff <= 32);
}

// ============================================================================
// Schema Validation Tests (simulates what would be sent to browser)
// ============================================================================

void test_full_schema_array_valid_json() {
    // Simulate building a full schema array like /api/ui/schema
    MockWebUIProvider provider1("Provider1", "1.0.0");
    provider1.addContext(WebUIContext::dashboard("p1_dash", "Dashboard 1")
        .withField(WebUIField("temp", "Temperature", WebUIFieldType::Number, "25.5", "°C")));

    MockWebUIProvider provider2("Provider2", "1.0.0");
    provider2.addContext(WebUIContext::settings("p2_settings", "Settings")
        .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true")));

    // Build schema array manually (like the chunked endpoint does)
    String schema = "[";
    bool first = true;

    std::vector<IWebUIProvider*> providers = {&provider1, &provider2};

    for (auto* provider : providers) {
        provider->forEachContext([&schema, &first](const WebUIContext& ctx) {
            StreamingContextSerializer serializer;
            serializer.begin(ctx);

            uint8_t buffer[2048];
            String ctxJson;

            while (!serializer.isComplete()) {
                size_t written = serializer.write(buffer, sizeof(buffer));
                if (written > 0) {
                    buffer[written] = '\0';
                    ctxJson += (const char*)buffer;
                }
            }

            if (!first) schema += ",";
            schema += ctxJson;
            first = false;
            return true;
        });
    }

    schema += "]";

    // Parse the full schema
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, schema);

    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    TEST_ASSERT_TRUE(doc.is<JsonArray>());
    TEST_ASSERT_EQUAL(2, doc.as<JsonArray>().size());

    // Verify contexts
    TEST_ASSERT_EQUAL_STRING("p1_dash", doc[0]["contextId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("p2_settings", doc[1]["contextId"].as<const char*>());
}

int main() {
    UNITY_BEGIN();

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
    RUN_TEST(test_provider_registry_iterate_contexts);
    RUN_TEST(test_provider_get_context_at);

    // StreamingContextSerializer tests
    RUN_TEST(test_streaming_serializer_simple_context);
    RUN_TEST(test_streaming_serializer_with_fields);
    RUN_TEST(test_streaming_serializer_with_custom_html);
    RUN_TEST(test_streaming_serializer_chunked_output);
    RUN_TEST(test_streaming_serializer_json_escaping);
    RUN_TEST(test_streaming_serializer_field_with_options);

    // Memory stability tests (heap leak detection)
    RUN_TEST(test_streaming_serializer_no_memory_leak);
    RUN_TEST(test_provider_registry_schema_generation_no_leak);

    // Schema validation tests
    RUN_TEST(test_full_schema_array_valid_json);

    return UNITY_END();
}
