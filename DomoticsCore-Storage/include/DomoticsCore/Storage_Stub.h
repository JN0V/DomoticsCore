#ifndef DOMOTICS_CORE_STORAGE_STUB_H
#define DOMOTICS_CORE_STORAGE_STUB_H

/**
 * @file Storage_Stub.h
 * @brief Stub storage implementation (RAM-only, no persistence).
 */

#if !DOMOTICS_PLATFORM_ESP32 && !DOMOTICS_PLATFORM_ESP8266

#include <cstring>  // strlen, strncpy
#include <cstdlib>  // strtoull
#include <cstdio>   // snprintf

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
        // BUG-16: Use proper uint64 serialization (no truncation to unsigned long)
        char buf[21]; // max uint64 is 20 digits + null
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
        return putString(key, String(buf));
    }

    uint64_t getULong64(const char* key, uint64_t defaultValue = 0) override {
        if (!opened) return defaultValue;
        int idx = findKey(key);
        // BUG-16: Use strtoull instead of toInt() which truncates
        return (idx >= 0) ? strtoull(entries[idx].value.c_str(), nullptr, 10) : defaultValue;
    }
    
    static constexpr const char* BYTES_PREFIX = "BYTES:";
    static constexpr size_t BYTES_PREFIX_LEN = 6; // strlen("BYTES:")
    static constexpr size_t MAX_BLOB_SIZE = 256; ///< Max blob size for test stub (avoids VLA)

    // BUG-17: Store actual byte data as hex-encoded string
    size_t putBytes(const char* key, const uint8_t* data, size_t len) override {
        if (!opened || !data) return 0;
        if (len > MAX_BLOB_SIZE) return 0; // Guard: reject oversized blobs
        // Encode bytes as hex string using fixed-size buffer (no VLA, no String concat in loop)
        char hex[MAX_BLOB_SIZE * 2 + 1];
        for (size_t i = 0; i < len; i++) {
            snprintf(hex + i * 2, 3, "%02X", data[i]);
        }
        hex[len * 2] = '\0';
        // Store with BYTES_PREFIX to distinguish from regular strings
        char prefixed[BYTES_PREFIX_LEN + MAX_BLOB_SIZE * 2 + 1];
        snprintf(prefixed, sizeof(prefixed), "%s%s", BYTES_PREFIX, hex);
        if (putString(key, String(prefixed))) return len;
        return 0;
    }

    size_t getBytes(const char* key, uint8_t* buffer, size_t maxLen) override {
        if (!opened || !buffer) return 0;
        int idx = findKey(key);
        if (idx < 0) return 0;
        const String& val = entries[idx].value;
        if (!val.startsWith(BYTES_PREFIX)) return 0;
        const char* hex = val.c_str() + BYTES_PREFIX_LEN;
        size_t hexLen = strlen(hex);
        size_t byteLen = hexLen / 2;
        if (byteLen > maxLen) byteLen = maxLen;
        for (size_t i = 0; i < byteLen; i++) {
            char hi = hex[i * 2];
            char lo = hex[i * 2 + 1];
            auto hexVal = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'A' && c <= 'F') return 10 + c - 'A';
                if (c >= 'a' && c <= 'f') return 10 + c - 'a';
                return 0;
            };
            buffer[i] = (hexVal(hi) << 4) | hexVal(lo);
        }
        return byteLen;
    }

    size_t getBytesLength(const char* key) override {
        if (!opened) return 0;
        int idx = findKey(key);
        if (idx < 0) return 0;
        const String& val = entries[idx].value;
        if (!val.startsWith(BYTES_PREFIX)) return 0;
        return (val.length() - BYTES_PREFIX_LEN) / 2; // hex chars / 2
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

    size_t maxEntries() override {
        return MAX_ENTRIES;
    }
};

using PlatformStorage = RAMOnlyStorage;

} // namespace HAL
} // namespace DomoticsCore

#endif // Stub

#endif // DOMOTICS_CORE_STORAGE_STUB_H
