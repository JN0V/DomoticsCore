#pragma once

#include "DomoticsCore/HomeAssistant.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/WebUI.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief WebUI provider for Home Assistant component
 * 
 * Provides web interface for HA configuration, entity management, and statistics.
 * 
 * UI Contexts:
 * - ha_status: Header badge showing connection status and entity count
 * - ha_dashboard: Dashboard card with entity list
 * - ha_settings: Settings card for configuration
 * - ha_detail: Component detail with statistics
 */
class HomeAssistantWebUI : public CachingWebUIProvider {
public:
    /**
     * @brief Construct WebUI provider
     * @param ha Pointer to HomeAssistant component (non-owning)
     */
    explicit HomeAssistantWebUI(HomeAssistant::HomeAssistantComponent* ha) : ha(ha) {}

    /**
     * @brief Set callback for HomeAssistant configuration persistence (optional)
     */
    void setConfigSaveCallback(std::function<void(const HomeAssistant::HAConfig&)> callback) {
        onConfigSaved = callback;
    }

    // ========== IWebUIProvider Interface ==========

    String getWebUIName() const override {
        return ha ? ha->getMetadata().name : String("HomeAssistant");
    }

    String getWebUIVersion() const override {
        return ha ? ha->getMetadata().version : String("1.4.0");
    }

protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        if (!ha) return;

        // Status badge - placeholder values, real values from getWebUIData()
        contexts.push_back(WebUIContext::statusBadge("ha_status", "Home Assistant", "dc-home-assistant")
            .withField(WebUIField("status", "Status", WebUIFieldType::Display, "0 entities", "", true))
            .withRealTime(5000)
            .withAPI("/api/ha/status")
            .withPriority(80));

        // Dashboard card - Entity overview - placeholder values
        WebUIContext dashboard = WebUIContext::dashboard("ha_dashboard", "Home Assistant", "dc-home-assistant");
        dashboard.withField(WebUIField("node_id", "Node ID", WebUIFieldType::Display, "", "", true))
                 .withField(WebUIField("device_name", "Device", WebUIFieldType::Display, "", "", true))
                 .withField(WebUIField("entity_count", "Entities", WebUIFieldType::Display, "0", "", true))
                 .withField(WebUIField("discovery_count", "Discoveries", WebUIFieldType::Display, "0", "", true))
                 .withField(WebUIField("state_updates", "State Updates", WebUIFieldType::Display, "0", "", true))
                 .withField(WebUIField("commands", "Commands", WebUIFieldType::Display, "0", "", true))
                 .withRealTime(5000)
                 .withAPI("/api/ha/dashboard")
                 .withPriority(75);

        contexts.push_back(dashboard);

        // Settings card - placeholder values
        WebUIContext settings = WebUIContext::settings("ha_settings", "Home Assistant Configuration");
        settings.withField(WebUIField("node_id", "Node ID", WebUIFieldType::Text, ""))
                .withField(WebUIField("device_name", "Device Name", WebUIFieldType::Text, ""))
                .withField(WebUIField("manufacturer", "Manufacturer", WebUIFieldType::Text, ""))
                .withField(WebUIField("model", "Model", WebUIFieldType::Text, ""))
                .withField(WebUIField("discovery_prefix", "Discovery Prefix", WebUIFieldType::Text, "homeassistant"))
                .withField(WebUIField("suggested_area", "Suggested Area", WebUIFieldType::Text, ""))
                .withAPI("/api/ha/settings");

        contexts.push_back(settings);

        // Component detail - Full statistics - placeholder values
        contexts.push_back(WebUIContext("ha_detail", "Home Assistant Details", "dc-home-assistant",
                                       WebUILocation::ComponentDetail, WebUIPresentation::Card)
            .withField(WebUIField("entity_count", "Total Entities", WebUIFieldType::Display, "0", "", true))
            .withField(WebUIField("discovery_count", "Discovery Publishes", WebUIFieldType::Display, "0", "", true))
            .withField(WebUIField("state_updates", "State Updates Sent", WebUIFieldType::Display, "0", "", true))
            .withField(WebUIField("commands_received", "Commands Received", WebUIFieldType::Display, "0", "", true))
            .withField(WebUIField("availability_topic", "Availability Topic", WebUIFieldType::Display, "", "", true))
            .withField(WebUIField("config_url", "Config URL", WebUIFieldType::Display, "N/A", "", true))
            .withRealTime(5000)
            .withAPI("/api/ha/detail"));
    }

public:

    String getWebUIData(const String& contextId) override {
        if (!ha) return "{}";

        JsonDocument doc;
        const auto& cfg = ha->getConfig();
        const auto& stats = ha->getStatistics();

        if (contextId == "ha_status") {
            String statusText = String(stats.entityCount) + " entities";
            doc["status"] = statusText;

        } else if (contextId == "ha_dashboard") {
            doc["node_id"] = cfg.nodeId;
            doc["device_name"] = cfg.deviceName;
            doc["entity_count"] = stats.entityCount;
            doc["discovery_count"] = stats.discoveryCount;
            doc["state_updates"] = stats.stateUpdates;
            doc["commands"] = stats.commandsReceived;

        } else if (contextId == "ha_settings") {
            doc["node_id"] = cfg.nodeId;
            doc["device_name"] = cfg.deviceName;
            doc["manufacturer"] = cfg.manufacturer;
            doc["model"] = cfg.model;
            doc["discovery_prefix"] = cfg.discoveryPrefix;
            doc["suggested_area"] = cfg.suggestedArea;

        } else if (contextId == "ha_detail") {
            doc["entity_count"] = stats.entityCount;
            doc["discovery_count"] = stats.discoveryCount;
            doc["state_updates"] = stats.stateUpdates;
            doc["commands_received"] = stats.commandsReceived;
            doc["availability_topic"] = cfg.availabilityTopic;
            doc["config_url"] = cfg.configUrl.isEmpty() ? "N/A" : cfg.configUrl;
        }

        String json;
        serializeJson(doc, json);
        return json;
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override {
        if (!ha) return "{\"error\":\"Component not available\"}";

        if (contextId == "ha_settings" && method == "POST") {
            // Update configuration
            HomeAssistant::HAConfig newCfg = ha->getConfig();

            auto it = params.find("node_id");
            if (it != params.end()) newCfg.nodeId = it->second;

            it = params.find("device_name");
            if (it != params.end()) newCfg.deviceName = it->second;

            it = params.find("manufacturer");
            if (it != params.end()) newCfg.manufacturer = it->second;

            it = params.find("model");
            if (it != params.end()) newCfg.model = it->second;

            it = params.find("discovery_prefix");
            if (it != params.end()) newCfg.discoveryPrefix = it->second;

            it = params.find("suggested_area");
            if (it != params.end()) newCfg.suggestedArea = it->second;

            ha->setConfig(newCfg);

            // Invoke persistence callback if set
            if (onConfigSaved) {
                onConfigSaved(newCfg);
            }

            // Republish discovery with new configuration
            ha->publishDiscovery();

            return "{\"success\":true,\"message\":\"Configuration updated and discovery republished\"}";
        }

        return "{\"error\":\"Unsupported operation\"}";
    }

private:
    HomeAssistant::HomeAssistantComponent* ha;
    std::function<void(const HomeAssistant::HAConfig&)> onConfigSaved;  // Callback for persistence
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
