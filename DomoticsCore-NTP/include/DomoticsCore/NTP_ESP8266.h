#ifndef DOMOTICS_CORE_NTP_ESP8266_H
#define DOMOTICS_CORE_NTP_ESP8266_H

/**
 * @file NTP_ESP8266.h
 * @brief ESP8266-specific NTP implementation using configTime().
 */

#if DOMOTICS_PLATFORM_ESP8266

#include <string.h>
#include <time.h>
#include <sntp.h>

namespace DomoticsCore {
namespace HAL {
namespace NTPImpl {

inline void init(const char* server1, const char* server2, const char* server3) {
    // configTime() may store the raw char* pointers without copying, so the
    // buffers must outlive the call. Callers pass String::c_str(), which does not.
    static char serverBuf[3][64];

    const char* s1 = nullptr;
    const char* s2 = nullptr;
    const char* s3 = nullptr;

    if (server1) {
        strncpy(serverBuf[0], server1, sizeof(serverBuf[0]) - 1);
        serverBuf[0][sizeof(serverBuf[0]) - 1] = '\0';
        s1 = serverBuf[0];
    }
    if (server2) {
        strncpy(serverBuf[1], server2, sizeof(serverBuf[1]) - 1);
        serverBuf[1][sizeof(serverBuf[1]) - 1] = '\0';
        s2 = serverBuf[1];
    }
    if (server3) {
        strncpy(serverBuf[2], server3, sizeof(serverBuf[2]) - 1);
        serverBuf[2][sizeof(serverBuf[2]) - 1] = '\0';
        s3 = serverBuf[2];
    }
    configTime(0, 0, s1, s2, s3);
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
