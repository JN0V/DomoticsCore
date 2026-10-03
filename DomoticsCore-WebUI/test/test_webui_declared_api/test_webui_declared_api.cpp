/**
 * @file test_webui_declared_api.cpp
 * @brief A path a context declares with withAPI() answers a GET with its data.
 *
 * Requests go through the mock server's dispatch(): an exact route for the uri
 * and method first, then the handlers. The library walks one list in insertion
 * order instead, which is why the handler also declines a path a route serves.
 */

#include <unity.h>

#include <DomoticsCore/WebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

namespace {

class DeclaringProvider : public CachingWebUIProvider {
public:
    String getWebUIName() const override { return "Declaring"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    String handleWebUIRequest(const String&, const String&, const String&,
                              const std::map<String, String>&) override {
        return "{\"success\":true}";
    }
    String getWebUIData(const String& contextId) override {
        if (contextId == "alpha") return "{\"a\":1}";
        if (contextId == "beta") return "{\"b\":2}";
        if (contextId == "gamma") return "{\"g\":3}";
        return "";
    }

protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        contexts.push_back(WebUIContext::dashboard("alpha", "Alpha").withAPI("/api/test/one"));
        contexts.push_back(WebUIContext::dashboard("beta", "Beta").withAPI("/api/test/shared"));
        contexts.push_back(WebUIContext::dashboard("gamma", "Gamma").withAPI("/api/test/shared"));
        contexts.push_back(WebUIContext::dashboard("quiet", "Quiet").withAPI("/api/test/empty"));
    }
};

struct Fixture {
    WebUIComponent webui;
    DeclaringProvider provider;

    explicit Fixture(bool auth = false) {
        if (auth) {
            WebUIConfig cfg = webui.getConfig();
            cfg.enableAuth = true;
            cfg.setUsername("admin");
            cfg.setPassword("secret");
            webui.setConfig(cfg);
        }
        webui.begin();
        webui.registerProvider(&provider);
    }

    bool get(AsyncWebServerRequest& request, const char* url, WebRequestMethod method = HTTP_GET) {
        request.setUrl(url);
        request.setMethod(method);
        return AsyncWebServer::last()->dispatch(request);
    }
};

}  // namespace

void setUp() { HAL::setCanOpenServerForTest(true); }
void tearDown() {}

void test_a_declared_path_answers_with_its_context_data() {
    Fixture f;
    AsyncWebServerRequest request;
    TEST_ASSERT_TRUE(f.get(request, "/api/test/one"));
    TEST_ASSERT_EQUAL_INT(200, request.sentCode);
    TEST_ASSERT_EQUAL_STRING("{\"alpha\":{\"a\":1}}", request.sentBody.c_str());
}

void test_a_path_declared_twice_answers_with_both_contexts() {
    Fixture f;
    AsyncWebServerRequest request;
    TEST_ASSERT_TRUE(f.get(request, "/api/test/shared"));
    TEST_ASSERT_EQUAL_STRING("{\"beta\":{\"b\":2},\"gamma\":{\"g\":3}}", request.sentBody.c_str());
}

void test_a_context_without_data_answers_null() {
    Fixture f;
    AsyncWebServerRequest request;
    TEST_ASSERT_TRUE(f.get(request, "/api/test/empty"));
    TEST_ASSERT_EQUAL_STRING("{\"quiet\":null}", request.sentBody.c_str());
}

void test_a_post_on_a_declared_path_is_not_handled() {
    Fixture f;
    AsyncWebServerRequest request;
    TEST_ASSERT_FALSE(f.get(request, "/api/test/one", HTTP_POST));
    TEST_ASSERT_EQUAL_INT(0, request.sendCount);
}

void test_an_undeclared_path_is_not_handled() {
    Fixture f;
    AsyncWebServerRequest request;
    TEST_ASSERT_FALSE(f.get(request, "/api/test/none"));
}

// The handler declines a path a real route serves, whichever was added first.
void test_an_explicit_route_on_a_declared_path_wins() {
    Fixture f;
    f.webui.registerApiRoute("/api/test/one", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "application/json", "{\"explicit\":true}");
    });
    AsyncWebServer::last()->routes.clear();   // the handler alone must decline it
    AsyncWebServerRequest request;
    TEST_ASSERT_FALSE(f.get(request, "/api/test/one"));
}

void test_auth_applies_to_a_declared_path() {
    Fixture f(true);
    AsyncWebServerRequest refused;
    TEST_ASSERT_TRUE(f.get(refused, "/api/test/one"));
    TEST_ASSERT_TRUE(refused.authenticationRequested);
    TEST_ASSERT_EQUAL_INT(401, refused.sentCode);

    AsyncWebServerRequest allowed;
    allowed.setCredentials("admin", "secret");
    TEST_ASSERT_TRUE(f.get(allowed, "/api/test/one"));
    TEST_ASSERT_EQUAL_INT(200, allowed.sentCode);
}

void test_low_memory_answers_503() {
    Fixture f;
    HAL::Platform::setFreeHeapForTest(4095);
    AsyncWebServerRequest request;
    TEST_ASSERT_TRUE(f.get(request, "/api/test/one"));
    HAL::Platform::resetFreeHeapForTest();
    TEST_ASSERT_EQUAL_INT(503, request.sentCode);
}

// canHandle() and handleRequest() are two calls; the provider can go in between.
void test_a_provider_gone_since_can_handle_answers_404() {
    Fixture f;
    AsyncWebServerRequest request;
    request.setUrl("/api/test/one");
    AsyncWebHandler* declared = nullptr;
    for (auto* h : AsyncWebServer::last()->handlers) {
        if (h->canHandle(&request)) declared = h;
    }
    TEST_ASSERT_NOT_NULL(declared);
    f.webui.unregisterProvider(&f.provider);
    declared->handleRequest(&request);
    TEST_ASSERT_EQUAL_INT(404, request.sentCode);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_declared_path_answers_with_its_context_data);
    RUN_TEST(test_a_path_declared_twice_answers_with_both_contexts);
    RUN_TEST(test_a_context_without_data_answers_null);
    RUN_TEST(test_a_post_on_a_declared_path_is_not_handled);
    RUN_TEST(test_an_undeclared_path_is_not_handled);
    RUN_TEST(test_an_explicit_route_on_a_declared_path_wins);
    RUN_TEST(test_auth_applies_to_a_declared_path);
    RUN_TEST(test_low_memory_answers_503);
    RUN_TEST(test_a_provider_gone_since_can_handle_answers_404);
    return UNITY_END();
}
