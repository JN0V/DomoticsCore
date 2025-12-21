#ifndef DOMOTICS_CORE_HAL_PLATFORM_H
#define DOMOTICS_CORE_HAL_PLATFORM_H

/**
 * @file Platform_HAL.h
 * @brief Platform detection and compatibility macros for DomoticsCore.
 * 
 * This header provides compile-time platform detection and defines common
 * macros to enable platform-specific code paths throughout the library.
 * 
 * Supported Platforms:
 * - ESP32 (full support)
 * - ESP8266 (partial support - planned)
 * - Arduino AVR (very limited - LED only)
 * - Arduino ARM (limited support - planned)
 */

#include <Arduino.h>

// ============================================================================
// Platform Detection
// ============================================================================

// ESP32 (primary platform)
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    #define DOMOTICS_PLATFORM_ESP32 1
    #define DOMOTICS_PLATFORM_NAME "ESP32"
    #define DOMOTICS_HAS_WIFI 1
    #define DOMOTICS_HAS_PREFERENCES 1
    #define DOMOTICS_HAS_FREERTOS 1
    #define DOMOTICS_HAS_ASYNC_TCP 1
    #define DOMOTICS_HAS_SNTP 1
    #define DOMOTICS_HAS_OTA 1
    #define DOMOTICS_HAS_SPIFFS 1
    #define DOMOTICS_HAS_LITTLEFS 1
    #define DOMOTICS_RAM_SIZE_KB 320
    #define DOMOTICS_FLASH_SIZE_KB 4096

// ESP8266
#elif defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
    #define DOMOTICS_PLATFORM_ESP8266 1
    #define DOMOTICS_PLATFORM_NAME "ESP8266"
    #define DOMOTICS_HAS_WIFI 1
    #define DOMOTICS_HAS_PREFERENCES 0  // Use EEPROM or LittleFS instead
    #define DOMOTICS_HAS_FREERTOS 0
    #define DOMOTICS_HAS_ASYNC_TCP 0    // Different library needed
    #define DOMOTICS_HAS_SNTP 0         // Use configTime() instead
    #define DOMOTICS_HAS_OTA 1
    #define DOMOTICS_HAS_SPIFFS 1
    #define DOMOTICS_HAS_LITTLEFS 1
    #define DOMOTICS_RAM_SIZE_KB 80
    #define DOMOTICS_FLASH_SIZE_KB 4096

// Arduino AVR (Uno, Mega, etc.)
#elif defined(__AVR__)
    #define DOMOTICS_PLATFORM_AVR 1
    #define DOMOTICS_PLATFORM_NAME "AVR"
    #define DOMOTICS_HAS_WIFI 0
    #define DOMOTICS_HAS_PREFERENCES 0
    #define DOMOTICS_HAS_FREERTOS 0
    #define DOMOTICS_HAS_ASYNC_TCP 0
    #define DOMOTICS_HAS_SNTP 0
    #define DOMOTICS_HAS_OTA 0
    #define DOMOTICS_HAS_SPIFFS 0
    #define DOMOTICS_HAS_LITTLEFS 0
    #define DOMOTICS_RAM_SIZE_KB 2      // Uno has 2KB
    #define DOMOTICS_FLASH_SIZE_KB 32

// Arduino ARM (Due, SAMD, etc.)
#elif defined(__arm__) && defined(ARDUINO)
    #define DOMOTICS_PLATFORM_ARM 1
    #define DOMOTICS_PLATFORM_NAME "ARM"
    #define DOMOTICS_HAS_WIFI 0         // May be available with shield
    #define DOMOTICS_HAS_PREFERENCES 0
    #define DOMOTICS_HAS_FREERTOS 0
    #define DOMOTICS_HAS_ASYNC_TCP 0
    #define DOMOTICS_HAS_SNTP 0
    #define DOMOTICS_HAS_OTA 0
    #define DOMOTICS_HAS_SPIFFS 0
    #define DOMOTICS_HAS_LITTLEFS 0
    #define DOMOTICS_RAM_SIZE_KB 96     // Due has 96KB
    #define DOMOTICS_FLASH_SIZE_KB 512

// Unknown platform
#else
    #define DOMOTICS_PLATFORM_UNKNOWN 1
    #define DOMOTICS_PLATFORM_NAME "Unknown"
    #define DOMOTICS_HAS_WIFI 0
    #define DOMOTICS_HAS_PREFERENCES 0
    #define DOMOTICS_HAS_FREERTOS 0
    #define DOMOTICS_HAS_ASYNC_TCP 0
    #define DOMOTICS_HAS_SNTP 0
    #define DOMOTICS_HAS_OTA 0
    #define DOMOTICS_HAS_SPIFFS 0
    #define DOMOTICS_HAS_LITTLEFS 0
    #define DOMOTICS_RAM_SIZE_KB 0
    #define DOMOTICS_FLASH_SIZE_KB 0
#endif

// ============================================================================
// Feature Availability Checks (compile-time)
// ============================================================================

/**
 * @brief Check if platform supports WiFi
 */
#define DOMOTICS_SUPPORTS_WIFI() (DOMOTICS_HAS_WIFI == 1)

/**
 * @brief Check if platform supports persistent storage
 */
#define DOMOTICS_SUPPORTS_STORAGE() (DOMOTICS_HAS_PREFERENCES == 1 || DOMOTICS_HAS_LITTLEFS == 1)

/**
 * @brief Check if platform supports network time
 */
#define DOMOTICS_SUPPORTS_NTP() (DOMOTICS_HAS_WIFI == 1)

/**
 * @brief Check if platform supports OTA updates
 */
#define DOMOTICS_SUPPORTS_OTA() (DOMOTICS_HAS_OTA == 1)

/**
 * @brief Check if platform has enough RAM for full framework
 * (EventBus, WebUI, JSON parsing require ~20KB+ RAM)
 */
#define DOMOTICS_SUPPORTS_FULL_FRAMEWORK() (DOMOTICS_RAM_SIZE_KB >= 80)

// ============================================================================
// Platform-specific Implementation Files
// ============================================================================

#if DOMOTICS_PLATFORM_ESP32
    #include "Platform_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "Platform_ESP8266.h"
#else
    #include "Platform_Stub.h"
#endif

// ============================================================================
// Backward-Compatible API (delegates to Platform namespace)
// ============================================================================

namespace DomoticsCore {
namespace HAL {

/**
 * @brief Get platform name as string
 */
inline const char* getPlatformName() {
    return DOMOTICS_PLATFORM_NAME;
}

/**
 * @brief Get chip model/ID (delegates to Platform namespace)
 */
inline String getChipModel() {
    return Platform::getChipModel();
}

/**
 * @brief Get chip revision (delegates to Platform namespace)
 */
inline uint8_t getChipRevision() {
    return Platform::getChipRevision();
}

/**
 * @brief Get unique chip ID (delegates to Platform namespace)
 */
inline uint64_t getChipId() {
    return Platform::getChipId();
}

/**
 * @brief Get free heap memory (delegates to Platform namespace)
 */
inline uint32_t getFreeHeap() {
    return Platform::getFreeHeap();
}

/**
 * @brief Get total RAM size in KB
 */
inline uint32_t getTotalRAM_KB() {
    return DOMOTICS_RAM_SIZE_KB;
}

/**
 * @brief Get CPU frequency in MHz (delegates to Platform namespace)
 */
inline uint32_t getCpuFreqMHz() {
    return Platform::getCpuFreqMHz();
}

/**
 * @brief Software reset (delegates to Platform namespace)
 */
inline void restart() {
    Platform::restart();
}

/**
 * @brief Get the correct value to turn LED_BUILTIN ON for the current platform
 * @return HIGH or LOW depending on platform LED polarity
 */
inline int ledBuiltinOn() {
    return Platform::ledBuiltinOn();
}

/**
 * @brief Get the correct value to turn LED_BUILTIN OFF for the current platform
 * @return HIGH or LOW depending on platform LED polarity
 */
inline int ledBuiltinOff() {
    return Platform::ledBuiltinOff();
}

/**
 * @brief SHA256 - use Platform::SHA256 directly or this alias
 */
using SHA256 = Platform::SHA256;

} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_HAL_PLATFORM_H
