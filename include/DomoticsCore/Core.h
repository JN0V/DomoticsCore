#pragma once

#include <Arduino.h>
#include "Logger.h"
#include "Utils/Timer.h"

namespace DomoticsCore {

/**
 * Minimal Core configuration
 */
struct CoreConfig {
    String deviceName = "DomoticsCore";
    String deviceId = "";
    uint8_t logLevel = 3; // INFO level
};

/**
 * Minimal DomoticsCore framework
 * Provides just logging and basic device configuration
 */
class Core {
private:
    CoreConfig config;
    bool initialized;
    
public:
    Core();
    ~Core();
    
    // Core lifecycle
    bool begin(const CoreConfig& cfg = CoreConfig());
    void loop();
    void shutdown();
    
    // Configuration access
    CoreConfig getConfiguration() const { return config; }
    void setConfiguration(const CoreConfig& cfg) { config = cfg; }
    
    // Device info
    String getDeviceId() const { return config.deviceId; }
    String getDeviceName() const { return config.deviceName; }
    
    // Utility functions - convenience factory for timers
    static Utils::NonBlockingDelay createTimer(unsigned long intervalMs) {
        return Utils::NonBlockingDelay(intervalMs);
    }
};

} // namespace DomoticsCore
