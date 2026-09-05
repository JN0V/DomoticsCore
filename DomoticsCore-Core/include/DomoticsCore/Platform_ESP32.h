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

// ESP32-specific resource limits (must be defined before Platform_HAL.h fallback)
#define DOMOTICS_LOG_BUFFER_SIZE 50  // ESP32 has plenty of RAM (~320KB)

#include "Platform_Arduino.h"
#include <mbedtls/sha256.h>
#include <esp_system.h>
#include <esp_core_dump.h>
#include <esp_partition.h>
#include <esp_task_wdt.h>

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
 * @brief Fill a buffer with cryptographically-usable random bytes (ESP32).
 *
 * Uses the hardware RNG via esp_random(). NOT Arduino's random(), which
 * WMath.cpp downgrades to a millis-seeded rand() as soon as any sketch calls
 * randomSeed() — unusable for a CSRF token. Used by the WebUI per-boot token.
 */
inline void getRandomBytes(void* buf, size_t len) {
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t i = 0;
    while (i < len) {
        uint32_t r = esp_random();
        for (size_t b = 0; b < sizeof(r) && i < len; ++b, ++i) {
            p[i] = static_cast<uint8_t>(r & 0xFF);
            r >>= 8;
        }
    }
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
// Post-mortem diagnostics (OBS-1, OBS-2, OBS-7)
// =============================================================================

/**
 * @brief Exception registers the SDK preserved across the last reset.
 *
 * ESP8266 only. `valid` is true when the reset was an exception or a
 * watchdog (SDK reasons 1-3); `epc1` then locates the fault, or the loop the
 * watchdog interrupted, through `xtensa-lx106-elf-addr2line -e firmware.elf`.
 * An abort(), an assert and an out-of-memory `new` reach the next boot as
 * ResetReason::Software with nothing here — measured 2026-09-05 (OBS-2).
 */
struct ResetDetail {
    uint32_t exccause = 0, epc1 = 0, epc2 = 0, epc3 = 0, excvaddr = 0, depc = 0;
    bool valid = false;
};

/** @brief What the ESP32 core dump partition holds; unsupported elsewhere (OBS-1). */
struct CoreDumpStatus {
    bool supported = false;        // the platform can hold a core dump at all
    bool partitionPresent = false; // a `coredump` partition exists in the table
    bool dumpPresent = false;      // a dump from a previous panic is waiting in it
    uint32_t size = 0;             // its size in bytes
};

/** @brief Nothing to read here: the core dump carries the registers (OBS-1). */
inline ResetDetail getResetDetail() { return ResetDetail{}; }

inline String getResetInfoString() { return getResetReasonString(getResetReason()); }

/** @brief Nothing hidden here: abort() and a failed `new` panic and say so. */
inline String getResetReasonCaveat(ResetReason /*reason*/) { return String(); }

inline constexpr bool tracksMinFreeHeap() { return true; }

/**
 * @brief Whether a `coredump` partition exists and whether a dump is waiting in it.
 *
 * Every stock partition table but bare_minimum_2MB.csv carries the partition,
 * and the precompiled core writes an ELF dump there on every panic; nothing
 * read it before OBS-1. Measured 2026-09-05 on a WROOM-32D: ~9 KB per panic.
 */
inline CoreDumpStatus getCoreDumpStatus() {
    CoreDumpStatus s;
    s.supported = true;
    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    s.partitionPresent = (part != nullptr);
    if (!part) return s;
    size_t addr = 0, size = 0;
    if (esp_core_dump_image_get(&addr, &size) == ESP_OK && size > 0) {
        s.dumpPresent = true;
        s.size = static_cast<uint32_t>(size);
    }
    return s;
}

inline bool loopWatchdogEnabled_ = false;

/**
 * @brief Put the Arduino loop task on the task watchdog (OBS-7).
 *
 * The core creates loopTask with the watchdog off and the TWDT watches IDLE0
 * only, so a stuck loop() never reboots — measured 2026-09-05, >40 s of
 * silence against a 5 s TWDT. With `panic` set, expiry is a panic and so
 * leaves a core dump with the hang's backtrace. Reconfigures the TWDT
 * timeout globally. System::loop() feeds it.
 */
inline bool enableLoopWatchdog(uint32_t seconds) {
    if (seconds == 0) return false;
    if (esp_task_wdt_init(seconds, true) != ESP_OK) return false;
    enableLoopWDT();
    loopWatchdogEnabled_ = true;
    return true;
}

inline void feedLoopWatchdog() {
    if (loopWatchdogEnabled_) feedLoopWDT();
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
 * @note Only define if not already provided by the board variant
 */
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

} // namespace Platform
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP32

#endif // DOMOTICS_CORE_PLATFORM_ESP32_H
