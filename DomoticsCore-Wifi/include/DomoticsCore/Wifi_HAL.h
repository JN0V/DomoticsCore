#ifndef DOMOTICS_CORE_HAL_WIFI_H
#define DOMOTICS_CORE_HAL_WIFI_H

/**
 * @file Wifi_HAL.h
 * @brief WiFi Hardware Abstraction Layer.
 * 
 * Provides a unified WiFi interface across platforms:
 * - ESP32: Uses WiFi.h (ESP-IDF based)
 * - ESP8266: Uses ESP8266WiFi.h
 * - Other platforms: Stub (no WiFi)
 */

#include "DomoticsCore/Platform_HAL.h"
#include <Arduino.h>

#if DOMOTICS_PLATFORM_ESP32
    #include <WiFi.h>
#elif DOMOTICS_PLATFORM_ESP8266
    #include <ESP8266WiFi.h>
#endif

namespace DomoticsCore {
namespace HAL {
namespace WiFiHAL {

/**
 * @brief WiFi connection status
 */
enum class Status {
    Disconnected,
    Connecting,
    Connected,
    ConnectionFailed,
    NotSupported
};

/**
 * @brief WiFi mode
 */
enum class Mode {
    Off,
    Station,      // Client mode
    AccessPoint,  // AP mode
    StationAndAP  // Dual mode
};

/**
 * @brief Check if WiFi is supported on this platform
 */
inline bool isSupported() {
#if DOMOTICS_HAS_WIFI
    return true;
#else
    return false;
#endif
}

/**
 * @brief Initialize WiFi subsystem
 */
inline void init() {
#if DOMOTICS_PLATFORM_ESP32
    WiFi.mode(WIFI_MODE_NULL);  // Start with WiFi off
#elif DOMOTICS_PLATFORM_ESP8266
    WiFi.mode(WIFI_OFF);
#endif
}

/**
 * @brief Set WiFi mode
 */
inline void setMode(Mode mode) {
#if DOMOTICS_PLATFORM_ESP32
    switch (mode) {
        case Mode::Off:         WiFi.mode(WIFI_OFF); break;
        case Mode::Station:     WiFi.mode(WIFI_STA); break;
        case Mode::AccessPoint: WiFi.mode(WIFI_AP); break;
        case Mode::StationAndAP: WiFi.mode(WIFI_AP_STA); break;
    }
#elif DOMOTICS_PLATFORM_ESP8266
    switch (mode) {
        case Mode::Off:         WiFi.mode(WIFI_OFF); break;
        case Mode::Station:     WiFi.mode(WIFI_STA); break;
        case Mode::AccessPoint: WiFi.mode(WIFI_AP); break;
        case Mode::StationAndAP: WiFi.mode(WIFI_AP_STA); break;
    }
#else
    (void)mode;
#endif
}

/**
 * @brief Connect to a WiFi network (station mode)
 */
inline void connect(const char* ssid, const char* password = nullptr) {
#if DOMOTICS_HAS_WIFI
    if (password && strlen(password) > 0) {
        WiFi.begin(ssid, password);
    } else {
        WiFi.begin(ssid);
    }
#else
    (void)ssid;
    (void)password;
#endif
}

/**
 * @brief Disconnect from WiFi
 */
inline void disconnect() {
#if DOMOTICS_HAS_WIFI
    WiFi.disconnect();
#endif
}

/**
 * @brief Start Access Point
 */
inline bool startAP(const char* ssid, const char* password = nullptr) {
#if DOMOTICS_HAS_WIFI
    if (password && strlen(password) > 0) {
        return WiFi.softAP(ssid, password);
    } else {
        return WiFi.softAP(ssid);
    }
#else
    (void)ssid;
    (void)password;
    return false;
#endif
}

/**
 * @brief Stop Access Point
 */
inline void stopAP() {
#if DOMOTICS_PLATFORM_ESP32
    WiFi.softAPdisconnect(true);
#elif DOMOTICS_PLATFORM_ESP8266
    WiFi.softAPdisconnect(true);
#endif
}

/**
 * @brief Get current connection status
 */
inline Status getStatus() {
#if DOMOTICS_HAS_WIFI
    switch (WiFi.status()) {
        case WL_CONNECTED:      return Status::Connected;
        case WL_DISCONNECTED:   return Status::Disconnected;
        case WL_CONNECT_FAILED: return Status::ConnectionFailed;
        case WL_IDLE_STATUS:    
        default:                return Status::Connecting;
    }
#else
    return Status::NotSupported;
#endif
}

/**
 * @brief Check if connected to WiFi (station mode)
 */
inline bool isConnected() {
#if DOMOTICS_HAS_WIFI
    return WiFi.status() == WL_CONNECTED;
#else
    return false;
#endif
}

/**
 * @brief Get local IP address (station mode)
 */
inline String getLocalIP() {
#if DOMOTICS_HAS_WIFI
    return WiFi.localIP().toString();
#else
    return "0.0.0.0";
#endif
}

/**
 * @brief Get AP IP address
 */
inline String getAPIP() {
#if DOMOTICS_HAS_WIFI
    return WiFi.softAPIP().toString();
#else
    return "0.0.0.0";
#endif
}

/**
 * @brief Get connected SSID
 */
inline String getSSID() {
#if DOMOTICS_HAS_WIFI
    return WiFi.SSID();
#else
    return "";
#endif
}

/**
 * @brief Get signal strength (RSSI)
 */
inline int32_t getRSSI() {
#if DOMOTICS_HAS_WIFI
    return WiFi.RSSI();
#else
    return 0;
#endif
}

/**
 * @brief Get MAC address
 */
inline String getMacAddress() {
#if DOMOTICS_HAS_WIFI
    return WiFi.macAddress();
#else
    return "00:00:00:00:00:00";
#endif
}

/**
 * @brief Set hostname
 */
inline void setHostname(const char* hostname) {
#if DOMOTICS_PLATFORM_ESP32
    WiFi.setHostname(hostname);
#elif DOMOTICS_PLATFORM_ESP8266
    WiFi.hostname(hostname);
#else
    (void)hostname;
#endif
}

/**
 * @brief Enable auto-reconnect
 */
inline void setAutoReconnect(bool enabled) {
#if DOMOTICS_HAS_WIFI
    WiFi.setAutoReconnect(enabled);
#else
    (void)enabled;
#endif
}

/**
 * @brief Scan for available networks
 * @return Number of networks found, or -1 for async scan in progress
 */
inline int16_t scanNetworks(bool async = false) {
#if DOMOTICS_HAS_WIFI
    return WiFi.scanNetworks(async);
#else
    (void)async;
    return 0;
#endif
}

/**
 * @brief Get scanned network SSID by index
 */
inline String getScannedSSID(uint8_t index) {
#if DOMOTICS_HAS_WIFI
    return WiFi.SSID(index);
#else
    (void)index;
    return "";
#endif
}

/**
 * @brief Get scanned network RSSI by index
 */
inline int32_t getScannedRSSI(uint8_t index) {
#if DOMOTICS_HAS_WIFI
    return WiFi.RSSI(index);
#else
    (void)index;
    return 0;
#endif
}

/**
 * @brief Get current WiFi mode
 */
inline Mode getMode() {
#if DOMOTICS_HAS_WIFI
    wifi_mode_t mode = WiFi.getMode();
    switch (mode) {
        case WIFI_OFF:    return Mode::Off;
        case WIFI_STA:    return Mode::Station;
        case WIFI_AP:     return Mode::AccessPoint;
        case WIFI_AP_STA: return Mode::StationAndAP;
        default:          return Mode::Off;
    }
#else
    return Mode::Off;
#endif
}

/**
 * @brief Get AP SSID
 */
inline String getAPSSID() {
#if DOMOTICS_HAS_WIFI
    return WiFi.softAPSSID();
#else
    return "";
#endif
}

/**
 * @brief Get number of stations connected to AP
 */
inline uint8_t getAPStationCount() {
#if DOMOTICS_HAS_WIFI
    return WiFi.softAPgetStationNum();
#else
    return 0;
#endif
}

/**
 * @brief Check async scan status
 * @return Number of networks found, -1 if scan in progress, -2 if scan failed
 */
inline int16_t scanComplete() {
#if DOMOTICS_HAS_WIFI
    return WiFi.scanComplete();
#else
    return 0;
#endif
}

/**
 * @brief Delete scan results (free memory)
 */
inline void scanDelete() {
#if DOMOTICS_HAS_WIFI
    WiFi.scanDelete();
#endif
}

/**
 * @brief Disconnect and turn off WiFi completely
 */
inline void disconnectAndOff() {
#if DOMOTICS_HAS_WIFI
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
#endif
}

/**
 * @brief Get raw WiFi status code (platform-specific)
 */
inline uint8_t getRawStatus() {
#if DOMOTICS_HAS_WIFI
    return (uint8_t)WiFi.status();
#else
    return 0;
#endif
}

} // namespace WiFiHAL
} // namespace HAL
} // namespace DomoticsCore

// ============================================================================
// Network Client Types (for MQTT, HTTP, etc.)
// Platform-specific typedefs in DomoticsCore::HAL namespace
// ============================================================================

#if DOMOTICS_PLATFORM_ESP32
    #include <WiFiClient.h>
    #include <WiFiClientSecure.h>
    namespace DomoticsCore { namespace HAL {
        using NetworkClient = ::WiFiClient;
        using SecureNetworkClient = ::WiFiClientSecure;
    }}
#elif DOMOTICS_PLATFORM_ESP8266
    // ESP8266WiFi.h already included above, provides WiFiClient
    namespace DomoticsCore { namespace HAL {
        using NetworkClient = ::WiFiClient;
        using SecureNetworkClient = ::BearSSL::WiFiClientSecure;
    }}
#else
    // Stub for platforms without WiFi
    namespace DomoticsCore { namespace HAL {
        class NetworkClient {};
        class SecureNetworkClient {};
    }}
#endif

#endif // DOMOTICS_CORE_HAL_WIFI_H
