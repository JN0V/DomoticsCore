#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "IComponent.h"
#include "../Logger.h"
#include "../Utils/Timer.h"

namespace DomoticsCore {
namespace Components {

/**
 * WiFi connectivity component
 * Provides WiFi connection management with automatic reconnection
 * Header-only implementation - only compiled when included
 */
class WiFiComponent : public IComponent {
private:
    String ssid;
    String password;
    Utils::NonBlockingDelay reconnectTimer;
    Utils::NonBlockingDelay statusTimer;
    Utils::NonBlockingDelay connectionTimer;
    bool shouldConnect;
    bool isConnecting;
    unsigned long connectionStartTime;
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
          shouldConnect(true), isConnecting(false), connectionStartTime(0) {
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
        // Handle ongoing connection attempt
        if (isConnecting) {
            if (connectionTimer.isReady()) {
                wl_status_t status = WiFi.status();
                
                if (status == WL_CONNECTED) {
                    // Connection successful
                    isConnecting = false;
                    DLOG_I(LOG_CORE, "WiFi connected successfully");
                    DLOG_I(LOG_CORE, "IP Address: %s", WiFi.localIP().toString().c_str());
                    DLOG_I(LOG_CORE, "Signal strength: %d dBm", WiFi.RSSI());
                    setStatus(ComponentStatus::Success);
                } else if (millis() - connectionStartTime > CONNECTION_TIMEOUT) {
                    // Connection timeout
                    isConnecting = false;
                    DLOG_E(LOG_CORE, "WiFi connection timeout - status: %d", status);
                    setStatus(ComponentStatus::NetworkError);
                } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
                    // Connection failed immediately
                    isConnecting = false;
                    DLOG_E(LOG_CORE, "WiFi connection failed - status: %d", status);
                    setStatus(ComponentStatus::NetworkError);
                }
                // Continue waiting for connection...
            }
        }
        
        // Check connection status periodically
        if (statusTimer.isReady()) {
            if (WiFi.status() == WL_CONNECTED) {
                DLOG_D(LOG_CORE, "WiFi connected - IP: %s, RSSI: %d dBm", 
                       WiFi.localIP().toString().c_str(), WiFi.RSSI());
            } else if (shouldConnect && !isConnecting) {
                DLOG_W(LOG_CORE, "WiFi disconnected - status: %d", WiFi.status());
            }
        }
        
        // Handle reconnection
        if (shouldConnect && !isConnecting && WiFi.status() != WL_CONNECTED && reconnectTimer.isReady()) {
            DLOG_I(LOG_CORE, "Attempting WiFi reconnection...");
            startConnection();
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
        return WiFi.status() == WL_CONNECTED;
    }
    
    String getLocalIP() const {
        return WiFi.localIP().toString();
    }
    
    String getSSID() const {
        return WiFi.SSID();
    }
    
    int32_t getRSSI() const {
        return WiFi.RSSI();
    }
    
    String getMacAddress() const {
        return WiFi.macAddress();
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
        String status = "WiFi Status: " + getConnectionStatusString();
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

private:
    ComponentStatus connectToWiFi() {
        if (ssid.isEmpty()) {
            DLOG_E(LOG_CORE, "WiFi SSID not configured");
            return ComponentStatus::ConfigError;
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
    
};

} // namespace Components
} // namespace DomoticsCore
