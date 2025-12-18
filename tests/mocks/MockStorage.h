#pragma once

/**
 * @file MockStorage.h
 * @brief Mock Storage for isolated unit testing without real NVS/Preferences
 * 
 * This mock replaces StorageComponent to allow testing components that
 * persist data without requiring real flash storage.
 */

#include <Arduino.h>
#include <map>

namespace DomoticsCore {
namespace Mocks {

/**
 * @brief Mock Storage that uses in-memory map instead of NVS
 */
class MockStorage {
public:
    // In-memory storage by namespace
    static std::map<String, std::map<String, String>> storage;
    
    // Operation tracking
    static int putCount;
    static int getCount;
    static int removeCount;
    
    // Current namespace
    static String currentNamespace;
    
    // Storage operations
    static bool begin(const String& ns = "default") {
        currentNamespace = ns;
        return true;
    }
    
    static void end() {
        currentNamespace = "";
    }
    
    static bool putString(const String& key, const String& value) {
        storage[currentNamespace][key] = value;
        putCount++;
        return true;
    }
    
    static String getString(const String& key, const String& defaultValue = "") {
        getCount++;
        auto nsIt = storage.find(currentNamespace);
        if (nsIt != storage.end()) {
            auto keyIt = nsIt->second.find(key);
            if (keyIt != nsIt->second.end()) {
                return keyIt->second;
            }
        }
        return defaultValue;
    }
    
    static bool putInt(const String& key, int value) {
        return putString(key, String(value));
    }
    
    static int getInt(const String& key, int defaultValue = 0) {
        String val = getString(key, String(defaultValue));
        return val.toInt();
    }
    
    static bool putFloat(const String& key, float value) {
        return putString(key, String(value, 6));
    }
    
    static float getFloat(const String& key, float defaultValue = 0.0f) {
        String val = getString(key, String(defaultValue, 6));
        return val.toFloat();
    }
    
    static bool putBool(const String& key, bool value) {
        return putString(key, value ? "1" : "0");
    }
    
    static bool getBool(const String& key, bool defaultValue = false) {
        String val = getString(key, defaultValue ? "1" : "0");
        return val == "1" || val == "true";
    }
    
    static bool remove(const String& key) {
        removeCount++;
        auto nsIt = storage.find(currentNamespace);
        if (nsIt != storage.end()) {
            nsIt->second.erase(key);
            return true;
        }
        return false;
    }
    
    static bool clear() {
        storage[currentNamespace].clear();
        return true;
    }
    
    static bool exists(const String& key) {
        auto nsIt = storage.find(currentNamespace);
        if (nsIt != storage.end()) {
            return nsIt->second.find(key) != nsIt->second.end();
        }
        return false;
    }
    
    // Test control methods
    static void reset() {
        storage.clear();
        currentNamespace = "";
        putCount = 0;
        getCount = 0;
        removeCount = 0;
    }
    
    // Pre-populate storage for tests
    static void preload(const String& ns, const String& key, const String& value) {
        storage[ns][key] = value;
    }
    
    // Verification helpers
    static bool hasKey(const String& ns, const String& key) {
        auto nsIt = storage.find(ns);
        if (nsIt != storage.end()) {
            return nsIt->second.find(key) != nsIt->second.end();
        }
        return false;
    }
    
    static String getValue(const String& ns, const String& key) {
        auto nsIt = storage.find(ns);
        if (nsIt != storage.end()) {
            auto keyIt = nsIt->second.find(key);
            if (keyIt != nsIt->second.end()) {
                return keyIt->second;
            }
        }
        return "";
    }
    
    static int getKeyCount(const String& ns) {
        auto nsIt = storage.find(ns);
        return nsIt != storage.end() ? nsIt->second.size() : 0;
    }
};

// Static member initialization
std::map<String, std::map<String, String>> MockStorage::storage;
String MockStorage::currentNamespace = "";
int MockStorage::putCount = 0;
int MockStorage::getCount = 0;
int MockStorage::removeCount = 0;

} // namespace Mocks
} // namespace DomoticsCore
