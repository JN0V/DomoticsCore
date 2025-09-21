#pragma once

// Enable WebUI features when this component is included
#define DOMOTICSCORE_WEBUI_ENABLED 1

#include "IWebUIProvider.h"
#include "WebUI/BaseWebUIComponents.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <pgmspace.h>

#include "IComponent.h"
#include "IWebUIProvider.h"
#include "../Logger.h"
#include "WebUIContent.h"

namespace DomoticsCore {
namespace Components {

/**
 * Optimized WebUI Component Configuration
 */
struct WebUIConfig {
    String deviceName = "DomoticsCore Device";
    String manufacturer = "DomoticsCore";
    String version = "1.0.0";
    String copyright = "© 2024 DomoticsCore";
    
    uint16_t port = 80;
    bool enableWebSocket = true;
    int wsUpdateInterval = 5000;        // WebSocket update interval in ms
    bool useFileSystem = false;         // Use SPIFFS/LittleFS for content
    String staticPath = "/webui";
    
    // Theme and customization
    String theme = "dark";              // dark, light, auto
    String primaryColor = "#007acc";    // Primary accent color
    String logoUrl = "";                // Custom logo URL
    
    // Security
    bool enableAuth = false;            // Enable basic authentication
    String username = "admin";          // Auth username
    String password = "";               // Auth password
    
    // Performance
    int maxWebSocketClients = 3;        // Max concurrent WebSocket connections
    int apiTimeout = 5000;              // API request timeout in ms
    bool enableCompression = true;      // Enable gzip compression
    bool enableCaching = true;          // Enable browser caching
    bool enableCORS = false;            // Enable CORS headers
};

// WebUIContent is now provided by WebUIContent.h as the single source of truth

/**
 * Optimized WebUI Component with PROGMEM content storage and crash resistance
 * Uses IWebUIProvider interface for modern multi-context support
 */
class WebUIComponent : public IComponent, public virtual IWebUIProvider {
private:
    WebUIConfig config;
    AsyncWebServer* server = nullptr;
    AsyncWebSocket* webSocket = nullptr;
    
    // Provider support
    std::map<String, IWebUIProvider*> contextProviders;
    
    unsigned long lastWebSocketUpdate = 0;

public:
    WebUIComponent(const WebUIConfig& cfg = WebUIConfig()) 
        : config(cfg) {}

    ~WebUIComponent() {
        if (webSocket) {
            delete webSocket;
        }
        if (server) {
            delete server;
        }
    }

    // IComponent interface
    ComponentStatus begin() override {
        server = new AsyncWebServer(config.port);
        
        if (config.enableWebSocket) {
            webSocket = new AsyncWebSocket("/ws");
            webSocket->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                     AwsEventType type, void* arg, uint8_t* data, size_t len) {
                handleWebSocketEvent(client, type, arg, data, len);
            });
            server->addHandler(webSocket);
        }
        
        setupRoutes();
        server->begin();
        
        return ComponentStatus::Success;
    }

    void loop() override {
        if (config.enableWebSocket && webSocket && 
            millis() - lastWebSocketUpdate >= config.wsUpdateInterval) {
            sendWebSocketUpdates();
            lastWebSocketUpdate = millis();
        }
        
        if (webSocket) {
            webSocket->cleanupClients();
        }
    }

    ComponentStatus shutdown() override {
        if (server) {
            server->end();
        }
        return ComponentStatus::Success;
    }

    String getName() const override {
        return "WebUI";
    }

    String getVersion() const override {
        return "2.0.0";
    }

    // Provider management
    void registerProvider(IWebUIProvider* provider) {
        if (!provider) return;

        auto contexts = provider->getWebUIContexts();
        if (contexts.empty()) {
            DLOG_W(LOG_CORE, "[WebUI] Provider has no contexts to register.");
            return;
        }

        for (const auto& context : contexts) {
            contextProviders[context.contextId] = provider;
            DLOG_I(LOG_CORE, "[WebUI] Registered provider for context: %s", context.contextId.c_str());
        }
    }

    void unregisterProvider(IWebUIProvider* provider) {
        if (!provider) return;
        for (auto it = contextProviders.begin(); it != contextProviders.end(); ) {
            if (it->second == provider) {
                it = contextProviders.erase(it);
            } else {
                ++it;
            }
        }
    }

    int getWebSocketClients() const {
        return webSocket ? webSocket->count() : 0;
    }

    uint16_t getPort() const { 
        return config.port; 
    }

    // IWebUIProvider implementation for self-registration
    String getWebUIName() const override { return "WebUI"; }
    String getWebUIVersion() const override { return "2.0.0"; }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        
        // WebSocket connection status badge (provider-specific styling)
        contexts.push_back(DomoticsCore::Components::WebUI::BaseWebUIComponents::createStatusBadge("websocket_status", "Connection", "dc-plug")
            .withField(WebUIField("state", "State", WebUIFieldType::Status, (webSocket && webSocket->count() > 0) ? "ON" : "OFF"))
            .withRealTime(2000)
            .withCustomCss(R"(
                .status-indicator[data-context-id='websocket_status'] .status-icon { color: var(--text-secondary); }
                .status-indicator[data-context-id='websocket_status'].active .status-icon { color: #28a745; filter: drop-shadow(0 0 6px rgba(40,167,69,0.6)); }
            )"));
        
        // Settings context
        contexts.push_back(WebUIContext::settings("webui_settings", "Web Interface")
            .withField(WebUIField("device_name", "Device Name", WebUIFieldType::Text, config.deviceName))
            .withField(WebUIField("theme", "Theme", WebUIFieldType::Select, config.theme, "dark,light,auto"))
        );
        return contexts;
    }

    String getWebUIData(const String& contextId) override { 
        if (contextId == "websocket_status") {
            JsonDocument doc;
            doc["state"] = (webSocket && webSocket->count() > 0) ? "ON" : "OFF";
            String json;
            serializeJson(doc, json);
            return json;
        }
        return "{}"; 
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params) override {
        if (contextId == "webui_settings" && method == "POST") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");
            if (fieldIt != params.end() && valueIt != params.end()) {
                if (fieldIt->second == "device_name") {
                    config.deviceName = valueIt->second;
                    return "{\"success\":true}";
                }
            }
        }
        return "{\"success\":false, \"error\":\"Invalid request\"}";
    }

private:
    void setupRoutes() {
        // Serve main HTML page
        server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }
            
            if (config.useFileSystem) {
                serveFromFileSystem(request, "/webui/index.html", "text/html");
            } else {
                auto* resp = request->beginResponse(200, "text/html",
                    WebUIContent::htmlGz(), WebUIContent::htmlGzLen());
                resp->addHeader("Content-Encoding", "gzip");
                resp->addHeader("Cache-Control", "public, max-age=3600");
                request->send(resp);
            }
        });
        
        // Serve CSS
        server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.useFileSystem) {
                serveFromFileSystem(request, "/webui/style.css", "text/css");
            } else {
                auto* resp = request->beginResponse(200, "text/css",
                    WebUIContent::cssGz(), WebUIContent::cssGzLen());
                resp->addHeader("Content-Encoding", "gzip");
                resp->addHeader("Cache-Control", "public, max-age=86400");
                request->send(resp);
            }
        });
        
        // Serve JavaScript
        server->on("/app.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.useFileSystem) {
                serveFromFileSystem(request, "/webui/app.js", "application/javascript");
            } else {
                auto* resp = request->beginResponse(200, "application/javascript",
                    WebUIContent::jsGz(), WebUIContent::jsGzLen());
                resp->addHeader("Content-Encoding", "gzip");
                resp->addHeader("Cache-Control", "public, max-age=86400");
                request->send(resp);
            }
        });
        
        // System info API - minimal JSON
        server->on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
            String sysInfo = "{\"uptime\":" + String(millis()) + 
                           ",\"heap\":" + String(ESP.getFreeHeap()) + 
                           ",\"clients\":" + String(getWebSocketClients()) + "}";
            request->send(200, "application/json", sysInfo);
        });
        
        // New UI Schema API endpoint
        server->on("/api/components", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }
            
            AsyncResponseStream *response = request->beginResponseStream("application/json");
            JsonDocument doc;
            JsonArray comps = doc["components"].to<JsonArray>();

            std::vector<IWebUIProvider*> uniqueProviders;
            for (const auto& pair : contextProviders) {
                if (std::find(uniqueProviders.begin(), uniqueProviders.end(), pair.second) == uniqueProviders.end()) {
                    uniqueProviders.push_back(pair.second);
                }
            }

            for (IWebUIProvider* provider : uniqueProviders) {
                JsonObject compObj = comps.add<JsonObject>();
                compObj["name"] = provider->getWebUIName();
                compObj["version"] = provider->getWebUIVersion();
                compObj["status"] = "Active"; // Placeholder
            }
            
            serializeJson(doc, *response);
            request->send(response);
        });

        server->on("/api/ui/schema", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }
            
            AsyncResponseStream *response = request->beginResponseStream("application/json");
            JsonDocument doc;
            JsonArray schema = doc.to<JsonArray>();

            std::vector<IWebUIProvider*> uniqueProviders;
            for (const auto& pair : contextProviders) {
                if (std::find(uniqueProviders.begin(), uniqueProviders.end(), pair.second) == uniqueProviders.end()) {
                    uniqueProviders.push_back(pair.second);
                }
            }

            for (IWebUIProvider* provider : uniqueProviders) {
                if (provider && provider->isWebUIEnabled()) {
                    auto contexts = provider->getWebUIContexts();
                    for (const auto& context : contexts) {
                        JsonObject contextObj = schema.add<JsonObject>();
                        serializeContext(contextObj, context);
                    }
                }
            }
            
            serializeJson(doc, *response);
            request->send(response);
        });
        
        // Favicon handler
        server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(204);
        });
        
        // Static file serving from filesystem
        if (config.useFileSystem) {
            initializeFileSystem();
            if (LittleFS.exists("/webui")) {
                server->serveStatic("/", LittleFS, "/webui/");
            } else if (SPIFFS.exists("/webui")) {
                server->serveStatic("/", SPIFFS, "/webui/");
            }
        }
        
        // Fallback for SPA routing
        server->onNotFound([this](AsyncWebServerRequest* request) {
            if (request->url().startsWith("/api/")) {
                request->send(404, "application/json", "{\"error\":\"API endpoint not found\"}");
            } else {
                if (config.useFileSystem) {
                    serveFromFileSystem(request, "/webui/index.html", "text/html");
                } else {
                    auto* resp = request->beginResponse(200, "text/html",
                        WebUIContent::htmlGz(), WebUIContent::htmlGzLen());
                    resp->addHeader("Content-Encoding", "gzip");
                    resp->addHeader("Cache-Control", "public, max-age=3600");
                    request->send(resp);
                }
            }
        });
    }
    
    bool authenticate(AsyncWebServerRequest* request) {
        if (!config.enableAuth) return true;
        return request->authenticate(config.username.c_str(), config.password.c_str());
    }
    
    bool initializeFileSystem() {
        if (LittleFS.begin()) {
            DLOG_I(LOG_CORE, "LittleFS initialized for WebUI static files");
            return true;
        } else if (SPIFFS.begin()) {
            DLOG_I(LOG_CORE, "SPIFFS initialized for WebUI static files");
            return true;
        }
        DLOG_W(LOG_CORE, "File system initialization failed");
        return false;
    }
    
    void serveFromFileSystem(AsyncWebServerRequest* request, const String& path, const String& contentType) {
        if (LittleFS.exists(path)) {
            request->send(LittleFS, path, contentType);
        } else if (SPIFFS.exists(path)) {
            request->send(SPIFFS, path, contentType);
        } else {
            request->send(404, "text/plain", "File not found");
        }
    }
    
    void handleWebSocketEvent(AsyncWebSocketClient* client, AwsEventType type, 
                             void* arg, uint8_t* data, size_t len) {
        if (!client) return;
        
        switch (type) {
            case WS_EVT_CONNECT:
                DLOG_I(LOG_CORE, "[WebUI] WebSocket client connected: %u", client->id());
                break;
                
            case WS_EVT_DISCONNECT:
                DLOG_I(LOG_CORE, "[WebUI] WebSocket client disconnected: %u", client->id());
                break;
                
            case WS_EVT_DATA: {
                if (!arg || !data || len == 0 || len > 512) break;
                
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && 
                    info->opcode == WS_TEXT && len < 256) {
                    
                    String message = String((char*)data, len);
                    handleWebSocketMessage(client, message);
                }
                break;
            }
            
            case WS_EVT_ERROR:
                break;
        }
    }
    
    void handleWebSocketMessage(AsyncWebSocketClient* client, const String& message) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);

        if (error) {
            DLOG_W(LOG_CORE, "[WebUI] Failed to parse WebSocket message: %s", message.c_str());
            return;
        }

        const char* type = doc["type"];
        if (type && strcmp(type, "ui_action") == 0) {
            String contextId = doc["contextId"].as<String>();
            String field = doc["field"].as<String>();
            JsonVariant value = doc["value"];

            auto it = contextProviders.find(contextId);
            if (it != contextProviders.end()) {
                IWebUIProvider* provider = it->second;
                std::map<String, String> params;
                params["field"] = field;
                params["value"] = value.as<String>();

                // Call the provider's handler
                provider->handleWebUIRequest(contextId, "/", "POST", params);
            } else {
                 DLOG_W(LOG_CORE, "[WebUI] No provider found for contextId: %s", contextId.c_str());
            }
        }
    }
    
    void sendWebSocketUpdates() {
        if (!webSocket || webSocket->count() == 0) return;
        
        // Build JSON manually to avoid ArduinoJson recursion issues
        String message = "{\"system\":{";
        message += "\"uptime\":" + String(millis()) + ",";
        message += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
        message += "\"clients\":" + String(webSocket->count()) + ",";
        message += "\"device_name\":\"" + config.deviceName + "\",";
        message += "\"manufacturer\":\"" + config.manufacturer + "\",";
        message += "\"version\":\"" + config.version + "\"";
        message += "},\"contexts\":{";
        
        int contextCount = 0;
        for (const auto& pair : contextProviders) {
            // Smart limit: Stop if message gets too large (prevent ESP32 memory issues)
            if (message.length() > 800) {
                log_w("WebSocket message size limit reached (%d bytes), skipping remaining contexts", message.length());
                break;
            }
            
            const String& contextId = pair.first;
            IWebUIProvider* provider = pair.second;
            
            String contextData = provider->getWebUIData(contextId);
            if (!contextData.isEmpty() && contextData != "{}") {
                if (contextCount > 0) message += ",";
                message += "\"" + contextId + "\":" + contextData;
                contextCount++;
            }
        }
        
        message += "}}";
        
        // Safety check
        if (message.length() < 1024) {
            webSocket->textAll(message);
        }
    }
    
    void sendWebSocketUpdate(AsyncWebSocketClient* client) {
        if (!client || client->status() != WS_CONNECTED) return;
        
        // Build JSON manually to avoid recursion
        String message = "{\"system\":{";
        message += "\"uptime\":" + String(millis()) + ",";
        message += "\"heap\":" + String(ESP.getFreeHeap());
        message += "},\"contexts\":{";
        
        int contextCount = 0;
        for (const auto& pair : contextProviders) {
            // Smart limit: Stop if message gets too large (prevent ESP32 memory issues)
            if (message.length() > 512) {
                log_w("Single client message size limit reached (%d bytes), skipping remaining contexts", message.length());
                break;
            }
            
            const String& contextId = pair.first;
            IWebUIProvider* provider = pair.second;
            
            String contextData = provider->getWebUIData(contextId);
            if (!contextData.isEmpty() && contextData != "{}") {
                if (contextCount > 0) message += ",";
                message += "\"" + contextId + "\":" + contextData;
                contextCount++;
            }
        }
        
        message += "}}";
        
        // Safety check
        if (message.length() < 512) {
            client->text(message);
        }
    }
    
    // Helper function to serialize a WebUIContext to a JsonObject
    void serializeContext(JsonObject& obj, const WebUIContext& context) {
        obj["contextId"] = context.contextId;
        obj["title"] = context.title;
        obj["icon"] = context.icon;
        obj["location"] = (int)context.location;
        obj["presentation"] = (int)context.presentation;
        obj["priority"] = context.priority;
        obj["apiEndpoint"] = context.apiEndpoint;
        
        // Include custom UI elements if provided
        if (!context.customHtml.isEmpty()) {
            obj["customHtml"] = context.customHtml;
        }
        if (!context.customCss.isEmpty()) {
            obj["customCss"] = context.customCss;
        }
        if (!context.customJs.isEmpty()) {
            obj["customJs"] = context.customJs;
        }

        JsonArray fields = obj["fields"].to<JsonArray>();
        for (const auto& field : context.fields) {
            JsonObject fieldObj = fields.add<JsonObject>();
            fieldObj["name"] = field.name;
            fieldObj["label"] = field.label;
            fieldObj["type"] = (int)field.type;
            fieldObj["value"] = field.value;
            fieldObj["unit"] = field.unit;
            fieldObj["readOnly"] = field.readOnly;
            fieldObj["minValue"] = field.minValue;
            fieldObj["maxValue"] = field.maxValue;
            fieldObj["endpoint"] = field.endpoint;
            if (!field.options.empty()) {
                JsonArray options = fieldObj["options"].to<JsonArray>();
                for (const auto& opt : field.options) {
                    options.add(opt);
                }
            }
        }
    }
};

} // namespace Components
} // namespace DomoticsCore
