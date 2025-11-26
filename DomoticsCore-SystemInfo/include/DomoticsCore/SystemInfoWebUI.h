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
    std::function<void(const String&)> onDeviceNameChanged; // callback for device name persistence

    // State tracking using LazyState for change detection
    struct SystemInfoState {
        String deviceName;
        String manufacturer;
        String firmwareVersion;
        bool operator==(const SystemInfoState& other) const {
            return deviceName == other.deviceName && 
                   manufacturer == other.manufacturer && 
                   firmwareVersion == other.firmwareVersion;
        }
        bool operator!=(const SystemInfoState& other) const { return !(*this == other); }
    };
    LazyState<SystemInfoState> systemInfoState;

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

    // Set callback for device name persistence (optional)
    void setDeviceNameCallback(std::function<void(const String&)> callback) {
        onDeviceNameChanged = callback;
    }

    // IWebUIProvider
    String getWebUIName() const override { return sys ? sys->metadata.name : String("System Info"); }
    String getWebUIVersion() const override { return sys ? sys->metadata.version : String("1.2.1"); }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!sys) return contexts;

        const auto& metrics = sys->getMetrics();
        if (sys->isDetailedInfoEnabled()) {
            // Component detail card so it shows in the Components tab
            WebUIContext detail;
            detail.contextId = "system_component";
            detail.title = "System Info";
            detail.icon = "dc-info";
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

            // Merged settings card (Hardware + Memory)
            WebUIContext sysSettings = WebUIContext::settings("system_info", "System Info");
            // Device identity (editable)
            const auto& cfg = sys->getConfig();
            sysSettings.withField(WebUIField("device_name", "Device Name", WebUIFieldType::Text, cfg.deviceName));
            sysSettings.withField(WebUIField("manufacturer", "Manufacturer", WebUIFieldType::Display, cfg.manufacturer, "", true));
            sysSettings.withField(WebUIField("firmware_version", "Firmware Version", WebUIFieldType::Display, cfg.firmwareVersion, "", true));
            // Hardware details
            sysSettings.withField(WebUIField("chip_model", "Chip", WebUIFieldType::Display, metrics.chipModel, "", true));
            sysSettings.withField(WebUIField("chip_revision", "Revision", WebUIFieldType::Display, String(metrics.chipRevision), "", true));
            sysSettings.withField(WebUIField("cpu_freq", "CPU Frequency", WebUIFieldType::Display, String(metrics.cpuFreq) + " MHz", "", true));
            // Memory details
            if (sys->isMemoryInfoEnabled()) {
                sysSettings.withField(WebUIField("free_heap", "Free Heap", WebUIFieldType::Display, sys->formatBytesPublic(metrics.freeHeap), "", true));
                sysSettings.withField(WebUIField("min_free_heap", "Min Free", WebUIFieldType::Display, sys->formatBytesPublic(metrics.minFreeHeap), "", true));
                sysSettings.withField(WebUIField("flash_size", "Flash", WebUIFieldType::Display, sys->formatBytesPublic(metrics.flashSize), "", true));
            }
            contexts.push_back(sysSettings);
        }
        // If detailed info disabled but memory info enabled, still expose combined card with memory only
        else if (sys->isMemoryInfoEnabled()) {
            WebUIContext sysSettings = WebUIContext::settings("system_info", "System Info");
            sysSettings.withField(WebUIField("free_heap", "Free Heap", WebUIFieldType::Display, sys->formatBytesPublic(metrics.freeHeap), "", true));
            sysSettings.withField(WebUIField("min_free_heap", "Min Free", WebUIFieldType::Display, sys->formatBytesPublic(metrics.minFreeHeap), "", true));
            sysSettings.withField(WebUIField("flash_size", "Flash", WebUIFieldType::Display, sys->formatBytesPublic(metrics.flashSize), "", true));
            contexts.push_back(sysSettings);
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
        } else if (contextId == "system_info") {
            JsonDocument doc;
            const auto& cfg = sys->getConfig();
            doc["device_name"] = cfg.deviceName;
            doc["manufacturer"] = cfg.manufacturer;
            doc["firmware_version"] = cfg.firmwareVersion;
            doc["chip_model"] = metrics.chipModel;
            doc["chip_revision"] = String(metrics.chipRevision);
            doc["cpu_freq"] = String(metrics.cpuFreq) + " MHz";
            doc["free_heap"] = sys->formatBytesPublic(metrics.freeHeap);
            doc["min_free_heap"] = sys->formatBytesPublic(metrics.minFreeHeap);
            doc["flash_size"] = sys->formatBytesPublic(metrics.flashSize);
            String json; serializeJson(doc, json); return json;
        }
        return "{}";
    }

    String handleWebUIRequest(const String& contextId, const String& /*endpoint*/, const String& method, const std::map<String, String>& params) override {
        if (contextId == "system_info" && method == "POST") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");
            if (fieldIt != params.end() && valueIt != params.end()) {
                const String& field = fieldIt->second;
                const String& value = valueIt->second;
                
                if (field == "device_name") {
                    // Use Get → Override → Set pattern
                    SystemInfoConfig cfg = sys->getConfig();
                    cfg.deviceName = value;
                    sys->setConfig(cfg);
                    
                    // Invoke persistence callback
                    if (onDeviceNameChanged) {
                        onDeviceNameChanged(value);
                    }
                    
                    // Force state reset to push update immediately
                    systemInfoState.reset();
                    
                    return "{\"success\":true}";
                }
            }
        }
        return "{\"success\":false}";
    }

    bool hasDataChanged(const String& contextId) override {
        if (!sys) return false;
        
        if (contextId == "system_info") {
            const auto& cfg = sys->getConfig();
            SystemInfoState current{
                cfg.deviceName,
                cfg.manufacturer,
                cfg.firmwareVersion
            };
            return systemInfoState.hasChanged(current);
        }
        
        // Always send updates for other contexts (metrics change frequently)
        return true;
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
