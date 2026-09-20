#ifndef DOMOTICS_CORE_NTP_STUB_H
#define DOMOTICS_CORE_NTP_STUB_H

/**
 * @file NTP_Stub.h
 * @brief Stub NTP implementation for unsupported platforms.
 */

#include <cstdint>

#if !DOMOTICS_PLATFORM_ESP32 && !DOMOTICS_PLATFORM_ESP8266

namespace DomoticsCore {
namespace HAL {
namespace NTPImpl {

/// The last interval handed over, in milliseconds. A host suite has no SNTP
/// client to interrogate, so the stub keeps what it was given.
inline uint32_t& lastSyncIntervalMs() {
    static uint32_t value = 0;
    return value;
}

inline void init(const char*, const char*, const char*) {}
inline void setTimezone(const char*) {}
inline void setSyncInterval(uint32_t intervalMs) { lastSyncIntervalMs() = intervalMs; }
inline void stop() {}
inline void forceSync() {}

} // namespace NTPImpl
} // namespace HAL
} // namespace DomoticsCore

#endif // Stub

#endif // DOMOTICS_CORE_NTP_STUB_H
