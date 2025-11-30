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

} // namespace SystemInfo
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_SYSTEMINFO_HAL_H
