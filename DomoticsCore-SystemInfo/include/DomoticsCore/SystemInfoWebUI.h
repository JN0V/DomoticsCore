#pragma once

#include "DomoticsCore/SystemInfo.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/BaseWebUIComponents.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

// Composition-based WebUI provider for SystemInfoComponent
class SystemInfoWebUI : public IWebUIProvider {
private:
    SystemInfoComponent* sys; // non-owning

    // Chart data storage (circular buffer for efficiency)
    static const int CHART_DATA_SIZE = 20;
    float heapHistory[CHART_DATA_SIZE] = {0};
    float cpuHistory[CHART_DATA_SIZE] = {0};
    int chartIndex = 0;
    bool chartInitialized = false;
    unsigned long lastChartUpdate = 0;

    void updateChartData() {
        if (!sys) return;
        const auto& metrics = sys->getMetrics();

        // Initialize with current values if not done yet
        if (!chartInitialized) {
            float heapPercent = metrics.totalHeap > 0 ? ((float)metrics.freeHeap / metrics.totalHeap) * 100.0f : 0.0f;
            for (int i = 0; i < CHART_DATA_SIZE; i++) {
                heapHistory[i] = heapPercent;
                cpuHistory[i] = metrics.cpuLoad;
            }
            chartInitialized = true;
            return;
        }

        // Throttle updates to the component interval
        int interval = sys->getUpdateInterval();
        if (millis() - lastChartUpdate < (unsigned long)interval) return;

        // Update with new values (scrolling buffer)
        float heapPercent = metrics.totalHeap > 0 ? ((float)metrics.freeHeap / metrics.totalHeap) * 100.0f : 0.0f;
        heapHistory[chartIndex] = heapPercent;
        cpuHistory[chartIndex] = metrics.cpuLoad;

        // Move to next position (circular buffer)
        chartIndex = (chartIndex + 1) % CHART_DATA_SIZE;
        lastChartUpdate = millis();
    }

    String getChartDataJson(const float* data, int size) const {
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
    explicit SystemInfoWebUI(SystemInfoComponent* component)
        : sys(component) {}

    // IWebUIProvider
    String getWebUIName() const override { return sys ? sys->getName() : String("System Info"); }
    String getWebUIVersion() const override { return sys ? sys->getVersion() : String("1.0.0"); }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!sys) return contexts;

        const auto& metrics = sys->getMetrics();
        if (sys->isDetailedInfoEnabled()) {
            // Component detail card so it shows in the Components tab
            WebUIContext detail;
            detail.contextId = "system_component";
            detail.title = "System Info";
            detail.icon = "fas fa-microchip";
            detail.location = WebUILocation::ComponentDetail;
            detail.presentation = WebUIPresentation::Card;
            detail
                .withField(WebUIField("uptime", "Uptime", WebUIFieldType::Display, sys->getFormattedUptimePublic(), ""))
                .withField(WebUIField("heap", "Free Heap", WebUIFieldType::Display, sys->formatBytesPublic(metrics.freeHeap), ""))
                .withRealTime(sys->getUpdateInterval());
            contexts.push_back(detail);

            contexts.push_back(WebUIContext::dashboard("system_overview", "System Overview")
                .withField(WebUIField("uptime", "Uptime", WebUIFieldType::Display))
                .withField(WebUIField("heap", "Free Heap", WebUIFieldType::Display, "", "KB"))
                .withRealTime(sys->getUpdateInterval()));

            contexts.push_back(BaseWebUIComponents::createLineChart(
                "heap_chart", "Memory Usage", "heapChart", "heapValue", "#007acc", "%"
            ).withRealTime(sys->getUpdateInterval()));

            contexts.push_back(BaseWebUIComponents::createLineChart(
                "cpu_chart", "CPU Usage", "cpuChart", "cpuValue", "#ffc107", "%"
            ).withRealTime(sys->getUpdateInterval()));

            contexts.push_back(WebUIContext::settings("hardware_info", "Hardware")
                .withField(WebUIField("chip_model", "Chip", WebUIFieldType::Display, metrics.chipModel, "", true))
                .withField(WebUIField("chip_revision", "Revision", WebUIFieldType::Display, String(metrics.chipRevision), "", true))
                .withField(WebUIField("cpu_freq", "CPU Frequency", WebUIFieldType::Display, String(metrics.cpuFreq) + " MHz", "", true)));
        }

        if (sys->isMemoryInfoEnabled()) {
            contexts.push_back(WebUIContext::settings("memory_info", "Memory")
                .withField(WebUIField("free_heap", "Free Heap", WebUIFieldType::Display, sys->formatBytesPublic(metrics.freeHeap), "", true))
                .withField(WebUIField("min_free_heap", "Min Free", WebUIFieldType::Display, sys->formatBytesPublic(metrics.minFreeHeap), "", true))
                .withField(WebUIField("flash_size", "Flash", WebUIFieldType::Display, sys->formatBytesPublic(metrics.flashSize), "", true)));
        }
        return contexts;
    }

    String getWebUIData(const String& contextId) override {
        if (!sys) return "{}";
        const auto& metrics = sys->getMetrics();
        updateChartData();

        if (contextId == "system_component") {
            JsonDocument doc;
            doc["uptime"] = sys->getFormattedUptimePublic();
            doc["heap"] = sys->formatBytesPublic(metrics.freeHeap);
            String json; serializeJson(doc, json); return json;
        }

        if (contextId == "system_overview") {
            JsonDocument doc;
            doc["uptime"] = sys->getFormattedUptimePublic();
            doc["heap"] = sys->formatBytesPublic(metrics.freeHeap);
            String json; serializeJson(doc, json); return json;
        } else if (contextId == "heap_chart") {
            JsonDocument doc;
            doc["heap_chart_data"] = getChartDataJson(heapHistory, CHART_DATA_SIZE);
            String json; serializeJson(doc, json); return json;
        } else if (contextId == "cpu_chart") {
            JsonDocument doc;
            doc["cpu_chart_data"] = getChartDataJson(cpuHistory, CHART_DATA_SIZE);
            String json; serializeJson(doc, json); return json;
        }
        return "{}";
    }

    String handleWebUIRequest(const String& /*contextId*/, const String& /*endpoint*/, const String& /*method*/, const std::map<String, String>& /*params*/) override {
        return "{\"success\":false, \"error\":\"Not supported\"}";
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
