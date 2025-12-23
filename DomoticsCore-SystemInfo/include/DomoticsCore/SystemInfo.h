#pragma once

/**
 * @file SystemInfo.h
 * @brief Declares the DomoticsCore SystemInfo component for runtime diagnostics.
 * 
 * Uses HAL abstraction for multi-platform support (ESP32, ESP8266).
 */

#include "DomoticsCore/IComponent.h"
#include "DomoticsCore/Platform_HAL.h"  // Hardware Abstraction Layer
#include <cmath>  // For fabsf

namespace DomoticsCore {
namespace Components {

/**
 * Boot diagnostics data structure
 * 
 * Volatile data (reset_reason, heap) captured by SystemInfo at boot.
 * Persistent data (bootCount) managed by System via Storage component.
 */
struct BootDiagnostics {
    uint32_t bootCount = 0;              // Incrementing boot counter (set by System via Storage)
    HAL::Platform::ResetReason resetReason = HAL::Platform::ResetReason::Unknown;
    uint32_t lastBootHeap = 0;           // Free heap at boot (captured at boot)
    uint32_t lastBootMinHeap = 0;        // Min free heap at boot (captured at boot)
    bool valid = false;                  // Data captured successfully

    /**
     * @brief Get human-readable reset reason string (delegates to HAL)
     */
    String getResetReasonString() const {
        return HAL::Platform::getResetReasonString(resetReason);
    }

    /**
     * @brief Check if last reset was unexpected (delegates to HAL)
     */
    bool wasUnexpectedReset() const {
        return HAL::Platform::wasUnexpectedReset(resetReason);
    }
};

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

    // Boot diagnostics settings
    bool enableBootDiagnostics = true;  // Enable boot diagnostics capture
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
    
    // Boot diagnostics (volatile data captured at boot, bootCount set by System)
    BootDiagnostics bootDiag;
    
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
        unsigned long currentTime = HAL::Platform::getMillis();
        uint32_t currentHeap = HAL::Platform::getFreeHeap();

        if (lastHeapCheck > 0) {
            unsigned long dt = currentTime - lastHeapCheck;
            if (dt > 0) {
                // Heap activity in KB per second
                float heapDiffKB = fabsf((float)((int32_t)currentHeap - (int32_t)lastHeapValue)) / 1024.0f;
                float activityPerSec = heapDiffKB * (1000.0f / (float)dt);

                // Map activity to an arbitrary 0-100 range
                // Tuned scale: 10 KB/s ~ 100% load (cap at 100)
                float instantLoad = HAL::Platform::constrain(activityPerSec * 10.0f, 0.0f, 100.0f);

                // Exponential moving average for stability
                const float alpha = 0.3f; // smoothing factor
                cpuLoadEma = (alpha * instantLoad) + ((1.0f - alpha) * cpuLoadEma);
                metrics.cpuLoad = HAL::Platform::constrain(cpuLoadEma, 0.0f, 100.0f);
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
        metadata.version = "1.4.0";
    }

    // IComponent interface
    ComponentStatus begin() override {
        // Initialize boot diagnostics before metrics
        if (config.enableBootDiagnostics) {
            initBootDiagnostics();
        }
        updateMetrics();
        return ComponentStatus::Success;
    }

    void loop() override {
        if (HAL::Platform::getMillis() - lastUpdate >= (unsigned long)config.updateInterval) {
            updateMetrics();
            lastUpdate = HAL::Platform::getMillis();
        }
    }

    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    const char* getTypeKey() const override { return "system_info"; }

    // Public accessors for metrics (for WebUI extensions)
    const SystemMetrics& getMetrics() const { return metrics; }
    
    // Boot diagnostics accessors
    const BootDiagnostics& getBootDiagnostics() const { return bootDiag; }
    
    /**
     * @brief Set boot count (called by System after loading from Storage)
     * @param count Boot count value from persistent storage
     */
    void setBootCount(uint32_t count) {
        bootDiag.bootCount = count;
    }
    
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
    /**
     * @brief Capture volatile boot diagnostics (reset reason, heap)
     * 
     * Only captures data available at boot time. Boot count is managed
     * separately by System component via Storage for persistence.
     */
    void initBootDiagnostics() {
        // Capture volatile data at boot using HAL abstraction
        bootDiag.resetReason = HAL::Platform::getResetReason();
        bootDiag.lastBootHeap = HAL::Platform::getFreeHeap();
        bootDiag.lastBootMinHeap = HAL::Platform::getMinFreeHeap();
        bootDiag.valid = true;
        
        DLOG_I(LOG_SYSTEM, "Boot diagnostics captured: Reset=%s, Heap=%u/%u",
               bootDiag.getResetReasonString().c_str(),
               bootDiag.lastBootHeap,
               bootDiag.lastBootMinHeap);
        
        // Warn if unexpected reset
        if (bootDiag.wasUnexpectedReset()) {
            DLOG_W(LOG_SYSTEM, "⚠ Previous boot ended unexpectedly: %s",
                   bootDiag.getResetReasonString().c_str());
        }
    }
    
    void updateMetrics() {
        metrics.freeHeap = HAL::Platform::getFreeHeap();
        metrics.totalHeap = HAL::Platform::getTotalHeap();
        metrics.minFreeHeap = HAL::Platform::getMinFreeHeap();
        metrics.maxAllocHeap = HAL::Platform::getMaxAllocHeap();
        metrics.cpuFreq = HAL::Platform::getCpuFreqMHz();
        metrics.flashSize = HAL::Platform::getFlashSize();
        metrics.sketchSize = HAL::Platform::getSketchSize();
        metrics.freeSketchSpace = HAL::Platform::getFreeSketchSpace();
        metrics.chipModel = HAL::Platform::getChipModel();
        metrics.chipRevision = HAL::Platform::getChipRevision();
        metrics.uptime = HAL::Platform::getMillis() / 1000;
        
        // Calculate CPU load (simplified estimation)
        calculateCpuLoad();
        
        metrics.valid = true;
    }
};

} // namespace Components
} // namespace DomoticsCore
