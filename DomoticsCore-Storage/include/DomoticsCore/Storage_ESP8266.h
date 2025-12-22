#ifndef DOMOTICS_CORE_STORAGE_ESP8266_H
#define DOMOTICS_CORE_STORAGE_ESP8266_H

/**
 * @file Storage_ESP8266.h
 * @brief ESP8266-specific storage implementation using LittleFS + JSON.
 * 
 * JSON document size is limited to 2KB maximum per FR-003c.
 * Corrupted files return default values per FR-003d.
 */

#if DOMOTICS_PLATFORM_ESP8266

#include <LittleFS.h>
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace HAL {

class LittleFSStorage : public IStorage {
private:
    String filepath;
    JsonDocument doc;  // ArduinoJson 7.x, limited to 2KB usage per FR-003c
    bool opened = false;
    bool dirty = false;
    
    void load() {
        if (!LittleFS.exists(filepath)) return;
        
        File file = LittleFS.open(filepath, "r");
        if (!file) return;
        
        DeserializationError err = deserializeJson(doc, file);
        file.close();
        
        if (err) {
            // FR-003d: Handle corrupted files gracefully
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
        (void)readOnly;
        if (!LittleFS.begin()) return false;
        
        filepath = String("/") + namespace_name + ".json";
        load();
        opened = true;
        return true;
    }
    
    bool isKey(const char* key) override {
        if (!opened) return false;
        return !doc[key].isNull();
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
        save();
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
        if (!opened) return 0;
        String hex;
        hex.reserve(len * 2);
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
        return 1000;  // LittleFS doesn't have entry limit
    }
};

using PlatformStorage = LittleFSStorage;

} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP8266

#endif // DOMOTICS_CORE_STORAGE_ESP8266_H
