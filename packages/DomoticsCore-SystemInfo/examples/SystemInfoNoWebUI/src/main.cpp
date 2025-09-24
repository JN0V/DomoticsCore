#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/SystemInfo.h>
#include <DomoticsCore/Timer.h>
#include <memory>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

std::unique_ptr<Core> core;

class SystemInfoDemoComponent : public IComponent {
private:
    SystemInfoComponent sys;
    Utils::NonBlockingDelay statusTimer{5000};
public:
    String getName() const override { return "SystemInfoDemo"; }
    ComponentStatus begin() override {
        sys.begin();
        return ComponentStatus::Success;
    }
    void loop() override {
        sys.loop();
        if (statusTimer.isReady()) {
            const auto& m = sys.getMetrics();
            DLOG_I(LOG_SYSTEM, "=== System Metrics ===");
            DLOG_I(LOG_SYSTEM, "Uptime: %s", sys.getFormattedUptimePublic().c_str());
            DLOG_I(LOG_SYSTEM, "CPU Frequency: %.1f MHz", m.cpuFreq);
            DLOG_I(LOG_SYSTEM, "CPU Load (est.): %.1f%%", m.cpuLoad);
            DLOG_I(LOG_SYSTEM, "Free Heap: %s", sys.formatBytesPublic(m.freeHeap).c_str());
            DLOG_I(LOG_SYSTEM, "Total Heap: %s", sys.formatBytesPublic(m.totalHeap).c_str());
            DLOG_I(LOG_SYSTEM, "Min Free Heap: %s", sys.formatBytesPublic(m.minFreeHeap).c_str());
            DLOG_I(LOG_SYSTEM, "Max Alloc Heap: %s", sys.formatBytesPublic(m.maxAllocHeap).c_str());
            DLOG_I(LOG_SYSTEM, "Flash Size: %s", sys.formatBytesPublic(m.flashSize).c_str());
            DLOG_I(LOG_SYSTEM, "Sketch Size: %s", sys.formatBytesPublic(m.sketchSize).c_str());
            DLOG_I(LOG_SYSTEM, "Free Sketch Space: %s", sys.formatBytesPublic(m.freeSketchSpace).c_str());
            DLOG_I(LOG_SYSTEM, "Chip Model: %s (rev %u)", m.chipModel.c_str(), m.chipRevision);
        }
    }
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
};

void setup() {
    CoreConfig cfg; cfg.deviceName = "SystemInfoDemo"; cfg.logLevel = 3;
    core.reset(new Core());
    core->addComponent(std::unique_ptr<SystemInfoDemoComponent>(new SystemInfoDemoComponent()));
    if (!core->begin(cfg)) {
        DLOG_E(LOG_CORE, "Core initialization failed");
        return;
    }
}

void loop() {
    if (core) core->loop();
}
