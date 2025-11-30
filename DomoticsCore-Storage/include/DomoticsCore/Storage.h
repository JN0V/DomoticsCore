#pragma once

/**
 * @file Storage.h
 * 
 * @example DomoticsCore-Storage/examples/BasicStorage/src/main.cpp
 * @example DomoticsCore-Storage/examples/NamespaceDemo/src/main.cpp
 * @example DomoticsCore-Storage/examples/StorageWithWebUI/src/main.cpp
 * @brief Declares the DomoticsCore Storage component with HAL abstraction.
 * 
 * Uses HAL::PlatformStorage for multi-platform support:
 * - ESP32: Uses Preferences (NVS)
 * - ESP8266: Uses LittleFS + JSON
 * - Other: RAM-only storage
 */

#include "DomoticsCore/IComponent.h"
#include "DomoticsCore/Timer.h"
#include "Storage_HAL.h"  // Hardware Abstraction Layer for Storage
#include <Arduino.h>
#include <map>
#include <vector>

namespace DomoticsCore {
namespace Components {

// Storage value types
enum class StorageValueType {
    String,
    Integer,
    Float,
    Boolean,
    Blob
};

// Storage entry structure
struct StorageEntry {
    String key;
    StorageValueType type;
    String stringValue;
    int32_t intValue;
    float floatValue;
    bool boolValue;
    std::vector<uint8_t> blobValue;
    size_t size;
    
    StorageEntry() : type(StorageValueType::String), intValue(0), floatValue(0.0f), boolValue(false), size(0) {}
};

// Storage configuration
struct StorageConfig {
    String namespace_name = "domotics";
    bool readOnly = false;
    size_t maxEntries = 100;
    bool autoCommit = true;
};

/**
 * @class DomoticsCore::Components::StorageComponent
 * @brief Key-value storage manager with HAL abstraction for multi-platform support.
 *
 * Opens a storage namespace, provides typed getters/setters, optional auto-commit,
 * and periodic maintenance/status reporting. Uses HAL::PlatformStorage which maps to
 * Preferences (ESP32), LittleFS (ESP8266), or RAM-only storage (other platforms).
 */
class StorageComponent : public IComponent {
private:
    StorageConfig storageConfig;
    HAL::PlatformStorage storage;  // HAL abstraction for multi-platform
    Utils::NonBlockingDelay statusTimer;
    Utils::NonBlockingDelay maintenanceTimer;
    std::map<String, StorageEntry> cache;
    bool isOpen;
    size_t entryCount;
    
public:
    /**
     * Constructor
     * @param config Storage configuration
     */
    StorageComponent(const StorageConfig& config = StorageConfig()) 
        : storageConfig(config), statusTimer(30000), maintenanceTimer(300000), // 5 minutes
          isOpen(false), entryCount(0) {
        // Initialize component metadata immediately for dependency resolution
        metadata.name = "Storage";
        metadata.version = "1.2.1";
        metadata.author = "DomoticsCore";
        metadata.description = "Key-value storage component for preferences and app data";
        metadata.category = "Storage";
        metadata.tags = {"storage", "preferences", "nvs", "settings", "config"};
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_STORAGE, "Initializing...");
        
        // Validate configuration
        if (storageConfig.namespace_name.isEmpty()) {
            DLOG_E(LOG_STORAGE, "Namespace cannot be empty");
            setStatus(ComponentStatus::ConfigError);
            return ComponentStatus::ConfigError;
        }
        
        if (storageConfig.namespace_name.length() > 15) {
            DLOG_E(LOG_STORAGE, "Namespace too long (max 15 chars): %s", storageConfig.namespace_name.c_str());
            setStatus(ComponentStatus::ConfigError);
            return ComponentStatus::ConfigError;
        }
        
        if (storageConfig.maxEntries < 1 || storageConfig.maxEntries > 500) {
            DLOG_E(LOG_STORAGE, "Invalid max_entries: %d (must be 1-500)", storageConfig.maxEntries);
            setStatus(ComponentStatus::ConfigError);
            return ComponentStatus::ConfigError;
        }
        
        // Initialize preferences storage
        ComponentStatus status = initializeStorage();
        setStatus(status);
        return status;
    }
    
    void loop() override {
        if (getLastStatus() != ComponentStatus::Success) return;
        
        // Periodic status reporting
        if (statusTimer.isReady()) {
            updateStorageInfo();
            reportStorageStatus();
        }
        
        // Periodic maintenance
        if (maintenanceTimer.isReady()) {
            performMaintenance();
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_STORAGE, "Shutting down...");
        
        if (isOpen) {
            storage.end();
            isOpen = false;
        }
        cache.clear();
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    
    // Storage operations
    bool putString(const String& key, const String& value) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return false;
        }
        
        size_t written = storage.putString(key.c_str(), value);
        if (written > 0) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::String;
            entry.stringValue = value;
            entry.size = value.length();
            cache[key] = entry;
            
            DLOG_D(LOG_STORAGE, "Stored string '%s' = '%s' (%d bytes)", key.c_str(), value.c_str(), written);
            return true;
        }
        return false;
    }
    
    bool putInt(const String& key, int32_t value) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return false;
        }
        
        size_t written = storage.putInt(key.c_str(), value);
        if (written > 0) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::Integer;
            entry.intValue = value;
            entry.size = sizeof(int32_t);
            cache[key] = entry;
            
            DLOG_D(LOG_STORAGE, "Stored int '%s' = %d", key.c_str(), value);
            return true;
        }
        return false;
    }
    
    bool putFloat(const String& key, float value) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return false;
        }
        
        size_t written = storage.putFloat(key.c_str(), value);
        if (written > 0) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::Float;
            entry.floatValue = value;
            entry.size = sizeof(float);
            cache[key] = entry;
            
            DLOG_D(LOG_STORAGE, "Stored float '%s' = %.2f", key.c_str(), value);
            return true;
        }
        return false;
    }
    
    bool putBool(const String& key, bool value) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return false;
        }
        
        size_t written = storage.putBool(key.c_str(), value);
        if (written > 0) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::Boolean;
            entry.boolValue = value;
            entry.size = sizeof(bool);
            cache[key] = entry;
            
            DLOG_D(LOG_STORAGE, "Stored bool '%s' = %s", key.c_str(), value ? "true" : "false");
            return true;
        }
        return false;
    }
    
    bool putULong64(const String& key, uint64_t value) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return false;
        }
        
        size_t written = storage.putULong64(key.c_str(), value);
        if (written > 0) {
            DLOG_D(LOG_STORAGE, "Stored uint64 '%s' = %llu", key.c_str(), value);
            return true;
        }
        return false;
    }
    
    bool putBlob(const String& key, const uint8_t* data, size_t length) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return false;
        }
        
        size_t written = storage.putBytes(key.c_str(), data, length);
        if (written == length) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::Blob;
            entry.blobValue.assign(data, data + length);
            entry.size = length;
            cache[key] = entry;
            
            DLOG_D(LOG_STORAGE, "Stored blob '%s' (%d bytes)", key.c_str(), length);
            return true;
        }
        return false;
    }
    
    String getString(const String& key, const String& defaultValue = "") {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return defaultValue;
        }
        
        String value = storage.getString(key.c_str(), defaultValue);
        DLOG_D(LOG_STORAGE, "Retrieved string '%s' = '%s'", key.c_str(), value.c_str());
        return value;
    }
    
    int32_t getInt(const String& key, int32_t defaultValue = 0) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return defaultValue;
        }
        
        int32_t value = storage.getInt(key.c_str(), defaultValue);
        DLOG_D(LOG_STORAGE, "Retrieved int '%s' = %d", key.c_str(), value);
        return value;
    }
    
    float getFloat(const String& key, float defaultValue = 0.0f) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return defaultValue;
        }
        
        float value = storage.getFloat(key.c_str(), defaultValue);
        DLOG_D(LOG_STORAGE, "Retrieved float '%s' = %.2f", key.c_str(), value);
        return value;
    }
    
    bool getBool(const String& key, bool defaultValue = false) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return defaultValue;
        }
        
        bool value = storage.getBool(key.c_str(), defaultValue);
        DLOG_D(LOG_STORAGE, "Retrieved bool '%s' = %s", key.c_str(), value ? "true" : "false");
        return value;
    }
    
    uint64_t getULong64(const String& key, uint64_t defaultValue = 0) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return defaultValue;
        }
        
        uint64_t value = storage.getULong64(key.c_str(), defaultValue);
        DLOG_D(LOG_STORAGE, "Retrieved uint64 '%s' = %llu", key.c_str(), value);
        return value;
    }
    
    size_t getBlob(const String& key, uint8_t* buffer, size_t maxLength) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return 0;
        }
        
        size_t length = storage.getBytesLength(key.c_str());
        if (length == 0) {
            DLOG_D(LOG_STORAGE, "Blob '%s' not found", key.c_str());
            return 0;
        }
        
        if (length > maxLength) {
            DLOG_W(LOG_STORAGE, "Blob '%s' too large (%d > %d)", key.c_str(), length, maxLength);
            length = maxLength;
        }
        
        size_t read = storage.getBytes(key.c_str(), buffer, length);
        DLOG_D(LOG_STORAGE, "Retrieved blob '%s' (%d bytes)", key.c_str(), read);
        return read;
    }
    
    bool remove(const String& key) {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return false;
        }
        
        bool success = storage.remove(key.c_str());
        if (success) {
            cache.erase(key);
            DLOG_I(LOG_STORAGE, "Removed key: %s", key.c_str());
        } else {
            DLOG_E(LOG_STORAGE, "Failed to remove key: %s", key.c_str());
        }
        return success;
    }
    
    bool clear() {
        if (!isOpen) {
            DLOG_E(LOG_STORAGE, "Not open");
            return false;
        }
        
        bool success = storage.clear();
        if (success) {
            cache.clear();
            DLOG_I(LOG_STORAGE, "Cleared all entries");
        } else {
            DLOG_E(LOG_STORAGE, "Failed to clear");
        }
        return success;
    }
    
    bool exists(const String& key) {
        if (!isOpen) return false;
        return storage.isKey(key.c_str());
    }
    
    // Storage information
    bool isOpenStorage() const { return isOpen; }
    size_t getEntryCount() const { return entryCount; }
    size_t getFreeEntries() const { 
        return storageConfig.maxEntries > entryCount ? storageConfig.maxEntries - entryCount : 0; 
    }
    
    String getNamespace() const {
        return storageConfig.namespace_name;
    }
    
    String getStorageInfo() const {
        String info = "Storage: HAL PlatformStorage";
        info += "\nNamespace: " + storageConfig.namespace_name;
        info += "\nOpen: " + String(isOpen ? "Yes" : "No");
        info += "\nRead-only: " + String(storageConfig.readOnly ? "Yes" : "No");
        if (isOpen) {
            info += "\nEntries: " + String(entryCount) + "/" + String(storageConfig.maxEntries);
            info += "\nCached: " + String(cache.size());
        }
        return info;
    }
    
    std::vector<String> getKeys() {
        std::vector<String> keys;
        if (!isOpen) return keys;
        
        // Note: Storage backend may not provide a direct way to list all keys
        // We return cached keys instead
        for (const auto& pair : cache) {
            keys.push_back(pair.first);
        }
        
        return keys;
    }

private:
    ComponentStatus initializeStorage() {
        DLOG_I(LOG_STORAGE, "Initializing storage via HAL...");
        
        // Open preferences with namespace
        bool success = storage.begin(storageConfig.namespace_name.c_str(), storageConfig.readOnly);
        
        if (success) {
            isOpen = true;
            updateStorageInfo();
            DLOG_I(LOG_STORAGE, "Storage opened successfully (namespace: %s)", 
                   storageConfig.namespace_name.c_str());
            return ComponentStatus::Success;
        } else {
            DLOG_E(LOG_STORAGE, "Failed to open preferences");
            return ComponentStatus::HardwareError;
        }
    }

    void updateStorageInfo() {
        if (!isOpen) {
            entryCount = 0;
            return;
        }
        
        // Update entry count from cache
        entryCount = cache.size();
        
        DLOG_D(LOG_STORAGE, "Info updated: %d entries cached", entryCount);
    }

    void reportStorageStatus() {
        if (!isOpen) {
            DLOG_W(LOG_STORAGE, "Not open");
            return;
        }
        
        DLOG_I(LOG_STORAGE, "=== Status ===");
        DLOG_I(LOG_STORAGE, "%s", getStorageInfo().c_str());
        
        // Check storage usage
        float usagePercent = (float)entryCount / (float)storageConfig.maxEntries * 100.0f;
        if (usagePercent > 90.0f) {
            DLOG_W(LOG_STORAGE, "Usage high: %.1f%%", usagePercent);
        }
    }

    void performMaintenance() {
        if (!isOpen) return;
        
        DLOG_D(LOG_STORAGE, "Performing maintenance...");
        
        // Update storage information
        updateStorageInfo();
        
        // Log cache statistics
        DLOG_D(LOG_STORAGE, "Cache contains %d entries", cache.size());
        
        // Check for storage health
        if (entryCount >= storageConfig.maxEntries) {
            DLOG_W(LOG_STORAGE, "At maximum capacity (%d entries)", entryCount);
        }
    }

#if DOMOTICSCORE_WEBUI_ENABLED
    // Remove WebUI methods from core Storage component - they should be in wrapper classes only
    /*
    WebUISection getWebUISection() WEBUI_OVERRIDE {
        WebUISection section("storage", "Storage Management", "fas fa-database", "system");
        
        section.withField(WebUIField("namespace", "Namespace", WebUIFieldType::Display, 
                                   storageConfig.namespace_name, "", true))
               .withField(WebUIField("entries", "Used Entries", WebUIFieldType::Display, 
                                   String(getEntryCount()), "", true))
               .withField(WebUIField("free_entries", "Free Entries", WebUIFieldType::Display, 
                                   String(getFreeEntries()), "", true))
               .withField(WebUIField("max_entries", "Max Entries", WebUIFieldType::Display, 
                                   String(storageConfig.maxEntries), "", true))
               .withField(WebUIField("readonly", "Read Only", WebUIFieldType::Display, 
                                   storageConfig.readOnly ? "Yes" : "No", "", true))
               .withField(WebUIField("auto_commit", "Auto Commit", WebUIFieldType::Display, 
                                   storageConfig.autoCommit ? "Yes" : "No", "", true))
               .withAPI("/api/storage")
               .withRealTime(10000);
        
        return section;
    }

    String handleWebUIRequest(const String& endpoint, const String& method, 
                            const std::map<String, String>& params) WEBUI_OVERRIDE {
        if (endpoint == "/api/storage") {
            if (method == "GET") {
                return "{\"namespace\":\"" + storageConfig.namespace_name + "\","
                       "\"entries\":" + String(getEntryCount()) + ","
                       "\"free_entries\":" + String(getFreeEntries()) + ","
                       "\"max_entries\":" + String(storageConfig.maxEntries) + ","
                       "\"readonly\":" + (storageConfig.readOnly ? "true" : "false") + ","
                       "\"auto_commit\":" + (storageConfig.autoCommit ? "true" : "false") + "}";
            } else if (method == "POST") {
                auto actionIt = params.find("action");
                if (actionIt != params.end()) {
                    if (actionIt->second == "clear") {
                        bool success = clear();
                        return "{\"status\":\"" + String(success ? "success" : "error") + "\","
                               "\"message\":\"" + String(success ? "Storage cleared" : "Failed to clear storage") + "\"}";
                    } else if (actionIt->second == "get_keys") {
                        std::vector<String> keys = getKeys();
                        String result = "{\"keys\":[";
                        for (size_t i = 0; i < keys.size(); i++) {
                            if (i > 0) result += ",";
                            result += "\"" + keys[i] + "\"";
                        }
                        result += "]}";
                        return result;
                    }
                }
                return "{\"status\":\"error\",\"message\":\"Unknown action\"}";
            }
        }
        return "{\"error\":\"not found\"}";
    }
    */
#endif
};

} // namespace Components
} // namespace DomoticsCore
