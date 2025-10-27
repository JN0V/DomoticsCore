#include <Arduino.h>
#include <WiFi.h>

#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/OTA.h>
#include <DomoticsCore/OTAWebUI.h>

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;

Core core;

void setup() {
    DLOG_I(LOG_APP, "=== DomoticsCore OTAWithWebUI Starting ===");

    // Start soft AP for easy access
    String apSSID = String("DomoticsCore-OTA-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    WiFi.softAP(apSSID.c_str());
    DLOG_I(LOG_APP, "AP IP: %s", WiFi.softAPIP().toString().c_str());

    // Core initialized

    // Web UI component
    WebUIConfig webCfg; webCfg.deviceName = "OTA With WebUI"; webCfg.wsUpdateInterval = 2000;
    core.addComponent(std::make_unique<WebUIComponent>(webCfg));

    // OTA component with default config
    OTAConfig otaCfg;
    core.addComponent(std::make_unique<OTAComponent>(otaCfg));

    CoreConfig cfg; cfg.deviceName = "OTAWithWebUI"; cfg.logLevel = 3;
    core.begin(cfg);

    // Register OTA WebUI provider AFTER core begin (so WebUI server is started)
    auto* webui = core.getComponent<WebUIComponent>("WebUI");
    auto* ota = core.getComponent<OTAComponent>("OTA");
    if (webui && ota) {
        // Provide transport-agnostic hooks implemented via HTTPClient here in the app layer
        ota->setManifestFetcher([ota](const String& manifestUrl, String& outJson) -> bool {
            // Decide client based on URL scheme
            std::unique_ptr<WiFiClient> client;
            if (manifestUrl.startsWith("https://")) {
                auto* secure = new WiFiClientSecure();
                const auto& cfg = ota->getOTAConfig();
                if (!cfg.rootCA.isEmpty()) secure->setCACert(cfg.rootCA.c_str()); else secure->setInsecure();
                client.reset(secure);
            } else {
                client.reset(new WiFiClient());
            }
            HTTPClient http;
            if (!http.begin(*client, manifestUrl)) {
                return false;
            }
            const auto& cfg = ota->getOTAConfig();
            if (!cfg.bearerToken.isEmpty()) http.addHeader("Authorization", String("Bearer ") + cfg.bearerToken);
            if (!cfg.basicAuthUser.isEmpty()) http.setAuthorization(cfg.basicAuthUser.c_str(), cfg.basicAuthPassword.c_str());
            int code = http.GET();
            if (code != HTTP_CODE_OK) { http.end(); return false; }
            outJson = http.getString();
            http.end();
            return true;
        });

        ota->setDownloader([ota](const String& url, size_t& totalSize, OTAComponent::DownloadCallback onChunk) -> bool {
            std::unique_ptr<WiFiClient> client;
            if (url.startsWith("https://")) {
                auto* secure = new WiFiClientSecure();
                const auto& cfg = ota->getOTAConfig();
                if (!cfg.rootCA.isEmpty()) secure->setCACert(cfg.rootCA.c_str()); else secure->setInsecure();
                client.reset(secure);
            } else {
                client.reset(new WiFiClient());
            }
            HTTPClient http;
            if (!http.begin(*client, url)) {
                return false;
            }
            const auto& cfg = ota->getOTAConfig();
            if (!cfg.bearerToken.isEmpty()) http.addHeader("Authorization", String("Bearer ") + cfg.bearerToken);
            if (!cfg.basicAuthUser.isEmpty()) http.setAuthorization(cfg.basicAuthUser.c_str(), cfg.basicAuthPassword.c_str());
            int code = http.GET();
            if (code != HTTP_CODE_OK) { http.end(); return false; }
            int len = http.getSize();
            totalSize = len > 0 ? static_cast<size_t>(len) : 0;
            WiFiClient* stream = http.getStreamPtr();
            if (!stream) { http.end(); return false; }
            uint8_t buf[2048];
            unsigned long lastData = millis();
            const unsigned long idleMs = 10000;
            while (true) {
                int avail = stream->available();
                if (avail <= 0) {
                    if (totalSize == 0 && (millis() - lastData) > idleMs) break; // assume end
                    delay(1);
                    continue;
                }
                size_t toRead = avail > (int)sizeof(buf) ? sizeof(buf) : (size_t)avail;
                int r = stream->readBytes(buf, toRead);
                if (r <= 0) continue;
                lastData = millis();
                if (!onChunk(buf, (size_t)r)) { http.end(); return false; }
                if (totalSize > 0) {
                    // Let OTA core track progress; we just stream
                    // No break until server closes or all bytes read
                }
            }
            http.end();
            return true;
        });

        auto* otaWebUI = new DomoticsCore::Components::WebUI::OTAWebUI(ota);
        otaWebUI->init(webui); // Initialize routes after WebUI is ready
        webui->registerProviderWithComponent(otaWebUI, ota);
    }

    DLOG_I(LOG_APP, "WebUI available at http://%s/", WiFi.softAPIP().toString().c_str());
}

void loop() {
    core.loop();
}
