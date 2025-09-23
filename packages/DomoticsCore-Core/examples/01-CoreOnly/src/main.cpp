#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Utils/Timer.h>
#include <memory>

using namespace DomoticsCore;

std::unique_ptr<Core> core;
Utils::NonBlockingDelay heartbeatTimer(10000); // 10 second heartbeat
Utils::NonBlockingDelay statusTimer(30000);    // 30 second status

void setup() {
    // Create core with custom device name
    CoreConfig config;
    config.deviceName = "MyESP32Device";
    config.logLevel = 3; // INFO level
    
    core.reset(new Core());
    
    if (!core->begin(config)) {
        DLOG_E(LOG_CORE, "Failed to initialize core!");
        return;
    }
    
    DLOG_I(LOG_CORE, "Device configured: %s (ID: %s)", 
           core->getDeviceName().c_str(), 
           core->getDeviceId().c_str());
    DLOG_I(LOG_CORE, "Setup complete - device ready");
}

void loop() {
    if (core) {
        core->loop();
    }
    
    // Non-blocking heartbeat every 10 seconds
    if (heartbeatTimer.isReady()) {
        DLOG_I(LOG_CORE, "Heartbeat - uptime: %lu seconds", millis() / 1000);
    }
    
    // Non-blocking status report every 30 seconds
    if (statusTimer.isReady()) {
        DLOG_I(LOG_SYSTEM, "Free heap: %d bytes", ESP.getFreeHeap());
        DLOG_I(LOG_SYSTEM, "Chip temperature: %.1f°C", temperatureRead());
    }
    
    // No blocking delay needed - timers handle everything
}
