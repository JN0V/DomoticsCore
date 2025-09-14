#include "DomoticsCore/Core.h"

namespace DomoticsCore {

Core::Core() : initialized(false) {
}

Core::~Core() {
    shutdown();
}

bool Core::begin(const CoreConfig& cfg) {
    // Ensure Serial is ready for logging
    if (!Serial) {
        Serial.begin(115200);
        delay(100);
    }
    
    config = cfg;
    
    // Generate device ID if not set
    if (config.deviceId.isEmpty()) {
        config.deviceId = "DC" + String((uint32_t)ESP.getEfuseMac(), HEX);
    }
    
    DLOG_I(LOG_CORE, "DomoticsCore initializing...");
    DLOG_I(LOG_CORE, "Device: %s (ID: %s)", config.deviceName.c_str(), config.deviceId.c_str());
    DLOG_I(LOG_CORE, "Free heap: %d bytes", ESP.getFreeHeap());
    
    initialized = true;
    DLOG_I(LOG_CORE, "Core initialization complete");
    
    return true;
}

void Core::loop() {
    if (!initialized) return;
    
    // Minimal core loop - just keep system alive
    // Components can be added later
}

void Core::shutdown() {
    if (!initialized) return;
    
    DLOG_I(LOG_CORE, "Shutting down DomoticsCore");
    initialized = false;
}

} // namespace DomoticsCore
