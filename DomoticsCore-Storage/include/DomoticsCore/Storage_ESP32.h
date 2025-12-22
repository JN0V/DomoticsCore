#ifndef DOMOTICS_CORE_STORAGE_ESP32_H
#define DOMOTICS_CORE_STORAGE_ESP32_H

/**
 * @file Storage_ESP32.h
 * @brief ESP32-specific storage implementation using Preferences (NVS).
 */

#if DOMOTICS_PLATFORM_ESP32

#include <Preferences.h>

namespace DomoticsCore {
namespace HAL {

class PreferencesStorage : public IStorage {
private:
    Preferences prefs;
    bool opened = false;
    
public:
    bool begin(const char* namespace_name, bool readOnly = false) override {
        if (opened) prefs.end();
        opened = prefs.begin(namespace_name, readOnly);
        return opened;
    }
    
    bool isKey(const char* key) override {
        if (!opened) return false;
        return prefs.isKey(key);
    }
    
    void end() override {
        if (opened) {
            prefs.end();
            opened = false;
        }
    }
    
    bool putString(const char* key, const String& value) override {
        if (!opened) return false;
        return prefs.putString(key, value) > 0;
    }
    
    String getString(const char* key, const String& defaultValue = "") override {
        if (!opened) return defaultValue;
        return prefs.getString(key, defaultValue);
    }
    
    bool putInt(const char* key, int32_t value) override {
        if (!opened) return false;
        return prefs.putInt(key, value) > 0;
    }
    
    int32_t getInt(const char* key, int32_t defaultValue = 0) override {
        if (!opened) return defaultValue;
        return prefs.getInt(key, defaultValue);
    }
    
    bool putBool(const char* key, bool value) override {
        if (!opened) return false;
        return prefs.putBool(key, value) > 0;
    }
    
    bool getBool(const char* key, bool defaultValue = false) override {
        if (!opened) return defaultValue;
        return prefs.getBool(key, defaultValue);
    }
    
    bool putFloat(const char* key, float value) override {
        if (!opened) return false;
        return prefs.putFloat(key, value) > 0;
    }
    
    float getFloat(const char* key, float defaultValue = 0.0f) override {
        if (!opened) return defaultValue;
        return prefs.getFloat(key, defaultValue);
    }
    
    bool putULong64(const char* key, uint64_t value) override {
        if (!opened) return false;
        return prefs.putULong64(key, value) > 0;
    }
    
    uint64_t getULong64(const char* key, uint64_t defaultValue = 0) override {
        if (!opened) return defaultValue;
        return prefs.getULong64(key, defaultValue);
    }
    
    size_t putBytes(const char* key, const uint8_t* data, size_t len) override {
        if (!opened) return 0;
        return prefs.putBytes(key, data, len);
    }
    
    size_t getBytes(const char* key, uint8_t* buffer, size_t maxLen) override {
        if (!opened) return 0;
        return prefs.getBytes(key, buffer, maxLen);
    }
    
    size_t getBytesLength(const char* key) override {
        if (!opened) return 0;
        return prefs.getBytesLength(key);
    }
    
    bool remove(const char* key) override {
        if (!opened) return false;
        return prefs.remove(key);
    }
    
    bool clear() override {
        if (!opened) return false;
        return prefs.clear();
    }
    
    size_t freeEntries() override {
        if (!opened) return 0;
        return prefs.freeEntries();
    }
};

using PlatformStorage = PreferencesStorage;

} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP32

#endif // DOMOTICS_CORE_STORAGE_ESP32_H
