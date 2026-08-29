#pragma once

#include <DomoticsCore/Platform_HAL.h>
#include <ESPAsyncWebServer.h>
#include <AsyncEventSource.h>
#include <functional>
#include "DomoticsCore/Logger.h"
#include "WebUIConfig.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @class WebSocketHandler
 * @brief Manages real-time server→client communication via SSE or polling.
 *
 * Dual-mode, decided at runtime based on available heap (no #ifdef):
 * - High heap (≥20KB, typical ESP32): SSE via AsyncEventSource for instant push.
 * - Low heap (<20KB, typical ESP8266): Client polls GET /api/ui/updates every 2s.
 *
 * Client→server actions always use HTTP POST /api/ui/action (both modes),
 * carrying the per-boot CSRF token (SEC-10).
 */
class WebSocketHandler {
public:
    using UIActionCallback = std::function<void(const String&, const String&, const String&)>;

    static constexpr uint32_t SSE_HEAP_THRESHOLD = 20000;

private:
    WebUIConfig config;
    
    // SSE source — only created when heap is sufficient
    AsyncEventSource* sseSource = nullptr;
    bool sseEnabled = false;
    unsigned long lastSseBroadcast = 0;

    // Callbacks to WebUIComponent
    UIActionCallback onUIAction;
    std::function<void()> onForceUpdate;
    
    // Polling fallback state
    volatile int pollingClients = 0;
    unsigned long lastPollTime = 0;

public:
    WebSocketHandler(const WebUIConfig& cfg) : config(cfg) {}
    
    ~WebSocketHandler() {
        if (sseSource) {
            sseSource->close();
            delete sseSource;
            sseSource = nullptr;
        }
    }

    void setUIActionCallback(UIActionCallback cb) { onUIAction = cb; }
    void setForceUpdateCallback(std::function<void()> cb) { onForceUpdate = cb; }

    void begin(AsyncWebServer* server) {
        if (!config.enableWebSocket || !server) return;

        uint32_t heap = HAL::Platform::getFreeHeap();
        if (heap >= SSE_HEAP_THRESHOLD) {
            sseSource = new AsyncEventSource("/api/ui/events");
            sseSource->onConnect([this](AsyncEventSourceClient* client) {
                DLOG_I(LOG_WEB, "SSE client connected (id=%u, clients=%u)",
                       (unsigned)client->lastId(), (unsigned)sseSource->count());
                if (onForceUpdate) onForceUpdate();
            });
            server->addHandler(sseSource);
            sseEnabled = true;
            DLOG_I(LOG_WEB, "SSE mode enabled on /api/ui/events (heap=%u)", (unsigned)heap);
        } else {
            DLOG_I(LOG_WEB, "Polling mode (heap=%u < %u)", (unsigned)heap, (unsigned)SSE_HEAP_THRESHOLD);
        }
    }

    void loop() {
        // Mark polling clients as gone if no poll received for 10s
        if (pollingClients > 0 &&
            HAL::Platform::getMillis() - lastPollTime > 10000) {
            pollingClients = 0;
        }
    }

    // Called from /api/ui/updates handler to track active polling clients
    void onPollRequest() {
        pollingClients = 1;
        lastPollTime = HAL::Platform::getMillis();
        if (onForceUpdate) onForceUpdate();
    }

    bool isSSEEnabled() const { return sseEnabled; }

    int getClientCount() const {
        int count = pollingClients;
        if (sseSource) count += (int)sseSource->count();
        return count;
    }

    int getSSEClientCount() const {
        return sseSource ? (int)sseSource->count() : 0;
    }

    void notifyWiFiNetworkChanged() {
        if (sseSource) {
            sseSource->send("{\"type\":\"wifi_network_changed\"}", "message", 0, 0);
        }
    }

    void closeAllConnections() {
        pollingClients = 0;
        if (sseSource) sseSource->close();
    }

    void broadcastSchemaChange(const String& componentName) {
        if (sseSource && sseSource->count() > 0) {
            sseSource->send("{\"type\":\"schema_changed\"}", "message", 0, 0);
        }
    }
    
    void broadcast(const String& message) {
        if (sseSource && sseSource->count() > 0) {
            sseSource->send(message.c_str(), "message", 0, 0);
        }
    }

    void broadcast(const char* buffer, size_t len) {
        if (sseSource && sseSource->count() > 0) {
            sseSource->send(buffer, "message", 0, 0);
        }
    }

    // Check if it's time to send periodic SSE updates
    bool shouldSendUpdates() {
        if (!sseSource || sseSource->count() == 0) return false;
        unsigned long now = HAL::Platform::getMillis();
        if (now - lastSseBroadcast < (unsigned long)config.wsUpdateInterval) return false;
        lastSseBroadcast = now;
        return true;
    }

    // Expose UIActionCallback for POST handler setup in WebUI.h
    void handleUIAction(const String& contextId, const String& field, const String& value) {
        if (onUIAction) {
            onUIAction(contextId, field, value);
        }
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
