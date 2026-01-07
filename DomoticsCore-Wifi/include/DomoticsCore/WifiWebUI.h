#pragma once

#include "DomoticsCore/Wifi.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/Logger.h"
#include <functional>

#define LOG_WIFI_WEBUI "WIFI_WEBUI"

namespace DomoticsCore {
namespace Components {

namespace WebUI {

class WifiWebUI : public CachingWebUIProvider {
    WifiComponent* wifi; // non-owning
    std::function<void(const Components::WifiConfig&)> onConfigChanged; // unified callback for config persistence
    // Keep pending credentials updated from UI
    String pendingSsid;
    String pendingPassword;
    // Pending AP SSID typed while AP is disabled (applied on next enable)
    String pendingApSsid;
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

    // Set callback for config persistence (optional) - unified callback
    void setConfigSaveCallback(std::function<void(const Components::WifiConfig&)> callback) {
        onConfigChanged = callback;
    }

    String getWebUIName() const override { return wifi ? wifi->metadata.name : String("Wifi"); }
    String getWebUIVersion() const override { return wifi ? wifi->metadata.version : String("1.4.0"); }

protected:
    // CachingWebUIProvider: build contexts once, they're cached
    // OPTIMIZED for ESP8266: reduced from 5 to 3 contexts
    void buildContexts(std::vector<WebUIContext>& ctxs) override {
        DLOG_I(LOG_WIFI_WEBUI, "Building WiFi WebUI contexts, wifi=%p", (void*)wifi);
        if (!wifi) {
            DLOG_W(LOG_WIFI_WEBUI, "WiFi pointer is null, skipping context build");
            return;
        }

        // Single header badge showing network status (STA or AP)
        ctxs.push_back(WebUIContext::statusBadge("wifi_status", "Network", "dc-wifi").withRealTime(2000));

        // Components tab - compact status display
        ctxs.push_back(WebUIContext{
            "wifi_component", "WiFi", "dc-wifi", WebUILocation::ComponentDetail, WebUIPresentation::Card
        }
        .withField(WebUIField("mode", "Mode", WebUIFieldType::Display, "AP", "", true))
        .withField(WebUIField("ssid_now", "Network", WebUIFieldType::Display, "", "", true))
        .withField(WebUIField("ip", "IP", WebUIFieldType::Display, "0.0.0.0", "", true))
        .withRealTime(2000));

        // Unified settings - STA and AP in one card with clear sections
        ctxs.push_back(WebUIContext::settings("wifi_settings", "WiFi Configuration")
            // STA Mode
            .withField(WebUIField("wifi_enabled", "Connect to Network", WebUIFieldType::Boolean, "false"))
            .withField(WebUIField("ssid", "Network SSID", WebUIFieldType::Text, ""))
            .withField(WebUIField("sta_password", "Password", WebUIFieldType::Password, ""))
            // AP Mode  
            .withField(WebUIField("ap_enabled", "Enable Access Point", WebUIFieldType::Boolean, "true"))
            .withField(WebUIField("ap_ssid", "AP Name", WebUIFieldType::Text, "DomoticsCore-AP"))
            .withAPI("/api/wifi")
            .withRealTime(5000)
        );
    }

public:

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
            } else if (field == "sta_password") {
                DLOG_D(LOG_WIFI_WEBUI, "Updated password (length: %d)", value.length());
                pendingPassword = value;
                return "{\"success\":true}";
            } else if (field == "wifi_enabled") {
                bool enable = (value == "true" || value == "1" || value == "on");
                
                // Use Get → Override → Set pattern
                Components::WifiConfig cfg = wifi->getConfig();
                
                if (enable) {
                    // Validate: cannot enable STA without SSID
                    if (pendingSsid.isEmpty()) {
                        DLOG_W(LOG_WIFI_WEBUI, "Cannot enable WiFi: no SSID configured");
                        // Force UI refresh to revert toggle to actual state
                        staSettingsState.reset();
                        return "{\"success\":false,\"error\":\"SSID required\",\"refresh\":true}";
                    }
                    
                    // Enabling: apply pending credentials and connect
                    DLOG_I(LOG_WIFI_WEBUI, "Enabling WiFi with SSID='%s'", pendingSsid.c_str());
                    
                    // Override config with new values
                    cfg.ssid = pendingSsid;
                    cfg.password = pendingPassword;
                    cfg.autoConnect = true;
                    
                    // Apply config
                    wifi->setConfig(cfg);
                    wifi->updateWifiMode();
                    
                    // Invoke persistence callback if set (unified callback)
                    if (onConfigChanged) {
                        DLOG_I(LOG_WIFI_WEBUI, "Invoking config save callback");
                        onConfigChanged(cfg);
                    } else {
                        DLOG_W(LOG_WIFI_WEBUI, "WARNING: No save callback set!");
                    }
                    
                    // Note: WebSocket notification removed - clients will get updates via periodic refresh
                    // Calling notifyWiFiNetworkChanged() during mode transition can crash ESP8266
                    
                    // Clear password for safety
                    pendingPassword = "";
                    // Force next delta update for header badges and cards
                    wifiStatusState.reset();
                    apStatusState.reset();
                    staComponentState.reset();
                    staSettingsState.reset();
                } else {
                    // Disabling: just disable WiFi
                    DLOG_I(LOG_WIFI_WEBUI, "Disabling WiFi");
                    
                    // Override config
                    cfg.autoConnect = false;
                    
                    // Apply config
                    wifi->setConfig(cfg);
                    wifi->updateWifiMode();
                    
                    // Invoke persistence callback if set (unified callback)
                    if (onConfigChanged) {
                        DLOG_I(LOG_WIFI_WEBUI, "Invoking config save callback");
                        onConfigChanged(cfg);
                    }
                    
                    // Force next delta update for header badges and cards
                    wifiStatusState.reset();
                    apStatusState.reset();
                    staComponentState.reset();
                    staSettingsState.reset();
                }
                
                return "{\"success\":true}";
            } else if (field == "scan_networks") {
                wifi->startScanAsync();
                lastScanSummary = "Scanning...";
                return "{\"success\":true}";
            } else if (field == "ap_enabled") {
                // AP fields now in unified wifi_settings context
                bool en = (value == "true" || value == "1" || value == "on");
                
                // Use Get → Override → Set pattern
                Components::WifiConfig cfg = wifi->getConfig();
                
                if (en) {
                    // Use pending AP SSID if set; otherwise current configured AP SSID; else default
                    String apName = pendingApSsid.length() ? pendingApSsid : (cfg.apSSID.length() ? cfg.apSSID : String("DomoticsCore-AP"));
                    cfg.enableAP = true;
                    cfg.apSSID = apName;
                    // Clear pending once applied
                    pendingApSsid = "";
                } else {
                    cfg.enableAP = false;
                    // When disabling AP, apply pending STA credentials if available
                    if (pendingSsid.length()) {
                        cfg.ssid = pendingSsid;
                        cfg.password = pendingPassword;
                        cfg.autoConnect = true;
                        DLOG_I(LOG_WIFI_WEBUI, "Applying pending STA credentials: SSID='%s'", pendingSsid.c_str());
                    }
                }
                
                // Apply config and update mode
                wifi->setConfig(cfg);
                wifi->updateWifiMode();
                
                // Invoke persistence callback if set
                if (onConfigChanged) {
                    onConfigChanged(cfg);
                }
                
                // Force next delta update for header badges and cards
                wifiStatusState.reset();
                apStatusState.reset();
                staComponentState.reset();
                apSettingsState.reset();
                
                return "{\"success\":true}";
            } else if (field == "ap_ssid") {
                // Update AP SSID immediately if AP enabled
                String newAp = value;
                if (newAp.isEmpty()) newAp = "DomoticsCore-AP";
                
                if (wifi->isAPEnabled()) {
                    // Use Get → Override → Set pattern
                    Components::WifiConfig cfg = wifi->getConfig();
                    cfg.apSSID = newAp;
                    wifi->setConfig(cfg);
                    wifi->updateWifiMode();
                    
                    // Invoke persistence callback if set (unified callback)
                    if (onConfigChanged) {
                        DLOG_I(LOG_WIFI_WEBUI, "Invoking config save callback");
                        onConfigChanged(cfg);
                    }
                } else {
                    // Store for when AP gets enabled later
                    pendingApSsid = newAp;
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
            // Determine mode: STA, AP, or STA+AP
            bool sta = wifi->isSTAConnected();
            bool ap = wifi->isAPEnabled();
            doc["mode"] = sta && ap ? "STA+AP" : (sta ? "STA" : (ap ? "AP" : "Off"));
            doc["ssid_now"] = sta ? wifi->getSSID() : wifi->getAPSSID();
            doc["ip"] = wifi->getLocalIP();
            String json; serializeJson(doc, json); return json;
        }
        if (contextId == "wifi_settings" || contextId == "wifi_sta_settings") {
            JsonDocument doc;
            // STA fields
            doc["wifi_enabled"] = wifi->isWifiEnabled() ? "true" : "false";
            doc["ssid"] = pendingSsid.length() ? pendingSsid : wifi->getConfiguredSSID();
            doc["sta_password"] = ""; // never echo back
            // AP fields
            doc["ap_enabled"] = wifi->isAPEnabled() ? "true" : "false";
            doc["ap_ssid"] = wifi->getAPSSID().length() ? wifi->getAPSSID() : (pendingApSsid.length() ? pendingApSsid : String("DomoticsCore-AP"));
            String json; serializeJson(doc, json); return json;
        }
        if (contextId == "wifi_status") {
            JsonDocument doc;
            bool sta = wifi->isSTAConnected();
            bool ap = wifi->isAPEnabled();
            // State for badge color
            doc["state"] = (sta || ap) ? "ON" : "OFF";
            // Icon for dynamic switching (uses existing CSS icons)
            doc["icon"] = sta && ap ? "dc-wifi-both" : (sta ? "dc-wifi" : (ap ? "dc-wifi-ap" : "dc-wifi-off"));
            // Tooltip: mode + network name with fallbacks
            if (sta && ap) {
                String ssid = wifi->getSSID();
                doc["tooltip"] = ssid.length() ? ssid.c_str() : "WiFi+AP";
            } else if (sta) {
                String ssid = wifi->getSSID();
                doc["tooltip"] = ssid.length() ? ssid.c_str() : "Connected";
            } else if (ap) {
                String apSsid = wifi->getAPSSID();
                doc["tooltip"] = apSsid.length() ? apSsid.c_str() : "AP Mode";
            } else {
                doc["tooltip"] = "Off";
            }
            String json; serializeJson(doc, json); return json;
        }
        return "{}";
    }
    
    bool hasDataChanged(const String& contextId) override {
        if (!wifi) return false;
        
        // Use LazyState helper for timing-independent change tracking
        if (contextId == "wifi_status") {
            // Track both STA and AP status changes
            bool current = wifi->isSTAConnected() || wifi->isAPEnabled();
            return wifiStatusState.hasChanged(current);
        }
        else if (contextId == "wifi_component") {
            STAComponentState current = {
                wifi->isSTAConnected(),
                wifi->getSSID(),
                wifi->getLocalIP()
            };
            return staComponentState.hasChanged(current);
        }
        else if (contextId == "wifi_settings" || contextId == "wifi_sta_settings") {
            // Unified settings - track both STA and AP changes
            STASettingsState current = {
                wifi->isWifiEnabled(),
                wifi->getConfiguredSSID()
            };
            bool staChanged = staSettingsState.hasChanged(current);
            APSettingsState apCurrent = {
                wifi->isAPEnabled(),
                wifi->getAPSSID()
            };
            bool apChanged = apSettingsState.hasChanged(apCurrent);
            return staChanged || apChanged;
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
