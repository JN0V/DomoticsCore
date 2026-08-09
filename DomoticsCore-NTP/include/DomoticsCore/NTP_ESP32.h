#ifndef DOMOTICS_CORE_NTP_ESP32_H
#define DOMOTICS_CORE_NTP_ESP32_H

/**
 * @file NTP_ESP32.h
 * @brief ESP32-specific NTP implementation using esp_sntp API.
 */

#if DOMOTICS_PLATFORM_ESP32

#include <esp_sntp.h>
#include <time.h>

namespace DomoticsCore {
namespace HAL {
namespace NTPImpl {

inline void init(const char* server1, const char* server2, const char* server3) {
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, server1);
    if (server2) esp_sntp_setservername(1, server2);
    if (server3) esp_sntp_setservername(2, server3);
    esp_sntp_init();
}

inline void setTimezone(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
}

inline void setSyncInterval(uint32_t intervalMs) {
    esp_sntp_set_sync_interval(intervalMs);
}

inline void stop() {
    esp_sntp_stop();
}

inline void forceSync() {
    esp_sntp_restart();
}

} // namespace NTPImpl
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP32

#endif // DOMOTICS_CORE_NTP_ESP32_H
