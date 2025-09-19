#pragma once

// Enable WebUI features when this component is included
#define DOMOTICSCORE_WEBUI_ENABLED 1

#include <Arduino.h>
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
class WebUIComponent : public IComponent {
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
    void registerProvider(const String& contextId, IWebUIProvider* provider) {
        if (provider) {
            contextProviders[contextId] = provider;
            DLOG_I(LOG_CORE, "Registered WebUI provider: %s", contextId.c_str());
        }
    }

    void unregisterProvider(const String& contextId) {
        contextProviders.erase(contextId);
    }

    int getWebSocketClients() const {
        return webSocket ? webSocket->count() : 0;
    }

    uint16_t getPort() const { 
        return config.port; 
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
                    (const uint8_t*)WebUIContent::htmlP(), strlen_P(WebUIContent::htmlP()));
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
                    (const uint8_t*)WebUIContent::cssP(), strlen_P(WebUIContent::cssP()));
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
                    (const uint8_t*)WebUIContent::jsP(), strlen_P(WebUIContent::jsP()));
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
        
        // Contexts API
        server->on("/api/contexts", HTTP_GET, [this](AsyncWebServerRequest* request) {
            String location = request->getParam("location") ? 
                             request->getParam("location")->value() : "";
            request->send(200, "application/json", getContextsJSON(location));
        });
        
        // Context data API
        server->on("/api/context", HTTP_GET, [this](AsyncWebServerRequest* request) {
            String contextId = request->getParam("id") ? 
                              request->getParam("id")->value() : "";
            
            auto it = contextProviders.find(contextId);
            if (it != contextProviders.end()) {
                String data = it->second->getWebUIData(contextId);
                request->send(200, "application/json", data);
            } else {
                request->send(404, "application/json", "{\"error\":\"Context not found\"}");
            }
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
                        (const uint8_t*)WebUIContent::htmlP(), strlen_P(WebUIContent::htmlP()));
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
        // Parse simple JSON commands manually to avoid ArduinoJson
        if (message.indexOf("\"action\":\"refresh\"") >= 0) {
            sendWebSocketUpdate(client);
        } else if (message.indexOf("\"action\":\"refreshAll\"") >= 0) {
            sendWebSocketUpdate(client);
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
            if (contextCount >= 3) break; // Strict limit
            
            const String& contextId = pair.first;
            IWebUIProvider* provider = pair.second;
            
            String contextData = provider->getWebUIData(contextId);
            if (!contextData.isEmpty() && contextData != "{}" && contextData.length() < 128) {
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
            if (contextCount >= 2) break; // Very strict limit for single client
            
            const String& contextId = pair.first;
            IWebUIProvider* provider = pair.second;
            
            String contextData = provider->getWebUIData(contextId);
            if (!contextData.isEmpty() && contextData != "{}" && contextData.length() < 64) {
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
    
    String getContextsJSON(const String& location) {
        String json = "{\"contexts\":[";
        bool first = true;
        
        for (const auto& pair : contextProviders) {
            if (!first) json += ",";
            first = false;
            
            const String& contextId = pair.first;
            IWebUIProvider* provider = pair.second;
            
            WebUIContext context = provider->getWebUIContext(contextId);
            
            json += "{";
            json += "\"id\":\"" + contextId + "\",";
            json += "\"name\":\"" + context.title + "\",";
            json += "\"location\":\"" + String((int)context.location) + "\",";
            json += "\"presentation\":\"" + String((int)context.presentation) + "\"";
            json += "}";
        }
        
        json += "]}";
        return json;
    }
};

} // namespace Components
} // namespace DomoticsCore
