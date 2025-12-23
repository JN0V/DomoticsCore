#ifndef DOMOTICS_CORE_NTP_STUB_H
#define DOMOTICS_CORE_NTP_STUB_H

/**
 * @file NTP_Stub.h
 * @brief Stub NTP implementation for unsupported platforms.
 */

#if !DOMOTICS_PLATFORM_ESP32 && !DOMOTICS_PLATFORM_ESP8266

namespace DomoticsCore {
namespace HAL {
namespace NTPImpl {

inline void init(const char*, const char*, const char*) {}
inline void setTimezone(const char*) {}
inline void setSyncInterval(uint32_t) {}
inline void stop() {}
inline void forceSync() {}

} // namespace NTPImpl
} // namespace HAL
} // namespace DomoticsCore

#endif // Stub

#endif // DOMOTICS_CORE_NTP_STUB_H
