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
#include <Arduino.h>

#if DOMOTICS_PLATFORM_ESP32
    #include <Preferences.h>
#elif DOMOTICS_PLATFORM_ESP8266
    #include <LittleFS.h>
    #include <ArduinoJson.h>
#endif

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

// ============================================================================
// ESP32 Implementation (Preferences/NVS)
// ============================================================================

#if DOMOTICS_PLATFORM_ESP32

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

// Default storage type for ESP32
using PlatformStorage = PreferencesStorage;

#endif // ESP32

// ============================================================================
// ESP8266 Implementation (LittleFS + JSON)
// ============================================================================

#if DOMOTICS_PLATFORM_ESP8266

class LittleFSStorage : public IStorage {
private:
    String filepath;
    JsonDocument doc;
    bool opened = false;
    bool dirty = false;
    
    void load() {
        if (!LittleFS.exists(filepath)) return;
        
        File file = LittleFS.open(filepath, "r");
        if (!file) return;
        
        DeserializationError err = deserializeJson(doc, file);
        file.close();
        
        if (err) {
            doc.clear();
        }
    }
    
    void save() {
        if (!dirty) return;
        
        File file = LittleFS.open(filepath, "w");
        if (!file) return;
        
        serializeJson(doc, file);
        file.close();
        dirty = false;
    }
    
public:
    bool begin(const char* namespace_name, bool readOnly = false) override {
        (void)readOnly;  // LittleFS doesn't support read-only mode
        if (!LittleFS.begin()) return false;
        
        filepath = String("/") + namespace_name + ".json";
        load();
        opened = true;
        return true;
    }
    
    bool isKey(const char* key) override {
        if (!opened) return false;
        return doc.containsKey(key);
    }
    
    void end() override {
        if (opened) {
            save();
            opened = false;
        }
    }
    
    bool putString(const char* key, const String& value) override {
        if (!opened) return false;
        doc[key] = value;
        dirty = true;
        save();  // Immediate save for reliability
        return true;
    }
    
    String getString(const char* key, const String& defaultValue = "") override {
        if (!opened) return defaultValue;
        return doc[key] | defaultValue;
    }
    
    bool putInt(const char* key, int32_t value) override {
        if (!opened) return false;
        doc[key] = value;
        dirty = true;
        save();
        return true;
    }
    
    int32_t getInt(const char* key, int32_t defaultValue = 0) override {
        if (!opened) return defaultValue;
        return doc[key] | defaultValue;
    }
    
    bool putBool(const char* key, bool value) override {
        if (!opened) return false;
        doc[key] = value;
        dirty = true;
        save();
        return true;
    }
    
    bool getBool(const char* key, bool defaultValue = false) override {
        if (!opened) return defaultValue;
        return doc[key] | defaultValue;
    }
    
    bool putFloat(const char* key, float value) override {
        if (!opened) return false;
        doc[key] = value;
        dirty = true;
        save();
        return true;
    }
    
    float getFloat(const char* key, float defaultValue = 0.0f) override {
        if (!opened) return defaultValue;
        return doc[key] | defaultValue;
    }
    
    bool putULong64(const char* key, uint64_t value) override {
        if (!opened) return false;
        doc[key] = value;
        dirty = true;
        save();
        return true;
    }
    
    uint64_t getULong64(const char* key, uint64_t defaultValue = 0) override {
        if (!opened) return defaultValue;
        return doc[key] | defaultValue;
    }
    
    size_t putBytes(const char* key, const uint8_t* data, size_t len) override {
        // Store as base64 or hex string in JSON
        if (!opened) return 0;
        String hex;
        for (size_t i = 0; i < len; i++) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02X", data[i]);
            hex += buf;
        }
        doc[key] = hex;
        dirty = true;
        save();
        return len;
    }
    
    size_t getBytes(const char* key, uint8_t* buffer, size_t maxLen) override {
        if (!opened) return 0;
        String hex = doc[key] | "";
        size_t len = hex.length() / 2;
        if (len > maxLen) len = maxLen;
        for (size_t i = 0; i < len; i++) {
            char buf[3] = {hex[i*2], hex[i*2+1], 0};
            buffer[i] = strtol(buf, nullptr, 16);
        }
        return len;
    }
    
    size_t getBytesLength(const char* key) override {
        if (!opened) return 0;
        String hex = doc[key] | "";
        return hex.length() / 2;
    }
    
    bool remove(const char* key) override {
        if (!opened) return false;
        doc.remove(key);
        dirty = true;
        save();
        return true;
    }
    
    bool clear() override {
        if (!opened) return false;
        doc.clear();
        dirty = true;
        save();
        return true;
    }
    
    size_t freeEntries() override {
        // LittleFS doesn't have entry limit like NVS
        return 1000;  // Return large number
    }
};

// Default storage type for ESP8266
using PlatformStorage = LittleFSStorage;

#endif // ESP8266

// ============================================================================
// Stub Implementation (No persistent storage)
// ============================================================================

#if !DOMOTICS_PLATFORM_ESP32 && !DOMOTICS_PLATFORM_ESP8266

class RAMOnlyStorage : public IStorage {
private:
    // Simple in-memory storage (cleared on reset)
    struct Entry {
        String key;
        String value;
    };
    static const size_t MAX_ENTRIES = 32;
    Entry entries[MAX_ENTRIES];
    size_t count = 0;
    
    int findKey(const char* key) {
        for (size_t i = 0; i < count; i++) {
            if (entries[i].key == key) return i;
        }
        return -1;
    }
    
public:
    bool begin(const char*, bool = false) override { return true; }
    void end() override {}
    
    bool isKey(const char* key) override {
        return findKey(key) >= 0;
    }
    
    bool putString(const char* key, const String& value) override {
        int idx = findKey(key);
        if (idx >= 0) {
            entries[idx].value = value;
        } else if (count < MAX_ENTRIES) {
            entries[count].key = key;
            entries[count].value = value;
            count++;
        } else {
            return false;
        }
        return true;
    }
    
    String getString(const char* key, const String& defaultValue = "") override {
        int idx = findKey(key);
        return (idx >= 0) ? entries[idx].value : defaultValue;
    }
    
    bool putInt(const char* key, int32_t value) override {
        return putString(key, String(value));
    }
    
    int32_t getInt(const char* key, int32_t defaultValue = 0) override {
        int idx = findKey(key);
        return (idx >= 0) ? entries[idx].value.toInt() : defaultValue;
    }
    
    bool putBool(const char* key, bool value) override {
        return putString(key, value ? "1" : "0");
    }
    
    bool getBool(const char* key, bool defaultValue = false) override {
        int idx = findKey(key);
        return (idx >= 0) ? (entries[idx].value == "1") : defaultValue;
    }
    
    bool putFloat(const char* key, float value) override {
        return putString(key, String(value, 6));
    }
    
    float getFloat(const char* key, float defaultValue = 0.0f) override {
        int idx = findKey(key);
        return (idx >= 0) ? entries[idx].value.toFloat() : defaultValue;
    }
    
    bool putULong64(const char* key, uint64_t value) override {
        return putString(key, String((unsigned long)value));  // Simplified
    }
    
    uint64_t getULong64(const char* key, uint64_t defaultValue = 0) override {
        int idx = findKey(key);
        return (idx >= 0) ? (uint64_t)entries[idx].value.toInt() : defaultValue;
    }
    
    size_t putBytes(const char* key, const uint8_t*, size_t len) override {
        // Stub: just track the key exists
        putString(key, String(len));
        return len;
    }
    
    size_t getBytes(const char*, uint8_t*, size_t) override {
        return 0;  // Stub: no blob support
    }
    
    size_t getBytesLength(const char*) override {
        return 0;  // Stub: no blob support
    }
    
    bool remove(const char* key) override {
        int idx = findKey(key);
        if (idx < 0) return false;
        // Shift entries
        for (size_t i = idx; i < count - 1; i++) {
            entries[i] = entries[i + 1];
        }
        count--;
        return true;
    }
    
    bool clear() override {
        count = 0;
        return true;
    }
    
    size_t freeEntries() override {
        return MAX_ENTRIES - count;
    }
};

using PlatformStorage = RAMOnlyStorage;

#endif // Stub

} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_HAL_STORAGE_H
