#ifndef DOMOTICS_CORE_SYSTEMINFO_HAL_H
#define DOMOTICS_CORE_SYSTEMINFO_HAL_H

/**
 * @file SystemInfo_HAL.h
 * @brief SystemInfo Hardware Abstraction Layer.
 * 
 * Provides unified system metrics across platforms:
 * - ESP32: Full metrics via ESP.* API
 * - ESP8266: Partial metrics
 * - Other platforms: Stub implementation
 */

#include "DomoticsCore/Platform_HAL.h"

#if DOMOTICS_PLATFORM_ESP32
#include <esp_system.h>
#endif

namespace DomoticsCore {
namespace HAL {
namespace SystemInfo {

// ============================================================================
// Memory Information
// ============================================================================

/**
 * @brief Get free heap memory in bytes
 */
inline uint32_t getFreeHeap() {
#if DOMOTICS_PLATFORM_ESP32 || DOMOTICS_PLATFORM_ESP8266
    return ESP.getFreeHeap();
#else
    return 0;
#endif
}

/**
 * @brief Get total heap size in bytes
 */
inline uint32_t getTotalHeap() {
#if DOMOTICS_PLATFORM_ESP32
    return ESP.getHeapSize();
#elif DOMOTICS_PLATFORM_ESP8266
    return 81920;  // ~80KB typical for ESP8266
#else
    return 0;
#endif
}

/**
 * @brief Get minimum free heap since boot
 */
inline uint32_t getMinFreeHeap() {
#if DOMOTICS_PLATFORM_ESP32
    return ESP.getMinFreeHeap();
#elif DOMOTICS_PLATFORM_ESP8266
    return ESP.getFreeHeap();  // ESP8266 doesn't track min
#else
    return 0;
#endif
}

/**
 * @brief Get largest allocatable block
 */
inline uint32_t getMaxAllocHeap() {
#if DOMOTICS_PLATFORM_ESP32
    return ESP.getMaxAllocHeap();
#elif DOMOTICS_PLATFORM_ESP8266
    return ESP.getMaxFreeBlockSize();
#else
    return 0;
#endif
}

// ============================================================================
// CPU Information
// ============================================================================

/**
 * @brief Get CPU frequency in MHz
 */
inline uint32_t getCpuFreqMHz() {
#if DOMOTICS_PLATFORM_ESP32 || DOMOTICS_PLATFORM_ESP8266
    return ESP.getCpuFreqMHz();
#elif DOMOTICS_PLATFORM_AVR
    return F_CPU / 1000000UL;
#else
    return 0;
#endif
}

// ============================================================================
// Flash/Firmware Information
// ============================================================================

/**
 * @brief Get flash chip size in bytes
 */
inline uint32_t getFlashSize() {
#if DOMOTICS_PLATFORM_ESP32 || DOMOTICS_PLATFORM_ESP8266
    return ESP.getFlashChipSize();
#else
    return 0;
#endif
}

/**
 * @brief Get current sketch/firmware size in bytes
 */
inline uint32_t getSketchSize() {
#if DOMOTICS_PLATFORM_ESP32 || DOMOTICS_PLATFORM_ESP8266
    return ESP.getSketchSize();
#else
    return 0;
#endif
}

/**
 * @brief Get free space for OTA/sketch in bytes
 */
inline uint32_t getFreeSketchSpace() {
#if DOMOTICS_PLATFORM_ESP32 || DOMOTICS_PLATFORM_ESP8266
    return ESP.getFreeSketchSpace();
#else
    return 0;
#endif
}

// ============================================================================
// Chip Information
// ============================================================================

/**
 * @brief Get chip model string
 */
inline String getChipModel() {
#if DOMOTICS_PLATFORM_ESP32
    return String(ESP.getChipModel());
#elif DOMOTICS_PLATFORM_ESP8266
    return "ESP8266";
#elif DOMOTICS_PLATFORM_AVR
    return "ATmega";
#else
    return "Unknown";
#endif
}

/**
 * @brief Get chip revision
 */
inline uint8_t getChipRevision() {
#if DOMOTICS_PLATFORM_ESP32
    return ESP.getChipRevision();
#elif DOMOTICS_PLATFORM_ESP8266
    return 0;  // Not available
#else
    return 0;
#endif
}

/**
 * @brief Get unique chip ID
 */
inline uint64_t getChipId() {
#if DOMOTICS_PLATFORM_ESP32
    return ESP.getEfuseMac();
#elif DOMOTICS_PLATFORM_ESP8266
    return ESP.getChipId();
#else
    return 0;
#endif
}

// ============================================================================
// Boot Diagnostics
// ============================================================================

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
 * @brief Get the reset reason for the last boot
 */
inline ResetReason getResetReason() {
#if DOMOTICS_PLATFORM_ESP32
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
#elif DOMOTICS_PLATFORM_ESP8266
    // ESP8266 has limited reset reason support
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
#else
    return ResetReason::Unknown;
#endif
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

} // namespace SystemInfo
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_SYSTEMINFO_HAL_H
