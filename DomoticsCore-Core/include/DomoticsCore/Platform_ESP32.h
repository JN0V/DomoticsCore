#ifndef DOMOTICS_CORE_PLATFORM_ESP32_H
#define DOMOTICS_CORE_PLATFORM_ESP32_H

/**
 * @file Platform_ESP32.h
 * @brief ESP32-specific platform utilities for DomoticsCore.
 * 
 * This file contains ESP32-specific implementations of platform utilities.
 * It is included by Platform_HAL.h when compiling for ESP32.
 */

#if DOMOTICS_PLATFORM_ESP32

#include <Arduino.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <mbedtls/sha256.h>

namespace DomoticsCore {
namespace HAL {
namespace Platform {

/**
 * @brief Initialize logging system for ESP32
 */
inline void initializeLogging(long baudrate = 115200) {
    Serial.begin(baudrate);
    delay(100);
}

/**
 * @brief Check if logging system is ready for ESP32
 */
inline bool isLoggerReady() {
    return Serial;
}

/**
 * @brief Get current milliseconds for ESP32
 */
inline unsigned long getMillis() {
    return millis();
}

/**
 * @brief Delay execution for ESP32
 */
inline void delay(unsigned long ms) {
    delay(ms);
}

/**
 * @brief Format chip ID as hexadecimal string for ESP32
 */
inline String formatChipIdHex() {
    uint64_t chipid = ESP.getEfuseMac();
    uint32_t id = (uint32_t)(chipid >> 24) ^ (uint32_t)(chipid);
    return String(id, HEX);
}

/**
 * @brief Convert string to uppercase for ESP32
 */
inline String toUpperCase(const String& str) {
    String result = str;
    result.toUpperCase();
    return result;
}

/**
 * @brief Get substring of string for ESP32
 */
inline String substring(const String& str, int start, int end = -1) {
    return str.substring(start, end);
}

/**
 * @brief Find index of character in string for ESP32
 */
inline int indexOf(const String& str, char ch) {
    return str.indexOf(ch);
}

/**
 * @brief Check if string starts with prefix for ESP32
 */
inline bool startsWith(const String& str, const String& prefix) {
    return str.startsWith(prefix);
}

/**
 * @brief Check if string ends with suffix for ESP32
 */
inline bool endsWith(const String& str, const String& suffix) {
    return str.endsWith(suffix);
}

/**
 * @brief Get chip model/ID for ESP32
 */
inline String getChipModel() {
    return String(ESP.getChipModel());
}

/**
 * @brief Get chip revision for ESP32
 */
inline uint8_t getChipRevision() {
    return ESP.getChipRevision();
}

/**
 * @brief Get unique chip ID for ESP32
 */
inline uint64_t getChipId() {
    return ESP.getEfuseMac();
}

/**
 * @brief Get free heap memory for ESP32
 */
inline uint32_t getFreeHeap() {
    return ESP.getFreeHeap();
}

/**
 * @brief Get CPU frequency in MHz for ESP32
 */
inline uint32_t getCpuFreqMHz() {
    return ESP.getCpuFreqMHz();
}

/**
 * @brief Software reset for ESP32
 */
inline void restart() {
    ESP.restart();
}

/**
 * @brief Get chip temperature for ESP32
 * @return Temperature in Celsius, or NAN if not available
 */
inline float getTemperature() {
    return temperatureRead();
}

/**
 * @brief Get the correct value to turn LED_BUILTIN ON for ESP32
 * @return HIGH (ESP32: LED_BUILTIN is normal)
 */
inline int ledBuiltinOn() {
    return HIGH;  // ESP32: LED_BUILTIN is normal (HIGH = ON)
}

/**
 * @brief Get the correct value to turn LED_BUILTIN OFF for ESP32
 * @return LOW (ESP32: LED_BUILTIN is normal)
 */
inline int ledBuiltinOff() {
    return LOW;   // ESP32: LED_BUILTIN is normal (LOW = OFF)
}

/**
 * @brief Check if internal LED (LED_BUILTIN) has inverted logic on ESP32
 * @return false (ESP32 LED is active-high)
 */
inline bool isInternalLEDInverted() {
    return false;  // ESP32: active-high LED
}

/**
 * @brief SHA256 hash computation for ESP32 using mbedtls.
 */
class SHA256 {
public:
    SHA256() { begin(); }
    
    void begin() {
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts_ret(&ctx, 0);
        active = true;
    }
    
    void update(const uint8_t* data, size_t len) {
        if (!active) return;
        mbedtls_sha256_update_ret(&ctx, data, len);
    }
    
    void finish(uint8_t* digest) {
        if (!active) return;
        mbedtls_sha256_finish_ret(&ctx, digest);
        mbedtls_sha256_free(&ctx);
        active = false;
    }
    
    void abort() {
        if (!active) return;
        mbedtls_sha256_free(&ctx);
        active = false;
    }
    
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
    
private:
    bool active = false;
    mbedtls_sha256_context ctx;
};

/**
 * @brief Write digital value to pin for ESP32
 */
inline void digitalWrite(uint8_t pin, uint8_t value) {
    ::digitalWrite(pin, value);
}

/**
 * @brief Set pin mode for ESP32
 */
inline void pinMode(uint8_t pin, uint8_t mode) {
    ::pinMode(pin, mode);
}

/**
 * @brief Write analog/PWM value to pin for ESP32
 */
inline void analogWrite(uint8_t pin, int value) {
    ::analogWrite(pin, value);
}

/**
 * @brief Map a number from one range to another for ESP32 (calls Arduino's map)
 */
inline long map(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
    return ::map(value, fromLow, fromHigh, toLow, toHigh);
}

/**
 * @brief Mathematical constant PI for ESP32
 * Arduino's PI macro is captured and redefined as constexpr
 */
#ifdef PI
    constexpr double PI_VALUE = PI;
    #undef PI
    constexpr double PI = PI_VALUE;
#endif

/**
 * @brief Built-in LED pin number for ESP32
 */
#define LED_BUILTIN 2

} // namespace Platform
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP32

#endif // DOMOTICS_CORE_PLATFORM_ESP32_H
