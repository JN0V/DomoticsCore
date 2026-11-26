// DomoticsCore - Core only: the bare minimum framework.
#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;

#define LOG_APP "APP"

Core core;
Utils::NonBlockingDelay heartbeatTimer(10000); // 10 second heartbeat

void setup() {
    HAL::initializeLogging(115200);

    CoreConfig config;
    config.deviceName = "MyESP32Device";
    config.logLevel = 3; // INFO

    if (!core.begin(config)) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        return;
    }
    DLOG_I(LOG_APP, "Device ready: %s", core.getDeviceName().c_str());
}

void loop() {
    core.loop();
    if (heartbeatTimer.isReady()) {
        DLOG_I(LOG_APP, "Heartbeat - uptime: %lu seconds", millis() / 1000);
    }
}
