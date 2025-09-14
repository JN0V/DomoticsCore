#pragma once

#include "IComponent.h"
#include "../Utils/Timer.h"
#include <Arduino.h>
#include <Preferences.h>
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
 * Storage management component for preferences and application data
 * Provides key-value storage using ESP32 Preferences library
 * Header-only implementation - only compiled when included
 */
class StorageComponent : public IComponent {
private:
    StorageConfig storageConfig;
    Preferences preferences;
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
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_CORE, "Storage component initializing...");
        
        // Initialize component metadata
        metadata.name = "Storage";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "Key-value storage component for preferences and app data";
        metadata.category = "Storage";
        metadata.tags = {"storage", "preferences", "nvs", "settings", "config"};
        
        // Define configuration parameters
        config.defineParameter(ConfigParam("namespace", ConfigType::String, false, 
                                         storageConfig.namespace_name, "Storage namespace (max 15 chars)")
                              .length(15)); // NVS namespace limit
        config.defineParameter(ConfigParam("read_only", ConfigType::Boolean, false, 
                                         storageConfig.readOnly ? "true" : "false", 
                                         "Open storage in read-only mode"));
        config.defineParameter(ConfigParam("max_entries", ConfigType::Integer, false, 
                                         String(storageConfig.maxEntries), "Maximum number of entries")
                              .min(1).max(500));
        config.defineParameter(ConfigParam("auto_commit", ConfigType::Boolean, false, 
                                         storageConfig.autoCommit ? "true" : "false", 
                                         "Automatically commit changes"));
        
        // Validate configuration
        auto validation = validateConfig();
        if (!validation.isValid()) {
            DLOG_E(LOG_CORE, "Storage config validation failed: %s", validation.toString().c_str());
            setStatus(ComponentStatus::ConfigError);
            return ComponentStatus::ConfigError;
        }
        
        // Initialize preferences storage
        ComponentStatus status = initializePreferences();
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
        DLOG_I(LOG_CORE, "Storage component shutting down...");
        
        if (isOpen) {
            preferences.end();
            isOpen = false;
        }
        cache.clear();
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    String getName() const override {
        return metadata.name;
    }
    
    // Storage operations
    bool putString(const String& key, const String& value) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return false;
        }
        
        size_t written = preferences.putString(key.c_str(), value);
        if (written > 0) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::String;
            entry.stringValue = value;
            entry.size = value.length();
            cache[key] = entry;
            
            DLOG_D(LOG_CORE, "Stored string '%s' = '%s' (%d bytes)", key.c_str(), value.c_str(), written);
            return true;
        }
        return false;
    }
    
    bool putInt(const String& key, int32_t value) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return false;
        }
        
        size_t written = preferences.putInt(key.c_str(), value);
        if (written > 0) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::Integer;
            entry.intValue = value;
            entry.size = sizeof(int32_t);
            cache[key] = entry;
            
            DLOG_D(LOG_CORE, "Stored int '%s' = %d", key.c_str(), value);
            return true;
        }
        return false;
    }
    
    bool putFloat(const String& key, float value) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return false;
        }
        
        size_t written = preferences.putFloat(key.c_str(), value);
        if (written > 0) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::Float;
            entry.floatValue = value;
            entry.size = sizeof(float);
            cache[key] = entry;
            
            DLOG_D(LOG_CORE, "Stored float '%s' = %.2f", key.c_str(), value);
            return true;
        }
        return false;
    }
    
    bool putBool(const String& key, bool value) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return false;
        }
        
        size_t written = preferences.putBool(key.c_str(), value);
        if (written > 0) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::Boolean;
            entry.boolValue = value;
            entry.size = sizeof(bool);
            cache[key] = entry;
            
            DLOG_D(LOG_CORE, "Stored bool '%s' = %s", key.c_str(), value ? "true" : "false");
            return true;
        }
        return false;
    }
    
    bool putBlob(const String& key, const uint8_t* data, size_t length) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return false;
        }
        
        size_t written = preferences.putBytes(key.c_str(), data, length);
        if (written == length) {
            StorageEntry entry;
            entry.key = key;
            entry.type = StorageValueType::Blob;
            entry.blobValue.assign(data, data + length);
            entry.size = length;
            cache[key] = entry;
            
            DLOG_D(LOG_CORE, "Stored blob '%s' (%d bytes)", key.c_str(), length);
            return true;
        }
        return false;
    }
    
    String getString(const String& key, const String& defaultValue = "") {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return defaultValue;
        }
        
        String value = preferences.getString(key.c_str(), defaultValue);
        DLOG_D(LOG_CORE, "Retrieved string '%s' = '%s'", key.c_str(), value.c_str());
        return value;
    }
    
    int32_t getInt(const String& key, int32_t defaultValue = 0) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return defaultValue;
        }
        
        int32_t value = preferences.getInt(key.c_str(), defaultValue);
        DLOG_D(LOG_CORE, "Retrieved int '%s' = %d", key.c_str(), value);
        return value;
    }
    
    float getFloat(const String& key, float defaultValue = 0.0f) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return defaultValue;
        }
        
        float value = preferences.getFloat(key.c_str(), defaultValue);
        DLOG_D(LOG_CORE, "Retrieved float '%s' = %.2f", key.c_str(), value);
        return value;
    }
    
    bool getBool(const String& key, bool defaultValue = false) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return defaultValue;
        }
        
        bool value = preferences.getBool(key.c_str(), defaultValue);
        DLOG_D(LOG_CORE, "Retrieved bool '%s' = %s", key.c_str(), value ? "true" : "false");
        return value;
    }
    
    size_t getBlob(const String& key, uint8_t* buffer, size_t maxLength) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return 0;
        }
        
        size_t length = preferences.getBytesLength(key.c_str());
        if (length == 0) {
            DLOG_D(LOG_CORE, "Blob '%s' not found", key.c_str());
            return 0;
        }
        
        if (length > maxLength) {
            DLOG_W(LOG_CORE, "Blob '%s' too large (%d > %d)", key.c_str(), length, maxLength);
            length = maxLength;
        }
        
        size_t read = preferences.getBytes(key.c_str(), buffer, length);
        DLOG_D(LOG_CORE, "Retrieved blob '%s' (%d bytes)", key.c_str(), read);
        return read;
    }
    
    bool remove(const String& key) {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return false;
        }
        
        bool success = preferences.remove(key.c_str());
        if (success) {
            cache.erase(key);
            DLOG_I(LOG_CORE, "Removed key: %s", key.c_str());
        } else {
            DLOG_E(LOG_CORE, "Failed to remove key: %s", key.c_str());
        }
        return success;
    }
    
    bool clear() {
        if (!isOpen) {
            DLOG_E(LOG_CORE, "Storage not open");
            return false;
        }
        
        bool success = preferences.clear();
        if (success) {
            cache.clear();
            DLOG_I(LOG_CORE, "Cleared all storage entries");
        } else {
            DLOG_E(LOG_CORE, "Failed to clear storage");
        }
        return success;
    }
    
    bool exists(const String& key) {
        if (!isOpen) return false;
        return preferences.isKey(key.c_str());
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
        String info = "Storage: NVS Preferences";
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
        
        // Note: ESP32 Preferences doesn't provide a direct way to list all keys
        // We return cached keys instead
        for (const auto& pair : cache) {
            keys.push_back(pair.first);
        }
        
        return keys;
    }

private:
    ComponentStatus initializePreferences() {
        DLOG_I(LOG_CORE, "Initializing NVS preferences storage...");
        
        // Open preferences with namespace
        bool success = preferences.begin(storageConfig.namespace_name.c_str(), storageConfig.readOnly);
        
        if (success) {
            isOpen = true;
            updateStorageInfo();
            DLOG_I(LOG_CORE, "Preferences storage opened successfully (namespace: %s)", 
                   storageConfig.namespace_name.c_str());
            return ComponentStatus::Success;
        } else {
            DLOG_E(LOG_CORE, "Failed to open preferences storage");
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
        
        DLOG_D(LOG_CORE, "Storage info updated: %d entries cached", entryCount);
    }

    void reportStorageStatus() {
        if (!isOpen) {
            DLOG_W(LOG_CORE, "Storage not open");
            return;
        }
        
        DLOG_I(LOG_CORE, "=== Storage Status ===");
        DLOG_I(LOG_CORE, "%s", getStorageInfo().c_str());
        
        // Check storage usage
        float usagePercent = (float)entryCount / (float)storageConfig.maxEntries * 100.0f;
        if (usagePercent > 90.0f) {
            DLOG_W(LOG_CORE, "Storage usage high: %.1f%%", usagePercent);
        }
    }

    void performMaintenance() {
        if (!isOpen) return;
        
        DLOG_D(LOG_CORE, "Performing storage maintenance...");
        
        // Update storage information
        updateStorageInfo();
        
        // Log cache statistics
        DLOG_D(LOG_CORE, "Cache contains %d entries", cache.size());
        
        // Check for storage health
        if (entryCount >= storageConfig.maxEntries) {
            DLOG_W(LOG_CORE, "Storage at maximum capacity (%d entries)", entryCount);
        }
    }

    ValidationResult validateConfig() const {
        // Validate namespace name
        if (storageConfig.namespace_name.isEmpty()) {
            return ValidationResult(ComponentStatus::ConfigError, "Namespace cannot be empty", "namespace");
        } else if (storageConfig.namespace_name.length() > 15) {
            return ValidationResult(ComponentStatus::ConfigError, "Namespace too long (max 15 characters)", "namespace");
        }
        
        // Validate max entries
        if (storageConfig.maxEntries == 0) {
            return ValidationResult(ComponentStatus::ConfigError, "Max entries must be greater than 0", "max_entries");
        }
        
        // Return success if all validations pass
        return ValidationResult(ComponentStatus::Success);
    }
};

} // namespace Components
} // namespace DomoticsCore
