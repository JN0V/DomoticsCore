#ifndef DOMOTICS_CORE_HAL_NTP_H
#define DOMOTICS_CORE_HAL_NTP_H

/**
 * @file NTP_HAL.h
 * @brief NTP (Network Time Protocol) Hardware Abstraction Layer.
 * 
 * Provides a unified interface for time synchronization across platforms:
 * - ESP32: Uses esp_sntp API
 * - ESP8266: Uses configTime() 
 * - Other platforms: Stub implementation
 */

#include "DomoticsCore/Platform_HAL.h"
#include <time.h>

#if DOMOTICS_PLATFORM_ESP32
    #include <esp_sntp.h>
#elif DOMOTICS_PLATFORM_ESP8266
    #include <time.h>
    #include <sntp.h>
#endif

namespace DomoticsCore {
namespace HAL {
namespace NTP {

/**
 * @brief Initialize SNTP client
 * @param server1 Primary NTP server
 * @param server2 Secondary NTP server (optional)
 * @param server3 Tertiary NTP server (optional)
 */
inline void init(const char* server1, const char* server2 = nullptr, const char* server3 = nullptr) {
#if DOMOTICS_PLATFORM_ESP32
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, server1);
    if (server2) sntp_setservername(1, server2);
    if (server3) sntp_setservername(2, server3);
    sntp_init();
    
#elif DOMOTICS_PLATFORM_ESP8266
    // ESP8266 uses configTime with timezone offset
    configTime(0, 0, server1, server2, server3);
    
#else
    // No NTP support on other platforms
    (void)server1;
    (void)server2;
    (void)server3;
#endif
}

/**
 * @brief Set timezone using POSIX TZ string
 * @param tz POSIX timezone string (e.g., "CET-1CEST,M3.5.0,M10.5.0/3")
 */
inline void setTimezone(const char* tz) {
#if DOMOTICS_PLATFORM_ESP32 || DOMOTICS_PLATFORM_ESP8266
    setenv("TZ", tz, 1);
    tzset();
#else
    (void)tz;
#endif
}

/**
 * @brief Set sync interval in milliseconds
 * @param intervalMs Sync interval (default: 1 hour)
 */
inline void setSyncInterval(uint32_t intervalMs) {
#if DOMOTICS_PLATFORM_ESP32
    sntp_set_sync_interval(intervalMs);
#elif DOMOTICS_PLATFORM_ESP8266
    // ESP8266 SNTP doesn't have direct interval control
    // It syncs every 1 hour by default
    (void)intervalMs;
#else
    (void)intervalMs;
#endif
}

/**
 * @brief Stop SNTP client
 */
inline void stop() {
#if DOMOTICS_PLATFORM_ESP32
    sntp_stop();
#elif DOMOTICS_PLATFORM_ESP8266
    sntp_stop();
#endif
}

/**
 * @brief Force immediate sync
 */
inline void forceSync() {
#if DOMOTICS_PLATFORM_ESP32
    sntp_restart();
#elif DOMOTICS_PLATFORM_ESP8266
    // ESP8266: No direct API, but stopping and starting should work
    sntp_stop();
    sntp_init();
#endif
}

/**
 * @brief Check if time has been synced (valid time > year 2020)
 */
inline bool isSynced() {
    time_t now = time(nullptr);
    return now > 1577836800;  // 2020-01-01 00:00:00 UTC
}

/**
 * @brief Get current Unix timestamp
 */
inline time_t getTime() {
    return time(nullptr);
}

/**
 * @brief Get formatted time string
 * @param format strftime format (default: ISO 8601)
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @return true if successful
 */
inline bool getFormattedTime(const char* format, char* buffer, size_t bufferSize) {
    if (!isSynced()) {
        if (bufferSize > 0) buffer[0] = '\0';
        return false;
    }
    
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buffer, bufferSize, format, &timeinfo);
    return true;
}

} // namespace NTP
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_HAL_NTP_H
