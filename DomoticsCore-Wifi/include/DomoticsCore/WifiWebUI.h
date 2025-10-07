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
    
    // State tracking using LazyState helper for timing-independent initialization
    // Simple states
    LazyState<bool> wifiStatusState;        // WiFi STA connection status
    LazyState<bool> apStatusState;          // AP enabled status
    LazyState<bool> wifiSTAEnabledState;    // WiFi STA enabled config
    LazyState<bool> wifiAPEnabledState;     // WiFi AP enabled config
    
    // Composite states for contexts with multiple values
    struct ComponentState {
        bool connected;
        String ssid;
        String ip;
        
        bool operator==(const ComponentState& other) const {
            return connected == other.connected && ssid == other.ssid && ip == other.ip;
        }
        bool operator!=(const ComponentState& other) const { return !(*this == other); }
    };
    LazyState<ComponentState> wifiComponentState;
    
    struct STASettingsState {
        bool enabled;
        String ssid;
        
        bool operator==(const STASettingsState& other) const {
            return enabled == other.enabled && ssid == other.ssid;
        }
        bool operator!=(const STASettingsState& other) const { return !(*this == other); }
    };
    LazyState<STASettingsState> wifiSTASettingsState;
    
public:
    explicit WifiWebUI(WifiComponent* c) : wifi(c) {
        if (wifi) {
            pendingSsid = wifi->getConfiguredSSID();
            // State will be lazily initialized on first hasDataChanged() call
            // to ensure WiFi component is fully initialized
        }
    }

    String getWebUIName() const override { return wifi ? wifi->getName() : String("Wifi"); }
    String getWebUIVersion() const override { return String("1.0.0"); }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> ctxs;
        if (!wifi) return ctxs;

        // Header badge for quick status
        ctxs.push_back(WebUIContext::statusBadge("wifi_status", "WiFi", "dc-wifi").withRealTime(2000));
        // AP status badge with custom icon
        ctxs.push_back(WebUIContext::statusBadge("ap_status", "AP", "antenna-radiowaves-left-right").withRealTime(2000));

        // Components tab card
        ctxs.push_back(WebUIContext{
            "wifi_component", "WiFi", "fas fa-wifi", WebUILocation::ComponentDetail, WebUIPresentation::Card
        }
        .withField(WebUIField("connected", "Connected", WebUIFieldType::Display, wifi->isSTAConnected() ? "Yes" : "No", "", true))
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
            doc["connected"] = wifi->isSTAConnected() ? "Yes" : "No";
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
            doc["state"] = wifi->isSTAConnected() ? "ON" : "OFF";
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
    
    bool hasDataChanged(const String& contextId) override {
        if (!wifi) return false;
        
        // Use LazyState helper for timing-independent change tracking
        if (contextId == "wifi_status") {
            return wifiStatusState.hasChanged(wifi->isSTAConnected());
        } 
        else if (contextId == "ap_status") {
            return apStatusState.hasChanged(wifi->isAPEnabled());
        }
        else if (contextId == "wifi_component") {
            ComponentState current = {
                wifi->isSTAConnected(),
                wifi->getSSID(),
                wifi->getLocalIP()
            };
            return wifiComponentState.hasChanged(current);
        }
        else if (contextId == "wifi_sta_settings" || contextId == "wifi_settings") {
            STASettingsState current = {
                wifi->isWifiEnabled(),
                wifi->getConfiguredSSID()
            };
            return wifiSTASettingsState.hasChanged(current);
        }
        else if (contextId == "wifi_ap_settings") {
            return wifiAPEnabledState.hasChanged(wifi->isAPEnabled());
        }
        else {
            // Unknown context, always send
            return true;
        }
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
