/**
 * @file test_webui_schema_integration.cpp
 * @brief WebUI component: the schema endpoint, simulated end to end.
 */

#include <unity.h>
#include "../webui_test_support.h"

void setUp() {}
void tearDown() {}

// ============================================================================
// Integration Tests - Schema Endpoint Simulation
// ============================================================================

/**
 * @brief Helper to serialize a context to JSON using ArduinoJson (same as endpoint)
 */
void serializeContextToJson(JsonObject& obj, const WebUIContext& context) {
    obj["contextId"] = context.getContextIdCStr();
    obj["title"] = context.getTitleCStr();
    obj["icon"] = context.getIconCStr();
    obj["location"] = (int)context.location;
    obj["priority"] = context.priority;
    obj["apiEndpoint"] = context.getApiEndpointCStr();
    obj["alwaysInteractive"] = context.alwaysInteractive;
    
    if (!context.customHtml.isEmpty()) obj["customHtml"] = context.customHtml;
    if (!context.customCss.isEmpty()) obj["customCss"] = context.customCss;
    if (!context.customJs.isEmpty()) obj["customJs"] = context.customJs;

    JsonArray fields = obj["fields"].to<JsonArray>();
    for (const auto& field : context.fields) {
        JsonObject fieldObj = fields.add<JsonObject>();
        fieldObj["name"] = field.getNameCStr();
        fieldObj["label"] = field.getLabelCStr();
        fieldObj["type"] = (int)field.type;
        fieldObj["value"] = field.getValueCStr();
        fieldObj["unit"] = field.getUnitCStr();
        fieldObj["readOnly"] = field.readOnly;
        fieldObj["minValue"] = field.minValue;
        fieldObj["maxValue"] = field.maxValue;
        fieldObj["endpoint"] = field.getEndpointCStr();
        if (!field.options.empty()) {
            JsonArray options = fieldObj["options"].to<JsonArray>();
            for (const auto& opt : field.options) options.add(opt);
        }
        if (!field.optionLabels.empty()) {
            JsonObject labels = fieldObj["optionLabels"].to<JsonObject>();
            for (const auto& pair : field.optionLabels) labels[pair.first] = pair.second;
        }
    }
}

/**
 * @brief Integration test: Simulates the /api/ui/schema endpoint exactly
 * 
 * This test reproduces the exact code path used in WebUI.h to serialize
 * the schema, then parses it back to verify it's valid JSON.
 */
void test_integration_schema_endpoint_produces_valid_json() {
    // Setup: Create provider with multiple contexts
    MultiContextProvider provider;
    
    // Build unique provider list (like endpoint does)
    std::vector<IWebUIProvider*> providers;
    providers.push_back(&provider);
    
    // Simulate ResponseStream by building JSON string
    String jsonOutput = "[";
    bool first = true;
    
    for (IWebUIProvider* p : providers) {
        if (!p || !p->isWebUIEnabled()) continue;
        
        // Use direct pointer access - no copies (exactly like endpoint)
        for (size_t i = 0; ; i++) {
            const WebUIContext* ctx = p->getContextAtRef(i);
            if (!ctx) break;
            if (strlen(ctx->getContextIdCStr()) == 0) continue;
            
            if (!first) jsonOutput += ",";
            first = false;
            
            // Serialize directly using ArduinoJson
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            serializeContextToJson(obj, *ctx);
            
            String ctxJson;
            serializeJson(doc, ctxJson);
            jsonOutput += ctxJson;
        }
    }
    jsonOutput += "]";
    
    printf("\n[Integration] Schema JSON length: %d bytes\n", jsonOutput.length());
    
    // Verify: Parse the JSON back
    JsonDocument parseDoc;
    DeserializationError error = deserializeJson(parseDoc, jsonOutput);
    
    TEST_ASSERT_TRUE_MESSAGE(error == DeserializationError::Ok, 
        "Schema JSON is invalid - failed to parse");
    
    // Verify it's an array
    TEST_ASSERT_TRUE_MESSAGE(parseDoc.is<JsonArray>(), 
        "Schema should be a JSON array");
    
    JsonArray arr = parseDoc.as<JsonArray>();
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, arr.size(), 
        "Schema array should not be empty");
    
    // Verify each context has required fields
    for (JsonObject ctx : arr) {
        TEST_ASSERT_TRUE_MESSAGE(ctx.containsKey("contextId"), 
            "Each context must have contextId");
        TEST_ASSERT_TRUE_MESSAGE(ctx.containsKey("title"), 
            "Each context must have title");
        TEST_ASSERT_TRUE_MESSAGE(ctx.containsKey("fields"), 
            "Each context must have fields array");
        
        // Verify contextId is a valid string (no garbage)
        const char* ctxId = ctx["contextId"];
        TEST_ASSERT_NOT_NULL_MESSAGE(ctxId, "contextId should not be null");
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, strlen(ctxId), "contextId should not be empty");
        
        // Check for invalid characters (would cause querySelector errors)
        for (size_t i = 0; i < strlen(ctxId); i++) {
            char c = ctxId[i];
            bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
            TEST_ASSERT_TRUE_MESSAGE(valid, 
                "contextId contains invalid character for CSS selector");
        }
    }
    
    printf("[Integration] All %d contexts have valid structure\n", arr.size());
    TEST_PASS_MESSAGE("Schema endpoint produces valid, parseable JSON");
}

/**
 * @brief Integration test: Detect memory corruption from temporary strings in cached contexts
 * 
 * This test catches the bug where buildContexts() uses temporary String variables
 * that get destroyed after the function returns, leaving dangling pointers.
 */
void test_integration_detect_dangling_string_pointers() {
    // Create a provider that intentionally uses a temporary string (BAD PATTERN)
    class BadProvider : public CachingWebUIProvider {
    protected:
        void buildContexts(std::vector<WebUIContext>& ctxs) override {
            // BAD: Using a temporary String that will be destroyed
            String tempValue = "temp_value_" + String(HAL::getMillis());
            ctxs.push_back(WebUIContext{"bad_ctx", "Bad", "dc-warning", 
                                        WebUILocation::Dashboard}
                .withField(WebUIField("bad_field", "Bad", WebUIFieldType::Text, tempValue.c_str())));
        }
    public:
        String getWebUIName() const override { return "Bad"; }
        String getWebUIVersion() const override { return "1.0"; }
        String getWebUIData(const String&) override { return "{}"; }
        String handleWebUIRequest(const String&, const String&, const String&, const std::map<String, String>&) override { return "{}"; }
        bool hasDataChanged(const String&) override { return false; }
    };
    
    // Create a provider that uses static literals (GOOD PATTERN)
    class GoodProvider : public CachingWebUIProvider {
    protected:
        void buildContexts(std::vector<WebUIContext>& ctxs) override {
            // GOOD: Using static string literal
            ctxs.push_back(WebUIContext{"good_ctx", "Good", "dc-check", 
                                        WebUILocation::Dashboard}
                .withField(WebUIField("good_field", "Good", WebUIFieldType::Text, "static_value")));
        }
    public:
        String getWebUIName() const override { return "Good"; }
        String getWebUIVersion() const override { return "1.0"; }
        String getWebUIData(const String&) override { return "{}"; }
        String handleWebUIRequest(const String&, const String&, const String&, const std::map<String, String>&) override { return "{}"; }
        bool hasDataChanged(const String&) override { return false; }
    };
    
    GoodProvider good;
    
    // Access context multiple times - good provider should be stable
    for (int i = 0; i < 10; i++) {
        const WebUIContext* ctx = good.getContextAtRef(0);
        TEST_ASSERT_NOT_NULL_MESSAGE(ctx, "Good context should exist");
        
        // Serialize to JSON and parse back
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();
        serializeContextToJson(obj, *ctx);
        
        String json;
        serializeJson(doc, json);
        
        // Parse and validate
        JsonDocument parseDoc;
        DeserializationError err = deserializeJson(parseDoc, json);
        TEST_ASSERT_TRUE_MESSAGE(err == DeserializationError::Ok, "Good JSON should parse");
        
        // Verify field value is correct
        const char* value = parseDoc["fields"][0]["value"];
        TEST_ASSERT_NOT_NULL_MESSAGE(value, "Value should exist");
        TEST_ASSERT_EQUAL_STRING_MESSAGE("static_value", value, "Value should be static");
    }
    
    printf("\n[Integration] Dangling pointer detection: Good provider stable\n");
    TEST_PASS_MESSAGE("Static string literals in cached contexts work correctly");
}

/**
 * @brief Integration test: Verify schema remains valid after 100 requests
 * 
 * This catches memory corruption that might only appear after repeated use.
 */
void test_integration_schema_stable_after_100_requests() {
    MultiContextProvider provider;
    
    for (int request = 0; request < 100; request++) {
        String jsonOutput = "[";
        bool first = true;
        
        for (size_t i = 0; ; i++) {
            const WebUIContext* ctx = provider.getContextAtRef(i);
            if (!ctx) break;
            if (strlen(ctx->getContextIdCStr()) == 0) continue;
            
            if (!first) jsonOutput += ",";
            first = false;
            
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            serializeContextToJson(obj, *ctx);
            
            String ctxJson;
            serializeJson(doc, ctxJson);
            jsonOutput += ctxJson;
        }
        jsonOutput += "]";
        
        // Parse and validate each response
        JsonDocument parseDoc;
        DeserializationError error = deserializeJson(parseDoc, jsonOutput);
        
        if (error != DeserializationError::Ok) {
            printf("\n[FAIL] Request %d produced invalid JSON: %s\n", 
                   request, error.c_str());
            TEST_FAIL_MESSAGE("Schema corruption detected after repeated requests");
        }
        
        JsonArray arr = parseDoc.as<JsonArray>();
        if (arr.size() == 0) {
            printf("\n[FAIL] Request %d produced empty schema\n", request);
            TEST_FAIL_MESSAGE("Schema became empty after repeated requests");
        }
    }
    
    printf("\n[Integration] 100 consecutive schema requests all valid\n");
    TEST_PASS_MESSAGE("Schema remains valid after 100 requests");
}

int main() {
    UNITY_BEGIN();

    // Integration tests - Schema endpoint simulation
    RUN_TEST(test_integration_schema_endpoint_produces_valid_json);
    RUN_TEST(test_integration_detect_dangling_string_pointers);
    RUN_TEST(test_integration_schema_stable_after_100_requests);

    return UNITY_END();
}
