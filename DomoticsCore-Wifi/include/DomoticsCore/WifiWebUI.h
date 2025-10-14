#pragma once

#include "DomoticsCore/Wifi.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/Logger.h"
#include <functional>

#define LOG_WIFI_WEBUI "WIFI_WEBUI"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

class WifiWebUI : public IWebUIProvider {
    WifiComponent* wifi; // non-owning
    std::function<void(const String&, const String&)> onCredentialsSaved; // callback for persistence
    // Keep pending credentials updated from UI
    String pendingSsid;
    String pendingPassword;
    // Last scan results (comma-separated for simple display)
    String lastScanSummary;
    
    // ============================================================================
    // State tracking using LazyState helper for timing-independent initialization
    // ============================================================================
    
    // BADGE STATES (Simple bool for header badges)
    LazyState<bool> wifiStatusState;     // STA badge: connected?
    LazyState<bool> apStatusState;       // AP badge: active?
    
    // COMPONENT CARD STATES (Runtime details for Components tab)
    struct STAComponentState {
        bool connected;
        String ssid;      // Currently connected network
        String ip;
        
        bool operator==(const STAComponentState& other) const {
            return connected == other.connected && ssid == other.ssid && ip == other.ip;
        }
        bool operator!=(const STAComponentState& other) const { return !(*this == other); }
    };
    LazyState<STAComponentState> staComponentState;
    
    // SETTINGS CARD STATES (Configuration for Settings tab)
    struct STASettingsState {
        bool enabled;
        String ssid;      // Configured/target network
        
        bool operator==(const STASettingsState& other) const {
            return enabled == other.enabled && ssid == other.ssid;
        }
        bool operator!=(const STASettingsState& other) const { return !(*this == other); }
    };
    LazyState<STASettingsState> staSettingsState;
    
    struct APSettingsState {
        bool enabled;
        String ssid;      // Configured AP SSID
        
        bool operator==(const APSettingsState& other) const {
            return enabled == other.enabled && ssid == other.ssid;
        }
        bool operator!=(const APSettingsState& other) const { return !(*this == other); }
    };
    LazyState<APSettingsState> apSettingsState;
    
public:
    explicit WifiWebUI(WifiComponent* c) : wifi(c) {
        if (wifi) {
            pendingSsid = wifi->getConfiguredSSID();
            // State will be lazily initialized on first hasDataChanged() call
            // to ensure WiFi component is fully initialized
        }
    }
    
    // Set callback for credential persistence (optional)
    void setCredentialsSaveCallback(std::function<void(const String&, const String&)> callback) {
        onCredentialsSaved = callback;
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
            "wifi_component", "WiFi", "dc-wifi", WebUILocation::ComponentDetail, WebUIPresentation::Card
        }
        .withField(WebUIField("connected", "Connected", WebUIFieldType::Display, wifi->isSTAConnected() ? "Yes" : "No", "", true))
        .withField(WebUIField("ssid_now", "SSID", WebUIFieldType::Display, wifi->getSSID(), "", true))
        .withField(WebUIField("ip", "IP", WebUIFieldType::Display, wifi->getLocalIP(), "", true))
        .withRealTime(2000));

        // Settings controls - STA section
        ctxs.push_back(WebUIContext::settings("wifi_sta_settings", "WiFi Network")
            .withField(WebUIField("ssid", "Network SSID", WebUIFieldType::Text, wifi->getConfiguredSSID()))
            .withField(WebUIField("password", "Password", WebUIFieldType::Text, ""))
            .withField(WebUIField("scan_networks", "Scan Networks", WebUIFieldType::Button, ""))
            .withField(WebUIField("networks", "Available Networks", WebUIFieldType::Display, ""))
            .withField(WebUIField("wifi_enabled", "Enable WiFi", WebUIFieldType::Boolean, wifi->isWifiEnabled() ? "true" : "false"))
            .withAPI("/api/wifi")
            .withRealTime(2000)
        );

        // Settings controls - AP section
        ctxs.push_back(WebUIContext::settings("wifi_ap_settings", "Access Point (AP)")
            .withField(WebUIField("ap_ssid", "AP SSID", WebUIFieldType::Text, wifi->isAPEnabled() ? wifi->getAPSSID() : String("DomoticsCore-AP")))
            .withField(WebUIField("ap_enabled", "Enable AP", WebUIFieldType::Boolean, wifi->isAPEnabled() ? "true" : "false"))
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
            if (field == "ssid") {
                DLOG_D(LOG_WIFI_WEBUI, "Updated SSID to: '%s'", value.c_str());
                pendingSsid = value;
                return "{\"success\":true}";
            } else if (field == "password") {
                DLOG_D(LOG_WIFI_WEBUI, "Updated password (length: %d)", value.length());
                pendingPassword = value;
                return "{\"success\":true}";
            } else if (field == "wifi_enabled") {
                bool enable = (value == "true" || value == "1" || value == "on");
                
                if (enable) {
                    // Enabling: apply pending credentials and connect
                    DLOG_I(LOG_WIFI_WEBUI, "Enabling WiFi with SSID='%s'", pendingSsid.c_str());
                    wifi->setCredentials(pendingSsid, pendingPassword, true);
                    
                    // Invoke persistence callback if set
                    if (onCredentialsSaved) {
                        DLOG_I(LOG_WIFI_WEBUI, "Invoking credentials save callback");
                        onCredentialsSaved(pendingSsid, pendingPassword);
                    } else {
                        DLOG_W(LOG_WIFI_WEBUI, "WARNING: No save callback set!");
                    }
                    
                    // Clear password for safety
                    pendingPassword = "";
                } else {
                    // Disabling: just disable WiFi
                    DLOG_I(LOG_WIFI_WEBUI, "Disabling WiFi");
                    wifi->enableWifi(false);
                }
                
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
            STAComponentState current = {
                wifi->isSTAConnected(),
                wifi->getSSID(),
                wifi->getLocalIP()
            };
            return staComponentState.hasChanged(current);
        }
        else if (contextId == "wifi_sta_settings" || contextId == "wifi_settings") {
            STASettingsState current = {
                wifi->isWifiEnabled(),
                wifi->getConfiguredSSID()
            };
            return staSettingsState.hasChanged(current);
        }
        else if (contextId == "wifi_ap_settings") {
            APSettingsState current = {
                wifi->isAPEnabled(),
                wifi->isAPEnabled() ? wifi->getAPSSID() : String("")
            };
            return apSettingsState.hasChanged(current);
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
