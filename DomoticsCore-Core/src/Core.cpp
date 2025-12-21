#include "DomoticsCore/Core.h"
#include "DomoticsCore/Logger.h"
#include "DomoticsCore/ComponentConfig.h"
#include "DomoticsCore/Platform_HAL.h"      // For HAL::getChipId(), HAL::getFreeHeap()

namespace DomoticsCore {

Core::Core() : initialized(false) {
}

Core::~Core() {
    if (initialized) {
        shutdown();
    }
}

bool Core::begin(const CoreConfig& cfg) {
    if (initialized) {
        DLOG_W(LOG_CORE, "Core already initialized");
        return true;
    }
    
    config = cfg;
    
    // Initialize Serial if not already done
    if (!HAL::isLoggerReady()) {
        HAL::initializeLogging(115200);
    }
    
    // Generate unique device ID if not provided
    if (config.deviceId.isEmpty()) {
        config.deviceId = "DC" + HAL::formatChipIdHex();
        config.deviceId = HAL::toUpperCase(config.deviceId);
    }
    
    DLOG_I(LOG_CORE, "DomoticsCore initializing...");
    DLOG_I(LOG_CORE, "Device: %s (ID: %s)", config.deviceName.c_str(), config.deviceId.c_str());
    DLOG_I(LOG_CORE, "Free heap: %d bytes", HAL::getFreeHeap());
    
    // Provide Core reference to registry for component injection
    componentRegistry.setCore(this);
    
    // Initialize all registered components
    Components::ComponentStatus status = componentRegistry.initializeAll();
    if (status != Components::ComponentStatus::Success) {
        DLOG_E(LOG_CORE, "Failed to initialize components: %s", Components::statusToString(status));
        return false;
    }
    
    initialized = true;
    DLOG_I(LOG_CORE, "Core initialization complete");
    return true;
}

void Core::loop() {
    if (!initialized) return;
    
    // Run component loops
    componentRegistry.loopAll();
    
    // Core minimal heartbeat
    static unsigned long lastHeartbeat = 0;
    if (HAL::getMillis() - lastHeartbeat >= 60000) { // Every minute
        lastHeartbeat = HAL::getMillis();
        DLOG_D(LOG_CORE, "Core heartbeat - uptime: %lu seconds, components: %zu", 
               HAL::getMillis() / 1000, componentRegistry.getComponentCount());
    }
}

void Core::shutdown() {
    if (!initialized) return;
    
    DLOG_I(LOG_CORE, "Shutting down DomoticsCore");
    // Shutdown all components first
    componentRegistry.shutdownAll();
    
    initialized = false;
    DLOG_I(LOG_CORE, "Core shutdown complete");
}

} // namespace DomoticsCore
