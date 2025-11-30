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
// Platform-specific Includes
// ============================================================================

#if DOMOTICS_PLATFORM_ESP32
    #include <esp_system.h>
    #include <esp_chip_info.h>
#elif DOMOTICS_PLATFORM_ESP8266
    #include <ESP8266WiFi.h>
    #include <user_interface.h>
#endif

// ============================================================================
// Utility Functions
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
 * @brief Get chip model/ID
 */
inline String getChipModel() {
#if DOMOTICS_PLATFORM_ESP32
    return String(ESP.getChipModel());
#elif DOMOTICS_PLATFORM_ESP8266
    return "ESP8266";
#elif DOMOTICS_PLATFORM_AVR
    return "ATmega";
#elif DOMOTICS_PLATFORM_ARM
    return "ARM Cortex";
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
#else
    return 0;
#endif
}

/**
 * @brief Get unique chip ID (for device identification)
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

/**
 * @brief Get free heap memory
 */
inline uint32_t getFreeHeap() {
#if DOMOTICS_PLATFORM_ESP32 || DOMOTICS_PLATFORM_ESP8266
    return ESP.getFreeHeap();
#else
    // No reliable way on AVR/ARM without platform-specific code
    return 0;
#endif
}

/**
 * @brief Get total RAM size in KB
 */
inline uint32_t getTotalRAM_KB() {
    return DOMOTICS_RAM_SIZE_KB;
}

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

/**
 * @brief Software reset
 */
inline void restart() {
#if DOMOTICS_PLATFORM_ESP32 || DOMOTICS_PLATFORM_ESP8266
    ESP.restart();
#elif DOMOTICS_PLATFORM_AVR
    // AVR watchdog reset
    asm volatile ("jmp 0");
#else
    // No reliable reset on unknown platforms
#endif
}

// ============================================================================
// Cryptographic Utilities
// ============================================================================

#if DOMOTICS_PLATFORM_ESP32
    #include <mbedtls/sha256.h>
#elif DOMOTICS_PLATFORM_ESP8266
    #include <Hash.h>
#endif

/**
 * @brief SHA256 hash computation with streaming support.
 * 
 * Usage:
 *   SHA256 sha;
 *   sha.begin();
 *   sha.update(data1, len1);
 *   sha.update(data2, len2);
 *   uint8_t digest[32];
 *   sha.finish(digest);
 */
class SHA256 {
public:
    SHA256() { begin(); }
    
    void begin() {
#if DOMOTICS_PLATFORM_ESP32
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts_ret(&ctx, 0);
#elif DOMOTICS_PLATFORM_ESP8266
        sha256_init(&ctx);
#endif
        active = true;
    }
    
    void update(const uint8_t* data, size_t len) {
        if (!active) return;
#if DOMOTICS_PLATFORM_ESP32
        mbedtls_sha256_update_ret(&ctx, data, len);
#elif DOMOTICS_PLATFORM_ESP8266
        sha256_update(&ctx, data, len);
#endif
    }
    
    void finish(uint8_t* digest) {
        if (!active) return;
#if DOMOTICS_PLATFORM_ESP32
        mbedtls_sha256_finish_ret(&ctx, digest);
        mbedtls_sha256_free(&ctx);
#elif DOMOTICS_PLATFORM_ESP8266
        sha256_final(digest, &ctx);
#else
        memset(digest, 0, 32);
#endif
        active = false;
    }
    
    void abort() {
        if (!active) return;
#if DOMOTICS_PLATFORM_ESP32
        mbedtls_sha256_free(&ctx);
#endif
        active = false;
    }
    
    /**
     * @brief Convert digest to hex string
     */
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
#if DOMOTICS_PLATFORM_ESP32
    mbedtls_sha256_context ctx;
#elif DOMOTICS_PLATFORM_ESP8266
    sha256_context_t ctx;
#endif
};

} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_HAL_PLATFORM_H
