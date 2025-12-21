#ifndef DOMOTICS_CORE_PLATFORM_STUB_H
#define DOMOTICS_CORE_PLATFORM_STUB_H

/**
 * @file Platform_Stub.h
 * @brief Stub platform utilities for unsupported platforms.
 * 
 * This file provides empty/default implementations for platforms
 * that are not fully supported (AVR, ARM, unknown).
 */

#include <Arduino.h>

// Stub logging macros for native/unsupported platforms
#ifndef log_e
#define log_e(fmt, ...) ((void)0)
#define log_w(fmt, ...) ((void)0)
#define log_i(fmt, ...) ((void)0)
#define log_d(fmt, ...) ((void)0)
#define log_v(fmt, ...) ((void)0)
#endif

namespace DomoticsCore {
namespace HAL {
namespace Platform {

/**
 * @brief Get chip model/ID (stub)
 */
inline String getChipModel() {
#if defined(__AVR__)
    return "ATmega";
#elif defined(__arm__) && defined(ARDUINO)
    return "ARM Cortex";
#else
    return "Unknown";
#endif
}

/**
 * @brief Get chip revision (stub - always 0)
 */
inline uint8_t getChipRevision() {
    return 0;
}

/**
 * @brief Get unique chip ID (stub - always 0)
 */
inline uint64_t getChipId() {
    return 0;
}

/**
 * @brief Get free heap memory (stub - always 0)
 */
inline uint32_t getFreeHeap() {
    return 0;
}

/**
 * @brief Get CPU frequency in MHz (stub)
 */
inline uint32_t getCpuFreqMHz() {
#if defined(__AVR__)
    return F_CPU / 1000000UL;
#else
    return 0;
#endif
}

/**
 * @brief Software reset (stub)
 */
inline void restart() {
#if defined(__AVR__)
    asm volatile ("jmp 0");
#endif
}

/**
 * @brief Get chip temperature (stub)
 * @return NAN (temperature sensor not available)
 */
inline float getTemperature() {
    return NAN;
}

/**
 * @brief SHA256 hash computation (stub - no-op).
 */
class SHA256 {
public:
    SHA256() {}
    void begin() {}
    void update(const uint8_t*, size_t) {}
    void finish(uint8_t* digest) { memset(digest, 0, 32); }
    void abort() {}
    
    static String toHex(const uint8_t* digest, size_t len = 32) {
        static const char* hex = "0123456789abcdef";
        String out;
        out.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            out += hex[(digest[i] >> 4) & 0x0F];
            out += hex[digest[i] & 0x0F];
        }
        return out;
    }
};

} // namespace Platform
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_PLATFORM_STUB_H
