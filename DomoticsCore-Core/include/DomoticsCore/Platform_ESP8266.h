#ifndef DOMOTICS_CORE_PLATFORM_ESP8266_H
#define DOMOTICS_CORE_PLATFORM_ESP8266_H

/**
 * @file Platform_ESP8266.h
 * @brief ESP8266-specific platform utilities for DomoticsCore.
 *
 * This file contains ESP8266-specific implementations of platform utilities.
 * It is included by Platform_HAL.h when compiling for ESP8266.
 *
 * Common Arduino utilities are provided by Platform_Arduino.h.
 */

#if DOMOTICS_PLATFORM_ESP8266

// ESP8266-specific resource limits (must be defined before Platform_HAL.h fallback)
#define DOMOTICS_LOG_BUFFER_SIZE 5  // Minimal buffer due to limited RAM (~80KB total)

#include "Platform_Arduino.h"
#include <bearssl/bearssl_hash.h>

extern "C" {
#include <user_interface.h>
#include <osapi.h>  // os_get_random() — hardware RNG for the WebUI CSRF token
}

// ESP8266 logging macros (ESP32 has these built-in)
// Use printf_P + PSTR to keep format strings in Flash instead of DRAM
#define log_e(fmt, ...) Serial.printf_P(PSTR("[E] " fmt "\n"), ##__VA_ARGS__)
#define log_w(fmt, ...) Serial.printf_P(PSTR("[W] " fmt "\n"), ##__VA_ARGS__)
#define log_i(fmt, ...) Serial.printf_P(PSTR("[I] " fmt "\n"), ##__VA_ARGS__)
#define log_d(fmt, ...) Serial.printf_P(PSTR("[D] " fmt "\n"), ##__VA_ARGS__)
#define log_v(fmt, ...) Serial.printf_P(PSTR("[V] " fmt "\n"), ##__VA_ARGS__)

// Helper: snprintf with PROGMEM format string (snprintf_P doesn't exist on ESP8266)
// Used by DLOG macros (Logger.h) and DSNPRINTF_P (Platform_HAL.h)
#include <stdarg.h>
static inline int _dlog_snprintf_P(char* buf, size_t size, PGM_P fmt, ...)
    __attribute__((format(printf, 3, 4)));
static inline int _dlog_snprintf_P(char* buf, size_t size, PGM_P fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf_P(buf, size, fmt, args);
    va_end(args);
    return ret;
}

// DLOG platform abstraction: format string goes to PROGMEM, buffer reduced for RAM savings
#define DLOG_SNPRINTF(buf, size, fmt, ...) _dlog_snprintf_P(buf, size, PSTR(fmt), ##__VA_ARGS__)
#define DOMOTICS_DLOG_BUF_SIZE 128
// General PROGMEM snprintf for non-DLOG use (WebUI JSON, etc.)
#define DSNPRINTF_P(buf, size, fmt, ...) _dlog_snprintf_P(buf, size, PSTR(fmt), ##__VA_ARGS__)

namespace DomoticsCore {
namespace HAL {
namespace Platform {

// =============================================================================
// ESP8266-Specific: Logging Initialization
// =============================================================================

/**
 * @brief Initialize logging system for ESP8266
 */
inline void initializeLogging(long baudrate = 115200) {
    Serial.begin(baudrate);
    delayMs(500);
}

// =============================================================================
// ESP8266-Specific: Chip Information
// =============================================================================

/**
 * @brief Format chip ID as hexadecimal string for ESP8266
 */
inline String formatChipIdHex() {
    uint32_t chipid = ESP.getChipId();
    return String(chipid, HEX);
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
 * @brief Fill a buffer with cryptographically-usable random bytes (ESP8266).
 *
 * Uses the SDK hardware RNG via os_get_random(). NOT Arduino's random(), which
 * WMath.cpp downgrades to a millis-seeded rand() as soon as any sketch calls
 * randomSeed() — unusable for a CSRF token. Used by the WebUI per-boot token.
 */
inline void getRandomBytes(void* buf, size_t len) {
    os_get_random(static_cast<unsigned char*>(buf), len);
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

// =============================================================================
// ESP8266-Specific: System Information (Extended)
// =============================================================================

/**
 * @brief Get total heap size for ESP8266
 * @note ESP8266 doesn't provide a direct API, return typical value
 */
inline uint32_t getTotalHeap() {
    return 81920;  // ~80KB typical for ESP8266
}

/**
 * @brief Get minimum free heap ever recorded for ESP8266
 * @note ESP8266 doesn't track this, return current free heap
 */
inline uint32_t getMinFreeHeap() {
    return ESP.getFreeHeap();
}

/**
 * @brief Get maximum allocatable block size for ESP8266
 */
inline uint32_t getMaxAllocHeap() {
    return ESP.getMaxFreeBlockSize();
}

/**
 * @brief Get flash chip size for ESP8266
 */
inline uint32_t getFlashSize() {
    return ESP.getFlashChipSize();
}

/**
 * @brief Get sketch (program) size for ESP8266
 */
inline uint32_t getSketchSize() {
    return ESP.getSketchSize();
}

/**
 * @brief Get free sketch space for ESP8266
 */
inline uint32_t getFreeSketchSpace() {
    return ESP.getFreeSketchSpace();
}

// =============================================================================
// ESP8266-Specific: Reset Reason
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
 * @brief Get reset reason for ESP8266
 */
inline ResetReason getResetReason() {
    struct rst_info* resetInfo = ESP.getResetInfoPtr();
    if (resetInfo) {
        switch (resetInfo->reason) {
            case REASON_DEFAULT_RST:      return ResetReason::PowerOn;
            case REASON_WDT_RST:          return ResetReason::Watchdog;
            case REASON_EXCEPTION_RST:    return ResetReason::Panic;
            case REASON_SOFT_WDT_RST:     return ResetReason::TaskWatchdog;
            case REASON_SOFT_RESTART:     return ResetReason::Software;
            case REASON_DEEP_SLEEP_AWAKE: return ResetReason::DeepSleep;
            case REASON_EXT_SYS_RST:      return ResetReason::External;
            default:                      return ResetReason::Unknown;
        }
    }
    return ResetReason::Unknown;
}

/**
 * @brief Get human-readable reset reason string
 */
inline String getResetReasonString(ResetReason reason) {
    // Store strings in flash memory (PROGMEM) to save RAM on ESP8266
    switch (reason) {
        case ResetReason::PowerOn:      return F("Power-on");
        case ResetReason::External:     return F("External reset");
        case ResetReason::Software:     return F("Software reset");
        case ResetReason::Panic:        return F("Panic/Exception");
        case ResetReason::IntWatchdog:  return F("Interrupt watchdog");
        case ResetReason::TaskWatchdog: return F("Task watchdog");
        case ResetReason::Watchdog:     return F("Other watchdog");
        case ResetReason::DeepSleep:    return F("Deep sleep wake");
        case ResetReason::Brownout:     return F("Brownout");
        case ResetReason::SDIO:         return F("SDIO reset");
        default:                        return F("Unknown");
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

inline ResetDetail getResetDetail() {
    ResetDetail d;
    struct rst_info* ri = ESP.getResetInfoPtr();
    if (ri && ri->reason >= REASON_WDT_RST && ri->reason <= REASON_SOFT_WDT_RST) {
        d.exccause = ri->exccause; d.epc1 = ri->epc1; d.epc2 = ri->epc2; d.epc3 = ri->epc3;
        d.excvaddr = ri->excvaddr; d.depc = ri->depc;
        d.valid = true;
    }
    return d;
}

/**
 * @brief What a reset reason hides on this platform, or an empty String.
 *
 * Measured 2026-09-05 on a nodemcuv2: abort(), assert and an out-of-memory
 * `new` all reach the next boot as REASON_SOFT_RESTART, with nothing in
 * rst_info — indistinguishable from ESP.restart() by reason alone. Consumers
 * log this when it is not empty; they never test the platform themselves
 * (Constitution IX).
 */
inline String getResetReasonCaveat(ResetReason reason) {
    if (reason == ResetReason::Software) {
        return F("Software/System restart is also what abort(), assert and an out-of-memory new report on this platform");
    }
    return String();
}

/** @brief getMinFreeHeap() returns the current heap here (see above), so no. */
inline constexpr bool tracksMinFreeHeap() { return false; }

inline CoreDumpStatus getCoreDumpStatus() { return CoreDumpStatus{}; }

/** @brief No loop watchdog to arm: the SDK's soft WDT already resets a stuck loop in about 3 s (OBS-7). */
inline constexpr bool supportsLoopWatchdog() { return false; }
inline bool enableLoopWatchdog(uint32_t /*seconds*/) { return false; }
inline void feedLoopWatchdog() {}

// =============================================================================
// ESP8266-Specific: LED Polarity
// =============================================================================

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

// =============================================================================
// ESP8266-Specific: SHA256 (BearSSL)
// =============================================================================

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
        return digestToHex(digest, len);
    }

private:
    bool active = false;
    br_sha256_context ctx;
};

// =============================================================================
// ESP8266-Specific: Built-in LED Pin
// =============================================================================

/**
 * @brief Built-in LED pin number for ESP8266
 */
#define LED_BUILTIN 2

} // namespace Platform
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP8266

#endif // DOMOTICS_CORE_PLATFORM_ESP8266_H
