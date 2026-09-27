/**
 * @file test_webui_component_toggle.cpp
 * @brief Enabling or disabling a component from the page runs in loop().
 *
 * The route runs on the web server's task. A component's begin() or shutdown(),
 * and the provider registry the loop reads, must not be touched from there.
 */

#include <unity.h>

#include <DomoticsCore/WebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

class CountedComponent : public IComponent {
public:
    int begins = 0;
    int shutdowns = 0;
    ComponentStatus begin() override { begins++; return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { shutdowns++; return ComponentStatus::Success; }
};

class ToggleProvider : public CachingWebUIProvider {
protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        contexts.push_back(WebUIContext::dashboard("toggle_ctx", "Toggle"));
    }
public:
    String getWebUIName() const override { return "Toggle"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    String handleWebUIRequest(const String&, const String&, const String&,
                              const std::map<String, String>&) override { return "{}"; }
    String getWebUIData(const String&) override { return "{}"; }
};

String csrfToken() {
    AsyncWebServerRequest request;
    AsyncWebServer::findRouteAnywhere("/api/ui/token")->handler(&request);
    const String body = request.sentBody;
    const int start = body.indexOf("\":\"");
    const int end = body.lastIndexOf('"');
    return (start < 0 || end <= start + 3) ? String("") : body.substring(start + 3, end);
}

// What the page reads back: the Toggle entry's "enabled" in GET /api/components.
bool listedEnabled() {
    AsyncWebServerRequest request;
    AsyncWebServer::findRouteAnywhere("/api/components")->handler(&request);
    JsonDocument doc;
    deserializeJson(doc, request.sentBody);
    for (JsonObject c : doc["components"].as<JsonArray>()) {
        if (strcmp(c["name"] | "", "Toggle") == 0) return c["enabled"] | false;
    }
    TEST_FAIL_MESSAGE("Toggle is not listed");
    return false;
}

void post(const char* name, const char* enabled, AsyncWebServerRequest& request) {
    request.addHeader("X-DC-Token", csrfToken());
    request.addParam("name", name, true);
    request.addParam("enabled", enabled, true);
    RecordedRoute* route = AsyncWebServer::findRouteAnywhere("/api/components/enable");
    TEST_ASSERT_NOT_NULL(route);
    route->handler(&request);
}

}  // namespace

void setUp() { HAL::setCanOpenServerForTest(true); }
void tearDown() {}

void test_a_disable_from_the_page_waits_for_loop() {
    WebUIComponent webui;
    webui.begin();
    CountedComponent component;
    ToggleProvider provider;
    webui.registerProviderWithComponent(&provider, &component);

    AsyncWebServerRequest request;
    post("Toggle", "false", request);
    TEST_ASSERT_TRUE_MESSAGE(request.sentBody.indexOf("\"success\":true") >= 0, request.sentBody.c_str());
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, component.shutdowns, "shutdown() ran on the web server's task");
    TEST_ASSERT_TRUE_MESSAGE(listedEnabled(), "the registry was changed on the web server's task");

    webui.loop();
    TEST_ASSERT_EQUAL_INT(1, component.shutdowns);
    TEST_ASSERT_FALSE(listedEnabled());
}

void test_an_enable_from_the_page_waits_for_loop() {
    WebUIComponent webui;
    webui.begin();
    CountedComponent component;
    ToggleProvider provider;
    webui.registerProviderWithComponent(&provider, &component);

    AsyncWebServerRequest off;
    post("Toggle", "false", off);
    webui.loop();

    AsyncWebServerRequest on;
    post("Toggle", "true", on);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, component.begins, "begin() ran on the web server's task");
    webui.loop();
    TEST_ASSERT_EQUAL_INT(1, component.begins);
    TEST_ASSERT_TRUE(listedEnabled());
}

// The page cannot switch the WebUI off; that answer needs nothing from the loop.
void test_disabling_the_webui_is_refused_at_once() {
    WebUIComponent webui;
    webui.begin();
    AsyncWebServerRequest request;
    post("WebUI", "false", request);
    TEST_ASSERT_TRUE(request.sentBody.indexOf("\"success\":false") >= 0);
    TEST_ASSERT_TRUE(request.sentBody.indexOf("warning") >= 0);
}

// The queue is bounded: a request beyond it is refused, not dropped in silence.
void test_a_request_beyond_the_queue_is_refused() {
    WebUIComponent webui;
    webui.begin();
    for (int i = 0; i < 4; i++) {
        AsyncWebServerRequest request;
        post("Toggle", (i % 2) ? "true" : "false", request);
        TEST_ASSERT_TRUE(request.sentBody.indexOf("\"success\":true") >= 0);
    }
    AsyncWebServerRequest fifth;
    post("Toggle", "false", fifth);
    TEST_ASSERT_TRUE_MESSAGE(fifth.sentBody.indexOf("\"success\":false") >= 0, fifth.sentBody.c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_disable_from_the_page_waits_for_loop);
    RUN_TEST(test_an_enable_from_the_page_waits_for_loop);
    RUN_TEST(test_disabling_the_webui_is_refused_at_once);
    RUN_TEST(test_a_request_beyond_the_queue_is_refused);
    return UNITY_END();
}
