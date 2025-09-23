#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "IComponent.h"
#include "INetworkProvider.h"
#include "WebUIHelpers.h"
#include "../Logger.h"
#include "../Utils/Timer.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {

/**
 * WiFi connectivity component
 * Provides WiFi connection management with automatic reconnection
 * Header-only implementation - only compiled when included
 */
class WiFiComponent : public IComponent, public INetworkProvider {
private:
    String ssid;
    String password;
    Utils::NonBlockingDelay reconnectTimer;
    Utils::NonBlockingDelay statusTimer;
    Utils::NonBlockingDelay connectionTimer;
    
    bool shouldConnect;
    bool isConnecting;
    unsigned long connectionStartTime;
    
    // New API state
    bool wifiEnabled;
    bool apEnabled;
    String apSSID_;
    String apPassword_;
    static const unsigned long CONNECTION_TIMEOUT = 15000; // 15 seconds
    
public:
    /**
     * Constructor
     * @param ssid WiFi network name
     * @param password WiFi password
     */
    WiFiComponent(const String& ssid, const String& password) 
        : ssid(ssid), password(password), 
          reconnectTimer(5000), statusTimer(30000), connectionTimer(100),
          shouldConnect(true), isConnecting(false), connectionStartTime(0),
          wifiEnabled(true), apEnabled(false) {
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_CORE, "WiFi component initializing...");
        
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false); // We handle reconnection ourselves
        
        // Initialize component metadata
        metadata.name = "WiFi";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "WiFi connectivity management component";
        
        // Define configuration parameters
        config.defineParameter(ConfigParam("ssid", ConfigType::String, true, ssid, "WiFi network name")
                              .length(32));
        config.defineParameter(ConfigParam("password", ConfigType::String, false, "", "WiFi password")
                              .length(64));
        config.defineParameter(ConfigParam("reconnect_interval", ConfigType::Integer, false, "5000", 
                                         "Reconnection attempt interval in ms").min(1000).max(60000));
        config.defineParameter(ConfigParam("connection_timeout", ConfigType::Integer, false, "15000",
                                         "Connection timeout in ms").min(5000).max(60000));
        config.defineParameter(ConfigParam("auto_reconnect", ConfigType::Boolean, false, "true",
                                         "Enable automatic reconnection"));
        
        ComponentStatus status = connectToWiFi();
        setStatus(status);
        return status;
    }
    
    void loop() override {
        // Skip WiFi connection logic if in AP mode (empty SSID)
        if (ssid.isEmpty()) {
            return; // AP mode - no connection attempts needed
        }
        
        // Handle ongoing connection attempt
        if (isConnecting) {
            if (connectionTimer.isReady()) {
                wl_status_t status = WiFi.status();
                
                if (status == WL_CONNECTED) {
                    // Connection successful
                    isConnecting = false;
                    DLOG_I(LOG_CORE, "WiFi connected successfully");
                    DLOG_I(LOG_CORE, "IP address: %s", WiFi.localIP().toString().c_str());
                    setStatus(ComponentStatus::Success);
                } else if (millis() - connectionStartTime > CONNECTION_TIMEOUT) {
                    // Connection timeout
                    isConnecting = false;
                    DLOG_E(LOG_CORE, "WiFi connection timeout - status: %d", status);
                    setStatus(ComponentStatus::TimeoutError);
                }
            }
        }
        
        // Handle reconnection attempts
        if (shouldConnect && !isConnecting && !isConnected() && reconnectTimer.isReady()) {
            DLOG_I(LOG_CORE, "Attempting WiFi reconnection...");
            startConnection();
        }
        
        // Periodic status updates
        if (statusTimer.isReady()) {
            if (isConnected()) {
                DLOG_D(LOG_CORE, "WiFi connected - IP: %s, RSSI: %d dBm", 
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
            } else {
                DLOG_D(LOG_CORE, "WiFi disconnected - status: %s", 
                      getConnectionStatusString().c_str());
            }
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_CORE, "WiFi component shutting down...");
        shouldConnect = false;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    String getName() const override {
        return "WiFi";
    }
    
    String getVersion() const override {
        return "1.0.0";
    }
    
    // WiFi-specific methods
    bool isConnected() const {
        // In AP mode, consider "connected" if AP is active
        if (isAPMode()) {
            return true; // AP mode is considered "connected" for status purposes
        }
        return WiFi.status() == WL_CONNECTED;
    }
    
    String getLocalIP() const {
        // In STA+AP mode, prioritize station IP for connectivity
        if (isSTAAPMode() && WiFi.status() == WL_CONNECTED) {
            return WiFi.localIP().toString();
        }
        // In AP-only mode, return AP IP
        else if (isAPMode()) {
            return WiFi.softAPIP().toString();
        }
        // In station mode, return station IP
        return WiFi.localIP().toString();
    }
    
    String getSSID() const {
        // In STA+AP mode, prioritize station SSID for connectivity
        if (isSTAAPMode() && WiFi.status() == WL_CONNECTED) {
            return WiFi.SSID();
        }
        // In AP-only mode, return AP SSID
        else if (isAPMode()) {
            return WiFi.softAPSSID();
        }
        // In station mode, return station SSID
        return WiFi.SSID();
    }
    
    int32_t getRSSI() const {
        return WiFi.RSSI();
    }
    
    String getMacAddress() const {
        return WiFi.macAddress();
    }
    
    // INetworkProvider interface implementation
    String getNetworkType() const override {
        return "WiFi";
    }
    
    String getConnectionStatus() const override {
        return getConnectionStatusString();
    }
    
    String getNetworkInfo() const override {
        JsonDocument info;
        info["type"] = "WiFi";
        info["connected"] = isConnected();
        
        if (isConnected()) {
            info["ssid"] = getSSID();
            info["ip_address"] = getLocalIP();
            info["signal_strength"] = getRSSI();
            info["mac_address"] = getMacAddress();
        }
        
        // AP mode info
        bool apMode = (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA);
        info["ap_mode"] = apMode;
        if (apMode) {
            info["ap_ssid"] = WiFi.softAPSSID();
            info["ap_ip"] = WiFi.softAPIP().toString();
        }
        
        String result;
        serializeJson(info, result);
        return result;
    }
    
    void disconnect() {
        shouldConnect = false;
        WiFi.disconnect();
        DLOG_I(LOG_CORE, "WiFi manually disconnected");
    }
    
    void reconnect() {
        shouldConnect = true;
        reconnectTimer.reset();
        if (!isConnecting) {
            startConnection();
        }
        DLOG_I(LOG_CORE, "WiFi reconnection requested");
    }
    
    bool isConnectionInProgress() const {
        return isConnecting;
    }
    
    String getDetailedStatus() const {
        String status;
        
        if (isAPMode()) {
            status = "WiFi Status: AP Mode Active";
            status += "\n  AP SSID: " + WiFi.softAPSSID();
            status += "\n  AP IP: " + WiFi.softAPIP().toString();
            status += "\n  Clients: " + String(WiFi.softAPgetStationNum());
            status += "\n  MAC: " + WiFi.macAddress();
        } else {
            status = "WiFi Status: " + getConnectionStatusString();
            if (WiFi.status() == WL_CONNECTED) {
                status += "\n  IP: " + WiFi.localIP().toString();
                status += "\n  SSID: " + WiFi.SSID();
                status += "\n  RSSI: " + String(WiFi.RSSI()) + " dBm";
                status += "\n  MAC: " + WiFi.macAddress();
            }
            if (isConnecting) {
                unsigned long elapsed = millis() - connectionStartTime;
                status += "\n  Connecting... (" + String(elapsed / 1000) + "s)";
            }
        }
        
        return status;
    }
    
    bool scanNetworks(std::vector<String>& networks) {
        int n = WiFi.scanNetworks();
        networks.clear();
        
        if (n == -1) {
            DLOG_E(LOG_CORE, "WiFi scan failed");
            return false;
        }
        
        DLOG_I(LOG_CORE, "Found %d WiFi networks", n);
        for (int i = 0; i < n; i++) {
            String network = WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)";
            networks.push_back(network);
            DLOG_D(LOG_CORE, "  %s", network.c_str());
        }
        
        return true;
    }
    
    bool isSTAAPMode() const {
        return WiFi.getMode() == WIFI_AP_STA;
    }
    
    /**
     * Check if currently in AP mode
     * @return true if in AP mode
     */
    bool isAPMode() const {
        wifi_mode_t mode = WiFi.getMode();
        return (mode == WIFI_AP || mode == WIFI_AP_STA);
    }
    
    /**
     * Get AP mode information
     * @return JSON string with AP details
     */
    String getAPInfo() const {
        JsonDocument info;
        
        if (isAPMode()) {
            info["active"] = true;
            info["ssid"] = WiFi.softAPSSID();
            info["ip"] = WiFi.softAPIP().toString();
            info["clients"] = WiFi.softAPgetStationNum();
        } else {
            info["active"] = false;
        }
        
        String result;
        serializeJson(info, result);
        return result;
    }
    
    // Simple WiFi and AP management - hides mode complexity
    bool enableWiFi(bool enable = true) {
        wifiEnabled = enable;
        return updateWiFiMode();
    }
    
    bool enableAP(const String& apSSID, const String& apPassword = "", bool enable = true) {
        if (enable) {
            apSSID_ = apSSID;
            apPassword_ = apPassword;
            apEnabled = true;
        } else {
            apEnabled = false;
        }
        return updateWiFiMode();
    }
    
    bool disableAP() {
        return enableAP("", "", false);
    }
    
    // Status methods that work with the new API
    bool isWiFiEnabled() const { return wifiEnabled; }
    bool isAPEnabled() const { return apEnabled; }
    String getAPSSID() const { return apSSID_; }

private:
    ComponentStatus connectToWiFi() {
        if (ssid.isEmpty()) {
            DLOG_I(LOG_CORE, "WiFi SSID not configured - starting in AP mode");
            
            // Generate AP SSID from MAC address for uniqueness
            String macAddress = WiFi.macAddress();
            macAddress.replace(":", "");
            String apSSID = "DomoticsCore-" + macAddress.substring(6); // Last 6 chars of MAC
            
            WiFi.mode(WIFI_AP);
            WiFi.softAP(apSSID.c_str()); // No password for easy access
            DLOG_I(LOG_CORE, "AP mode started: %s (open network)", apSSID.c_str());
            DLOG_I(LOG_CORE, "AP IP address: %s", WiFi.softAPIP().toString().c_str());
            return ComponentStatus::Success;
        }
        
        // Start non-blocking connection
        startConnection();
        
        // Return pending status - actual result will be determined in loop()
        return ComponentStatus::Success;
    }
    
    void startConnection() {
        if (isConnecting) return; // Already connecting
        
        DLOG_I(LOG_CORE, "Connecting to WiFi: %s", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        
        isConnecting = true;
        connectionStartTime = millis();
        connectionTimer.reset();
    }
    
    // Additional utility methods
    String getConnectionStatusString() const {
        switch (WiFi.status()) {
            case WL_IDLE_STATUS: return "Idle";
            case WL_NO_SSID_AVAIL: return "SSID not available";
            case WL_SCAN_COMPLETED: return "Scan completed";
            case WL_CONNECTED: return "Connected";
            case WL_CONNECT_FAILED: return "Connection failed";
            case WL_CONNECTION_LOST: return "Connection lost";
            case WL_DISCONNECTED: return "Disconnected";
            default: return "Unknown (" + String(WiFi.status()) + ")";
        }
    }
    
    // Internal method to update WiFi mode based on enabled features
    bool updateWiFiMode() {
        DLOG_I(LOG_CORE, "Updating WiFi mode - WiFi: %s, AP: %s", 
               wifiEnabled ? "enabled" : "disabled", 
               apEnabled ? "enabled" : "disabled");
        
        if (wifiEnabled && apEnabled) {
            // Both WiFi and AP requested - use STA+AP mode
            DLOG_I(LOG_CORE, "Enabling STA+AP mode");
            WiFi.mode(WIFI_AP_STA);
            delay(100);
            
            // Start AP
            bool apSuccess;
            if (apPassword_.isEmpty()) {
                apSuccess = WiFi.softAP(apSSID_.c_str());
            } else {
                apSuccess = WiFi.softAP(apSSID_.c_str(), apPassword_.c_str());
            }
            
            if (apSuccess) {
                DLOG_I(LOG_CORE, "AP started: %s (IP: %s)", apSSID_.c_str(), WiFi.softAPIP().toString().c_str());
            }
            
            // Enable station connection attempts
            shouldConnect = true;
            reconnectTimer.reset();
            
            return apSuccess;
        } else if (wifiEnabled && !apEnabled) {
            // Only WiFi requested - use STA mode
            DLOG_I(LOG_CORE, "Enabling station mode only");
            WiFi.softAPdisconnect(true);
            delay(100);
            WiFi.mode(WIFI_STA);
            delay(100);
            shouldConnect = true;
            reconnectTimer.reset();
            return true;
        } else if (!wifiEnabled && apEnabled) {
            // Only AP requested - use AP mode
            DLOG_I(LOG_CORE, "Enabling AP mode only");
            shouldConnect = false;
            isConnecting = false;
            WiFi.disconnect();
            WiFi.mode(WIFI_AP);
            delay(100);
            
            bool success;
            if (apPassword_.isEmpty()) {
                success = WiFi.softAP(apSSID_.c_str());
            } else {
                success = WiFi.softAP(apSSID_.c_str(), apPassword_.c_str());
            }
            
            if (success) {
                DLOG_I(LOG_CORE, "AP-only mode started: %s (IP: %s)", apSSID_.c_str(), WiFi.softAPIP().toString().c_str());
            }
            
            return success;
        } else {
            // Both disabled - turn off WiFi
            DLOG_I(LOG_CORE, "Disabling all WiFi features");
            shouldConnect = false;
            isConnecting = false;
            WiFi.softAPdisconnect(true);
            WiFi.disconnect();
            WiFi.mode(WIFI_OFF);
            return true;
        }
    }

};

} // namespace Components
} // namespace DomoticsCore
