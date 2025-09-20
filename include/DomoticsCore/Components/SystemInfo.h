#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IComponent.h"
#include "IWebUIProvider.h"

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
 * System Information Component
 * Provides system metrics and hardware information
 */
class SystemInfoComponent : public IComponent, public virtual IWebUIProvider {
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

public:
    SystemInfoComponent(const SystemInfoConfig& cfg = SystemInfoConfig()) 
        : config(cfg) {
        metadata.name = "System Info";
        metadata.version = "1.1.0";
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

    String getWebUIName() const override { return metadata.name; }
    String getWebUIVersion() const override { return metadata.version; }

    // IWebUIProvider interface
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        
        // Always update metrics before generating contexts to ensure initial values are not zero
        if (!metrics.valid) {
            updateMetrics();
        }

        if (config.enableDetailedInfo) {
            contexts.push_back(WebUIContext::dashboard("system_overview", "System Overview")
                .withField(WebUIField("uptime", "Uptime", WebUIFieldType::Display))
                .withField(WebUIField("heap", "Free Heap", WebUIFieldType::Display, "", "KB"))
                .withRealTime(config.updateInterval));
            
            contexts.push_back(WebUIContext::settings("hardware_info", "Hardware")
                .withField(WebUIField("chip_model", "Chip", WebUIFieldType::Display, metrics.chipModel, "", true))
                .withField(WebUIField("chip_revision", "Revision", WebUIFieldType::Display, String(metrics.chipRevision), "", true)));
        }
        if (config.enableMemoryInfo) {
            contexts.push_back(WebUIContext::settings("memory_info", "Memory")
                .withField(WebUIField("free_heap", "Free Heap", WebUIFieldType::Display, formatBytes(metrics.freeHeap), "", true))
                .withField(WebUIField("min_free_heap", "Min Free", WebUIFieldType::Display, formatBytes(metrics.minFreeHeap), "", true))
                .withField(WebUIField("flash_size", "Flash", WebUIFieldType::Display, formatBytes(metrics.flashSize), "", true)));
        }
        return contexts;
    }

    String getWebUIData(const String& contextId) override {
        if (!metrics.valid) updateMetrics();

        if (contextId == "system_overview") {
            JsonDocument doc;
            doc["uptime"] = getFormattedUptime();
            doc["heap"] = formatBytes(metrics.freeHeap);
            String json;
            serializeJson(doc, json);
            return json;
        }
        return "{}";
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params) override {
        return "{\"success\":false, \"error\":\"Not supported\"}";
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
};

} // namespace Components
} // namespace DomoticsCore
