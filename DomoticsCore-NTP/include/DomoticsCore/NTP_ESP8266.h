#ifndef DOMOTICS_CORE_NTP_ESP8266_H
#define DOMOTICS_CORE_NTP_ESP8266_H

/**
 * @file NTP_ESP8266.h
 * @brief ESP8266-specific NTP implementation using configTime().
 */

#if DOMOTICS_PLATFORM_ESP8266

#include <time.h>
#include <sntp.h>

namespace DomoticsCore {
namespace HAL {
namespace NTPImpl {

inline void init(const char* server1, const char* server2, const char* server3) {
    configTime(0, 0, server1, server2, server3);
}

inline void setTimezone(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
}

inline void setSyncInterval(uint32_t) {
    // ESP8266 SNTP doesn't have direct interval control
}

inline void stop() {
    sntp_stop();
}

inline void forceSync() {
    sntp_stop();
    sntp_init();
}

} // namespace NTPImpl
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP8266

#endif // DOMOTICS_CORE_NTP_ESP8266_H
