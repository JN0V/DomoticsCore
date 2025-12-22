#ifndef DOMOTICS_CORE_STORAGE_STUB_H
#define DOMOTICS_CORE_STORAGE_STUB_H

/**
 * @file Storage_Stub.h
 * @brief Stub storage implementation (RAM-only, no persistence).
 */

#if !DOMOTICS_PLATFORM_ESP32 && !DOMOTICS_PLATFORM_ESP8266

namespace DomoticsCore {
namespace HAL {

class RAMOnlyStorage : public IStorage {
private:
    struct Entry {
        String key;
        String value;
    };
    static const size_t MAX_ENTRIES = 32;
    Entry entries[MAX_ENTRIES];
    size_t count = 0;
    String currentNamespace;
    bool opened = false;

    // Create namespaced key for proper isolation
    String makeKey(const char* key) const {
        return currentNamespace + ":" + String(key);
    }

    int findKey(const char* key) {
        String nsKey = makeKey(key);
        for (size_t i = 0; i < count; i++) {
            if (entries[i].key == nsKey) return i;
        }
        return -1;
    }

public:
    bool begin(const char* namespace_name, bool = false) override {
        currentNamespace = namespace_name;
        opened = true;
        return true;
    }

    void end() override {
        opened = false;
    }
    
    bool isKey(const char* key) override {
        if (!opened) return false;
        return findKey(key) >= 0;
    }
    
    bool putString(const char* key, const String& value) override {
        if (!opened) return false;
        int idx = findKey(key);
        if (idx >= 0) {
            entries[idx].value = value;
        } else if (count < MAX_ENTRIES) {
            entries[count].key = makeKey(key);
            entries[count].value = value;
            count++;
        } else {
            return false;
        }
        return true;
    }
    
    String getString(const char* key, const String& defaultValue = "") override {
        if (!opened) return defaultValue;
        int idx = findKey(key);
        return (idx >= 0) ? entries[idx].value : defaultValue;
    }
    
    bool putInt(const char* key, int32_t value) override {
        return putString(key, String(value));
    }
    
    int32_t getInt(const char* key, int32_t defaultValue = 0) override {
        if (!opened) return defaultValue;
        int idx = findKey(key);
        return (idx >= 0) ? entries[idx].value.toInt() : defaultValue;
    }
    
    bool putBool(const char* key, bool value) override {
        return putString(key, value ? "1" : "0");
    }
    
    bool getBool(const char* key, bool defaultValue = false) override {
        if (!opened) return defaultValue;
        int idx = findKey(key);
        return (idx >= 0) ? (entries[idx].value == "1") : defaultValue;
    }
    
    bool putFloat(const char* key, float value) override {
        return putString(key, String(value, 6));
    }
    
    float getFloat(const char* key, float defaultValue = 0.0f) override {
        if (!opened) return defaultValue;
        int idx = findKey(key);
        return (idx >= 0) ? entries[idx].value.toFloat() : defaultValue;
    }
    
    bool putULong64(const char* key, uint64_t value) override {
        return putString(key, String((unsigned long)value));
    }
    
    uint64_t getULong64(const char* key, uint64_t defaultValue = 0) override {
        if (!opened) return defaultValue;
        int idx = findKey(key);
        return (idx >= 0) ? (uint64_t)entries[idx].value.toInt() : defaultValue;
    }
    
    size_t putBytes(const char* key, const uint8_t*, size_t len) override {
        putString(key, String(len));
        return len;
    }
    
    size_t getBytes(const char*, uint8_t*, size_t) override {
        return 0;
    }
    
    size_t getBytesLength(const char*) override {
        return 0;
    }
    
    bool remove(const char* key) override {
        if (!opened) return false;
        int idx = findKey(key);
        if (idx < 0) return false;
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

} // namespace HAL
} // namespace DomoticsCore

#endif // Stub

#endif // DOMOTICS_CORE_STORAGE_STUB_H
