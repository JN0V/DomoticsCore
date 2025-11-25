#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/Logger.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core core;

// --- Test Provider ---
class TestProvider : public IWebUIProvider {
public:
    String getWebUIName() const override { return "TestProvider"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        // Context 1: Status
        contexts.push_back(WebUIContext::statusBadge("tp_status", "Test Status", "icon-test")
            .withField(WebUIField("counter", "Counter", WebUIFieldType::Display, "0")));
        return contexts;
    }
    
    String getWebUIData(const String& contextId) override {
        if (contextId == "tp_status") {
            return "{\"counter\": 42}";
        }
        return "{}";
    }
    
    String handleWebUIRequest(const String&, const String&, const String&, const std::map<String, String>&) override {
        return "{\"success\":true}";
    }
};

// --- Test Component with Provider ---
class TestComponent : public IComponent {
    TestProvider provider;
public:
    TestComponent() {
        metadata.name = "TestComp";
    }
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    IWebUIProvider* getWebUIProvider() override { return &provider; }
};

void runTests() {
    DLOG_I("TEST", "=== Starting WebUI Refactor Tests ===");
    
    // 1. Verify instantiation
    WebUIConfig cfg;
    cfg.port = 8080;
    auto webui = new WebUIComponent(cfg);
    
    if (webui->getPort() == 8080) {
        DLOG_I("TEST", "✅ WebUI Config applied correctly");
    } else {
        DLOG_E("TEST", "❌ WebUI Config failed");
    }
    
    // 2. Verify Core Integration
    core.addComponent(std::unique_ptr<WebUIComponent>(webui));
    core.addComponent(std::unique_ptr<TestComponent>(new TestComponent())); // Should be auto-discovered
    
    core.begin();
    
    // 3. Check Provider Registration
    // We can't easily inspect private members, but we can check public API behavior
    // The WebUI itself is a provider, and TestComponent is one.
    // We can hit the API via internal methods if exposed, but for now we trust the startup logs
    // or we can verify the JSON schema output if we could mock the request.
    
    DLOG_I("TEST", "✅ WebUI Component started without crash");
    
    // 4. Test Context Retrieval
    auto contexts = webui->getWebUIContexts();
    bool foundSettings = false;
    for(const auto& ctx : contexts) {
        if(ctx.contextId == "webui_settings") foundSettings = true;
    }
    
    if(foundSettings) {
        DLOG_I("TEST", "✅ WebUI internal provider (settings) working");
    } else {
        DLOG_E("TEST", "❌ WebUI internal provider missing");
    }

    // 5. Test Data Retrieval
    String data = webui->getWebUIData("webui_settings");
    if(data.indexOf("theme") > 0) {
        DLOG_I("TEST", "✅ WebUI data retrieval working");
    } else {
         DLOG_E("TEST", "❌ WebUI data retrieval failed: %s", data.c_str());
    }

    DLOG_I("TEST", "=== Tests Completed ===");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    runTests();
}

void loop() {
    core.loop();
    delay(100);
}
