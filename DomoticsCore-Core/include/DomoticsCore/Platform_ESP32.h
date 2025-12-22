#ifndef DOMOTICS_CORE_PLATFORM_ESP32_H
#define DOMOTICS_CORE_PLATFORM_ESP32_H

/**
 * @file Platform_ESP32.h
 * @brief ESP32-specific platform utilities for DomoticsCore.
 *
 * This file contains ESP32-specific implementations of platform utilities.
 * It is included by Platform_HAL.h when compiling for ESP32.
 *
 * Common Arduino utilities are provided by Platform_Arduino.h.
 */

#if DOMOTICS_PLATFORM_ESP32

#include "Platform_Arduino.h"
#include <mbedtls/sha256.h>

namespace DomoticsCore {
namespace HAL {
namespace Platform {

// =============================================================================
// ESP32-Specific: Logging Initialization
// =============================================================================

/**
 * @brief Initialize logging system for ESP32
 */
inline void initializeLogging(long baudrate = 115200) {
    Serial.begin(baudrate);
    delayMs(100);
}

// =============================================================================
// ESP32-Specific: Chip Information
// =============================================================================

/**
 * @brief Format chip ID as hexadecimal string for ESP32
 */
inline String formatChipIdHex() {
    uint64_t chipid = ESP.getEfuseMac();
    uint32_t id = (uint32_t)(chipid >> 24) ^ (uint32_t)(chipid);
    return String(id, HEX);
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

// =============================================================================
// ESP32-Specific: LED Polarity
// =============================================================================

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

// =============================================================================
// ESP32-Specific: SHA256 (mbedtls)
// =============================================================================

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
        return digestToHex(digest, len);
    }

private:
    bool active = false;
    mbedtls_sha256_context ctx;
};

// =============================================================================
// ESP32-Specific: Built-in LED Pin
// =============================================================================

/**
 * @brief Built-in LED pin number for ESP32
 */
#define LED_BUILTIN 2

} // namespace Platform
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP32

#endif // DOMOTICS_CORE_PLATFORM_ESP32_H
