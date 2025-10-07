#pragma once

/**
 * @file Wifi.h
 * @brief Declares the DomoticsCore WiFi component providing STA/AP management and async scanning.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "INetworkProvider.h"
#include "DomoticsCore/IComponent.h"
#include "DomoticsCore/Logger.h"
#include "DomoticsCore/Timer.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {

/**
 * @class DomoticsCore::Components::WifiComponent
 * @brief Manages WiFi connectivity for ESP32, including STA/AP modes and async scanning.
 *
 * Handles connection lifecycle, reconnection strategies, and exposes helpers for enabling AP,
 * switching credentials, and collecting scan results without blocking the event loop. Can be
 * paired with a WebUI provider to expose runtime settings.
 */
/**
 * WiFi component providing network connectivity
 * 
 * Note: For WiFi-specific checks, use isSTAConnected() and isAPEnabled().
 * The INetworkProvider::isConnected() method is implemented as hasConnectivity()
 * which returns true if either STA or AP mode is active.
 */
class WifiComponent : public IComponent, public INetworkProvider {
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
    // Non-blocking scan state
    bool scanInProgress = false;
    String lastScanSummary_;
    static const unsigned long CONNECTION_TIMEOUT = 15000; // 15 seconds
    
public:
    /**
     * Constructor
     * @param ssid Wifi network name
     * @param password Wifi password
     */
    WifiComponent(const String& ssid, const String& password) 
        : ssid(ssid), password(password), 
          reconnectTimer(5000), statusTimer(30000), connectionTimer(100),
          shouldConnect(true), isConnecting(false), connectionStartTime(0),
          wifiEnabled(true), apEnabled(false) {
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_WIFI, "Initializing...");
        
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false); // We handle reconnection ourselves
        
        // Initialize component metadata
        metadata.name = "Wifi";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "Wifi connectivity management component";
        
        // Define configuration parameters
        config.defineParameter(ConfigParam("ssid", ConfigType::String, true, ssid, "Wifi network name")
                              .length(32));
        config.defineParameter(ConfigParam("password", ConfigType::String, false, "", "Wifi password")
                              .length(64));
        config.defineParameter(ConfigParam("reconnect_interval", ConfigType::Integer, false, "5000", 
                                         "Reconnection attempt interval in ms").min(1000).max(60000));
        config.defineParameter(ConfigParam("connection_timeout", ConfigType::Integer, false, "15000",
                                         "Connection timeout in ms").min(5000).max(60000));
        config.defineParameter(ConfigParam("auto_reconnect", ConfigType::Boolean, false, "true",
                                         "Enable automatic reconnection"));
        
        ComponentStatus status = connectToWifi();
        setStatus(status);
        return status;
    }
    
    void loop() override {
        // Skip Wifi connection logic if in AP mode (empty SSID)
        if (ssid.isEmpty()) {
            return; // AP-only mode handled; flags set in connectToWifi()
        }
        
        // Handle ongoing connection attempt
        if (isConnecting) {
            if (connectionTimer.isReady()) {
                wl_status_t status = WiFi.status();
                
                if (status == WL_CONNECTED) {
                    // Connection successful
                    isConnecting = false;
                    DLOG_I(LOG_WIFI, "Wifi connected successfully");
                    DLOG_I(LOG_WIFI, "IP address: %s", WiFi.localIP().toString().c_str());
                    setStatus(ComponentStatus::Success);
                } else if (millis() - connectionStartTime > CONNECTION_TIMEOUT) {
                    // Connection timeout
                    isConnecting = false;
                    DLOG_E(LOG_WIFI, "Wifi connection timeout - status: %d", status);
                    setStatus(ComponentStatus::TimeoutError);
                }
            }
        }
        
        // Handle reconnection attempts
        if (shouldConnect && !isConnecting && !isSTAConnected() && reconnectTimer.isReady()) {
            DLOG_I(LOG_WIFI, "Attempting Wifi reconnection...");
            startConnection();
        }
        
        // Periodic status updates
        if (statusTimer.isReady()) {
            if (isSTAConnected()) {
                DLOG_D(LOG_WIFI, "Wifi connected - IP: %s, RSSI: %d dBm", 
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
            } else {
                DLOG_D(LOG_WIFI, "Wifi disconnected - status: %s", 
                      getConnectionStatusString().c_str());
            }
        }

        // Poll async scan completion without blocking
        if (scanInProgress) {
            int res = WiFi.scanComplete();
            if (res == WIFI_SCAN_FAILED) {
                DLOG_W(LOG_WIFI, "Wifi async scan failed");
                lastScanSummary_ = "Scan failed";
                scanInProgress = false;
            } else if (res >= 0) {
                String summary;
                for (int i = 0; i < res && i < 10; ++i) {
                    if (i) summary += ", ";
                    summary += WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)";
                }
                lastScanSummary_ = summary;
                WiFi.scanDelete();
                scanInProgress = false;
                DLOG_I(LOG_WIFI, "Async scan complete: %d networks", res);
            }
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_WIFI, "Wifi Shutting down component...");
        shouldConnect = false;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    String getName() const override {
        return "Wifi";
    }
    
    String getVersion() const override {
        return "1.0.0";
    }
    
    // Wifi-specific methods
    
    /**
     * @brief Check if STA (station) mode is connected to a WiFi network
     * @return true if connected as a station to a WiFi network, false otherwise
     * 
     * This checks actual WiFi network connectivity (STA mode).
     */
    bool isSTAConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }
    
    /**
     * @brief Alias for isAPEnabled() for semantic clarity
     * @return true if AP mode is active, false otherwise
     */
    bool isAPConnected() const {
        return isAPEnabled();
    }
    
    /**
     * @brief Check if either STA or AP mode is active
     * @return true if STA connected OR AP enabled, false otherwise
     * 
     * Use this to check if WiFi subsystem has any connectivity.
     * For specific checks, use isSTAConnected() or isAPEnabled().
     */
    bool hasConnectivity() const {
        return isSTAConnected() || isAPEnabled();
    }
    
    // INetworkProvider interface implementation
    /**
     * @brief INetworkProvider interface: Check if network is available
     * @return true if WiFi has any connectivity (STA or AP), false otherwise
     * 
     * Note: For WiFi-specific checks, prefer isSTAConnected() or isAPEnabled().
     * This generic method returns true if WiFi subsystem has any connectivity.
     */
    bool isConnected() const override {
        return hasConnectivity();
    }
    
    String getLocalIP() const override {
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
    // Configured (target) SSID string (not necessarily connected)
    String getConfiguredSSID() const { return ssid; }
    
    int32_t getRSSI() const {
        return WiFi.RSSI();
    }
    
    String getMacAddress() const {
        return WiFi.macAddress();
    }

    // Update credentials and (optionally) start reconnecting
    void setCredentials(const String& newSsid, const String& newPassword, bool reconnectNow = true) {
        ssid = newSsid;
        password = newPassword;
        if (reconnectNow) {
            shouldConnect = true;
            isConnecting = false;
            reconnectTimer.reset();
            startConnection();
        }
    }
    
    // INetworkProvider interface implementation
    String getNetworkType() const override {
        return "Wifi";
    }
    
    String getConnectionStatus() const override {
        return getConnectionStatusString();
    }
    
    String getNetworkInfo() const override {
        JsonDocument info;
        info["type"] = "Wifi";
        info["sta_connected"] = isSTAConnected();
        info["ap_enabled"] = isAPEnabled();
        
        if (isSTAConnected()) {
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
        DLOG_I(LOG_WIFI, "Wifi manually disconnected");
    }
    
    void reconnect() {
        shouldConnect = true;
        reconnectTimer.reset();
        if (!isConnecting) {
            startConnection();
        }
        DLOG_I(LOG_WIFI, "Wifi reconnection requested");
    }
    
    bool isConnectionInProgress() const {
        return isConnecting;
    }
    
    String getDetailedStatus() const {
        String status;
        
        if (isAPMode()) {
            status = "Wifi Status: AP Mode Active";
            status += "\n  AP SSID: " + WiFi.softAPSSID();
            status += "\n  AP IP: " + WiFi.softAPIP().toString();
            status += "\n  Clients: " + String(WiFi.softAPgetStationNum());
            status += "\n  MAC: " + WiFi.macAddress();
        } else {
            status = "Wifi Status: " + getConnectionStatusString();
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
            DLOG_E(LOG_WIFI, "Wifi scan failed");
            return false;
        }
        
        DLOG_I(LOG_WIFI, "Found %d Wifi networks", n);
        for (int i = 0; i < n; i++) {
            String network = WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)";
            networks.push_back(network);
            DLOG_D(LOG_WIFI, "  %s", network.c_str());
        }
        
        return true;
    }

    // Start non-blocking scan (returns immediately)
    void startScanAsync() {
        if (scanInProgress) return;
        WiFi.scanNetworks(true /* async */);
        scanInProgress = true;
        lastScanSummary_ = "Scanning...";
        DLOG_I(LOG_WIFI, "Started async WiFi scan");
    }

    String getLastScanSummary() const { return lastScanSummary_; }
    
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
    
    // Simple Wifi and AP management - hides mode complexity
    bool enableWifi(bool enable = true) {
        wifiEnabled = enable;
        return updateWifiMode();
    }
    
    bool enableAP(const String& apSSID, const String& apPassword = "", bool enable = true) {
        if (enable) {
            apSSID_ = apSSID;
            apPassword_ = apPassword;
            apEnabled = true;
        } else {
            apEnabled = false;
        }
        return updateWifiMode();
    }
    
    bool disableAP() {
        return enableAP("", "", false);
    }
    
    // Status methods that work with the new API
    bool isWifiEnabled() const { return wifiEnabled; }
    bool isAPEnabled() const { return apEnabled; }
    String getAPSSID() const { return apSSID_; }

private:
    ComponentStatus connectToWifi() {
        if (ssid.isEmpty()) {
            DLOG_I(LOG_WIFI, "Wifi SSID not configured - starting in AP mode");
            
            // Generate AP SSID from MAC address for uniqueness
            String macAddress = WiFi.macAddress();
            macAddress.replace(":", "");
            String apSSID = "DomoticsCore-" + macAddress.substring(6); // Last 6 chars of MAC
            
            WiFi.mode(WIFI_AP);
            WiFi.softAP(apSSID.c_str()); // No password for easy access
            DLOG_I(LOG_WIFI, "AP mode started: %s (open network)", apSSID.c_str());
            DLOG_I(LOG_WIFI, "AP IP address: %s", WiFi.softAPIP().toString().c_str());
            // Reflect state in internal flags so UI initial values are correct
            apEnabled = true;
            wifiEnabled = false;
            apSSID_ = apSSID;
            return ComponentStatus::Success;
        }
        
        // Start non-blocking connection
        startConnection();
        
        // Return pending status - actual result will be determined in loop()
        return ComponentStatus::Success;
    }
    
    void startConnection() {
        if (isConnecting) return; // Already connecting
        
        DLOG_I(LOG_WIFI, "Connecting to Wifi: %s", ssid.c_str());
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
    
    // Internal method to update Wifi mode based on enabled features
    bool updateWifiMode() {
        DLOG_I(LOG_WIFI, "Updating Wifi mode - Wifi: %s, AP: %s", 
               wifiEnabled ? "enabled" : "disabled", 
               apEnabled ? "enabled" : "disabled");
        
        if (wifiEnabled && apEnabled) {
            // Both Wifi and AP requested - use STA+AP mode
            DLOG_I(LOG_WIFI, "Enabling STA+AP mode");
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
                DLOG_I(LOG_WIFI, "AP started: %s (IP: %s)", apSSID_.c_str(), WiFi.softAPIP().toString().c_str());
            }
            
            // Enable station connection attempts
            shouldConnect = true;
            reconnectTimer.reset();
            
            return apSuccess;
        } else if (wifiEnabled && !apEnabled) {
            // Only Wifi requested - use STA mode
            DLOG_I(LOG_WIFI, "Enabling station mode only");
            WiFi.softAPdisconnect(true);
            delay(100);
            WiFi.mode(WIFI_STA);
            delay(100);
            shouldConnect = true;
            reconnectTimer.reset();
            return true;
        } else if (!wifiEnabled && apEnabled) {
            // Only AP requested - use AP mode
            DLOG_I(LOG_WIFI, "Enabling AP mode only");
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
                DLOG_I(LOG_WIFI, "AP-only mode started: %s (IP: %s)", apSSID_.c_str(), WiFi.softAPIP().toString().c_str());
            }
            
            return success;
        } else {
            // Both disabled - turn off Wifi
            DLOG_I(LOG_WIFI, "Disabling all Wifi features");
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
