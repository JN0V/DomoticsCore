#pragma once

#include "DomoticsCore/Wifi.h"
#include "DomoticsCore/IWebUIProvider.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

class WifiWebUI : public IWebUIProvider {
    WifiComponent* wifi; // non-owning
    // Keep pending credentials updated from UI
    String pendingSsid;
    String pendingPassword;
    // Last scan results (comma-separated for simple display)
    String lastScanSummary;
public:
    explicit WifiWebUI(WifiComponent* c) : wifi(c) {
        if (wifi) pendingSsid = wifi->getConfiguredSSID();
    }

    String getWebUIName() const override { return wifi ? wifi->getName() : String("Wifi"); }
    String getWebUIVersion() const override { return String("1.0.0"); }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> ctxs;
        if (!wifi) return ctxs;

        // Header badge for quick status
        ctxs.push_back(BaseWebUIComponents::createStatusBadge("wifi_status", "WiFi", "dc-wifi").withRealTime(2000));
        // AP status badge with custom icon
        ctxs.push_back(BaseWebUIComponents::createStatusBadge("ap_status", "AP", "antenna-radiowaves-left-right").withRealTime(2000));

        // Components tab card
        ctxs.push_back(WebUIContext{
            "wifi_component", "WiFi", "fas fa-wifi", WebUILocation::ComponentDetail, WebUIPresentation::Card
        }
        .withField(WebUIField("connected", "Connected", WebUIFieldType::Display, wifi->isConnected() ? "Yes" : "No", "", true))
        .withField(WebUIField("ssid_now", "SSID", WebUIFieldType::Display, wifi->getSSID(), "", true))
        .withField(WebUIField("ip", "IP", WebUIFieldType::Display, wifi->getLocalIP(), "", true))
        .withRealTime(2000));

        // Settings controls - STA section
        ctxs.push_back(WebUIContext::settings("wifi_sta_settings", "WiFi (STA)")
            .withField(WebUIField("wifi_enabled", "Enabled", WebUIFieldType::Boolean, wifi->isWifiEnabled() ? "true" : "false"))
            .withField(WebUIField("ssid", "SSID", WebUIFieldType::Text, wifi->getConfiguredSSID()))
            .withField(WebUIField("password", "Password", WebUIFieldType::Text, ""))
            .withField(WebUIField("connect", "Connect", WebUIFieldType::Button, ""))
            .withField(WebUIField("scan_networks", "Scan Networks", WebUIFieldType::Button, ""))
            .withField(WebUIField("networks", "Networks", WebUIFieldType::Display, ""))
            .withAPI("/api/wifi")
            .withRealTime(2000)
        );

        // Settings controls - AP section
        ctxs.push_back(WebUIContext::settings("wifi_ap_settings", "Access Point (AP)")
            .withField(WebUIField("ap_enabled", "AP Enabled", WebUIFieldType::Boolean, wifi->isAPEnabled() ? "true" : "false"))
            .withField(WebUIField("ap_ssid", "AP SSID", WebUIFieldType::Text, wifi->isAPEnabled() ? wifi->getAPSSID() : String("DomoticsCore-AP")))
            .withAPI("/api/wifi")
            .withRealTime(2000)
        );

        return ctxs;
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params) override {
        if (!wifi) return "{\"success\":false}";
        // Backward compatibility: accept legacy context id as STA settings
        if ((contextId == "wifi_settings" || contextId == "wifi_sta_settings") && method == "POST") {
            auto f = params.find("field");
            auto v = params.find("value");
            if (f == params.end() || v == params.end()) return "{\"success\":false}";
            String field = f->second; String value = v->second;
            if (field == "wifi_enabled") {
                bool en = (value == "true" || value == "1" || value == "on");
                wifi->enableWifi(en);
                if (en) wifi->reconnect();
                return "{\"success\":true}";
            } else if (field == "ssid") {
                pendingSsid = value;
                return "{\"success\":true}";
            } else if (field == "password") {
                pendingPassword = value;
                return "{\"success\":true}";
            } else if (field == "connect") {
                wifi->setCredentials(pendingSsid, pendingPassword, true);
                // clear password for safety
                pendingPassword = "";
                return "{\"success\":true}";
            } else if (field == "scan_networks") {
                wifi->startScanAsync();
                lastScanSummary = "Scanning...";
                return "{\"success\":true}";
            }
        } else if (contextId == "wifi_ap_settings" && method == "POST") {
            auto f = params.find("field");
            auto v = params.find("value");
            if (f == params.end() || v == params.end()) return "{\"success\":false}";
            String field = f->second; String value = v->second;
            if (field == "ap_enabled") {
                bool en = (value == "true" || value == "1" || value == "on");
                if (en) {
                    String apName = pendingSsid.length() ? pendingSsid : (wifi->getAPSSID().length() ? wifi->getAPSSID() : String("DomoticsCore-AP"));
                    wifi->enableAP(apName);
                } else {
                    wifi->disableAP();
                }
                return "{\"success\":true}";
            } else if (field == "ap_ssid") {
                // Update AP SSID immediately if AP enabled
                String newAp = value;
                if (newAp.isEmpty()) newAp = "DomoticsCore-AP";
                if (wifi->isAPEnabled()) {
                    wifi->enableAP(newAp);
                }
                return "{\"success\":true}";
            }
        }
        return "{\"success\":false}";
    }

    String getWebUIData(const String& contextId) override {
        if (!wifi) return "{}";
        if (contextId == "wifi_component") {
            JsonDocument doc;
            doc["connected"] = wifi->isConnected() ? "Yes" : "No";
            doc["ssid_now"] = wifi->getSSID();
            doc["ip"] = wifi->getLocalIP();
            String json; serializeJson(doc, json); return json;
        }
        if (contextId == "wifi_sta_settings" || contextId == "wifi_settings") {
            JsonDocument doc;
            doc["wifi_enabled"] = wifi->isWifiEnabled() ? "true" : "false";
            // Prefer pending SSID if user typed one
            doc["ssid"] = pendingSsid.length() ? pendingSsid : wifi->getConfiguredSSID();
            doc["password"] = ""; // never echo back
            doc["networks"] = wifi->getLastScanSummary();
            String json; serializeJson(doc, json); return json;
        }
        if (contextId == "ap_status") {
            JsonDocument doc;
            doc["state"] = wifi->isAPEnabled() ? "ON" : "OFF";
            String json; serializeJson(doc, json); return json;
        }
        if (contextId == "wifi_status") {
            JsonDocument doc;
            doc["state"] = wifi->isConnected() ? "ON" : "OFF";
            String json; serializeJson(doc, json); return json;
        }
        if (contextId == "wifi_ap_settings") {
            JsonDocument doc;
            doc["ap_enabled"] = wifi->isAPEnabled() ? "true" : "false";
            doc["ap_ssid"] = wifi->isAPEnabled() ? wifi->getAPSSID() : String("DomoticsCore-AP");
            String json; serializeJson(doc, json); return json;
        }
        return "{}";
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
