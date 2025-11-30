#pragma once

/**
 * @file SystemInfo.h
 * @brief Declares the DomoticsCore SystemInfo component for runtime diagnostics.
 * 
 * Uses HAL abstraction for multi-platform support (ESP32, ESP8266).
 */

#include <Arduino.h>
#include "DomoticsCore/IComponent.h"
#include "SystemInfo_HAL.h"  // Hardware Abstraction Layer for SystemInfo

namespace DomoticsCore {
namespace Components {

/**
 * System Information Component Configuration
 */
struct SystemInfoConfig {
    // Device identity (populated by System.h from SystemConfig)
    String deviceName = "DomoticsCore Device";
    String manufacturer = "DomoticsCore";
    String firmwareVersion = "1.0.0";
    
    // Diagnostic settings
    bool enableDetailedInfo = true;     // Include detailed chip info
    bool enableMemoryInfo = true;       // Include memory statistics
    int updateInterval = 5000;          // Update interval in ms
};

/**
 * @class DomoticsCore::Components::SystemInfoComponent
 * @brief Collects ESP32 system metrics (uptime, heap, chip info) for dashboards and logs.
 *
 * Designed as a lightweight diagnostic component. When paired with a WebUI provider it exposes
 * real-time metrics across dashboard sections and WebSocket updates.
 */
/**
 * Core System Information Component
 * Provides system metrics and hardware information without WebUI dependencies
 */
class SystemInfoComponent : public IComponent {
protected:
    SystemInfoConfig config;
    unsigned long lastUpdate = 0;
    
    // Cached system info to avoid repeated ESP calls
    struct SystemMetrics {
        uint32_t freeHeap = 0;
        uint32_t totalHeap = 0;
        uint32_t minFreeHeap = 0;
        uint32_t maxAllocHeap = 0;
        float cpuFreq = 0;
        uint32_t flashSize = 0;
        uint32_t sketchSize = 0;
        uint32_t freeSketchSpace = 0;
        String chipModel;
        uint8_t chipRevision = 0;
        uint32_t uptime = 0;
        float cpuLoad = 0;              // Estimated CPU load percentage
        bool valid = false;
    } metrics;
    
    // CPU load estimation variables
    unsigned long lastHeapCheck = 0;
    uint32_t lastHeapValue = 0;
    float cpuLoadEma = 0.0f; // Exponential moving average for smoother CPU load

    String formatBytes(uint32_t bytes) {
        if (bytes < 1024) {
            return String(bytes) + " B";
        } else if (bytes < 1024 * 1024) {
            return String(bytes / 1024.0, 1) + " KB";
        } else {
            return String(bytes / (1024.0 * 1024.0), 1) + " MB";
        }
    }

    String getFormattedUptime() {
        uint32_t seconds = metrics.uptime;
        uint32_t days = seconds / 86400;
        seconds %= 86400;
        uint32_t hours = seconds / 3600;
        seconds %= 3600;
        uint32_t minutes = seconds / 60;
        seconds %= 60;
        
        if (days > 0) {
            return String(days) + "d " + String(hours) + "h";
        } else if (hours > 0) {
            return String(hours) + "h " + String(minutes) + "m";
        } else {
            return String(minutes) + "m " + String(seconds) + "s";
        }
    }
        float calculateCpuLoad() {
        // Heuristic CPU load estimation based on heap allocation activity.
        // On ESP32 Arduino, direct CPU usage isn't available without special FreeRTOS config.
        // We therefore estimate activity over time and smooth it with an EMA.
        unsigned long currentTime = millis();
        uint32_t currentHeap = HAL::SystemInfo::getFreeHeap();

        if (lastHeapCheck > 0) {
            unsigned long dt = currentTime - lastHeapCheck;
            if (dt > 0) {
                // Heap activity in KB per second
                float heapDiffKB = fabsf((float)((int32_t)currentHeap - (int32_t)lastHeapValue)) / 1024.0f;
                float activityPerSec = heapDiffKB * (1000.0f / (float)dt);

                // Map activity to an arbitrary 0-100 range
                // Tuned scale: 10 KB/s ~ 100% load (cap at 100)
                float instantLoad = constrain(activityPerSec * 10.0f, 0.0f, 100.0f);

                // Exponential moving average for stability
                const float alpha = 0.3f; // smoothing factor
                cpuLoadEma = (alpha * instantLoad) + ((1.0f - alpha) * cpuLoadEma);
                metrics.cpuLoad = constrain(cpuLoadEma, 0.0f, 100.0f);
            }
        }

        lastHeapCheck = currentTime;
        lastHeapValue = currentHeap;
        return metrics.cpuLoad;
    }

public:
    SystemInfoComponent(const SystemInfoConfig& cfg = SystemInfoConfig()) 
        : config(cfg) {
        metadata.name = "System Info";
        metadata.version = "1.2.1";
    }

    // IComponent interface
    ComponentStatus begin() override {
        updateMetrics();
        return ComponentStatus::Success;
    }

    void loop() override {
        if (millis() - lastUpdate >= config.updateInterval) {
            updateMetrics();
            lastUpdate = millis();
        }
    }

    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    const char* getTypeKey() const override { return "system_info"; }

    // Public accessors for metrics (for WebUI extensions)
    const SystemMetrics& getMetrics() const { return metrics; }
    
    // Standard config accessors (matching other components)
    const SystemInfoConfig& getConfig() const { return config; }
    void setConfig(const SystemInfoConfig& cfg) {
        config = cfg;
        DLOG_I(LOG_SYSTEM, "SystemInfo config updated: device='%s', mfg='%s', version='%s'",
               config.deviceName.c_str(), config.manufacturer.c_str(), config.firmwareVersion.c_str());
    }
    
    int getUpdateInterval() const { return config.updateInterval; }
    bool isDetailedInfoEnabled() const { return config.enableDetailedInfo; }
    bool isMemoryInfoEnabled() const { return config.enableMemoryInfo; }
    String getFormattedUptimePublic() { return getFormattedUptime(); }
    String formatBytesPublic(uint32_t bytes) { return formatBytes(bytes); }
    void forceUpdateMetrics() { updateMetrics(); } // For WebUI extensions

private:
    void updateMetrics() {
        metrics.freeHeap = HAL::SystemInfo::getFreeHeap();
        metrics.totalHeap = HAL::SystemInfo::getTotalHeap();
        metrics.minFreeHeap = HAL::SystemInfo::getMinFreeHeap();
        metrics.maxAllocHeap = HAL::SystemInfo::getMaxAllocHeap();
        metrics.cpuFreq = HAL::SystemInfo::getCpuFreqMHz();
        metrics.flashSize = HAL::SystemInfo::getFlashSize();
        metrics.sketchSize = HAL::SystemInfo::getSketchSize();
        metrics.freeSketchSpace = HAL::SystemInfo::getFreeSketchSpace();
        metrics.chipModel = HAL::SystemInfo::getChipModel();
        metrics.chipRevision = HAL::SystemInfo::getChipRevision();
        metrics.uptime = millis() / 1000;
        
        // Calculate CPU load (simplified estimation)
        calculateCpuLoad();
        
        metrics.valid = true;
    }
};

} // namespace Components
} // namespace DomoticsCore
