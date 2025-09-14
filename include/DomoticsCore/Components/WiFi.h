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
    bool shouldConnect;
    
public:
    /**
     * Constructor
     * @param ssid WiFi network name
     * @param password WiFi password
     */
    WiFiComponent(const String& ssid, const String& password) 
        : ssid(ssid), password(password), 
          reconnectTimer(5000), statusTimer(30000), shouldConnect(true) {
    }
    
    bool begin() override {
        DLOG_I(LOG_CORE, "WiFi component initializing...");
        
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false); // We handle reconnection ourselves
        
        return connectToWiFi();
    }
    
    void loop() override {
        // Check connection status periodically
        if (statusTimer.isReady()) {
            if (WiFi.status() == WL_CONNECTED) {
                DLOG_D(LOG_CORE, "WiFi connected - IP: %s, RSSI: %d dBm", 
                       WiFi.localIP().toString().c_str(), WiFi.RSSI());
            } else if (shouldConnect) {
                DLOG_W(LOG_CORE, "WiFi disconnected - status: %d", WiFi.status());
            }
        }
        
        // Handle reconnection
        if (shouldConnect && WiFi.status() != WL_CONNECTED && reconnectTimer.isReady()) {
            DLOG_I(LOG_CORE, "Attempting WiFi reconnection...");
            connectToWiFi();
        }
    }
    
    void shutdown() override {
        DLOG_I(LOG_CORE, "WiFi component shutting down...");
        shouldConnect = false;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
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
        DLOG_I(LOG_CORE, "WiFi reconnection requested");
    }

private:
    bool connectToWiFi() {
        if (ssid.isEmpty()) {
            DLOG_E(LOG_CORE, "WiFi SSID not configured");
            return false;
        }
        
        DLOG_I(LOG_CORE, "Connecting to WiFi: %s", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        
        // Wait up to 10 seconds for connection
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
            delay(100);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            DLOG_I(LOG_CORE, "WiFi connected successfully");
            DLOG_I(LOG_CORE, "IP Address: %s", WiFi.localIP().toString().c_str());
            DLOG_I(LOG_CORE, "Signal strength: %d dBm", WiFi.RSSI());
            return true;
        } else {
            DLOG_E(LOG_CORE, "WiFi connection failed - status: %d", WiFi.status());
            return false;
        }
    }
};

} // namespace Components
} // namespace DomoticsCore
