#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include <functional>
#include <ArduinoJson.h>
#include "DomoticsCore/Logger.h"
#include "WebUIConfig.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @class WebSocketHandler
 * @brief Manages WebSocket connections and real-time updates.
 */
class WebSocketHandler {
public:
    using ClientConnectedCallback = std::function<void(AsyncWebSocketClient*)>;
    using UIActionCallback = std::function<void(const String&, const String&, const String&)>;

private:
    AsyncWebSocket* webSocket = nullptr;
    WebUIConfig config;
    
    // Callbacks to WebUIComponent
    ClientConnectedCallback onClientConnected;
    UIActionCallback onUIAction;
    std::function<void()> onForceUpdate;
    
    // Connection management
    std::vector<uint32_t> activeClientIds;
    unsigned long lastConnectionCleanup = 0;
    static const unsigned long CONNECTION_CLEANUP_INTERVAL = 30000; // 30 seconds
    
    // State tracking
    unsigned long lastWebSocketUpdate = 0;

public:
    WebSocketHandler(const WebUIConfig& cfg) : config(cfg) {}
    
    void setClientConnectedCallback(ClientConnectedCallback cb) { onClientConnected = cb; }
    void setUIActionCallback(UIActionCallback cb) { onUIAction = cb; }
    void setForceUpdateCallback(std::function<void()> cb) { onForceUpdate = cb; }

    ~WebSocketHandler() {
        if (webSocket) {
            delete webSocket;
            webSocket = nullptr;
        }
    }

    void begin(AsyncWebServer* server) {
        if (config.enableWebSocket && server) {
            webSocket = new AsyncWebSocket("/ws");
            
            webSocket->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                     AwsEventType type, void* arg, uint8_t* data, size_t len) {
                handleWebSocketEvent(client, type, arg, data, len);
            });
            server->addHandler(webSocket);
            
            DLOG_I(LOG_WEB, "WebSocket configured: max %d clients", config.maxWebSocketClients);
        }
    }

    void loop() {
        // Periodic connection cleanup
        if (webSocket && millis() - lastConnectionCleanup >= CONNECTION_CLEANUP_INTERVAL) {
            cleanupStaleConnections();
            lastConnectionCleanup = millis();
        }
        
        if (webSocket) {
            webSocket->cleanupClients();
        }
    }

    int getClientCount() const {
        return webSocket ? webSocket->count() : 0;
    }

    void notifyWiFiNetworkChanged() {
        if (webSocket && webSocket->count() > 0) {
            String msg = "{\"type\":\"wifi_network_changed\"}";
            webSocket->textAll(msg);
            DLOG_I(LOG_WEB, "Notified %d clients about WiFi network change", webSocket->count());
        }
    }

    void broadcastSchemaChange(const String& componentName) {
         if (webSocket && webSocket->count() > 0) {
            String msg = String("{\"type\":\"schema_changed\",\"name\":\"") + componentName + "\"}";
            if (msg.length() < 128) {
                webSocket->textAll(msg);
            }
        }
    }
    
    void broadcast(const String& message) {
        if (webSocket) {
            webSocket->textAll(message);
        }
    }

    // Check if it's time to send periodic updates
    bool shouldSendUpdates() {
        if (config.enableWebSocket && webSocket && 
            millis() - lastWebSocketUpdate >= (unsigned long)config.wsUpdateInterval) {
            lastWebSocketUpdate = millis();
            return true;
        }
        return false;
    }

private:
    void handleWebSocketEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            DLOG_I(LOG_WEB, "WS Client connected: #%u", client->id());
            if (activeClientIds.size() >= (size_t)config.maxWebSocketClients) {
                DLOG_W(LOG_WEB, "Max clients reached, closing #%u", client->id());
                client->close();
                return;
            }
            activeClientIds.push_back(client->id());
            
            // Send initial data to new client
            if (onClientConnected) onClientConnected(client);
            if (onForceUpdate) onForceUpdate();
        } else if (type == WS_EVT_DISCONNECT) {
            DLOG_I(LOG_WEB, "WS Client disconnected: #%u", client->id());
            auto it = std::find(activeClientIds.begin(), activeClientIds.end(), client->id());
            if (it != activeClientIds.end()) {
                activeClientIds.erase(it);
            }
        } else if (type == WS_EVT_DATA) {
            if (!arg || !data || len == 0 || len > 512) return;
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT && len < 256) {
                handleWebSocketMessage(String((char*)data, len));
            }
        } else if (type == WS_EVT_ERROR) {
            DLOG_E(LOG_WEB, "WS Error client #%u", client->id());
        }
    }

    void cleanupStaleConnections() {
        if (!webSocket) return;
    }
    
    void handleWebSocketMessage(const String& message) {
        JsonDocument doc;
        if (deserializeJson(doc, message)) return;
        
        const char* type = doc["type"];
        if (type && strcmp(type, "ui_action") == 0 && onUIAction) {
            String contextId = doc["contextId"].as<String>();
            String field = doc["field"].as<String>();
            JsonVariant v = doc["value"];
            String value = v.is<bool>() ? (v.as<bool>() ? "true" : "false") : v.as<String>();
            onUIAction(contextId, field, value);
        }
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
