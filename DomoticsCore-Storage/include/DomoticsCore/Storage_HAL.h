#ifndef DOMOTICS_CORE_HAL_STORAGE_H
#define DOMOTICS_CORE_HAL_STORAGE_H

/**
 * @file Storage_HAL.h
 * @brief Storage Hardware Abstraction Layer.
 * 
 * Provides a unified key-value storage interface across platforms:
 * - ESP32: Uses Preferences (NVS)
 * - ESP8266: Uses LittleFS + JSON file
 * - Other platforms: Stub implementation (RAM-only)
 */

#include "DomoticsCore/Platform_HAL.h"

namespace DomoticsCore {
namespace HAL {

/**
 * @brief Abstract storage interface for key-value persistence
 */
class IStorage {
public:
    virtual ~IStorage() = default;
    
    virtual bool begin(const char* namespace_name, bool readOnly = false) = 0;
    virtual void end() = 0;
    virtual bool isKey(const char* key) = 0;
    
    virtual bool putString(const char* key, const String& value) = 0;
    virtual String getString(const char* key, const String& defaultValue = "") = 0;
    
    virtual bool putInt(const char* key, int32_t value) = 0;
    virtual int32_t getInt(const char* key, int32_t defaultValue = 0) = 0;
    
    virtual bool putBool(const char* key, bool value) = 0;
    virtual bool getBool(const char* key, bool defaultValue = false) = 0;
    
    virtual bool putFloat(const char* key, float value) = 0;
    virtual float getFloat(const char* key, float defaultValue = 0.0f) = 0;
    
    virtual bool putULong64(const char* key, uint64_t value) = 0;
    virtual uint64_t getULong64(const char* key, uint64_t defaultValue = 0) = 0;
    
    virtual size_t putBytes(const char* key, const uint8_t* data, size_t len) = 0;
    virtual size_t getBytes(const char* key, uint8_t* buffer, size_t maxLen) = 0;
    virtual size_t getBytesLength(const char* key) = 0;
    
    virtual bool remove(const char* key) = 0;
    virtual bool clear() = 0;
    
    virtual size_t freeEntries() = 0;
};

} // namespace HAL
} // namespace DomoticsCore

// ============================================================================
// Platform-specific Implementation Files
// ============================================================================

#if DOMOTICS_PLATFORM_ESP32
    #include "Storage_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "Storage_ESP8266.h"
#else
    #include "Storage_Stub.h"
#endif

#endif // DOMOTICS_CORE_HAL_STORAGE_H
