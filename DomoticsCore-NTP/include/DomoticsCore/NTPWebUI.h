#pragma once

#include "DomoticsCore/NTP.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/WebUI.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

// ========== Constexpr Timezone Lookup (stored in flash, not RAM) ==========

/**
 * @brief Timezone entry for constexpr lookup table
 */
struct TimezoneLookupEntry {
    const char* posix;       // POSIX timezone string
    const char* friendly;    // User-friendly name
};

/**
 * @brief Constexpr timezone lookup table (stored in flash on ESP8266/ESP32)
 *
 * This replaces std::map<String, String> to save ~3-4KB RAM on ESP8266.
 * Linear search is acceptable for 29 entries (O(29) worst case).
 */
static constexpr TimezoneLookupEntry TIMEZONE_LOOKUP[] = {
    {"UTC0", "UTC"},
    {"WET0WEST,M3.5.0/1,M10.5.0", "London"},
    {"CET-1CEST,M3.5.0,M10.5.0/3", "Paris (CET)"},
    {"EET-2EEST,M3.5.0/3,M10.5.0/4", "Athens (EET)"},
    {"MSK-3", "Moscow"},
    {"EST5EDT,M3.2.0,M11.1.0", "New York"},
    {"CST6CDT,M3.2.0,M11.1.0", "Chicago"},
    {"MST7MDT,M3.2.0,M11.1.0", "Denver"},
    {"PST8PDT,M3.2.0,M11.1.0", "Los Angeles"},
    {"AKST9AKDT,M3.2.0,M11.1.0", "Anchorage"},
    {"HST10", "Honolulu"},
    {"<-03>3", "Sao Paulo"},
    {"CST-8", "Shanghai"},
    {"JST-9", "Tokyo"},
    {"KST-9", "Seoul"},
    {"IST-5:30", "India"},
    {"PKT-5", "Karachi"},
    {"<+07>-7", "Bangkok"},
    {"WIB-7", "Jakarta"},
    {"GST-4", "Dubai"},
    {"AEST-10AEDT,M10.1.0,M4.1.0/3", "Sydney"},
    {"ACST-9:30ACDT,M10.1.0,M4.1.0/3", "Adelaide"},
    {"AWST-8", "Perth"},
    {"NZST-12NZDT,M9.5.0,M4.1.0/3", "Auckland"},
    {"EAT-3", "Nairobi"},
    {"SAST-2", "Johannesburg"},
    {"WAT-1", "Lagos"},
    {"EET-2EEST,M3.5.5/0,M10.5.5/0", "Jerusalem"},
    {"<+03>-3", "Riyadh"},
};

static constexpr size_t TIMEZONE_LOOKUP_COUNT = sizeof(TIMEZONE_LOOKUP) / sizeof(TIMEZONE_LOOKUP[0]);

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
    explicit NTPWebUI(NTPComponent* ntp) : ntp(ntp) {
        // State tracking uses LazyState helper - no manual initialization needed
        // States will be initialized on first hasDataChanged() call
    }

    /**
     * @brief Set callback for NTP configuration persistence (optional)
     */
    void setConfigSaveCallback(std::function<void(const NTPConfig&)> callback) {
        onConfigSaved = callback;
    }

private:
    std::function<void(const NTPConfig&)> onConfigSaved; // callback for persistence

    /**
     * @brief Get friendly timezone name from POSIX string
     * Uses constexpr lookup table (O(n) linear search, n=29)
     */
    String getTimezoneFriendlyName(const String& posixTz) const {
        for (size_t i = 0; i < TIMEZONE_LOOKUP_COUNT; ++i) {
            if (posixTz == TIMEZONE_LOOKUP[i].posix) {
                return String(TIMEZONE_LOOKUP[i].friendly);
            }
        }
        return posixTz;  // Fallback to POSIX string if not found
    }

public:

    // ========== IWebUIProvider Interface ==========

    String getWebUIName() const override {
        return ntp ? ntp->metadata.name : String("NTP");
    }

    String getWebUIVersion() const override {
        return ntp ? ntp->metadata.version : String("1.0.2");
    }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!ntp) return contexts;

        const NTPConfig& cfg = ntp->getConfig();

        // Header info - NTP provides time to the header zone (minimal, always included)
        String timeStr = ntp->isSynced() ? ntp->getFormattedTime("%H:%M:%S") : "--:--:--";
        contexts.push_back(WebUIContext::headerInfo("ntp_time", "Time", "dc-clock")
            .withField(WebUIField("time", "Time", WebUIFieldType::Display, timeStr, "", true))
            .withRealTime(1000)
            .withAPI("/api/ntp/time")
            .withPriority(100));

        // Dashboard card - Current time display
        WebUIContext dashboard = WebUIContext::dashboard("ntp_dashboard", "Current Time", "dc-clock");
        String friendlyTz = getTimezoneFriendlyName(cfg.timezone);
        dashboard.withField(WebUIField("time", "Time", WebUIFieldType::Display, ntp->getFormattedTime("%H:%M:%S"), "", true))
                 .withField(WebUIField("date", "Date", WebUIFieldType::Display, ntp->getFormattedTime("%Y-%m-%d"), "", true))
                 .withField(WebUIField("timezone", "Timezone", WebUIFieldType::Display, friendlyTz, "", true))
                 .withRealTime(1000)
                 .withAPI("/api/ntp/dashboard")
                 .withPriority(100);
        contexts.push_back(dashboard);

        // Settings card
        WebUIContext settings = WebUIContext::settings("ntp_settings", "NTP Configuration");

        // Build servers comma-separated string
        String serversStr;
        for (size_t i = 0; i < cfg.servers.size(); i++) {
            if (i > 0) serversStr += ", ";
            serversStr += cfg.servers[i];
        }

        // Build timezone dropdown with city names
        WebUIField timezoneField("timezone", "Timezone", WebUIFieldType::Select, cfg.timezone);
        
        // Major cities by region (one entry per unique POSIX timezone)
        timezoneField.addOption("UTC0", "UTC (Coordinated Universal Time)");
        
        // Europe
        timezoneField.addOption("WET0WEST,M3.5.0/1,M10.5.0", "Europe/London (UK, Ireland, Portugal)");
        timezoneField.addOption("CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Paris (FR, DE, IT, ES, BE, NL)");
        timezoneField.addOption("EET-2EEST,M3.5.0/3,M10.5.0/4", "Europe/Athens (Greece, Romania)");
        timezoneField.addOption("MSK-3", "Europe/Moscow (Russia)");
        
        // Americas
        timezoneField.addOption("EST5EDT,M3.2.0,M11.1.0", "America/New_York (US Eastern)");
        timezoneField.addOption("CST6CDT,M3.2.0,M11.1.0", "America/Chicago (US Central)");
        timezoneField.addOption("MST7MDT,M3.2.0,M11.1.0", "America/Denver (US Mountain)");
        timezoneField.addOption("PST8PDT,M3.2.0,M11.1.0", "America/Los_Angeles (US Pacific)");
        timezoneField.addOption("AKST9AKDT,M3.2.0,M11.1.0", "America/Anchorage (Alaska)");
        timezoneField.addOption("HST10", "Pacific/Honolulu (Hawaii)");
        timezoneField.addOption("<-03>3", "America/Sao_Paulo (Brazil, Argentina)");
        
        // Asia
        timezoneField.addOption("CST-8", "Asia/Shanghai (China)");
        timezoneField.addOption("JST-9", "Asia/Tokyo (Japan)");
        timezoneField.addOption("KST-9", "Asia/Seoul (South Korea)");
        timezoneField.addOption("IST-5:30", "Asia/Kolkata (India)");
        timezoneField.addOption("PKT-5", "Asia/Karachi (Pakistan)");
        timezoneField.addOption("<+07>-7", "Asia/Bangkok (Thailand, Vietnam)");
        timezoneField.addOption("WIB-7", "Asia/Jakarta (Indonesia)");
        timezoneField.addOption("GST-4", "Asia/Dubai (UAE)");
        
        // Australia & Pacific
        timezoneField.addOption("AEST-10AEDT,M10.1.0,M4.1.0/3", "Australia/Sydney (AEST)");
        timezoneField.addOption("ACST-9:30ACDT,M10.1.0,M4.1.0/3", "Australia/Adelaide (ACST)");
        timezoneField.addOption("AWST-8", "Australia/Perth (AWST)");
        timezoneField.addOption("NZST-12NZDT,M9.5.0,M4.1.0/3", "Pacific/Auckland (New Zealand)");
        
        // Africa
        timezoneField.addOption("EAT-3", "Africa/Nairobi (Kenya, Tanzania)");
        timezoneField.addOption("SAST-2", "Africa/Johannesburg (South Africa)");
        timezoneField.addOption("WAT-1", "Africa/Lagos (Nigeria)");
        
        // Middle East
        timezoneField.addOption("EET-2EEST,M3.5.5/0,M10.5.5/0", "Asia/Jerusalem (Israel)");
        timezoneField.addOption("<+03>-3", "Asia/Riyadh (Saudi Arabia)");

        settings.withField(WebUIField("enabled", "Enable NTP Sync", WebUIFieldType::Boolean, cfg.enabled ? "true" : "false"))
                .withField(WebUIField("servers", "NTP Servers", WebUIFieldType::Text, serversStr))
                .withField(WebUIField("sync_interval", "Sync Interval (hours)", WebUIFieldType::Number, String(cfg.syncInterval / 3600)))
                .withField(timezoneField)
                .withAPI("/api/ntp/settings");
        contexts.push_back(settings);

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
            doc["timezone"] = getTimezoneFriendlyName(ntp->getTimezone());

        } else if (contextId == "ntp_settings") {
            const NTPConfig& cfg = ntp->getConfig();
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

        }

        String json;
        if (serializeJson(doc, json) == 0) {
            return "{}";
        }
        return json;
    }

    String handleWebUIRequest(const String& contextId, const String& /*endpoint*/,
                              const String& method, const std::map<String, String>& params) override {
        DLOG_D(LOG_NTP, "[WebUI] handleWebUIRequest: contextId=%s, method=%s", contextId.c_str(), method.c_str());
        
        if (!ntp) {
            DLOG_E(LOG_NTP, "[WebUI] NTP component not available");
            return "{\"success\":false,\"error\":\"Component not available\"}";
        }

        if (method == "GET") {
            return "{\"success\":true}";
        }

        if (method != "POST") {
            DLOG_W(LOG_NTP, "[WebUI] Method not allowed: %s", method.c_str());
            return "{\"success\":false,\"error\":\"Method not allowed\"}";
        }

        // Handle settings updates
        if (contextId == "ntp_settings") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");

            if (fieldIt != params.end()) {
                DLOG_D(LOG_NTP, "[WebUI] Field: %s", fieldIt->second.c_str());
            }
            if (valueIt != params.end()) {
                DLOG_D(LOG_NTP, "[WebUI] Value: %s", valueIt->second.c_str());
            }

            NTPConfig cfg = ntp->getConfig();

            // Handle field updates
            if (fieldIt != params.end() && valueIt != params.end()) {
                const String& field = fieldIt->second;
                const String& value = valueIt->second;

                // Handle configuration field updates
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
                } else {
                    // Unknown field
                    DLOG_W(LOG_NTP, "[WebUI] Unknown field: %s", field.c_str());
                    return "{\"success\":false,\"error\":\"Unknown field\"}";
                }

                ntp->setConfig(cfg);

                // Invoke persistence callback if set
                if (onConfigSaved) {
                    DLOG_I(LOG_NTP, "[WebUI] Invoking config save callback");
                    onConfigSaved(cfg);
                }

                // Trigger immediate sync after configuration change
                if (cfg.enabled) {
                    DLOG_I(LOG_NTP, "[WebUI] Triggering immediate sync after config save");
                    ntp->syncNow();
                }

                return "{\"success\":true}";
            }
        }

        return "{\"success\":false,\"error\":\"Unknown request\"}";
    }

    bool hasDataChanged(const String& contextId) override {
        if (!ntp) return false;

        // Use LazyState helper for timing-independent change tracking
        if (contextId == "ntp_time") {
            return ntpTimeState.hasChanged(ntp->getUnixTime());
        }
        else if (contextId == "ntp_dashboard") {
            return ntpDashboardState.hasChanged(ntp->getUnixTime());
        }
        else if (contextId == "ntp_settings") {
            const NTPConfig& cfg = ntp->getConfig();
            NTPSettingsState current = {cfg.enabled, cfg.timezone};
            return ntpSettingsState.hasChanged(current);
        }
        return true;
    }

private:
    NTPComponent* ntp;  // Non-owning pointer
    // Timezone lookup uses constexpr TIMEZONE_LOOKUP[] table (stored in flash)

    // State tracking using LazyState helper for timing-independent initialization
    LazyState<time_t> ntpTimeState;           // ntp_time context
    LazyState<time_t> ntpDashboardState;      // ntp_dashboard context

    struct NTPSettingsState {
        bool enabled;
        String timezone;

        bool operator==(const NTPSettingsState& other) const {
            return enabled == other.enabled && timezone == other.timezone;
        }
        bool operator!=(const NTPSettingsState& other) const { return !(*this == other); }
    };
    LazyState<NTPSettingsState> ntpSettingsState;
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
