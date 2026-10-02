// Board probe: polls and SSE broadcasts share one update buffer. Read the
// RACE lines for the address, then run wsbuf_race_check.py against it.
#include <Arduino.h>
#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>

#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif
#ifndef DC_WIFI_SSID
#  define DC_WIFI_SSID ""
#endif
#ifndef DC_WIFI_PASSWORD
#  define DC_WIFI_PASSWORD ""
#endif

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

static constexpr int BULK_FIELDS = 24;

class BulkComponent : public IComponent {
public:
    BulkComponent() { metadata.name = "Bulk"; }
    const char* getTypeKey() const override { return "Bulk"; }
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
};

// Every build sees new values, so every SSE broadcast carries this context.
class BulkWebUI : public CachingWebUIProvider {
public:
    String getWebUIName() const override { return "Bulk"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    String getWebUIData(const String&) override {
        char buf[64];
        String json = "{";
        for (int i = 0; i < BULK_FIELDS; ++i) {
            snprintf(buf, sizeof(buf), "%s\"f%d\":\"%010lu-abcdefghijklmnopqrstuvwxyz\"",
                     i ? "," : "", i, (unsigned long)micros());
            json += buf;
        }
        return json + "}";
    }
    String handleWebUIRequest(const String&, const String&, const String&,
                              const std::map<String, String>&) override {
        return "{\"success\":false}";
    }
protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        WebUIContext ctx = WebUIContext::dashboard("bulk", "Bulk");
        for (int i = 0; i < BULK_FIELDS; ++i) {
            String name = String("f") + i;
            ctx.withField(WebUIField(name, name, WebUIFieldType::Display, "", "", true));
        }
        contexts.push_back(ctx.withRealTime(100));
    }
};

static Core core;

void setup() {
    Serial.begin(115200);
    delay(300);
    WiFi.mode(WIFI_STA);
    WiFi.begin(DC_WIFI_SSID, DC_WIFI_PASSWORD);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) delay(250);

    WebUIConfig ui;
    ui.setDeviceName("BufferRace");
    ui.wsUpdateInterval = 0;
    ui.useFileSystem = false;
    core.addComponent(std::make_unique<WebUIComponent>(ui));
    core.addComponent(std::make_unique<BulkComponent>());
    auto* webui = core.getComponent<WebUIComponent>("WebUI");
    webui->registerProviderFactory("Bulk", [](IComponent*) -> IWebUIProvider* { return new BulkWebUI(); });

    CoreConfig cfg;
    cfg.deviceName = "BufferRace";
    core.begin(cfg);
    Serial.printf("RACE ready ip=%s heap=%u\n", WiFi.localIP().toString().c_str(), (unsigned)ESP.getFreeHeap());
}

void loop() {
    core.loop();
    static uint32_t last = 0;
    if (millis() - last >= 10000) {
        last = millis();
        Serial.printf("RACE alive t=%lus ip=%s heap=%u\n", (unsigned long)(millis() / 1000),
                      WiFi.localIP().toString().c_str(), (unsigned)ESP.getFreeHeap());
    }
}
