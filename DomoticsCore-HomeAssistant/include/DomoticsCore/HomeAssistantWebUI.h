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
class HomeAssistantWebUI : public IWebUIProvider {
public:
    /**
     * @brief Construct WebUI provider
     * @param ha Pointer to HomeAssistant component (non-owning)
     */
    explicit HomeAssistantWebUI(HomeAssistant::HomeAssistantComponent* ha) : ha(ha) {}

    // ========== IWebUIProvider Interface ==========

    String getWebUIName() const override {
        return ha ? ha->getName() : String("HomeAssistant");
    }

    String getWebUIVersion() const override {
        return ha ? ha->getVersion() : String("0.1.0");
    }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!ha) return contexts;

        const auto& cfg = ha->getHAConfig();
        const auto& stats = ha->getStatistics();

        // Status badge - HA connection and entity count
        String statusText = String(stats.entityCount) + " entities";
        contexts.push_back(WebUIContext::statusBadge("ha_status", "Home Assistant", "dc-home-assistant")
            .withField(WebUIField("status", "Status", WebUIFieldType::Display, statusText, "", true))
            .withRealTime(5000)
            .withAPI("/api/ha/status")
            .withPriority(80));

        // Dashboard card - Entity overview
        WebUIContext dashboard = WebUIContext::dashboard("ha_dashboard", "Home Assistant", "dc-home-assistant");
        dashboard.withField(WebUIField("node_id", "Node ID", WebUIFieldType::Display, cfg.nodeId, "", true))
                 .withField(WebUIField("device_name", "Device", WebUIFieldType::Display, cfg.deviceName, "", true))
                 .withField(WebUIField("entity_count", "Entities", WebUIFieldType::Display, String(stats.entityCount), "", true))
                 .withField(WebUIField("discovery_count", "Discoveries", WebUIFieldType::Display, String(stats.discoveryCount), "", true))
                 .withField(WebUIField("state_updates", "State Updates", WebUIFieldType::Display, String(stats.stateUpdates), "", true))
                 .withField(WebUIField("commands", "Commands", WebUIFieldType::Display, String(stats.commandsReceived), "", true))
                 .withRealTime(5000)
                 .withAPI("/api/ha/dashboard")
                 .withPriority(75);

        contexts.push_back(dashboard);

        // Settings card
        WebUIContext settings = WebUIContext::settings("ha_settings", "Home Assistant Configuration");
        settings.withField(WebUIField("node_id", "Node ID", WebUIFieldType::Text, cfg.nodeId))
                .withField(WebUIField("device_name", "Device Name", WebUIFieldType::Text, cfg.deviceName))
                .withField(WebUIField("manufacturer", "Manufacturer", WebUIFieldType::Text, cfg.manufacturer))
                .withField(WebUIField("model", "Model", WebUIFieldType::Text, cfg.model))
                .withField(WebUIField("discovery_prefix", "Discovery Prefix", WebUIFieldType::Text, cfg.discoveryPrefix))
                .withField(WebUIField("suggested_area", "Suggested Area", WebUIFieldType::Text, cfg.suggestedArea))
                .withAPI("/api/ha/settings");

        contexts.push_back(settings);

        // Component detail - Full statistics (use dashboard for ComponentDetail location)
        contexts.push_back(WebUIContext("ha_detail", "Home Assistant Details", "dc-home-assistant", 
                                       WebUILocation::ComponentDetail, WebUIPresentation::Card)
            .withField(WebUIField("entity_count", "Total Entities", WebUIFieldType::Display, String(stats.entityCount), "", true))
            .withField(WebUIField("discovery_count", "Discovery Publishes", WebUIFieldType::Display, String(stats.discoveryCount), "", true))
            .withField(WebUIField("state_updates", "State Updates Sent", WebUIFieldType::Display, String(stats.stateUpdates), "", true))
            .withField(WebUIField("commands_received", "Commands Received", WebUIFieldType::Display, String(stats.commandsReceived), "", true))
            .withField(WebUIField("availability_topic", "Availability Topic", WebUIFieldType::Display, cfg.availabilityTopic, "", true))
            .withField(WebUIField("config_url", "Config URL", WebUIFieldType::Display, cfg.configUrl.isEmpty() ? "N/A" : cfg.configUrl, "", true))
            .withRealTime(5000)
            .withAPI("/api/ha/detail"));

        return contexts;
    }

    String getWebUIData(const String& contextId) override {
        if (!ha) return "{}";

        JsonDocument doc;
        const auto& cfg = ha->getHAConfig();
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
            HomeAssistant::HAConfig newCfg = ha->getHAConfig();

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

            // Republish discovery with new configuration
            ha->publishDiscovery();

            return "{\"success\":true,\"message\":\"Configuration updated and discovery republished\"}";
        }

        return "{\"error\":\"Unsupported operation\"}";
    }

private:
    HomeAssistant::HomeAssistantComponent* ha;
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
