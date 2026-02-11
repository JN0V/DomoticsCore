#pragma once

#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Logger.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * Optimized WebUI Component Configuration
 * Uses fixed-size char[] arrays instead of String to avoid heap fragmentation.
 * Pattern: same as MQTTConfig, WifiConfig, OTAConfig, HAConfig, NTPConfig.
 */
struct WebUIConfig {
    // User-configurable fields (exposed in WebUI settings)
    char deviceName[32] = "DomoticsCore Device";
    char theme[8] = "auto";
    
    // Advanced fields (configured at compile-time, not exposed in WebUI)
    uint16_t port = 80;
    bool enableWebSocket = true;
    int wsUpdateInterval = 5000;
    bool useFileSystem = false;
    char staticPath[16] = "/webui";
    char primaryColor[8] = "#007acc";
    bool enableAuth = false;
    char username[32] = "admin";
    char password[48] = "";
    int maxWebSocketClients = 3;
    int apiTimeout = 5000;
    bool enableCompression = true;
    bool enableCaching = true;
    bool enableCORS = false;

    // Getters (return String for backward compatibility)
    String getDeviceName() const { return String(deviceName); }
    String getTheme() const { return String(theme); }
    String getStaticPath() const { return String(staticPath); }
    String getPrimaryColor() const { return String(primaryColor); }
    String getUsername() const { return String(username); }
    String getPassword() const { return String(password); }

    // Setters (safe strncpy + null-termination + truncation warning)
    void setDeviceName(const char* v) {
        if (strlen(v) >= sizeof(deviceName)) {
            DLOG_W("WebUI", "deviceName truncated: '%s' (%zu >= %zu)", v, strlen(v), sizeof(deviceName));
        }
        strncpy(deviceName, v, sizeof(deviceName) - 1);
        deviceName[sizeof(deviceName) - 1] = '\0';
    }
    void setTheme(const char* v) {
        if (strlen(v) >= sizeof(theme)) {
            DLOG_W("WebUI", "theme truncated: '%s' (%zu >= %zu)", v, strlen(v), sizeof(theme));
        }
        strncpy(theme, v, sizeof(theme) - 1);
        theme[sizeof(theme) - 1] = '\0';
    }
    void setStaticPath(const char* v) {
        if (strlen(v) >= sizeof(staticPath)) {
            DLOG_W("WebUI", "staticPath truncated: '%s' (%zu >= %zu)", v, strlen(v), sizeof(staticPath));
        }
        strncpy(staticPath, v, sizeof(staticPath) - 1);
        staticPath[sizeof(staticPath) - 1] = '\0';
    }
    void setPrimaryColor(const char* v) {
        if (strlen(v) >= sizeof(primaryColor)) {
            DLOG_W("WebUI", "primaryColor truncated: '%s' (%zu >= %zu)", v, strlen(v), sizeof(primaryColor));
        }
        strncpy(primaryColor, v, sizeof(primaryColor) - 1);
        primaryColor[sizeof(primaryColor) - 1] = '\0';
    }
    void setUsername(const char* v) {
        if (strlen(v) >= sizeof(username)) {
            DLOG_W("WebUI", "username truncated: '%s' (%zu >= %zu)", v, strlen(v), sizeof(username));
        }
        strncpy(username, v, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    }
    void setPassword(const char* v) {
        if (strlen(v) >= sizeof(password)) {
            DLOG_W("WebUI", "password truncated (len %zu >= %zu)", strlen(v), sizeof(password));
        }
        strncpy(password, v, sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
