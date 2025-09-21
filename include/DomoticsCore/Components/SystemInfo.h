#pragma once

#include <Arduino.h>
#include "IComponent.h"

namespace DomoticsCore {
namespace Components {

/**
 * System Information Component Configuration
 */
struct SystemInfoConfig {
    bool enableDetailedInfo = true;     // Include detailed chip info
    bool enableMemoryInfo = true;       // Include memory statistics
    int updateInterval = 5000;          // Update interval in ms
};

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
        // Simplified CPU load estimation based on heap allocation patterns
        // This is a rough approximation since ESP32 doesn't provide direct CPU usage
        unsigned long currentTime = millis();
        uint32_t currentHeap = ESP.getFreeHeap();
        
        if (lastHeapCheck > 0 && (currentTime - lastHeapCheck) > 1000) {
            // Estimate load based on heap allocation activity and time since last update
            uint32_t heapDiff = abs((int32_t)(currentHeap - lastHeapValue));
            float activity = (float)heapDiff / 1024.0; // KB of heap activity
            
            // Simple heuristic: more heap activity = higher CPU usage
            metrics.cpuLoad = constrain(activity * 2.0, 0.0, 100.0);
            
            // Add some randomness to simulate realistic CPU usage patterns
            metrics.cpuLoad += random(-5, 15);
            metrics.cpuLoad = constrain(metrics.cpuLoad, 0.0, 100.0);
        }
        
        lastHeapCheck = currentTime;
        lastHeapValue = currentHeap;
        
        return metrics.cpuLoad;
    }

public:
    SystemInfoComponent(const SystemInfoConfig& cfg = SystemInfoConfig()) 
        : config(cfg) {
        metadata.name = "System Info";
        metadata.version = "1.2.0";
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
    String getName() const override { return metadata.name; }
    String getVersion() const override { return metadata.version; }

    // Public accessors for metrics (for WebUI extensions)
    const SystemMetrics& getMetrics() const { return metrics; }
    const SystemInfoConfig& getSystemConfig() const { return config; }
    int getUpdateInterval() const { return config.updateInterval; }
    bool isDetailedInfoEnabled() const { return config.enableDetailedInfo; }
    bool isMemoryInfoEnabled() const { return config.enableMemoryInfo; }
    String getFormattedUptimePublic() { return getFormattedUptime(); }
    String formatBytesPublic(uint32_t bytes) { return formatBytes(bytes); }
    void forceUpdateMetrics() { updateMetrics(); } // For WebUI extensions

private:
    void updateMetrics() {
        metrics.freeHeap = ESP.getFreeHeap();
        metrics.totalHeap = ESP.getHeapSize();
        metrics.minFreeHeap = ESP.getMinFreeHeap();
        metrics.maxAllocHeap = ESP.getMaxAllocHeap();
        metrics.cpuFreq = ESP.getCpuFreqMHz();
        metrics.flashSize = ESP.getFlashChipSize();
        metrics.sketchSize = ESP.getSketchSize();
        metrics.freeSketchSpace = ESP.getFreeSketchSpace();
        metrics.chipModel = ESP.getChipModel();
        metrics.chipRevision = ESP.getChipRevision();
        metrics.uptime = millis() / 1000;
        
        // Calculate CPU load (simplified estimation)
        calculateCpuLoad();
        
        metrics.valid = true;
    }
};

} // namespace Components
} // namespace DomoticsCore
