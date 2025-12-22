#ifndef DOMOTICS_CORE_PLATFORM_ESP8266_H
#define DOMOTICS_CORE_PLATFORM_ESP8266_H

/**
 * @file Platform_ESP8266.h
 * @brief ESP8266-specific platform utilities for DomoticsCore.
 * 
 * This file contains ESP8266-specific implementations of platform utilities.
 * It is included by Platform_HAL.h when compiling for ESP8266.
 */

#if DOMOTICS_PLATFORM_ESP8266

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <user_interface.h>
#include <bearssl/bearssl_hash.h>

// ESP8266 logging macros (ESP32 has these built-in)
#define log_e(fmt, ...) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)
#define log_w(fmt, ...) Serial.printf("[W] " fmt "\n", ##__VA_ARGS__)
#define log_i(fmt, ...) Serial.printf("[I] " fmt "\n", ##__VA_ARGS__)
#define log_d(fmt, ...) Serial.printf("[D] " fmt "\n", ##__VA_ARGS__)
#define log_v(fmt, ...) Serial.printf("[V] " fmt "\n", ##__VA_ARGS__)

namespace DomoticsCore {
namespace HAL {
namespace Platform {

/**
 * @brief Initialize logging system for ESP8266
 */
inline void initializeLogging(long baudrate = 115200) {
    Serial.begin(baudrate);
    delay(500);
}

/**
 * @brief Check if logging system is ready for ESP8266
 */
inline bool isLoggerReady() {
    return Serial;
}

/**
 * @brief Get current milliseconds for ESP8266
 */
inline unsigned long getMillis() {
    return millis();
}

/**
 * @brief Delay execution for ESP8266
 */
inline void delay(unsigned long ms) {
    delay(ms);
}

/**
 * @brief Format chip ID as hexadecimal string for ESP8266
 */
inline String formatChipIdHex() {
    uint32_t chipid = ESP.getChipId();
    return String(chipid, HEX);
}

/**
 * @brief Convert string to uppercase for ESP8266
 */
inline String toUpperCase(const String& str) {
    String result = str;
    result.toUpperCase();
    return result;
}

/**
 * @brief Get substring of string for ESP8266
 */
inline String substring(const String& str, int start, int end = -1) {
    return str.substring(start, end);
}

/**
 * @brief Find index of character in string for ESP8266
 */
inline int indexOf(const String& str, char ch) {
    return str.indexOf(ch);
}

/**
 * @brief Check if string starts with prefix for ESP8266
 */
inline bool startsWith(const String& str, const String& prefix) {
    return str.startsWith(prefix);
}

/**
 * @brief Check if string ends with suffix for ESP8266
 */
inline bool endsWith(const String& str, const String& suffix) {
    return str.endsWith(suffix);
}

/**
 * @brief Get chip model/ID for ESP8266
 */
inline String getChipModel() {
    return "ESP8266";
}

/**
 * @brief Get chip revision for ESP8266 (not available)
 */
inline uint8_t getChipRevision() {
    return 0;
}

/**
 * @brief Get unique chip ID for ESP8266
 */
inline uint64_t getChipId() {
    return ESP.getChipId();
}

/**
 * @brief Get free heap memory for ESP8266
 */
inline uint32_t getFreeHeap() {
    return ESP.getFreeHeap();
}

/**
 * @brief Get CPU frequency in MHz for ESP8266
 */
inline uint32_t getCpuFreqMHz() {
    return ESP.getCpuFreqMHz();
}

/**
 * @brief Software reset for ESP8266
 */
inline void restart() {
    ESP.restart();
}

/**
 * @brief Get chip temperature for ESP8266
 * @return NAN (temperature sensor not available on ESP8266)
 */
inline float getTemperature() {
    return NAN;
}

/**
 * @brief Get the correct value to turn LED_BUILTIN ON for ESP8266
 * @return LOW (ESP8266: LED_BUILTIN is inverted)
 */
inline int ledBuiltinOn() {
    return LOW;   // ESP8266: LED_BUILTIN is inverted (LOW = ON)
}

/**
 * @brief Get the correct value to turn LED_BUILTIN OFF for ESP8266
 * @return HIGH (ESP8266: LED_BUILTIN is inverted)
 */
inline int ledBuiltinOff() {
    return HIGH;  // ESP8266: LED_BUILTIN is inverted (HIGH = OFF)
}

/**
 * @brief Check if internal LED (LED_BUILTIN) has inverted logic on ESP8266
 * @return true (ESP8266 LED is active-low)
 */
inline bool isInternalLEDInverted() {
    return true;  // ESP8266: active-low LED
}

/**
 * @brief SHA256 hash computation for ESP8266 using BearSSL.
 */
class SHA256 {
public:
    SHA256() { begin(); }
    
    void begin() {
        br_sha256_init(&ctx);
        active = true;
    }
    
    void update(const uint8_t* data, size_t len) {
        if (!active) return;
        br_sha256_update(&ctx, data, len);
    }
    
    void finish(uint8_t* digest) {
        if (!active) return;
        br_sha256_out(&ctx, digest);
        active = false;
    }
    
    void abort() {
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
    br_sha256_context ctx;
};

/**
 * @brief Write digital value to pin for ESP8266
 */
inline void digitalWrite(uint8_t pin, uint8_t value) {
    ::digitalWrite(pin, value);
}

/**
 * @brief Set pin mode for ESP8266
 */
inline void pinMode(uint8_t pin, uint8_t mode) {
    ::pinMode(pin, mode);
}

/**
 * @brief Write analog/PWM value to pin for ESP8266
 */
inline void analogWrite(uint8_t pin, int value) {
    ::analogWrite(pin, value);
}

/**
 * @brief Map a number from one range to another for ESP8266 (calls Arduino's map)
 */
inline long map(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
    return ::map(value, fromLow, fromHigh, toLow, toHigh);
}

/**
 * @brief Mathematical constant PI for ESP8266
 * Arduino's PI macro is captured and redefined as constexpr
 */
#ifdef PI
    constexpr double PI_VALUE = PI;
    #undef PI
    constexpr double PI = PI_VALUE;
#endif

/**
 * @brief Built-in LED pin number for ESP8266
 */
#define LED_BUILTIN 2

} // namespace Platform
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP8266

#endif // DOMOTICS_CORE_PLATFORM_ESP8266_H
