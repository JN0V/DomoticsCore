#pragma once

#include "DomoticsCore/NTP.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/WebUI.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief WebUI provider for NTP component
 * 
 * Provides web interface for NTP configuration, time display, and statistics.
 * 
 * UI Contexts:
 * - ntp_status: Header badge showing sync status
 * - ntp_dashboard: Dashboard card with current time
 * - ntp_settings: Settings card for configuration
 * - ntp_detail: Component detail with statistics
 */
class NTPWebUI : public IWebUIProvider {
public:
    /**
     * @brief Construct WebUI provider
     * @param ntp Pointer to NTP component (non-owning)
     */
    explicit NTPWebUI(NTPComponent* ntp) : ntp(ntp) {}

    // ========== IWebUIProvider Interface ==========

    String getWebUIName() const override {
        return ntp ? ntp->getName() : String("NTP");
    }

    String getWebUIVersion() const override {
        return ntp ? ntp->getVersion() : String("0.1.0");
    }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!ntp) return contexts;

        const NTPConfig& cfg = ntp->getNTPConfig();

        // Header info - NTP provides time to the header zone
        String timeStr = ntp->isSynced() ? ntp->getFormattedTime("%H:%M:%S") : "--:--:--";
        contexts.push_back(WebUIContext::headerInfo("ntp_time", "Time", "dc-clock")
            .withField(WebUIField("time", "Time", WebUIFieldType::Display, timeStr, "", true))
            .withRealTime(1000)
            .withAPI("/api/ntp/time")
            .withPriority(100));  // High priority to appear first in header

        // Dashboard card - Current time display
        WebUIContext dashboard = WebUIContext::dashboard("ntp_dashboard", "Current Time", "dc-clock");
        dashboard.withField(WebUIField("time", "Time", WebUIFieldType::Display, ntp->getFormattedTime("%H:%M:%S"), "", true))
                 .withField(WebUIField("date", "Date", WebUIFieldType::Display, ntp->getFormattedTime("%Y-%m-%d"), "", true))
                 .withField(WebUIField("timezone", "Timezone", WebUIFieldType::Display, cfg.timezone, "", true))
                 .withField(WebUIField("uptime", "Uptime", WebUIFieldType::Display, ntp->getFormattedUptime(), "", true))
                 .withRealTime(1000)
                 .withAPI("/api/ntp/dashboard")
                 .withPriority(100);  // High priority for dashboard

        contexts.push_back(dashboard);

        // Settings card
        WebUIContext settings = WebUIContext::settings("ntp_settings", "NTP Configuration");
        
        // Build servers comma-separated string
        String serversStr;
        for (size_t i = 0; i < cfg.servers.size(); i++) {
            if (i > 0) serversStr += ", ";
            serversStr += cfg.servers[i];
        }

        settings.withField(WebUIField("enabled", "Enable NTP Sync", WebUIFieldType::Boolean, cfg.enabled ? "true" : "false"))
                .withField(WebUIField("servers", "NTP Servers", WebUIFieldType::Text, serversStr))
                .withField(WebUIField("sync_interval", "Sync Interval (hours)", WebUIFieldType::Number, String(cfg.syncInterval / 3600)))
                .withField(WebUIField("timezone", "Timezone", WebUIFieldType::Text, cfg.timezone))
                .withField(WebUIField("sync_now_btn", "Sync Now", WebUIFieldType::Button, ""))
                .withAPI("/api/ntp/settings");

        contexts.push_back(settings);

        // Component detail with statistics
        WebUIContext detail = WebUIContext("ntp_detail", "NTP Client", "dc-clock",
                                          WebUILocation::ComponentDetail, WebUIPresentation::Card);
        const auto& stats = ntp->getStatistics();

        String nextSyncStr;
        uint32_t nextSync = ntp->getNextSyncIn();
        if (nextSync > 0) {
            nextSyncStr = String(nextSync / 60) + "m " + String(nextSync % 60) + "s";
        } else {
            nextSyncStr = "N/A";
        }

        detail.withField(WebUIField("synced", "Synchronized", WebUIFieldType::Status, ntp->isSynced() ? "Yes" : "No"))
              .withField(WebUIField("current_time", "Current Time", WebUIFieldType::Text, ntp->getFormattedTime()))
              .withField(WebUIField("timezone_name", "Timezone", WebUIFieldType::Text, cfg.timezone))
              .withField(WebUIField("gmt_offset", "GMT Offset", WebUIFieldType::Text, String(ntp->getGMTOffset() / 3600) + "h"))
              .withField(WebUIField("dst", "DST Active", WebUIFieldType::Status, ntp->isDST() ? "Yes" : "No"))
              .withField(WebUIField("sync_count", "Sync Count", WebUIFieldType::Number, String(stats.syncCount)))
              .withField(WebUIField("sync_errors", "Sync Errors", WebUIFieldType::Number, String(stats.syncErrors)))
              .withField(WebUIField("last_sync", "Last Sync", WebUIFieldType::Text, 
                         stats.lastSyncTime > 0 ? ntp->getFormattedTime("%Y-%m-%d %H:%M:%S") : "Never"))
              .withField(WebUIField("next_sync", "Next Sync In", WebUIFieldType::Text, nextSyncStr))
              .withField(WebUIField("uptime", "System Uptime", WebUIFieldType::Text, ntp->getFormattedUptime()))
              .withRealTime(2000)
              .withAPI("/api/ntp/detail");

        contexts.push_back(detail);

        return contexts;
    }

    String getWebUIData(const String& contextId) override {
        if (!ntp) return "{}";

        JsonDocument doc;

        if (contextId == "ntp_time") {
            // Provide time for header info zone
            doc["time"] = ntp->isSynced() ? ntp->getFormattedTime("%H:%M:%S") : "--:--:--";

        } else if (contextId == "ntp_dashboard") {
            doc["time"] = ntp->getFormattedTime("%H:%M:%S");
            doc["date"] = ntp->getFormattedTime("%Y-%m-%d");
            doc["timezone"] = ntp->getTimezone();
            doc["uptime"] = ntp->getFormattedUptime();

        } else if (contextId == "ntp_settings") {
            const NTPConfig& cfg = ntp->getNTPConfig();
            doc["enabled"] = cfg.enabled;
            
            // Servers as comma-separated
            String serversStr;
            for (size_t i = 0; i < cfg.servers.size(); i++) {
                if (i > 0) serversStr += ", ";
                serversStr += cfg.servers[i];
            }
            doc["servers"] = serversStr;
            doc["sync_interval"] = cfg.syncInterval / 3600;  // Hours
            doc["timezone"] = cfg.timezone;

        } else if (contextId == "ntp_detail") {
            const NTPConfig& cfg = ntp->getNTPConfig();
            const auto& stats = ntp->getStatistics();

            doc["synced"] = ntp->isSynced() ? "Yes" : "No";
            doc["current_time"] = ntp->getFormattedTime();
            doc["timezone_name"] = cfg.timezone;
            doc["gmt_offset"] = String(ntp->getGMTOffset() / 3600) + "h";
            doc["dst"] = ntp->isDST() ? "Yes" : "No";
            doc["sync_count"] = stats.syncCount;
            doc["sync_errors"] = stats.syncErrors;
            doc["last_sync"] = stats.lastSyncTime > 0 ? 
                               ntp->getFormattedTime("%Y-%m-%d %H:%M:%S") : "Never";
            
            uint32_t nextSync = ntp->getNextSyncIn();
            if (nextSync > 0) {
                doc["next_sync"] = String(nextSync / 60) + "m " + String(nextSync % 60) + "s";
            } else {
                doc["next_sync"] = "N/A";
            }
            doc["uptime"] = ntp->getFormattedUptime();
        }

        String json;
        if (serializeJson(doc, json) == 0) {
            return "{}";
        }
        return json;
    }

    String handleWebUIRequest(const String& contextId, const String& /*endpoint*/,
                              const String& method, const std::map<String, String>& params) override {
        if (!ntp) {
            return "{\"success\":false,\"error\":\"Component not available\"}";
        }

        if (method == "GET") {
            return "{\"success\":true}";
        }

        if (method != "POST") {
            return "{\"success\":false,\"error\":\"Method not allowed\"}";
        }

        // Handle settings updates
        if (contextId == "ntp_settings") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");
            auto buttonIt = params.find("button");

            NTPConfig cfg = ntp->getNTPConfig();

            // Handle field updates
            if (fieldIt != params.end() && valueIt != params.end()) {
                const String& field = fieldIt->second;
                const String& value = valueIt->second;

                if (field == "enabled") {
                    cfg.enabled = (value == "true" || value == "1");
                } else if (field == "servers") {
                    // Parse comma-separated servers
                    cfg.servers.clear();
                    String servers = value;
                    servers.trim();
                    int start = 0;
                    int commaPos;
                    while ((commaPos = servers.indexOf(',', start)) != -1) {
                        String server = servers.substring(start, commaPos);
                        server.trim();
                        if (server.length() > 0) {
                            cfg.servers.push_back(server);
                        }
                        start = commaPos + 1;
                    }
                    // Last server
                    String server = servers.substring(start);
                    server.trim();
                    if (server.length() > 0) {
                        cfg.servers.push_back(server);
                    }
                } else if (field == "sync_interval") {
                    int hours = value.toInt();
                    if (hours > 0) {
                        cfg.syncInterval = hours * 3600;  // Convert to seconds
                    }
                } else if (field == "timezone") {
                    cfg.timezone = value;
                }

                ntp->setConfig(cfg);
                return "{\"success\":true}";
            }

            // Handle button actions
            if (buttonIt != params.end()) {
                const String& button = buttonIt->second;

                if (button == "sync_now") {
                    bool success = ntp->syncNow();
                    return success ? "{\"success\":true}" : "{\"success\":false,\"error\":\"Sync failed to start\"}";
                }
            }
        }

        return "{\"success\":false,\"error\":\"Unknown request\"}";
    }

private:
    NTPComponent* ntp;  // Non-owning pointer
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
