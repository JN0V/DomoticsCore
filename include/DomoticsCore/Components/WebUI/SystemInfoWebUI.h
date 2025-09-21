#pragma once

#include "../SystemInfo.h"
#include "../IWebUIProvider.h"
#include "BaseWebUIComponents.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * SystemInfo WebUI Extension
 * Provides WebUI integration for SystemInfoCore component
 */
class SystemInfoWebUI : public SystemInfoComponent, public virtual IWebUIProvider {
private:
    // Chart data storage (circular buffer for efficiency)
    static const int CHART_DATA_SIZE = 20;
    float heapHistory[CHART_DATA_SIZE] = {0};
    float cpuHistory[CHART_DATA_SIZE] = {0};
    int chartIndex = 0;
    bool chartInitialized = false;
    
    void updateChartData() {
        const auto& metrics = getMetrics();
        
        // Initialize with current values if not done yet
        if (!chartInitialized) {
            float heapPercent = ((float)metrics.freeHeap / metrics.totalHeap) * 100.0;
            for (int i = 0; i < CHART_DATA_SIZE; i++) {
                heapHistory[i] = heapPercent;
                cpuHistory[i] = metrics.cpuLoad;
            }
            chartInitialized = true;
            return;
        }
        
        // Update with new values (scrolling buffer)
        float heapPercent = ((float)metrics.freeHeap / metrics.totalHeap) * 100.0;
        heapHistory[chartIndex] = heapPercent;
        cpuHistory[chartIndex] = metrics.cpuLoad;
        
        // Move to next position (circular buffer)
        chartIndex = (chartIndex + 1) % CHART_DATA_SIZE;
    }
    
    String getChartDataJson(const float* data, int size) {
        String json = "[";
        
        // Return data in chronological order (oldest to newest)
        for (int i = 0; i < size; i++) {
            if (i > 0) json += ",";
            int index = (chartIndex + i) % size; // Start from oldest
            json += String(data[index], 1);
        }
        json += "]";
        return json;
    }

public:
    SystemInfoWebUI(const SystemInfoConfig& cfg = SystemInfoConfig()) 
        : SystemInfoComponent(cfg) {}

    // Override loop to include chart updates
    void loop() override {
        SystemInfoComponent::loop(); // Call parent loop
        
        // Update chart data when metrics are updated
        static unsigned long lastChartUpdate = 0;
        if (millis() - lastChartUpdate >= getUpdateInterval()) {
            updateChartData();
            lastChartUpdate = millis();
        }
    }

    // IWebUIProvider implementation
    String getWebUIName() const override { return getName(); }
    String getWebUIVersion() const override { return getVersion(); }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        const auto& config = getSystemConfig();
        const auto& metrics = getMetrics();
        
        // Always update metrics before generating contexts
        if (!metrics.valid) {
            const_cast<SystemInfoWebUI*>(this)->forceUpdateMetrics();
        }

        if (isDetailedInfoEnabled()) {
            // System Overview with basic info
            contexts.push_back(WebUIContext::dashboard("system_overview", "System Overview")
                .withField(WebUIField("uptime", "Uptime", WebUIFieldType::Display))
                .withField(WebUIField("heap", "Free Heap", WebUIFieldType::Display, "", "KB"))
                .withRealTime(getUpdateInterval()));
            
            // Memory Usage Chart using base component
            contexts.push_back(DomoticsCore::Components::WebUI::BaseWebUIComponents::createLineChart(
                "heap_chart", "Memory Usage", "heapChart", "heapValue", "#007acc", "%"
            ).withRealTime(getUpdateInterval()));
            
            // CPU Usage Chart using base component  
            contexts.push_back(DomoticsCore::Components::WebUI::BaseWebUIComponents::createLineChart(
                "cpu_chart", "CPU Usage", "cpuChart", "cpuValue", "#ffc107", "%"
            ).withRealTime(getUpdateInterval()));
            
            // Hardware Info (Settings)
            contexts.push_back(WebUIContext::settings("hardware_info", "Hardware")
                .withField(WebUIField("chip_model", "Chip", WebUIFieldType::Display, metrics.chipModel, "", true))
                .withField(WebUIField("chip_revision", "Revision", WebUIFieldType::Display, String(metrics.chipRevision), "", true))
                .withField(WebUIField("cpu_freq", "CPU Frequency", WebUIFieldType::Display, String(metrics.cpuFreq) + " MHz", "", true)));
        }
        
        if (isMemoryInfoEnabled()) {
            contexts.push_back(WebUIContext::settings("memory_info", "Memory")
                .withField(WebUIField("free_heap", "Free Heap", WebUIFieldType::Display, formatBytesPublic(metrics.freeHeap), "", true))
                .withField(WebUIField("min_free_heap", "Min Free", WebUIFieldType::Display, formatBytesPublic(metrics.minFreeHeap), "", true))
                .withField(WebUIField("flash_size", "Flash", WebUIFieldType::Display, formatBytesPublic(metrics.flashSize), "", true)));
        }
        
        return contexts;
    }

    String getWebUIData(const String& contextId) override {
        const auto& metrics = getMetrics();
        
        // Ensure chart data is initialized before returning it
        if (!chartInitialized) {
            const_cast<SystemInfoWebUI*>(this)->updateChartData();
        }
        
        if (contextId == "system_overview") {
            JsonDocument doc;
            doc["uptime"] = getFormattedUptimePublic();
            doc["heap"] = formatBytesPublic(metrics.freeHeap);
            String json;
            serializeJson(doc, json);
            return json;
        } else if (contextId == "heap_chart") {
            JsonDocument doc;
            doc["heap_chart_data"] = getChartDataJson(heapHistory, CHART_DATA_SIZE);
            String json;
            serializeJson(doc, json);
            return json;
        } else if (contextId == "cpu_chart") {
            JsonDocument doc;
            doc["cpu_chart_data"] = getChartDataJson(cpuHistory, CHART_DATA_SIZE);
            String json;
            serializeJson(doc, json);
            return json;
        }
        return "{}";
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params) override {
        return "{\"success\":false, \"error\":\"Not supported\"}";
    }

};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
