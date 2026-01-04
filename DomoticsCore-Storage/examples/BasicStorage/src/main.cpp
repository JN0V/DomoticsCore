#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Custom application log tag
#define LOG_APP "APP"

Core core;

/**
 * Storage demonstration component showcasing preferences and app data management
 */
class StorageDemoComponent : public IComponent {
private:
    std::unique_ptr<StorageComponent> storageManager;
    Utils::NonBlockingDelay demoTimer;
    Utils::NonBlockingDelay statusTimer;
    int demoPhase;
    int sessionCounter;
    
public:
    StorageDemoComponent() : demoTimer(8000), statusTimer(5000), demoPhase(0), sessionCounter(0) {
        // Set component metadata
        metadata.name = "StorageDemo";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "Storage component demonstration with preferences and app data";
        metadata.category = "Demo";
        metadata.tags = {"storage", "demo", "preferences", "settings"};
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_APP, "[StorageDemo] Initializing storage demonstration component...");
        
        // Configure storage for preferences
        StorageConfig config;
        config.namespace_name = "demo_app";
        config.readOnly = false;
        config.maxEntries = 50;
        config.autoCommit = true;
        
        // Create storage manager
        storageManager.reset(new StorageComponent(config));
        
        // Initialize storage manager
        ComponentStatus status = storageManager->begin();
        if (status != ComponentStatus::Success) {
            DLOG_E(LOG_APP, "[StorageDemo] Failed to initialize storage manager: %s", 
                         statusToString(status));
            setStatus(status);
            return status;
        }
        
        // Load session counter from storage
        sessionCounter = storageManager->getInt("session_count", 0);
        sessionCounter++;
        storageManager->putInt("session_count", sessionCounter);
        
        DLOG_I(LOG_APP, "[StorageDemo] Storage manager initialized successfully");
        DLOG_I(LOG_APP, "[StorageDemo] Session #%d started", sessionCounter);
        DLOG_I(LOG_APP, "[StorageDemo] Demo phases:");
        DLOG_I(LOG_APP, "[StorageDemo] - Phase 1: Basic preferences (strings, integers)");
        DLOG_I(LOG_APP, "[StorageDemo] - Phase 2: Advanced data types (floats, booleans)");
        DLOG_I(LOG_APP, "[StorageDemo] - Phase 3: Binary data (blobs)");
        DLOG_I(LOG_APP, "[StorageDemo] - Phase 4: Data management (listing, cleanup)");
        
        // Store initial app configuration
        storeInitialConfig();
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (getLastStatus() != ComponentStatus::Success) return;
        
        // Update storage manager
        storageManager->loop();
        
        // Regular status reporting
        if (statusTimer.isReady()) {
            reportStorageStatus();
        }
        
        // Demo phase execution
        if (demoTimer.isReady()) {
            executeDemo();
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_APP, "[StorageDemo] Shutting down storage demonstration component...");
        
        if (storageManager) {
            // Store shutdown timestamp
            storageManager->putString("last_shutdown", String(millis()));
            storageManager->shutdown();
        }
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
private:
    void storeInitialConfig() {
        // Store app configuration if not exists
        if (!storageManager->exists("app_name")) {
            storageManager->putString("app_name", "DomoticsCore Storage Demo");
            storageManager->putString("app_version", "1.0.0");
            storageManager->putBool("debug_enabled", true);
            storageManager->putFloat("update_interval", 5.0f);
            
            DLOG_I(LOG_APP, "Stored initial app configuration");
        }
        
        // Update boot count
        int bootCount = storageManager->getInt("boot_count", 0);
        bootCount++;
        storageManager->putInt("boot_count", bootCount);
        DLOG_I(LOG_APP, "Boot count: %d", bootCount);
    }
    
    void reportStorageStatus() {
        if (!storageManager->isOpenStorage()) {
            DLOG_W(LOG_APP, "Storage not open");
            return;
        }
        
        DLOG_I(LOG_APP, "=== Storage Status ===");
        DLOG_I(LOG_APP, "%s", storageManager->getStorageInfo().c_str());
        
        // Show some key statistics
        String appName = storageManager->getString("app_name", "Unknown");
        int bootCount = storageManager->getInt("boot_count", 0);
        bool debugEnabled = storageManager->getBool("debug_enabled", false);
        
        DLOG_I(LOG_APP, "App: %s (boots: %d, debug: %s)", 
               appName.c_str(), bootCount, debugEnabled ? "on" : "off");
    }
    
    void executeDemo() {
        demoPhase = (demoPhase % 4) + 1;
        
        DLOG_I(LOG_APP, "=== Demo Phase %d ===", demoPhase);
        
        switch (demoPhase) {
            case 1:
                demoBasicPreferences();
                break;
            case 2:
                demoAdvancedDataTypes();
                break;
            case 3:
                demoBinaryData();
                break;
            case 4:
                demoDataManagement();
                break;
        }
    }
    
    void demoBasicPreferences() {
        DLOG_I(LOG_APP, "Demo: Basic Preferences (Strings & Integers)");
        
        // User preferences simulation
        String userName = "User_" + String(sessionCounter);
        int userLevel = (sessionCounter % 5) + 1;
        String theme = (sessionCounter % 2 == 0) ? "dark" : "light";
        
        // Store user preferences
        storageManager->putString("user_name", userName);
        storageManager->putInt("user_level", userLevel);
        storageManager->putString("ui_theme", theme);
        
        DLOG_I(LOG_APP, "Stored user preferences:");
        DLOG_I(LOG_APP, "  Name: %s", userName.c_str());
        DLOG_I(LOG_APP, "  Level: %d", userLevel);
        DLOG_I(LOG_APP, "  Theme: %s", theme.c_str());
        
        // Device settings simulation
        int deviceId = 1000 + sessionCounter;
        String location = "Room_" + String((sessionCounter % 10) + 1);
        
        storageManager->putInt("device_id", deviceId);
        storageManager->putString("device_location", location);
        
        DLOG_I(LOG_APP, "Stored device settings:");
        DLOG_I(LOG_APP, "  ID: %d", deviceId);
        DLOG_I(LOG_APP, "  Location: %s", location.c_str());
        
        // Read back and verify
        String readName = storageManager->getString("user_name");
        int readLevel = storageManager->getInt("user_level");
        DLOG_I(LOG_APP, "Verification: %s (level %d)", readName.c_str(), readLevel);
    }
    
    void demoAdvancedDataTypes() {
        DLOG_I(LOG_APP, "Demo: Advanced Data Types (Floats & Booleans)");
        
        // Sensor calibration data
        float tempOffset = (sessionCounter % 10) * 0.5f - 2.5f;
        float humidityScale = 1.0f + (sessionCounter % 5) * 0.01f;
        bool sensorEnabled = (sessionCounter % 3) != 0;
        bool autoCalibrate = (sessionCounter % 2) == 0;
        
        storageManager->putFloat("temp_offset", tempOffset);
        storageManager->putFloat("humidity_scale", humidityScale);
        storageManager->putBool("sensor_enabled", sensorEnabled);
        storageManager->putBool("auto_calibrate", autoCalibrate);
        
        DLOG_I(LOG_APP, "Stored sensor calibration:");
        DLOG_I(LOG_APP, "  Temp offset: %.2f°C", tempOffset);
        DLOG_I(LOG_APP, "  Humidity scale: %.3f", humidityScale);
        DLOG_I(LOG_APP, "  Sensor enabled: %s", sensorEnabled ? "yes" : "no");
        DLOG_I(LOG_APP, "  Auto calibrate: %s", autoCalibrate ? "yes" : "no");
        
        // Network settings
        float signalThreshold = -70.0f + (sessionCounter % 20);
        bool wifiAutoReconnect = true;
        float connectionTimeout = 10.0f + (sessionCounter % 5);
        
        storageManager->putFloat("signal_thresh", signalThreshold);
        storageManager->putBool("wifi_auto", wifiAutoReconnect);
        storageManager->putFloat("conn_timeout", connectionTimeout);
        
        DLOG_I(LOG_APP, "Stored network settings:");
        DLOG_I(LOG_APP, "  Signal threshold: %.1f dBm", signalThreshold);
        DLOG_I(LOG_APP, "  Auto reconnect: %s", wifiAutoReconnect ? "yes" : "no");
        DLOG_I(LOG_APP, "  Timeout: %.1f seconds", connectionTimeout);
        
        // Read back and verify
        float readOffset = storageManager->getFloat("temp_offset");
        bool readEnabled = storageManager->getBool("sensor_enabled");
        float readThresh = storageManager->getFloat("signal_thresh");
        DLOG_I(LOG_APP, "Verification: offset %.2f, enabled %s, threshold %.1f", 
               readOffset, readEnabled ? "yes" : "no", readThresh);
    }
    
    void demoBinaryData() {
        DLOG_I(LOG_APP, "Demo: Binary Data (Blobs)");
        
        // Create binary configuration data
        struct DeviceConfig {
            uint32_t magic;
            uint16_t version;
            uint8_t deviceType;
            uint8_t flags;
            uint32_t serialNumber;
            uint8_t reserved[4];
        } __attribute__((packed));
        
        DeviceConfig config;
        config.magic = 0xDEADBEEF;
        config.version = 0x0100 + sessionCounter;
        config.deviceType = 0x42;
        config.flags = 0x80 | (sessionCounter & 0x0F);
        config.serialNumber = 100000 + sessionCounter;
        memset(config.reserved, 0, sizeof(config.reserved));
        
        // Store binary config
        if (storageManager->putBlob("dev_config", (uint8_t*)&config, sizeof(config))) {
            DLOG_I(LOG_APP, "Stored device config blob (%d bytes)", sizeof(config));
            DLOG_I(LOG_APP, "  Magic: 0x%08X", config.magic);
            DLOG_I(LOG_APP, "  Version: 0x%04X", config.version);
            DLOG_I(LOG_APP, "  Serial: %u", config.serialNumber);
        }
        
        // Create and store calibration matrix
        float calibMatrix[9];
        for (int i = 0; i < 9; i++) {
            calibMatrix[i] = (float)(sessionCounter + i) / 10.0f;
        }
        
        if (storageManager->putBlob("calib_mat", (uint8_t*)calibMatrix, sizeof(calibMatrix))) {
            DLOG_I(LOG_APP, "Stored calibration matrix (%d bytes)", sizeof(calibMatrix));
            DLOG_I(LOG_APP, "  Matrix[0]: %.2f, Matrix[4]: %.2f, Matrix[8]: %.2f", 
                   calibMatrix[0], calibMatrix[4], calibMatrix[8]);
        }
        
        // Read back and verify device config
        DeviceConfig readConfig;
        size_t bytesRead = storageManager->getBlob("dev_config", (uint8_t*)&readConfig, sizeof(readConfig));
        if (bytesRead == sizeof(readConfig)) {
            DLOG_I(LOG_APP, "Verification: magic 0x%08X, serial %u", 
                   readConfig.magic, readConfig.serialNumber);
        }
    }
    
    void demoDataManagement() {
        DLOG_I(LOG_APP, "Demo: Data Management (Listing & Cleanup)");
        
        // Get all stored keys
        auto keys = storageManager->getKeys();
        DLOG_I(LOG_APP, "Stored keys (%zu total):", keys.size());
        
        for (const auto& key : keys) {
            if (storageManager->exists(key)) {
                DLOG_I(LOG_APP, "  - %s", key.c_str());
            }
        }
        
        // Demonstrate key existence checking
        String testKeys[] = {"user_name", "device_id", "temp_offset", "dev_config", "nonexistent_key"};
        DLOG_I(LOG_APP, "Key existence check:");
        for (const auto& key : testKeys) {
            bool exists = storageManager->exists(key);
            DLOG_I(LOG_APP, "  %s: %s", key.c_str(), exists ? "EXISTS" : "NOT FOUND");
        }
        
        // Cleanup old session data if we have too many sessions
        if (sessionCounter > 5) {
            DLOG_I(LOG_APP, "Performing cleanup (session %d)...", sessionCounter);
            
            // Remove some temporary keys
            String tempKey = "temp_" + String(sessionCounter - 3);
            if (storageManager->exists(tempKey)) {
                storageManager->remove(tempKey);
                DLOG_I(LOG_APP, "Removed old temporary key: %s", tempKey.c_str());
            }
        }
        
        // Store some temporary session data
        String sessionKey = "temp_" + String(sessionCounter);
        String sessionData = "Session " + String(sessionCounter) + " at " + String(millis());
        storageManager->putString(sessionKey, sessionData);
        DLOG_I(LOG_APP, "Stored temporary session data: %s", sessionKey.c_str());
        
        // Demonstrate storage statistics
        DLOG_I(LOG_APP, "Storage statistics:");
        DLOG_I(LOG_APP, "  Entries: %d/%d", storageManager->getEntryCount(), 
               storageManager->getEntryCount() + storageManager->getFreeEntries());
        DLOG_I(LOG_APP, "  Free entries: %d", storageManager->getFreeEntries());
        DLOG_I(LOG_APP, "  Namespace: %s", storageManager->getNamespace().c_str());
    }
};

void setup() {
    // Create core with custom device name
    CoreConfig config;
    config.deviceName = "StorageDemoDevice";
    config.logLevel = 3; // INFO level
    
    // Core initialized
    
    // Add storage demonstration component
    DLOG_I(LOG_APP, "Adding storage demonstration component...");
    core.addComponent(std::unique_ptr<StorageDemoComponent>(new StorageDemoComponent()));
    
    DLOG_I(LOG_APP, "Starting core with %d components...", core.getComponentCount());
    
    if (!core.begin(config)) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        return;
    }
    
    DLOG_I(LOG_APP, "=== DomoticsCore Storage Demo Ready ===");
    DLOG_I(LOG_APP, "Features demonstrated:");
    DLOG_I(LOG_APP, "- NVS preferences storage");
    DLOG_I(LOG_APP, "- String, integer, float, boolean data types");
    DLOG_I(LOG_APP, "- Binary blob storage");
    DLOG_I(LOG_APP, "- Key management and cleanup");
    DLOG_I(LOG_APP, "- Persistent app configuration");
}

void loop() {
    core.loop();
    
    // System status reporting using non-blocking delay
    static Utils::NonBlockingDelay statusTimer(30000); // 30 seconds
    if (statusTimer.isReady()) {
        DLOG_I(LOG_SYSTEM, "=== Storage Demo System Status ===");
        DLOG_I(LOG_SYSTEM, "Uptime: %lu seconds", millis() / 1000);
        DLOG_I(LOG_SYSTEM, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_SYSTEM, "Storage demo running...");
    }
}
