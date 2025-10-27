#pragma once

#include "DomoticsCore/MQTT.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/WebUI.h"
#include "DomoticsCore/BaseWebUIComponents.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief WebUI provider for MQTT component
 * 
 * Provides web interface for MQTT configuration, monitoring, and testing.
 * Uses composition pattern - holds non-owning pointer to MQTT component.
 * 
 * UI Contexts:
 * - mqtt_status: Header badge showing connection status
 * - mqtt_settings: Settings card for configuration
 * - mqtt_detail: Component detail with statistics and test interface
 * 
 * @example Usage
 * ```cpp
 * auto* webui = core.getComponent<WebUIComponent>("WebUI");
 * auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
 * if (webui && mqtt) {
 *     webui->registerProviderWithComponent(new MQTTWebUI(mqtt), mqtt);
 * }
 * ```
 */
class MQTTWebUI : public IWebUIProvider {
public:
    /**
     * @brief Construct WebUI provider
     * @param component Non-owning pointer to MQTT component
     */
    explicit MQTTWebUI(MQTTComponent* component) 
        : mqtt(component) {}
    
    // ========== IWebUIProvider Interface ==========
    
    String getWebUIName() const override { 
        return mqtt ? mqtt->getName() : String("MQTT"); 
    }
    
    String getWebUIVersion() const override { 
        return mqtt ? mqtt->getVersion() : String("0.1.0"); 
    }
    
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!mqtt) return contexts;
        
        const MQTTConfig& cfg = mqtt->getMQTTConfig();
        
        // Header status badge
        // Frontend toggles color when first field is truthy; use boolean-like value for 'state'
        String initState = mqtt->isConnected() ? String("ON") : String("OFF");
        contexts.push_back(WebUIContext::statusBadge("mqtt_status", "MQTT", "dc-mqtt")
            .withField(WebUIField("state", "State", WebUIFieldType::Status, initState))
            .withRealTime(2000)
            .withAPI("/api/mqtt/status"));
        
        // Settings card
        WebUIContext settings = WebUIContext::settings("mqtt_settings", "MQTT Configuration");
        settings.withField(WebUIField("enabled", "MQTT Enabled", WebUIFieldType::Boolean, cfg.enabled ? "true" : "false"))
                .withField(WebUIField("broker", "Broker Address", WebUIFieldType::Text, cfg.broker))
                .withField(WebUIField("port", "Port", WebUIFieldType::Number, String(cfg.port)))
                .withField(WebUIField("username", "Username", WebUIFieldType::Text, cfg.username))
                .withField(WebUIField("password", "Password", WebUIFieldType::Text, ""))
                .withField(WebUIField("client_id", "Client ID", WebUIFieldType::Text, cfg.clientId))
                .withField(WebUIField("use_tls", "Use TLS/SSL", WebUIFieldType::Boolean, cfg.useTLS ? "true" : "false"))
                .withField(WebUIField("clean_session", "Clean Session", WebUIFieldType::Boolean, cfg.cleanSession ? "true" : "false"))
                .withField(WebUIField("lwt_enabled", "Last Will Enabled", WebUIFieldType::Boolean, cfg.enableLWT ? "true" : "false"))
                .withField(WebUIField("lwt_topic", "LWT Topic", WebUIFieldType::Text, cfg.lwtTopic))
                .withField(WebUIField("lwt_message", "LWT Message", WebUIFieldType::Text, cfg.lwtMessage))
                .withAPI("/api/mqtt/settings");
        
        contexts.push_back(settings);
        
        // Component detail with statistics
        WebUIContext detail = WebUIContext("mqtt_detail", "MQTT Client", "dc-mqtt", 
                                          WebUILocation::ComponentDetail, WebUIPresentation::Card);
        const auto& stats = mqtt->getStatistics();
        
        detail.withField(WebUIField("broker_addr", "Broker", WebUIFieldType::Text, cfg.broker + ":" + String(cfg.port)))
              .withField(WebUIField("state", "Connection State", WebUIFieldType::Status, mqtt->getStateString()))
              .withField(WebUIField("uptime", "Uptime", WebUIFieldType::Text, String(stats.uptime) + "s"))
              .withField(WebUIField("detail_client_id", "Client ID", WebUIFieldType::Text, cfg.clientId))
              .withField(WebUIField("publish_count", "Messages Published", WebUIFieldType::Number, String(stats.publishCount)))
              .withField(WebUIField("receive_count", "Messages Received", WebUIFieldType::Number, String(stats.receiveCount)))
              .withField(WebUIField("subscription_count", "Active Subscriptions", WebUIFieldType::Number, String(stats.subscriptionCount)))
              .withField(WebUIField("queue_size", "Queued Messages", WebUIFieldType::Number, String(mqtt->getQueuedMessageCount())))
              .withField(WebUIField("reconnect_count", "Reconnections", WebUIFieldType::Number, String(stats.reconnectCount)))
              .withField(WebUIField("error_count", "Publish Errors", WebUIFieldType::Number, String(stats.publishErrors)))
              .withRealTime(1000)
              .withAPI("/api/mqtt/detail");
        
        contexts.push_back(detail);
        
        return contexts;
    }
    
    String getWebUIData(const String& contextId) override {
        if (!mqtt) return "{}";
        
        JsonDocument doc;
        
        if (contextId == "mqtt_status") {
            const String label = mqtt->getStateString();
            // Primary state used by app.js to toggle 'active' class (true/false)
            doc["state"] = mqtt->isConnected() ? "true" : "false";
            // Provide normalized code + human label additionally
            String code = "unknown";
            if (label == "Connected") code = "connected";
            else if (label == "Connecting") code = "connecting";
            else if (label == "Disconnected") code = "disconnected";
            else if (label == "Error") code = "error";
            doc["state_label"] = label;
            doc["connected"] = mqtt->isConnected();
            doc["state_code"] = code;
            
        } else if (contextId == "mqtt_settings") {
            const MQTTConfig& cfg = mqtt->getMQTTConfig();
            doc["enabled"] = cfg.enabled;
            doc["broker"] = cfg.broker;
            doc["port"] = cfg.port;
            doc["username"] = cfg.username;
            doc["client_id"] = cfg.clientId;
            doc["use_tls"] = cfg.useTLS;
            doc["clean_session"] = cfg.cleanSession;
            doc["lwt_enabled"] = cfg.enableLWT;
            doc["lwt_topic"] = cfg.lwtTopic;
            doc["lwt_message"] = cfg.lwtMessage;
            doc["connected"] = mqtt->isConnected();
            
        } else if (contextId == "mqtt_detail") {
            const MQTTConfig& cfg = mqtt->getMQTTConfig();
            const auto& stats = mqtt->getStatistics();
            
            doc["broker_addr"] = cfg.broker + ":" + String(cfg.port);
            doc["state"] = mqtt->getStateString();
            doc["uptime"] = stats.uptime;
            doc["client_id"] = cfg.clientId;
            doc["publish_count"] = stats.publishCount;
            doc["receive_count"] = stats.receiveCount;
            doc["subscription_count"] = stats.subscriptionCount;
            doc["queue_size"] = mqtt->getQueuedMessageCount();
            doc["reconnect_count"] = stats.reconnectCount;
            doc["error_count"] = stats.publishErrors;
            
            // Add subscription list
            JsonArray subs = doc["subscriptions"].to<JsonArray>();
            for (const auto& topic : mqtt->getActiveSubscriptions()) {
                subs.add(topic);
            }
        }
        
        String json;
        if (serializeJson(doc, json) == 0) {
            return "{}";
        }
        return json;
    }
    
    bool hasDataChanged(const String& contextId) override {
        if (!mqtt) return false;
        if (contextId == "mqtt_status") {
            // Only push status updates when the connection state string changes
            return statusState.hasChanged(mqtt->getStateString());
        }
        return true; // default behavior for other contexts
    }
    
    String handleWebUIRequest(const String& contextId, const String& /*endpoint*/, 
                              const String& method, const std::map<String, String>& params) override {
        if (!mqtt) {
            return "{\"success\":false,\"error\":\"Component not available\"}";
        }
        
        if (method == "GET") {
            return "{\"success\":true}";
        }
        
        if (method != "POST") {
            return "{\"success\":false,\"error\":\"Method not allowed\"}";
        }
        
        // Handle settings updates
        if (contextId == "mqtt_settings") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");
            
            MQTTConfig cfg = mqtt->getMQTTConfig();
            
            // Handle field updates
            if (fieldIt != params.end() && valueIt != params.end()) {
                const String& field = fieldIt->second;
                const String& value = valueIt->second;
                
                if (field == "enabled") {
                    bool newEnabled = (value == "true" || value == "1");
                    bool wasEnabled = cfg.enabled;
                    cfg.enabled = newEnabled;
                    
                    // Handle state change
                    mqtt->setConfig(cfg);
                    if (!wasEnabled && newEnabled) {
                        // Enabling: connect
                        mqtt->connect();
                    } else if (wasEnabled && !newEnabled) {
                        // Disabling: disconnect
                        mqtt->disconnect();
                    }
                    return "{\"success\":true}";
                } else if (field == "broker") {
                    cfg.broker = value;
                } else if (field == "port") {
                    cfg.port = value.toInt();
                } else if (field == "username") {
                    cfg.username = value;
                } else if (field == "password") {
                    if (!value.isEmpty()) {  // Only update if not empty
                        cfg.password = value;
                    }
                } else if (field == "client_id") {
                    cfg.clientId = value;
                } else if (field == "use_tls") {
                    cfg.useTLS = (value == "true" || value == "1");
                } else if (field == "clean_session") {
                    cfg.cleanSession = (value == "true" || value == "1");
                } else if (field == "lwt_enabled") {
                    cfg.enableLWT = (value == "true" || value == "1");
                } else if (field == "lwt_topic") {
                    cfg.lwtTopic = value;
                } else if (field == "lwt_message") {
                    cfg.lwtMessage = value;
                }
                
                mqtt->setConfig(cfg);
                return "{\"success\":true}";
            }
        }
        
        return "{\"success\":false,\"error\":\"Unknown request\"}";
    }

private:
    MQTTComponent* mqtt;  // Non-owning pointer
    LazyState<String> statusState;
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
