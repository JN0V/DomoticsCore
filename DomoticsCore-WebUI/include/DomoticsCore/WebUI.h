#pragma once

/**
 * @file WebUI.h
 * @brief Declares the DomoticsCore WebUI component and supporting types for dashboard integration.
 */

#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/ComponentRegistry.h"
#include "DomoticsCore/BaseWebUIComponents.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <pgmspace.h>
#include <functional>

#include "DomoticsCore/IComponent.h"
#include "DomoticsCore/Logger.h"
#include "DomoticsCore/Generated/WebUIAssets.h"

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
 * @class DomoticsCore::Components::WebUIComponent
 * @brief Async web server + WebSocket frontend that aggregates `IWebUIProvider` contexts.
 *
 * Serves embedded HTML/CSS/JS assets, registers component providers, and pushes real-time updates
 * to connected clients. Acts as both a component and a provider to expose global WebUI settings.
 */
class WebUIComponent : public IComponent, public virtual IWebUIProvider, public Components::ComponentRegistry::IComponentLifecycleListener {
private:
    WebUIConfig config;
    AsyncWebServer* server = nullptr;
    AsyncWebSocket* webSocket = nullptr;
    
    // Provider support
    std::map<String, IWebUIProvider*> contextProviders;
    // Enable/disable state per provider (keyed by provider ptr)
    std::map<IWebUIProvider*, bool> providerEnabled;
    // Optional mapping to owning component to allow begin()/shutdown()
    std::map<IWebUIProvider*, IComponent*> providerComponent;
    // Provider factory registry (typeKey -> factory)
    std::map<String, std::function<IWebUIProvider*(IComponent*)>> providerFactories;
    // Owned providers created via factories (to manage lifetime)
    std::vector<std::unique_ptr<IWebUIProvider>> ownedProviders;
    
    unsigned long lastWebSocketUpdate = 0;

public:
    /**
     * @brief Construct a WebUI component with the provided configuration.
     */
    WebUIComponent(const WebUIConfig& cfg = WebUIConfig()) 
        : config(cfg) {}

    /**
     * @brief Release the AsyncWebServer and WebSocket instances.
     */
    ~WebUIComponent() {
        if (webSocket) {
            delete webSocket;
        }
        if (server) {
            delete server;
        }
    }

    // IComponent interface
    /**
     * @brief Initialize the AsyncWebServer, register websocket handler, and configure routes.
     */
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

    /**
     * @brief Pump periodic websocket updates and clean up disconnected clients.
     */
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

    /**
     * @brief Stop the web server but keep configuration for potential restart.
     */
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
    /**
     * @brief Register an `IWebUIProvider` and index all of its contexts.
     */
    void registerProvider(IWebUIProvider* provider) {
        if (!provider) return;

        auto contexts = provider->getWebUIContexts();
        if (contexts.empty()) {
            DLOG_W(LOG_WEB, "Provider has no contexts to register.");
            return;
        }

        for (const auto& context : contexts) {
            contextProviders[context.contextId] = provider;
            DLOG_I(LOG_WEB, "Registered provider for context: %s", context.contextId.c_str());
        }
        // Default to enabled if not already tracked
        if (providerEnabled.find(provider) == providerEnabled.end()) {
            providerEnabled[provider] = true;
        }
    }

    // Overload to explicitly associate a provider with its component for lifecycle control
    /**
     * @brief Register a provider and remember the owning component for lifecycle callbacks.
     */
    void registerProviderWithComponent(IWebUIProvider* provider, IComponent* component) {
        registerProvider(provider);
        if (provider && component) {
            providerComponent[provider] = component;
        }
    }

    /**
     * @brief Remove all contexts contributed by the given provider without deleting it.
     */
    void unregisterProvider(IWebUIProvider* provider) {
        if (!provider) return;
        for (auto it = contextProviders.begin(); it != contextProviders.end(); ) {
            if (it->second == provider) {
                it = contextProviders.erase(it);
            } else {
                ++it;
            }
        }
        // Keep providerEnabled and providerComponent entries so we can re-enable later
    }

    /**
     * @brief Number of websocket clients currently connected.
     */
    int getWebSocketClients() const {
        return webSocket ? webSocket->count() : 0;
    }

    /**
     * @brief HTTP port used by the AsyncWebServer instance.
     */
    uint16_t getPort() const { 
        return config.port; 
    }

    // Allow applications to register factories for their own components (composition-based UI)
    /**
     * @brief Register a factory that can create providers for components with a matching type key.
     */
    void registerProviderFactory(const String& typeKey, std::function<IWebUIProvider*(IComponent*)> factory) {
        if (!typeKey.isEmpty() && factory) {
            providerFactories[typeKey] = factory;
        }
    }

    /**
     * @brief Allow external components to register custom REST API routes served by the WebUI server.
     * @param uri Request URI (e.g. "/api/custom")
     * @param method HTTP method to match
     * @param handler Callback invoked when the route is requested
     */
    void registerApiRoute(const String& uri, WebRequestMethod method, ArRequestHandlerFunction handler) {
        if (!server || uri.isEmpty() || !handler) {
            return;
        }
        server->on(uri.c_str(), method, handler);
    }

    /**
     * @brief Register an API route that expects file uploads (multipart/form-data).
     * @param uri Request URI (e.g. "/api/upload")
     * @param handler Callback executed after upload completes
     * @param uploadHandler Upload stream handler receiving file chunks
     */
    void registerApiUploadRoute(const String& uri, ArRequestHandlerFunction handler, ArUploadHandlerFunction uploadHandler) {
        if (!server || uri.isEmpty() || !handler || !uploadHandler) {
            return;
        }
        server->on(uri.c_str(), HTTP_POST, handler, uploadHandler);
    }

    // Auto-discovery: register providers for all components exposing a WebUI provider
    /**
     * @brief Iterate through the registry and register providers (direct or via factories).
     */
    void discoverProviders(const Components::ComponentRegistry& registry) {
        auto comps = registry.getAllComponents();
        for (auto* comp : comps) {
            if (!comp) continue;
            IWebUIProvider* provider = comp->getWebUIProvider();
            if (provider) {
                // Avoid duplicate registration
                bool already = false;
                for (const auto& pair : contextProviders) {
                    if (pair.second == provider) { already = true; break; }
                }
                if (!already) {
                    registerProviderWithComponent(provider, comp);
                }
            } else {
                // Try factory by typeKey for composition-based providers
                const char* key = comp->getTypeKey();
                auto it = providerFactories.find(String(key));
                if (it != providerFactories.end()) {
                    // Create and own the provider instance
                    std::unique_ptr<IWebUIProvider> created(it->second(comp));
                    if (created) {
                        IWebUIProvider* raw = created.get();
                        ownedProviders.push_back(std::move(created));
                        registerProviderWithComponent(raw, comp);
                    }
                }
            }
        }
    }

    // IComponent override: post-initialization hook
    /**
     * @brief Called once all components are ready; auto-discovers providers and subscribes to changes.
     */
    void onComponentsReady(const Components::ComponentRegistry& registry) override {
        discoverProviders(registry);
        // Subscribe to future add/remove events
        auto& reg = const_cast<Components::ComponentRegistry&>(registry);
        reg.addListener(this);
    }

    // IWebUIProvider implementation for self-registration
    String getWebUIName() const override { return "WebUI"; }
    String getWebUIVersion() const override { return "2.0.0"; }
    // Expose as provider for auto-discovery
    IWebUIProvider* getWebUIProvider() override { return this; }

    /**
     * @brief Define built-in contexts (status badge + settings) for the WebUI component itself.
     */
    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        
        // Provide default uptime header info item
        uint32_t seconds = millis() / 1000;
        uint32_t days = seconds / 86400;
        seconds %= 86400;
        uint32_t hours = seconds / 3600;
        seconds %= 3600;
        uint32_t minutes = seconds / 60;
        seconds %= 60;
        
        String uptimeStr;
        if (days > 0) uptimeStr += String(days) + "d ";
        if (hours > 0 || days > 0) uptimeStr += String(hours) + "h ";
        if (minutes > 0 || hours > 0 || days > 0) uptimeStr += String(minutes) + "m ";
        uptimeStr += String(seconds) + "s";
        
        contexts.push_back(WebUIContext::headerInfo("webui_uptime", "Uptime", "dc-info")
            .withField(WebUIField("uptime", "Uptime", WebUIFieldType::Display, uptimeStr, "", true))
            .withRealTime(1000)
            .withAPI("/api/webui/uptime"));
        
        // Settings context
        contexts.push_back(WebUIContext::settings("webui_settings", "Web Interface")
            .withField(WebUIField("device_name", "Device Name", WebUIFieldType::Text, config.deviceName))
            .withField(WebUIField("theme", "Theme", WebUIFieldType::Select, config.theme, "dark,light,auto"))
        );
        return contexts;
    }

    /**
     * @brief Provide real-time JSON data for the requested context identifier.
     */
    String getWebUIData(const String& contextId) override { 
        if (contextId == "webui_uptime") {
            JsonDocument doc;
            uint32_t seconds = millis() / 1000;
            uint32_t days = seconds / 86400;
            seconds %= 86400;
            uint32_t hours = seconds / 3600;
            seconds %= 3600;
            uint32_t minutes = seconds / 60;
            seconds %= 60;
            
            String uptimeStr;
            if (days > 0) uptimeStr += String(days) + "d ";
            if (hours > 0 || days > 0) uptimeStr += String(hours) + "h ";
            if (minutes > 0 || hours > 0 || days > 0) uptimeStr += String(minutes) + "m ";
            uptimeStr += String(seconds) + "s";
            
            doc["uptime"] = uptimeStr;
            String json;
            serializeJson(doc, json);
            return json;
        }
        
        if (contextId == "webui_settings") {
            JsonDocument doc;
            doc["device_name"] = config.deviceName;
            doc["theme"] = config.theme;
            String json; serializeJson(doc, json); return json;
        }
        return "{}"; 
    }

    // Registry listener events
    /**
     * @brief React to components being added by registering any exposed WebUI provider.
     */
    void onComponentAdded(IComponent* comp) override {
        if (!comp) return;
        IWebUIProvider* provider = comp->getWebUIProvider();
        if (provider) {
            registerProviderWithComponent(provider, comp);
        }
    }
    /**
     * @brief When a component is removed, detach and disable its provider contexts.
     */
    void onComponentRemoved(IComponent* comp) override {
        if (!comp) return;
        // Unregister any providers belonging to this component
        std::vector<IWebUIProvider*> toRemove;
        for (const auto& kv : providerComponent) {
            if (kv.second == comp) {
                toRemove.push_back(kv.first);
            }
        }
        for (auto* prov : toRemove) {
            // Remove all contexts owned by this provider
            for (auto it = contextProviders.begin(); it != contextProviders.end(); ) {
                if (it->second == prov) it = contextProviders.erase(it); else ++it;
            }
            providerEnabled.erase(prov);
            providerComponent.erase(prov);
        }
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
                    WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);
                resp->addHeader("Content-Encoding", "gzip");
                resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                request->send(resp);
            }
        });
        
        // Serve CSS
        server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.useFileSystem) {
                serveFromFileSystem(request, "/webui/style.css", "text/css");
            } else {
                auto* resp = request->beginResponse(200, "text/css",
                    WEBUI_CSS_GZ, WEBUI_CSS_GZ_LEN);
                resp->addHeader("Content-Encoding", "gzip");
                resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                request->send(resp);
            }
        });
        
        // Serve JavaScript
        server->on("/app.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.useFileSystem) {
                serveFromFileSystem(request, "/webui/app.js", "application/javascript");
            } else {
                auto* resp = request->beginResponse(200, "application/javascript",
                    WEBUI_JS_GZ, WEBUI_JS_GZ_LEN);
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
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", sysInfo);
            addCorsHeaders(response);
            request->send(response);
        });
        
        // New UI Schema API endpoint
        server->on("/api/components", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }
            
            AsyncResponseStream *response = request->beginResponseStream("application/json");
            addCorsHeaders(response);
            JsonDocument doc;
            JsonArray comps = doc["components"].to<JsonArray>();

            // Build a unique list from providerEnabled to include disabled providers as well
            std::vector<IWebUIProvider*> providers;
            providers.reserve(providerEnabled.size());
            for (const auto& kv : providerEnabled) {
                if (kv.first && std::find(providers.begin(), providers.end(), kv.first) == providers.end()) {
                    providers.push_back(kv.first);
                }
            }
            // Also include any provider currently present in contexts but missing from map (safety)
            for (const auto& pair : contextProviders) {
                IWebUIProvider* prov = pair.second;
                if (prov && std::find(providers.begin(), providers.end(), prov) == providers.end()) {
                    providers.push_back(prov);
                }
            }

            for (IWebUIProvider* provider : providers) {
                JsonObject compObj = comps.add<JsonObject>();
                compObj["name"] = provider->getWebUIName();
                compObj["version"] = provider->getWebUIVersion();
                bool enabled = (providerEnabled.find(provider) != providerEnabled.end()) ? providerEnabled[provider] : true;
                compObj["status"] = enabled ? "Enabled" : "Disabled";
                compObj["enabled"] = enabled;
                compObj["canDisable"] = (provider->getWebUIName() != "WebUI");
            }
            
            serializeJson(doc, *response);
            request->send(response);
        });

        // Enable/disable component
        server->on("/api/components/enable", HTTP_POST, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }

            AsyncResponseStream *response = request->beginResponseStream("application/json");
            addCorsHeaders(response);
            JsonDocument doc;
            String name;
            bool enabled = true;
            if (request->hasParam("name", true)) {
                name = request->getParam("name", true)->value();
            }
            if (request->hasParam("enabled", true)) {
                String v = request->getParam("enabled", true)->value();
                enabled = (v == "true" || v == "1" || v == "on");
            }

            bool found = false;
            // Collect matching providers first from current contexts and tracked providers
            std::vector<IWebUIProvider*> matched;
            for (const auto& kv : contextProviders) {
                if (kv.second && kv.second->getWebUIName() == name) {
                    if (std::find(matched.begin(), matched.end(), kv.second) == matched.end()) {
                        matched.push_back(kv.second);
                    }
                }
            }
            for (const auto& kv : providerEnabled) {
                IWebUIProvider* prov = kv.first;
                if (prov && prov->getWebUIName() == name) {
                    if (std::find(matched.begin(), matched.end(), prov) == matched.end()) {
                        matched.push_back(prov);
                    }
                }
            }

            for (IWebUIProvider* provider : matched) {
                // Disallow disabling WebUI to avoid losing access
                if (name == "WebUI" && enabled == false) {
                    doc["success"] = false;
                    doc["warning"] = "Disabling WebUI may make the UI inaccessible until reboot/reset.";
                    serializeJson(doc, *response);
                    request->send(response);
                    return;
                }

                providerEnabled[provider] = enabled;
                found = true;

                // If we know its owning component, call lifecycle methods
                auto itComp = providerComponent.find(provider);
                if (itComp != providerComponent.end() && itComp->second) {
                    if (!enabled) {
                        itComp->second->shutdown();
                    } else {
                        itComp->second->begin();
                    }
                }

                // Keep server-side context index in sync so UI can hide/show without reload
                if (!enabled) {
                    unregisterProvider(provider);
                } else {
                    registerProviderWithComponent(provider, (providerComponent.find(provider) != providerComponent.end()) ? providerComponent[provider] : nullptr);
                }
            }

            doc["success"] = found;
            doc["name"] = name;
            doc["enabled"] = enabled;
            if (name == "WebUI" && enabled == false) {
                doc["warning"] = "Disabling WebUI may make the UI inaccessible until reboot/reset.";
            }
            serializeJson(doc, *response);
            request->send(response);
            // Broadcast schema change so clients re-fetch /api/ui/schema
            if (found && webSocket && webSocket->count() > 0) {
                String msg = String("{\"type\":\"schema_changed\",\"name\":\"") + name + "\"}";
                if (msg.length() < 128) {
                    webSocket->textAll(msg);
                }
            }
        });

        server->on("/api/ui/schema", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }
            
            AsyncResponseStream *response = request->beginResponseStream("application/json");
            addCorsHeaders(response);
            JsonDocument doc;
            JsonArray schema = doc.to<JsonArray>();

            // Prefer provider list from providerEnabled, merge any active context providers
            std::vector<IWebUIProvider*> providers;
            for (const auto& kv : providerEnabled) {
                if (kv.first && std::find(providers.begin(), providers.end(), kv.first) == providers.end()) {
                    providers.push_back(kv.first);
                }
            }
            for (const auto& pair : contextProviders) {
                if (pair.second && std::find(providers.begin(), providers.end(), pair.second) == providers.end()) {
                    providers.push_back(pair.second);
                }
            }

            for (IWebUIProvider* provider : providers) {
                bool enabled = true;
                auto enIt = providerEnabled.find(provider);
                if (enIt != providerEnabled.end()) enabled = enIt->second;
                if (provider && provider->isWebUIEnabled() && enabled) {
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
            // Handle CORS preflight requests
            if (config.enableCORS && request->method() == HTTP_OPTIONS) {
                AsyncWebServerResponse* response = request->beginResponse(200);
                addCorsHeaders(response);
                request->send(response);
                return;
            }
            
            if (request->url().startsWith("/api/")) {
                AsyncWebServerResponse* response = request->beginResponse(404, "application/json", 
                    "{\"error\":\"API endpoint not found\"}");
                addCorsHeaders(response);
                request->send(response);
            } else {
                if (config.useFileSystem) {
                    serveFromFileSystem(request, "/webui/index.html", "text/html");
                } else {
                    auto* resp = request->beginResponse(200, "text/html",
                        WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);
                    resp->addHeader("Content-Encoding", "gzip");
                    resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                    request->send(resp);
                }
            }
        });
    }
    
    bool authenticate(AsyncWebServerRequest* request) {
        if (!config.enableAuth) return true;
        return request->authenticate(config.username.c_str(), config.password.c_str());
    }
    
    /**
     * @brief Add CORS headers to response if enabled in config
     */
    void addCorsHeaders(AsyncWebServerResponse* response) {
        if (config.enableCORS) {
            response->addHeader("Access-Control-Allow-Origin", "*");
            response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            response->addHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Key, Authorization");
        }
    }
    
    bool initializeFileSystem() {
        if (LittleFS.begin()) {
            DLOG_I(LOG_WEB, "LittleFS initialized for WebUI static files");
            return true;
        } else if (SPIFFS.begin()) {
            DLOG_I(LOG_WEB, "SPIFFS initialized for WebUI static files");
            return true;
        }
        DLOG_W(LOG_WEB, "File system initialization failed");
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
                DLOG_I(LOG_WEB, "WebSocket client connected: %u", client->id());
                break;
                
            case WS_EVT_DISCONNECT:
                DLOG_I(LOG_WEB, "WebSocket client disconnected: %u", client->id());
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
            DLOG_W(LOG_WEB, "Failed to parse WebSocket message: %s", message.c_str());
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
                 DLOG_W(LOG_WEB, "No provider found for contextId: %s", contextId.c_str());
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
            if (providerEnabled.find(provider) != providerEnabled.end() && providerEnabled[provider] == false) {
                continue;
            }
            
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
            if (providerEnabled.find(provider) != providerEnabled.end() && providerEnabled[provider] == false) {
                continue;
            }
            
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
