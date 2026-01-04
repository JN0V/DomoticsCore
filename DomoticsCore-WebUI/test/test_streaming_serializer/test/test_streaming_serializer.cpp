/**
 * @file test_streaming_serializer.cpp
 * @brief Unit tests for StreamingContextSerializer JSON output validation
 *
 * Runs on native platform to verify:
 * 1. Streaming serializer produces valid JSON
 * 2. CachingWebUIProvider caches contexts correctly
 * 3. Large customHtml/customCss/customJs strings serialize correctly
 * 4. Multiple contexts serialize without corruption
 */

#include <unity.h>
#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/IWebUIProvider.h>
#include <DomoticsCore/WebUI/StreamingContextSerializer.h>
#include <ArduinoJson.h>
#include <cstring>
#include <vector>
#include <string>

using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// Helper to serialize a context to a std::string using the streaming serializer
// Uses std::string for native platform compatibility
std::string serializeContextToStdString(const WebUIContext& ctx) {
    StreamingContextSerializer serializer;
    serializer.begin(ctx);

    std::string result;
    uint8_t buffer[256];

    while (!serializer.isComplete()) {
        size_t written = serializer.write(buffer, sizeof(buffer));
        if (written > 0) {
            result.append(reinterpret_cast<const char*>(buffer), written);
        }
    }

    return result;
}

// Test basic context serialization
void test_basic_context_serialization(void) {
    WebUIContext ctx("test_id", "Test Title", "test-icon",
                     WebUILocation::Dashboard, WebUIPresentation::Card);
    ctx.withField(WebUIField("field1", "Field One", WebUIFieldType::Text, "value1"));
    ctx.priority = 10;
    ctx.apiEndpoint = "/api/test";

    std::string json = serializeContextToStdString(ctx);

    // Validate JSON can be parsed
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, error.code(),
                              "JSON should be valid");

    // Validate content
    TEST_ASSERT_EQUAL_STRING("test_id", doc["contextId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Test Title", doc["title"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("test-icon", doc["icon"].as<const char*>());
    TEST_ASSERT_EQUAL(0, doc["location"].as<int>()); // Dashboard = 0
    TEST_ASSERT_EQUAL(0, doc["presentation"].as<int>()); // Card = 0
    TEST_ASSERT_EQUAL(10, doc["priority"].as<int>());
    TEST_ASSERT_EQUAL_STRING("/api/test", doc["apiEndpoint"].as<const char*>());

    // Validate fields array
    JsonArray fields = doc["fields"].as<JsonArray>();
    TEST_ASSERT_EQUAL(1, fields.size());
    TEST_ASSERT_EQUAL_STRING("field1", fields[0]["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Field One", fields[0]["label"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("value1", fields[0]["value"].as<const char*>());
}

// Test context with special characters that need escaping
void test_json_escaping(void) {
    WebUIContext ctx("escape_test", "Title with \"quotes\"", "icon",
                     WebUILocation::Dashboard, WebUIPresentation::Card);
    ctx.withField(WebUIField("field1", "Label\nwith\nnewlines", WebUIFieldType::Text,
                             "value\\with\\backslash"));

    std::string json = serializeContextToStdString(ctx);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, error.code(),
                              "JSON with escaped characters should be valid");

    TEST_ASSERT_EQUAL_STRING("Title with \"quotes\"", doc["title"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Label\nwith\nnewlines",
                             doc["fields"][0]["label"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("value\\with\\backslash",
                             doc["fields"][0]["value"].as<const char*>());
}

// Test context with large customHtml/customCss/customJs
void test_large_custom_content(void) {
    WebUIContext ctx("custom_test", "Custom Test", "icon",
                     WebUILocation::Settings, WebUIPresentation::Card);

    // Simulate large HTML content like in LEDWebUI
    const char* largeHtml = R"(
        <div class="card-header">
            <h3 class="card-title">LED Control</h3>
        </div>
        <div class="card-content led-dashboard">
            <div class="led-bulb-container">
                <svg class="led-bulb" viewBox="0 0 1024 1024">
                    <use href="#bulb-twotone"/>
                </svg>
            </div>
        </div>
    )";

    const char* largeCss = R"(
        .led-dashboard .led-bulb-container {
            display: flex;
            justify-content: center;
            margin-bottom: 1rem;
        }
        .led-dashboard .led-bulb {
            width: 64px;
            height: 64px;
            transition: all 0.3s ease;
        }
    )";

    const char* largeJs = R"(
        function updateLEDBulb() {
            const bulb = document.querySelector('.led-dashboard .led-bulb');
            const toggle = document.querySelector('#state_toggle');
            if (bulb && toggle) {
                bulb.classList.toggle('on', toggle.checked);
            }
        }
    )";

    ctx.withCustomHtml(largeHtml);
    ctx.withCustomCss(largeCss);
    ctx.withCustomJs(largeJs);
    ctx.withField(WebUIField("state", "State", WebUIFieldType::Boolean, "false"));

    std::string json = serializeContextToStdString(ctx);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, error.code(),
                              "JSON with large custom content should be valid");

    // Verify custom content is present using std::string find
    std::string customHtml = doc["customHtml"].as<std::string>();
    std::string customCss = doc["customCss"].as<std::string>();
    std::string customJs = doc["customJs"].as<std::string>();
    TEST_ASSERT_TRUE(customHtml.find("LED Control") != std::string::npos);
    TEST_ASSERT_TRUE(customCss.find("led-bulb-container") != std::string::npos);
    TEST_ASSERT_TRUE(customJs.find("updateLEDBulb") != std::string::npos);
}

// Test multiple fields with options
void test_field_with_options(void) {
    WebUIContext ctx("select_test", "Select Test", "icon",
                     WebUILocation::Settings, WebUIPresentation::Card);

    WebUIField selectField("effect", "Effect", WebUIFieldType::Select, "Solid");
    selectField.choices({"Solid", "Blink", "Fade", "Pulse"});
    ctx.withField(selectField);

    std::string json = serializeContextToStdString(ctx);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, error.code(),
                              "JSON with select field should be valid");

    JsonArray options = doc["fields"][0]["options"].as<JsonArray>();
    TEST_ASSERT_EQUAL(4, options.size());
    TEST_ASSERT_EQUAL_STRING("Solid", options[0].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Blink", options[1].as<const char*>());
}

// Test CachingWebUIProvider returns same cached contexts
class TestCachingProvider : public CachingWebUIProvider {
protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        buildCount++;
        contexts.push_back(WebUIContext::dashboard("test_dash", "Test Dashboard")
            .withField(WebUIField("field1", "Field 1", WebUIFieldType::Text, "value1")));
    }

public:
    String getWebUIName() const override { return "Test"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    String handleWebUIRequest(const String&, const String&, const String&,
                              const std::map<String, String>&) override {
        return "{}";
    }

    int buildCount = 0;
};

void test_caching_provider_caches_contexts(void) {
    TestCachingProvider provider;

    // First call should build
    auto contexts1 = provider.getWebUIContexts();
    TEST_ASSERT_EQUAL(1, provider.buildCount);
    TEST_ASSERT_EQUAL(1, contexts1.size());

    // Second call should use cache
    auto contexts2 = provider.getWebUIContexts();
    TEST_ASSERT_EQUAL_MESSAGE(1, provider.buildCount,
                              "buildContexts should only be called once");
    TEST_ASSERT_EQUAL(1, contexts2.size());

    // Third call with getContextAt
    WebUIContext ctx;
    bool found = provider.getContextAt(0, ctx);
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_MESSAGE(1, provider.buildCount,
                              "getContextAt should use cache");
    TEST_ASSERT_EQUAL_STRING("test_dash", ctx.contextId.c_str());

    // After invalidation, buildContexts should be called again
    provider.invalidateContextCache();
    auto contexts3 = provider.getWebUIContexts();
    TEST_ASSERT_EQUAL_MESSAGE(2, provider.buildCount,
                              "After invalidation, buildContexts should be called again");
}

// Test serializing array of contexts (simulates schema endpoint)
void test_serialize_multiple_contexts(void) {
    std::vector<WebUIContext> contexts;

    contexts.push_back(WebUIContext::statusBadge("status1", "Status 1", "icon1")
        .withField(WebUIField("state", "State", WebUIFieldType::Status, "ON")));

    contexts.push_back(WebUIContext::dashboard("dash1", "Dashboard")
        .withField(WebUIField("value", "Value", WebUIFieldType::Number, "42")));

    contexts.push_back(WebUIContext::settings("settings1", "Settings")
        .withField(WebUIField("enabled", "Enabled", WebUIFieldType::Boolean, "true"))
        .withField(WebUIField("name", "Name", WebUIFieldType::Text, "Test")));

    // Simulate schema endpoint: serialize as JSON array
    std::string json = "[";
    bool first = true;
    for (const auto& ctx : contexts) {
        if (!first) json += ",";
        first = false;
        json += serializeContextToStdString(ctx);
    }
    json += "]";

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, error.code(),
                              "JSON array of contexts should be valid");

    JsonArray arr = doc.as<JsonArray>();
    TEST_ASSERT_EQUAL(3, arr.size());
    TEST_ASSERT_EQUAL_STRING("status1", arr[0]["contextId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("dash1", arr[1]["contextId"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("settings1", arr[2]["contextId"].as<const char*>());
}

// Test chunked writing (simulates small buffer conditions)
void test_chunked_serialization(void) {
    WebUIContext ctx("chunked_test", "Chunked Test", "icon",
                     WebUILocation::Dashboard, WebUIPresentation::Card);
    ctx.withField(WebUIField("field1", "Field One", WebUIFieldType::Text, "value1"));
    ctx.withField(WebUIField("field2", "Field Two", WebUIFieldType::Number, "42"));
    ctx.withCustomHtml("<div>Custom HTML Content</div>");

    // Serialize with tiny buffer (8 bytes) to test chunking
    StreamingContextSerializer serializer;
    serializer.begin(ctx);

    std::string result;
    uint8_t buffer[8];  // Very small buffer
    int iterations = 0;
    const int maxIterations = 1000;  // Prevent infinite loop

    while (!serializer.isComplete() && iterations < maxIterations) {
        size_t written = serializer.write(buffer, sizeof(buffer));
        if (written > 0) {
            result.append(reinterpret_cast<const char*>(buffer), written);
        }
        iterations++;
    }

    TEST_ASSERT_TRUE_MESSAGE(serializer.isComplete(),
                             "Serializer should complete");
    TEST_ASSERT_TRUE_MESSAGE(iterations < maxIterations,
                             "Should not need excessive iterations");

    // Validate the chunked output is valid JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, result);

    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, error.code(),
                              "Chunked JSON should be valid");

    TEST_ASSERT_EQUAL_STRING("chunked_test", doc["contextId"].as<const char*>());
    TEST_ASSERT_EQUAL(2, doc["fields"].as<JsonArray>().size());
}

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_basic_context_serialization);
    RUN_TEST(test_json_escaping);
    RUN_TEST(test_large_custom_content);
    RUN_TEST(test_field_with_options);
    RUN_TEST(test_caching_provider_caches_contexts);
    RUN_TEST(test_serialize_multiple_contexts);
    RUN_TEST(test_chunked_serialization);

    return UNITY_END();
}
