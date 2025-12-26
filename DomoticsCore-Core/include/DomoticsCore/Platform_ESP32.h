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
#include <esp_system.h>

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
// ESP32-Specific: System Information (Extended)
// =============================================================================

/**
 * @brief Get total heap size for ESP32
 */
inline uint32_t getTotalHeap() {
    return ESP.getHeapSize();
}

/**
 * @brief Get minimum free heap ever recorded for ESP32
 */
inline uint32_t getMinFreeHeap() {
    return ESP.getMinFreeHeap();
}

/**
 * @brief Get maximum allocatable block size for ESP32
 */
inline uint32_t getMaxAllocHeap() {
    return ESP.getMaxAllocHeap();
}

/**
 * @brief Get flash chip size for ESP32
 */
inline uint32_t getFlashSize() {
    return ESP.getFlashChipSize();
}

/**
 * @brief Get sketch (program) size for ESP32
 */
inline uint32_t getSketchSize() {
    return ESP.getSketchSize();
}

/**
 * @brief Get free sketch space for ESP32
 */
inline uint32_t getFreeSketchSpace() {
    return ESP.getFreeSketchSpace();
}

// =============================================================================
// ESP32-Specific: Reset Reason
// =============================================================================

/**
 * @brief Reset reason codes (platform-agnostic)
 */
enum class ResetReason : uint8_t {
    Unknown = 0,
    PowerOn = 1,
    External = 2,
    Software = 3,
    Panic = 4,
    IntWatchdog = 5,
    TaskWatchdog = 6,
    Watchdog = 7,
    DeepSleep = 8,
    Brownout = 9,
    SDIO = 10
};

/**
 * @brief Get reset reason for ESP32
 */
inline ResetReason getResetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:   return ResetReason::PowerOn;
        case ESP_RST_EXT:       return ResetReason::External;
        case ESP_RST_SW:        return ResetReason::Software;
        case ESP_RST_PANIC:     return ResetReason::Panic;
        case ESP_RST_INT_WDT:   return ResetReason::IntWatchdog;
        case ESP_RST_TASK_WDT:  return ResetReason::TaskWatchdog;
        case ESP_RST_WDT:       return ResetReason::Watchdog;
        case ESP_RST_DEEPSLEEP: return ResetReason::DeepSleep;
        case ESP_RST_BROWNOUT:  return ResetReason::Brownout;
        case ESP_RST_SDIO:      return ResetReason::SDIO;
        default:                return ResetReason::Unknown;
    }
}

/**
 * @brief Get human-readable reset reason string
 */
inline String getResetReasonString(ResetReason reason) {
    switch (reason) {
        case ResetReason::PowerOn:      return "Power-on";
        case ResetReason::External:     return "External reset";
        case ResetReason::Software:     return "Software reset";
        case ResetReason::Panic:        return "Panic/Exception";
        case ResetReason::IntWatchdog:  return "Interrupt watchdog";
        case ResetReason::TaskWatchdog: return "Task watchdog";
        case ResetReason::Watchdog:     return "Other watchdog";
        case ResetReason::DeepSleep:    return "Deep sleep wake";
        case ResetReason::Brownout:     return "Brownout";
        case ResetReason::SDIO:         return "SDIO reset";
        default:                        return "Unknown";
    }
}

/**
 * @brief Check if reset reason indicates an unexpected/crash reset
 */
inline bool wasUnexpectedReset(ResetReason reason) {
    return reason == ResetReason::Panic ||
           reason == ResetReason::IntWatchdog ||
           reason == ResetReason::TaskWatchdog ||
           reason == ResetReason::Watchdog ||
           reason == ResetReason::Brownout;
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
        mbedtls_sha256_starts(&ctx, 0);
        active = true;
    }

    void update(const uint8_t* data, size_t len) {
        if (!active) return;
        mbedtls_sha256_update(&ctx, data, len);
    }

    void finish(uint8_t* digest) {
        if (!active) return;
        mbedtls_sha256_finish(&ctx, digest);
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
