#ifndef DOMOTICS_CORE_PLATFORM_ARDUINO_H
#define DOMOTICS_CORE_PLATFORM_ARDUINO_H

/**
 * @file Platform_Arduino.h
 * @brief Common Arduino-based platform utilities for DomoticsCore.
 *
 * This file contains common implementations shared between ESP32 and ESP8266.
 * It is included by Platform_ESP32.h and Platform_ESP8266.h to avoid code duplication.
 *
 * Platform-specific code (SHA256, chip info, LED polarity) remains in the
 * respective Platform_ESP32.h and Platform_ESP8266.h files.
 */

#include <Arduino.h>

namespace DomoticsCore {
namespace HAL {
namespace Platform {

// =============================================================================
// Time Utilities
// =============================================================================

/**
 * @brief Get current milliseconds (Arduino millis())
 */
inline unsigned long getMillis() {
    return millis();
}

/**
 * @brief Delay execution (Arduino delay())
 */
inline void delayMs(unsigned long ms) {
    ::delay(ms);
}

// =============================================================================
// String Utilities
// =============================================================================

/**
 * @brief Convert string to uppercase
 */
inline String toUpperCase(const String& str) {
    String result = str;
    result.toUpperCase();
    return result;
}

/**
 * @brief Get substring of string
 */
inline String substring(const String& str, int start, int end = -1) {
    return str.substring(start, end);
}

/**
 * @brief Find index of character in string
 */
inline int indexOf(const String& str, char ch) {
    return str.indexOf(ch);
}

/**
 * @brief Check if string starts with prefix
 */
inline bool startsWith(const String& str, const String& prefix) {
    return str.startsWith(prefix);
}

/**
 * @brief Check if string ends with suffix
 */
inline bool endsWith(const String& str, const String& suffix) {
    return str.endsWith(suffix);
}

// =============================================================================
// GPIO Utilities
// =============================================================================

/**
 * @brief Write digital value to pin
 */
inline void digitalWrite(uint8_t pin, uint8_t value) {
    ::digitalWrite(pin, value);
}

/**
 * @brief Set pin mode
 */
inline void pinMode(uint8_t pin, uint8_t mode) {
    ::pinMode(pin, mode);
}

/**
 * @brief Write analog/PWM value to pin
 */
inline void analogWrite(uint8_t pin, int value) {
    ::analogWrite(pin, value);
}

// =============================================================================
// Math Utilities
// =============================================================================

/**
 * @brief Map a number from one range to another (calls Arduino's map)
 */
inline long map(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
    return ::map(value, fromLow, fromHigh, toLow, toHigh);
}

/**
 * @brief Constrain a value between min and max (template version for any type)
 * Arduino defines constrain as a macro, so we capture it first
 */
#ifdef constrain
    // Capture Arduino's constrain implementation before undefining
    #define _ARDUINO_CONSTRAIN(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
    #undef constrain

    // Provide template version that works with any numeric type
    template<typename T>
    inline T constrain(T value, T min_val, T max_val) {
        return (value < min_val) ? min_val : ((value > max_val) ? max_val : value);
    }
#else
    // Native/non-Arduino platforms
    template<typename T>
    inline T constrain(T value, T min_val, T max_val) {
        return (value < min_val) ? min_val : ((value > max_val) ? max_val : value);
    }
#endif

/**
 * @brief Mathematical constant PI
 * Arduino's PI macro is captured and redefined as constexpr for type safety
 */
#ifdef PI
    constexpr double PI_VALUE = PI;
    #undef PI
    constexpr double PI = PI_VALUE;
#endif

// =============================================================================
// Logging Utilities
// =============================================================================

/**
 * @brief Check if logging system is ready
 */
inline bool isLoggerReady() {
    return Serial;
}

// =============================================================================
// Hash Utilities (Helper only - SHA256 class is platform-specific)
// =============================================================================

/**
 * @brief Convert digest bytes to hexadecimal string
 */
inline String digestToHex(const uint8_t* digest, size_t len = 32) {
    static const char* hex = "0123456789abcdef";
    String out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out += hex[(digest[i] >> 4) & 0x0F];
        out += hex[digest[i] & 0x0F];
    }
    return out;
}

} // namespace Platform
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_PLATFORM_ARDUINO_H
