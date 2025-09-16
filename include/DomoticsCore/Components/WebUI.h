#pragma once

// Enable WebUI features when this component is included
#define DOMOTICSCORE_WEBUI_ENABLED 1

#include "IComponent.h"
#include "IWebUIProvider.h"
#include "WebUIContent.h"
#include "../Utils/Timer.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include <vector>
#include <map>
#include "INetworkProvider.h"
#include "WebUIHelpers.h"
#include "../Logger.h"

namespace DomoticsCore {
namespace Components {

// WebUI configuration
struct WebUIConfig {
    uint16_t port = 80;
    String deviceName = "DomoticsCore Device";
    String manufacturer = "DomoticsCore";
    String version = "1.0.0";
    String copyright = " 2024 DomoticsCore";
    bool enableCORS = false;
    bool enableWebSocket = true;
    String staticPath = "/webui";
    bool useFileSystem = true;
    int wsUpdateInterval = 2000; // WebSocket update interval in ms
};

/**
 * WebUI management component providing web interface and REST API
 * Serves responsive web interface with real-time updates via WebSocket
 */
class WebUIComponent : public IComponent {
private:
    WebUIConfig config;
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    Utils::NonBlockingDelay statusTimer;
    Utils::NonBlockingDelay wsUpdateTimer;
    std::vector<IWebUIProvider*> webUIProviders;
    std::vector<WebUISection> registeredSections;
    bool serverStarted;
    INetworkProvider* networkProvider;

public:
    /**
     * Constructor
     * @param config WebUI configuration
     */
    WebUIComponent(const WebUIConfig& config, INetworkProvider* netProvider = nullptr) : 
        config(config), 
        server(nullptr),
        ws(nullptr),
        statusTimer(30000), // 30 second status updates
        wsUpdateTimer(config.wsUpdateInterval),
        serverStarted(false),
        networkProvider(netProvider)
    {
        DLOG_I(LOG_CORE, "WebUI component created with port %d", config.port);
        
        // Initialize component metadata
        metadata.name = "WebUI";
        metadata.version = "1.0.0";
        metadata.author = "DomoticsCore";
        metadata.description = "Web-based user interface with real-time updates";
        metadata.category = "Interface";
        metadata.tags = {"web", "ui", "interface", "websocket"};
        
        // Configuration is already copied in initializer list
        
        setStatus(ComponentStatus::Success);
    }
    
    ComponentStatus begin() override {
        DLOG_I(LOG_CORE, "WebUI component initializing...");
        
        // Validate configuration
        auto validation = validateConfig();
        if (!validation.isValid()) {
            DLOG_E(LOG_CORE, "WebUI config validation failed: %s", validation.toString().c_str());
            setStatus(ComponentStatus::ConfigError);
            return ComponentStatus::ConfigError;
        }
        
        // Initialize file system for static files
        if (config.useFileSystem) {
            if (!initializeFileSystem()) {
                DLOG_W(LOG_CORE, "File system initialization failed, using embedded content");
                config.useFileSystem = false;
            }
        }
        
        // Initialize web server
        ComponentStatus status = initializeWebServer();
        if (status != ComponentStatus::Success) {
            setStatus(status);
            return status;
        }
        
        DLOG_I(LOG_CORE, "WebUI server started on port %d", config.port);
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    void loop() override {
        if (getLastStatus() != ComponentStatus::Success) return;
        
        // Clean up WebSocket connections
        if (ws) {
            ws->cleanupClients();
        }
        
        // Send WebSocket updates periodically to all connected clients
        if (wsUpdateTimer.isReady() && config.enableWebSocket && ws && ws->count() > 0) {
            sendWebSocketUpdate();
            wsUpdateTimer.reset();
        }
        
        // Periodic status reporting
        if (statusTimer.isReady()) {
            reportWebUIStatus();
        }
    }
    
    ComponentStatus shutdown() override {
        DLOG_I(LOG_CORE, "WebUI component shutting down...");
        
        if (server && serverStarted) {
            server->end();
            serverStarted = false;
        }
        
        if (ws) {
            ws->closeAll();
        }
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    String getName() const override {
        return metadata.name;
    }
    
    /**
     * Register a component as WebUI provider
     * @param provider Component implementing IWebUIProvider
     */
    void registerProvider(IWebUIProvider* provider) {
        DLOG_I(LOG_CORE, "registerProvider called with provider: %p", provider);
        if (provider) {
            DLOG_I(LOG_CORE, "Provider is valid, checking if WebUI enabled...");
            if (provider->isWebUIEnabled()) {
                webUIProviders.push_back(provider);
                WebUISection section = provider->getWebUISection();
                DLOG_I(LOG_CORE, "Registered WebUI provider: %s (%s)", section.title.c_str(), section.category.c_str());
            } else {
                DLOG_W(LOG_CORE, "Provider is not WebUI enabled");
            }
        } else {
            DLOG_E(LOG_CORE, "Provider is null!");
        }
    }
    
    /**
     * Get server port
     */
    uint16_t getPort() const { return config.port; }
    
    /**
     * Get number of WebSocket clients
     */
    size_t getWebSocketClients() const {
        return ws ? ws->count() : 0;
    }

private:
    bool initializeFileSystem() {
        // Try LittleFS first, then SPIFFS
        if (LittleFS.begin()) {
            DLOG_I(LOG_CORE, "LittleFS initialized for WebUI static files");
            return true;
        } else if (SPIFFS.begin()) {
            DLOG_I(LOG_CORE, "SPIFFS initialized for WebUI static files");
            return true;
        }
        return false;
    }
    
    ComponentStatus initializeWebServer() {
        // Create web server
        server = new AsyncWebServer(config.port);
        
        if (config.enableWebSocket) {
            // Create WebSocket
            ws = new AsyncWebSocket("/ws");
            ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                              AwsEventType type, void* arg, uint8_t* data, size_t len) {
                handleWebSocketEvent(server, client, type, arg, data, len);
            });
            server->addHandler(ws);
        }
        
        // Setup routes
        setupStaticRoutes();
        setupAPIRoutes();
        
        // Enable CORS if configured
        if (config.enableCORS) {
            DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
            DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
        }
        
        // Start server
        server->begin();
        serverStarted = true;
        
        return ComponentStatus::Success;
    }
    
    void setupStaticRoutes() {
        // Root redirect
        server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.useFileSystem) {
                if (LittleFS.exists("/webui/index.html")) {
                    request->send(LittleFS, "/webui/index.html", "text/html");
                } else {
                    request->send(SPIFFS, "/webui/index.html", "text/html");
                }
            } else {
                request->send(200, "text/html", getEmbeddedHTML());
            }
        });
        
        // Static file serving
        if (config.useFileSystem) {
            if (LittleFS.exists("/webui")) {
                server->serveStatic("/", LittleFS, "/webui/");
            } else {
                server->serveStatic("/", SPIFFS, "/webui/");
            }
        }
        
        // Embedded CSS and JS
        server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
            request->send(200, "text/css", getEmbeddedCSS());
        });
        
        server->on("/app.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
            request->send(200, "application/javascript", getEmbeddedJS());
        });
    }
    
    void setupAPIRoutes() {
        // System information
        server->on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
            request->send(200, "application/json", getSystemInfo());
        });
        
        // Component list
        server->on("/api/components", HTTP_GET, [this](AsyncWebServerRequest* request) {
            request->send(200, "application/json", getComponentsList());
        });
        
        // WebUI sections
        server->on("/api/webui/sections", HTTP_GET, [this](AsyncWebServerRequest* request) {
            request->send(200, "application/json", getWebUISections());
        });
        
        // Dynamic component API routes
        server->onNotFound([this](AsyncWebServerRequest* request) {
            handleDynamicAPI(request);
        });
    }
    
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                            AwsEventType type, void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                DLOG_I(LOG_CORE, "WebSocket client connected: %u", client->id());
                // Send initial data
                sendWebSocketUpdate(client);
                break;
                
            case WS_EVT_DISCONNECT:
                DLOG_I(LOG_CORE, "WebSocket client disconnected: %u", client->id());
                break;
                
            case WS_EVT_DATA: {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len) {
                    if (info->opcode == WS_TEXT) {
                        String message = String((char*)data, len);
                        handleWebSocketMessage(client, message);
                    }
                }
                break;
            }
            
            default:
                break;
        }
    }
    
    void handleWebSocketMessage(AsyncWebSocketClient* client, const String& message) {
        // Parse WebSocket commands (e.g., {"action": "refresh", "component": "led"})
        JsonDocument doc;
        if (deserializeJson(doc, message) == DeserializationError::Ok) {
            String action = doc["action"];
            String component = doc["component"];
            
            if (action == "refresh") {
                sendWebSocketUpdate(client, component);
            }
        }
    }
    
    void sendWebSocketUpdate(AsyncWebSocketClient* client = nullptr, const String& componentFilter = "") {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["timestamp"] = millis();
        
        // Create system info directly
        JsonObject system = root["system"].to<JsonObject>();
        system["device_name"] = config.deviceName;
        system["manufacturer"] = config.manufacturer;
        system["version"] = config.version;
        system["copyright"] = config.copyright;
        system["uptime"] = millis() / 1000;
        system["free_heap"] = ESP.getFreeHeap();
        system["chip_model"] = ESP.getChipModel();
        system["chip_revision"] = ESP.getChipRevision();
        system["flash_size"] = ESP.getFlashChipSize();
        system["ws_clients"] = getWebSocketClients();
        
        // Network status information (network-agnostic)
        if (networkProvider) {
            system["network_connected"] = networkProvider->isConnected();
            system["network_type"] = networkProvider->getNetworkType();
            system["network_ip"] = networkProvider->getLocalIP();
            system["network_status"] = networkProvider->getConnectionStatus();
            
            // Parse network-specific info from JSON
            JsonDocument networkInfo;
            deserializeJson(networkInfo, networkProvider->getNetworkInfo());
            system["network_info"] = networkInfo;
        } else {
            system["network_connected"] = false;
            system["network_type"] = "None";
            system["network_status"] = "No network provider";
        }
        
        // Add component data
        JsonObject components = root["components"].to<JsonObject>();
        DLOG_I(LOG_CORE, "WebSocket update: Found %d WebUI providers", webUIProviders.size());
        for (auto* provider : webUIProviders) {
            if (provider->isWebUIEnabled()) {
                WebUISection section = provider->getWebUISection();
                DLOG_I(LOG_CORE, "Processing provider: %s (category: %s)", section.id.c_str(), section.category.c_str());
                if (componentFilter.isEmpty() || section.id == componentFilter) {
                    String data = provider->getWebUIData();
                    DLOG_I(LOG_CORE, "Provider %s data: %s", section.id.c_str(), data.c_str());
                    if (!data.isEmpty() && data != "{}") {
                        JsonDocument componentDoc;
                        if (deserializeJson(componentDoc, data) == DeserializationError::Ok) {
                            components[section.id] = componentDoc;
                            DLOG_I(LOG_CORE, "Added component data for: %s", section.id.c_str());
                        } else {
                            DLOG_E(LOG_CORE, "Failed to parse JSON for provider: %s", section.id.c_str());
                        }
                    } else {
                        DLOG_W(LOG_CORE, "Provider %s returned empty data", section.id.c_str());
                    }
                }
            } else {
                DLOG_W(LOG_CORE, "Provider is not WebUI enabled");
            }
        }
        
        String response;
        serializeJson(doc, response);
        
        if (client) {
            client->text(response);
        } else {
            ws->textAll(response);
        }
    }
    
    void handleDynamicAPI(AsyncWebServerRequest* request) {
        String path = request->url();
        
        // Check if it's an API request
        if (!path.startsWith("/api/")) {
            request->send(404, "text/plain", "Not Found");
            return;
        }
        
        // Parse API path: /api/{component}/{endpoint}
        int firstSlash = path.indexOf('/', 5); // After "/api/"
        if (firstSlash == -1) {
            request->send(400, "text/plain", "Invalid API path");
            return;
        }
        
        String componentId = path.substring(5, firstSlash);
        String endpoint = path.substring(firstSlash);
        
        // Find matching provider
        for (auto* provider : webUIProviders) {
            WebUISection section = provider->getWebUISection();
            if (section.id == componentId) {
                // Collect request parameters
                std::map<String, String> params;
                for (int i = 0; i < request->params(); i++) {
                    const AsyncWebParameter* p = request->getParam(i);
                    params[p->name()] = p->value();
                }
                
                String method = (request->method() == HTTP_GET) ? "GET" :
                               (request->method() == HTTP_POST) ? "POST" :
                               (request->method() == HTTP_PUT) ? "PUT" :
                               (request->method() == HTTP_DELETE) ? "DELETE" : "UNKNOWN";
                
                String response = provider->handleWebUIRequest(endpoint, method, params);
                request->send(200, "application/json", response);
                return;
            }
        }
        
        request->send(404, "application/json", "{\"error\":\"Component not found\"}");
    }
    
    String getSystemInfo() {
        JsonDocument doc;
        JsonObject system = getSystemInfoJson(doc);
        
        String response;
        serializeJson(doc, response);
        return response;
    }
    
    JsonObject getSystemInfoJson(JsonDocument& doc) {
        JsonObject system = doc.to<JsonObject>();
        system["device_name"] = config.deviceName;
        system["manufacturer"] = config.manufacturer;
        system["version"] = config.version;
        system["copyright"] = config.copyright;
        system["uptime"] = millis() / 1000;
        system["free_heap"] = ESP.getFreeHeap();
        system["chip_model"] = ESP.getChipModel();
        system["chip_revision"] = ESP.getChipRevision();
        system["flash_size"] = ESP.getFlashChipSize();
        system["ws_clients"] = getWebSocketClients();
        
        // Network status information (network-agnostic)
        if (networkProvider) {
            system["network_connected"] = networkProvider->isConnected();
            system["network_type"] = networkProvider->getNetworkType();
            system["network_ip"] = networkProvider->getLocalIP();
            system["network_status"] = networkProvider->getConnectionStatus();
            
            // Parse network-specific info from JSON
            JsonDocument networkInfo;
            deserializeJson(networkInfo, networkProvider->getNetworkInfo());
            system["network_info"] = networkInfo;
        } else {
            system["network_connected"] = false;
            system["network_type"] = "None";
            system["network_status"] = "No network provider";
        }
        
        return system;
    }
    
    String getComponentsList() {
        JsonDocument doc;
        JsonArray components = doc["components"].to<JsonArray>();
        
        for (auto* provider : webUIProviders) {
            if (provider->isWebUIEnabled()) {
                WebUISection section = provider->getWebUISection();
                JsonObject comp = components.add<JsonObject>();
                comp["id"] = section.id;
                comp["title"] = section.title;
                comp["icon"] = section.icon;
                comp["enabled"] = true;
            }
        }
        
        String response;
        serializeJson(doc, response);
        return response;
    }
    
    String getWebUISections() {
        JsonDocument doc;
        JsonArray sections = doc.to<JsonArray>();
        
        for (auto* provider : webUIProviders) {
            if (provider->isWebUIEnabled()) {
                WebUISection section = provider->getWebUISection();
                JsonObject sectionObj = sections.add<JsonObject>();
                sectionObj["id"] = section.id;
                sectionObj["title"] = section.title;
                sectionObj["icon"] = section.icon;
                sectionObj["category"] = section.category;
                sectionObj["enabled"] = true;
                sectionObj["realTime"] = section.realTime;
                sectionObj["updateInterval"] = section.updateInterval;
                
                // Add field definitions
                JsonArray fields = sectionObj["fields"].to<JsonArray>();
                for (const auto& field : section.fields) {
                    JsonObject fieldObj = fields.add<JsonObject>();
                    fieldObj["name"] = field.name;
                    fieldObj["label"] = field.label;
                    fieldObj["type"] = static_cast<int>(field.type);
                    fieldObj["value"] = field.value;
                    fieldObj["unit"] = field.unit;
                    fieldObj["readOnly"] = field.readOnly;
                    fieldObj["minValue"] = field.minValue;
                    fieldObj["maxValue"] = field.maxValue;
                    fieldObj["endpoint"] = field.endpoint;
                    
                    if (!field.options.empty()) {
                        JsonArray options = fieldObj["options"].to<JsonArray>();
                        for (const auto& option : field.options) {
                            options.add(option);
                        }
                    }
                }
            }
        }
        
        String response;
        serializeJson(doc, response);
        return response;
    }
    
    void reportWebUIStatus() {
        DLOG_I(LOG_CORE, "=== WebUI Status ===");
        DLOG_I(LOG_CORE, "Server: %s (port %d)", serverStarted ? "Running" : "Stopped", config.port);
        DLOG_I(LOG_CORE, "WebSocket clients: %d", getWebSocketClients());
        DLOG_I(LOG_CORE, "Registered providers: %d", webUIProviders.size());
        DLOG_I(LOG_CORE, "File system: %s", config.useFileSystem ? "Enabled" : "Disabled");
    }
    
    ValidationResult validateConfig() const {
        // Validate port
        if (config.port == 0 || config.port > 65535) {
            return ValidationResult(ComponentStatus::ConfigError, "Invalid port number", "port");
        }
        
        // Validate device name
        if (config.deviceName.isEmpty()) {
            return ValidationResult(ComponentStatus::ConfigError, "Device name cannot be empty", "deviceName");
        }
        
        // Validate WebSocket update interval
        if (config.wsUpdateInterval < 500 || config.wsUpdateInterval > 30000) {
            return ValidationResult(ComponentStatus::ConfigError, "WebSocket update interval must be between 500-30000ms", "wsUpdateInterval");
        }
        
        return ValidationResult(ComponentStatus::Success);
    }
    
    String getEmbeddedHTML() {
        return WebUIContent::getHTML();
    }
    
    String getEmbeddedCSS() {
        return WebUIContent::getCSS();
    }
    
    String getEmbeddedJS() {
        return WebUIContent::getJS();
    }
};

} // namespace Components
} // namespace DomoticsCore
