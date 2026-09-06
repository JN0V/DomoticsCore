#include "DomoticsCore/Core.h"
#include "DomoticsCore/Logger.h"
#include "DomoticsCore/ComponentConfig.h"
#include "DomoticsCore/Platform_HAL.h"      // For HAL::getChipId(), HAL::getFreeHeap()
#include "DomoticsCore/MemoryManager.h"     // For memory profile detection
#include "DomoticsCore/FlightRecorder.h"    // OBS-3

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
    
    // OBS-3: read what the last death left in RTC before anything else runs.
    // Idempotent: System::begin() may already have called it with the hold.
    FlightRecorder& recorder = FlightRecorder::instance();
    recorder.begin(false);

    config = cfg;
    
    // Initialize Serial if not already done
    if (!HAL::isLoggerReady()) {
        HAL::initializeLogging(115200);
    }

#ifdef DOMOTICS_BUILD_ID
    DLOG_I(LOG_CORE, "Build id: %s", DOMOTICS_BUILD_ID);
#endif
    if (!recorder.restartHookInstalled()) {
        DLOG_W(LOG_CORE, "Restart hook not registered: a software reset will read as unexplained");
    }
    if (recorder.hasPromotedRecord()) {
        char line[240];
        recorder.format(line, sizeof(line));
        DLOG_W(LOG_CORE, "%s", line);
        // A bare Core has no Storage: the log line above is where the record
        // got out, so the fresh record can take RTC now. System persists
        // first and acknowledges itself.
        if (!recorder.acknowledgementDeferred()) recorder.acknowledge();
    }
    
    // Generate unique device ID if not provided
    if (config.deviceId.isEmpty()) {
        config.deviceId = "DC" + HAL::formatChipIdHex();
        config.deviceId = HAL::toUpperCase(config.deviceId);
    }
    
    DLOG_I(LOG_CORE, "DomoticsCore initializing...");
    DLOG_I(LOG_CORE, "Device: %s (ID: %s)", config.deviceName.c_str(), config.deviceId.c_str());

    // Detect memory profile BEFORE component initialization
    // Components can query MemoryManager::instance().getProfile() during their begin()
    auto& memMgr = MemoryManager::instance();
    memMgr.detectProfile();
    DLOG_I(LOG_CORE, "Memory profile: %s (heap: %u bytes)",
           memMgr.getProfileName(), memMgr.getHeapAtBoot());

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

    // OBS-3: heap running minimum every loop, a sample to RTC every 10 s.
    FlightRecorder& recorder = FlightRecorder::instance();
    const uint32_t drops = componentRegistry.getEventBus().getDroppedCount();
    recorder.noteEventDrops(drops);
    recorder.tick();

    // LO-5 / BUG-36: the queue drops silently; say so, at most once a minute.
    static uint32_t lastDropsLogged = 0;
    static unsigned long lastDropLog = 0;
    if (drops != lastDropsLogged && HAL::getMillis() - lastDropLog >= 60000) {
        DLOG_W(LOG_CORE, "EventBus dropped %lu events since boot (queue cap 32)", (unsigned long)drops);
        lastDropsLogged = drops;
        lastDropLog = HAL::getMillis();
    }
    
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
