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
            doc["node_id"] = (const char*)cfg.nodeId;
            doc["device_name"] = (const char*)cfg.deviceName;
            doc["entity_count"] = stats.entityCount;
            doc["discovery_count"] = stats.discoveryCount;
            doc["state_updates"] = stats.stateUpdates;
            doc["commands"] = stats.commandsReceived;

        } else if (contextId == "ha_settings") {
            doc["node_id"] = (const char*)cfg.nodeId;
            doc["device_name"] = (const char*)cfg.deviceName;
            doc["manufacturer"] = (const char*)cfg.manufacturer;
            doc["model"] = (const char*)cfg.model;
            doc["discovery_prefix"] = (const char*)cfg.discoveryPrefix;
            doc["suggested_area"] = (const char*)cfg.suggestedArea;

        } else if (contextId == "ha_detail") {
            doc["entity_count"] = stats.entityCount;
            doc["discovery_count"] = stats.discoveryCount;
            doc["state_updates"] = stats.stateUpdates;
            doc["commands_received"] = stats.commandsReceived;
            doc["availability_topic"] = (const char*)cfg.availabilityTopic;
            doc["config_url"] = cfg.configUrl[0] == '\0' ? "N/A" : (const char*)cfg.configUrl;
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

            using namespace HomeAssistant;
            auto it = params.find("node_id");
            if (it != params.end()) HA::setField(newCfg.nodeId, it->second.c_str(), HA::MAX_NODE_ID);

            it = params.find("device_name");
            if (it != params.end()) HA::setField(newCfg.deviceName, it->second.c_str(), HA::MAX_DEVICE_NAME);

            it = params.find("manufacturer");
            if (it != params.end()) HA::setField(newCfg.manufacturer, it->second.c_str(), HA::MAX_MANUFACTURER);

            it = params.find("model");
            if (it != params.end()) HA::setField(newCfg.model, it->second.c_str(), HA::MAX_MODEL);

            it = params.find("discovery_prefix");
            if (it != params.end()) HA::setField(newCfg.discoveryPrefix, it->second.c_str(), HA::MAX_DISCOVERY_PREFIX);

            it = params.find("suggested_area");
            if (it != params.end()) HA::setField(newCfg.suggestedArea, it->second.c_str(), HA::MAX_SUGGESTED_AREA);

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
