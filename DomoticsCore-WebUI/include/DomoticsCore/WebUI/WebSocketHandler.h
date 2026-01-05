#pragma once

#include <DomoticsCore/Platform_HAL.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include <functional>
#include <algorithm>
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
        if (webSocket && HAL::Platform::getMillis() - lastConnectionCleanup >= CONNECTION_CLEANUP_INTERVAL) {
            cleanupStaleConnections();
            lastConnectionCleanup = HAL::Platform::getMillis();
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
            HAL::Platform::getMillis() - lastWebSocketUpdate >= (unsigned long)config.wsUpdateInterval) {
            lastWebSocketUpdate = HAL::Platform::getMillis();
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
            DLOG_D(LOG_WEB, "WS data received: len=%u", (unsigned)len);
            if (!arg || !data || len == 0 || len > 512) {
                DLOG_W(LOG_WEB, "WS data rejected: arg=%p data=%p len=%u", arg, data, (unsigned)len);
                return;
            }
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            DLOG_D(LOG_WEB, "WS frame: final=%d index=%u len=%u opcode=%d", info->final, (unsigned)info->index, (unsigned)info->len, info->opcode);
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                // ESP8266 String doesn't have (char*, size_t) constructor - null-terminate manually
                char buf[513];
                size_t copyLen = len < 512 ? len : 512;
                memcpy(buf, data, copyLen);
                buf[copyLen] = '\0';
                handleWebSocketMessage(String(buf));
            }
        } else if (type == WS_EVT_ERROR) {
            DLOG_E(LOG_WEB, "WS Error client #%u", client->id());
        }
    }

    void cleanupStaleConnections() {
        if (!webSocket) return;
        
        // Remove IDs of clients that are no longer connected
        activeClientIds.erase(
            std::remove_if(activeClientIds.begin(), activeClientIds.end(),
                [this](uint32_t id) {
                    AsyncWebSocketClient* client = webSocket->client(id);
                    return client == nullptr || client->status() != WS_CONNECTED;
                }),
            activeClientIds.end()
        );
        
        DLOG_D(LOG_WEB, "WS cleanup: %d active clients", (int)activeClientIds.size());
    }
    
    void handleWebSocketMessage(const String& message) {
        DLOG_D(LOG_WEB, "WS message: %s", message.c_str());
        JsonDocument doc;
        if (deserializeJson(doc, message)) {
            DLOG_W(LOG_WEB, "WS JSON parse failed");
            return;
        }
        
        const char* type = doc["type"];
        DLOG_D(LOG_WEB, "WS type: %s, onUIAction: %s", type ? type : "null", onUIAction ? "set" : "null");
        if (type && strcmp(type, "ui_action") == 0 && onUIAction) {
            String contextId = doc["contextId"].as<String>();
            String field = doc["field"].as<String>();
            JsonVariant v = doc["value"];
            String value = v.is<bool>() ? (v.as<bool>() ? "true" : "false") : v.as<String>();
            DLOG_D(LOG_WEB, "WS ui_action: ctx=%s, field=%s, value=%s", contextId.c_str(), field.c_str(), value.c_str());
            onUIAction(contextId, field, value);
        }
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
