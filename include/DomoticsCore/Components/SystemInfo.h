#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IComponent.h"
#include "IWebUIProviderEnhanced.h"

namespace DomoticsCore {
namespace Components {

/**
 * System Information Component Configuration
 */
struct SystemInfoConfig {
    bool enableDetailedInfo = true;     // Include detailed chip info
    bool enableMemoryInfo = true;       // Include memory statistics
    bool enableNetworkInfo = true;      // Include network information
    int updateInterval = 5000;          // Update interval in ms
};

/**
 * System Information Component
 * Provides system metrics and hardware information
 */
class SystemInfoComponent : public IComponent, public IWebUIProviderEnhanced {
private:
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
        bool valid = false;
    } metrics;

public:
    SystemInfoComponent(const SystemInfoConfig& cfg = SystemInfoConfig()) 
        : config(cfg) {}

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

    ComponentStatus shutdown() override {
        return ComponentStatus::Success;
    }

    String getName() const override {
        return "SystemInfo";
    }

    String getVersion() const override {
        return "1.0.0";
    }

    // IWebUIProviderEnhanced interface
    std::vector<String> getWebUIContexts() const override {
        return {"system_overview", "memory_info", "hardware_info"};
    }

    String getWebUIData(const String& contextId) override {
        if (!metrics.valid) {
            updateMetrics();
        }

        if (contextId == "system_overview") {
            return getSystemOverviewJSON();
        } else if (contextId == "memory_info") {
            return getMemoryInfoJSON();
        } else if (contextId == "hardware_info") {
            return getHardwareInfoJSON();
        }
        
        return "{}";
    }

    WebUIContext getWebUIContext(const String& contextId) override {
        WebUIContext context;
        context.id = contextId;
        context.location = WebUILocation::Dashboard;
        
        if (contextId == "system_overview") {
            context.name = "System Overview";
            context.presentation = WebUIPresentation::Card;
            context.priority = 100;
            context.refreshInterval = config.updateInterval;
            
            context.fields = {
                {"uptime", "Uptime", WebUIFieldType::Text, ""},
                {"heap", "Free Memory", WebUIFieldType::Text, ""},
                {"cpu", "CPU Frequency", WebUIFieldType::Text, ""}
            };
        } else if (contextId == "memory_info") {
            context.name = "Memory Statistics";
            context.presentation = WebUIPresentation::Gauge;
            context.priority = 90;
            context.refreshInterval = config.updateInterval;
            
            context.fields = {
                {"heap_usage", "Heap Usage", WebUIFieldType::Number, "%"},
                {"free_heap", "Free Heap", WebUIFieldType::Text, "bytes"},
                {"min_free", "Min Free", WebUIFieldType::Text, "bytes"}
            };
        } else if (contextId == "hardware_info") {
            context.name = "Hardware Information";
            context.presentation = WebUIPresentation::Table;
            context.priority = 80;
            context.refreshInterval = 0; // Static data
            
            context.fields = {
                {"chip_model", "Chip Model", WebUIFieldType::Text, ""},
                {"chip_revision", "Revision", WebUIFieldType::Text, ""},
                {"flash_size", "Flash Size", WebUIFieldType::Text, "MB"},
                {"sketch_size", "Sketch Size", WebUIFieldType::Text, "bytes"}
            };
        }
        
        return context;
    }

    // Public API for other components
    uint32_t getFreeHeap() const { return metrics.freeHeap; }
    uint32_t getTotalHeap() const { return metrics.totalHeap; }
    uint32_t getUptime() const { return metrics.uptime; }
    float getCpuFrequency() const { return metrics.cpuFreq; }
    String getChipModel() const { return metrics.chipModel; }
    
    // Get formatted uptime string
    String getFormattedUptime() const {
        uint32_t seconds = metrics.uptime;
        uint32_t days = seconds / 86400;
        seconds %= 86400;
        uint32_t hours = seconds / 3600;
        seconds %= 3600;
        uint32_t minutes = seconds / 60;
        seconds %= 60;
        
        if (days > 0) {
            return String(days) + "d " + String(hours) + "h " + String(minutes) + "m";
        } else if (hours > 0) {
            return String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
        } else {
            return String(minutes) + "m " + String(seconds) + "s";
        }
    }

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
        metrics.valid = true;
    }

    String getSystemOverviewJSON() {
        String json = "{";
        json += "\"uptime\":\"" + getFormattedUptime() + "\",";
        json += "\"heap\":\"" + formatBytes(metrics.freeHeap) + "\",";
        json += "\"cpu\":\"" + String(metrics.cpuFreq) + " MHz\",";
        json += "\"status\":\"running\"";
        json += "}";
        return json;
    }

    String getMemoryInfoJSON() {
        float heapUsage = ((float)(metrics.totalHeap - metrics.freeHeap) / metrics.totalHeap) * 100;
        
        String json = "{";
        json += "\"heap_usage\":" + String(heapUsage, 1) + ",";
        json += "\"free_heap\":\"" + formatBytes(metrics.freeHeap) + "\",";
        json += "\"total_heap\":\"" + formatBytes(metrics.totalHeap) + "\",";
        json += "\"min_free\":\"" + formatBytes(metrics.minFreeHeap) + "\",";
        json += "\"max_alloc\":\"" + formatBytes(metrics.maxAllocHeap) + "\"";
        json += "}";
        return json;
    }

    String getHardwareInfoJSON() {
        String json = "{";
        json += "\"chip_model\":\"" + metrics.chipModel + "\",";
        json += "\"chip_revision\":\"Rev " + String(metrics.chipRevision) + "\",";
        json += "\"flash_size\":\"" + String(metrics.flashSize / (1024 * 1024)) + " MB\",";
        json += "\"sketch_size\":\"" + formatBytes(metrics.sketchSize) + "\",";
        json += "\"free_sketch\":\"" + formatBytes(metrics.freeSketchSpace) + "\"";
        json += "}";
        return json;
    }

    String formatBytes(uint32_t bytes) {
        if (bytes < 1024) {
            return String(bytes) + " B";
        } else if (bytes < 1024 * 1024) {
            return String(bytes / 1024.0, 1) + " KB";
        } else {
            return String(bytes / (1024.0 * 1024.0), 1) + " MB";
        }
    }
};

} // namespace Components
} // namespace DomoticsCore
