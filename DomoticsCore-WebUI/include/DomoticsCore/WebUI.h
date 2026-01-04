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
#include "DomoticsCore/Filesystem_HAL.h"
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <pgmspace.h>
#include <functional>

#include "DomoticsCore/IComponent.h"
#include "DomoticsCore/Logger.h"
#include "DomoticsCore/Platform_HAL.h"  // For HAL::getFreeHeap()
#include "DomoticsCore/WebUI_HAL.h"     // For WebUI buffer sizes
#include "DomoticsCore/Generated/WebUIAssets.h"

// New modular headers
#include "DomoticsCore/WebUI/WebUIConfig.h"
#include "DomoticsCore/WebUI/ProviderRegistry.h"
#include "DomoticsCore/WebUI/WebServerManager.h"
#include "DomoticsCore/WebUI/WebSocketHandler.h"

namespace DomoticsCore {
namespace Components {

// Re-export WebUIConfig in the main namespace for backward compatibility
using WebUIConfig = WebUI::WebUIConfig;

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
    
    // Sub-managers
    std::unique_ptr<WebUI::WebServerManager> webServer;
    std::unique_ptr<WebUI::WebSocketHandler> webSocket;
    std::unique_ptr<WebUI::ProviderRegistry> registry;

    // State
    bool forceNextUpdate = false; // force full contexts send on next tick (e.g., after WS reconnect)
    
    // Config persistence callback
    std::function<void(const WebUIConfig&)> onConfigChanged;

public:
    /**
     * @brief Construct a WebUI component with the provided configuration.
     */
    WebUIComponent(const WebUIConfig& cfg = WebUIConfig()) 
        : config(cfg) {
        // Initialize component metadata immediately for dependency resolution
        metadata.name = "WebUI";
        metadata.version = "1.4.0";
        metadata.author = "DomoticsCore";
        metadata.description = "Web dashboard and API component";

        registry = std::unique_ptr<WebUI::ProviderRegistry>(new WebUI::ProviderRegistry());
        webServer = std::unique_ptr<WebUI::WebServerManager>(new WebUI::WebServerManager(config));
        webSocket = std::unique_ptr<WebUI::WebSocketHandler>(new WebUI::WebSocketHandler(config));
    }

    /**
     * @brief Release the instances.
     */
    ~WebUIComponent() {
        // unique_ptrs clean themselves up
    }

    // IComponent interface
    /**
     * @brief Initialize the AsyncWebServer, register websocket handler, and configure routes.
     */
    ComponentStatus begin() override {
        webServer->begin();
        webServer->setAuthHandler([this](AsyncWebServerRequest* request) {
            return authenticate(request);
        });
        
        if (config.enableWebSocket) {
            webSocket->begin(webServer->getServer());
            
            // Restored: callbacks for initial data and UI actions
            webSocket->setClientConnectedCallback([this](AsyncWebSocketClient* client) {
                sendWebSocketUpdate(client);
            });
            webSocket->setForceUpdateCallback([this]() {
                forceNextUpdate = true;
            });
            webSocket->setUIActionCallback([this](const String& ctx, const String& field, const String& value) {
                handleUIAction(ctx, field, value);
            });
        }
        
        setupApiRoutes();

        webServer->start();
        
        return ComponentStatus::Success;
    }

    /**
     * @brief Pump periodic websocket updates and clean up disconnected clients.
     */
    void loop() override {
        webSocket->loop();

        if (webSocket->shouldSendUpdates()) {
            sendWebSocketUpdates();
        }
    }

    /**
     * @brief Stop the web server but keep configuration for potential restart.
     */
    ComponentStatus shutdown() override {
        webServer->stop();
        return ComponentStatus::Success;
    }

    // Provider management facade
    void registerProvider(IWebUIProvider* provider) {
        registry->registerProvider(provider);
    }

    void registerProviderWithComponent(IWebUIProvider* provider, IComponent* component) {
        registry->registerProviderWithComponent(provider, component);
    }

    void unregisterProvider(IWebUIProvider* provider) {
        registry->unregisterProvider(provider);
    }

    int getWebSocketClients() const {
        return webSocket->getClientCount();
    }

    uint16_t getPort() const { 
        return config.port; 
    }

    void notifyWiFiNetworkChanged() {
        webSocket->notifyWiFiNetworkChanged();
    }

    void setConfigCallback(std::function<void(const WebUIConfig&)> callback) {
        onConfigChanged = callback;
    }

    const WebUIConfig& getConfig() const {
        return config;
    }
    
    void setConfig(const WebUIConfig& cfg) {
        config = cfg;
        DLOG_I(LOG_WEB, "Config updated: theme=%s, deviceName=%s", 
               config.theme.c_str(), config.deviceName.c_str());
    }

    void registerProviderFactory(const String& typeKey, std::function<IWebUIProvider*(IComponent*)> factory) {
        registry->registerProviderFactory(typeKey, factory);
    }

    void registerApiRoute(const String& uri, WebRequestMethod method, ArRequestHandlerFunction handler) {
        webServer->registerRoute(uri, method, handler);
    }

    void registerApiUploadRoute(const String& uri, ArRequestHandlerFunction handler, ArUploadHandlerFunction uploadHandler) {
        webServer->registerUploadRoute(uri, handler, uploadHandler);
    }

    // IComponent override: post-initialization hook
    void onComponentsReady(const Components::ComponentRegistry& registry) override {
        this->registry->discoverProviders(registry);
        // Subscribe to future add/remove events
        auto& reg = const_cast<Components::ComponentRegistry&>(registry);
        reg.addListener(this);
    }

    // IWebUIProvider implementation for self-registration
    String getWebUIName() const override { return "WebUI"; }
    String getWebUIVersion() const override { return metadata.version; }
    IWebUIProvider* getWebUIProvider() override { return this; }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        
        // Provide default uptime header info item
        uint32_t seconds = HAL::Platform::getMillis() / 1000;
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
            .withField(WebUIField("theme", "Theme", WebUIFieldType::Select, config.theme, "dark,light,auto"))
            .withField(WebUIField("primary_color", "Primary Color", WebUIFieldType::Text, config.primaryColor))
            .withField(WebUIField("enable_auth", "Enable Authentication", WebUIFieldType::Boolean, config.enableAuth ? "true" : "false"))
            .withField(WebUIField("username", "Username", WebUIFieldType::Text, config.username))
            .withField(WebUIField("password", "Password", WebUIFieldType::Password, ""))
        );
        return contexts;
    }

    String getWebUIData(const String& contextId) override {
        if (contextId == "webui_uptime") {
            JsonDocument doc;
            uint32_t seconds = HAL::Platform::getMillis() / 1000;
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
            doc.shrinkToFit();  // ArduinoJson 7: Release over-allocated memory
            return json;
        }

        if (contextId == "webui_settings") {
            JsonDocument doc;
            doc["theme"] = config.theme;
            doc["primary_color"] = config.primaryColor;
            doc["enable_auth"] = config.enableAuth ? "true" : "false";
            doc["username"] = config.username;
            doc["password"] = ""; // Never send password back
            String json;
            serializeJson(doc, json);
            doc.shrinkToFit();  // ArduinoJson 7: Release over-allocated memory
            return json;
        }
        return "{}";
    }

    // Registry listener events
    void onComponentAdded(IComponent* comp) override {
        if (!comp) return;
        IWebUIProvider* provider = comp->getWebUIProvider();
        if (provider) {
            registerProviderWithComponent(provider, comp);
        }
    }

    void onComponentRemoved(IComponent* comp) override {
        registry->handleComponentRemoved(comp);
    }

    String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params) override {
        if (contextId == "webui_settings" && method == "POST") {
            auto fieldIt = params.find("field");
            auto valueIt = params.find("value");
            if (fieldIt != params.end() && valueIt != params.end()) {
                const String& field = fieldIt->second;
                const String& value = valueIt->second;
                
                if (field == "theme") {
                    config.theme = value;
                } else if (field == "primary_color") {
                    config.primaryColor = value;
                } else if (field == "enable_auth") {
                    config.enableAuth = (value == "true" || value == "1");
                } else if (field == "username") {
                    config.username = value;
                } else if (field == "password") {
                    if (value.length() > 0) {
                        config.password = value;
                    }
                } else {
                    return "{\"success\":false, \"error\":\"Unknown field\"}";
                }
                
                if (onConfigChanged) {
                    onConfigChanged(config);
                }
                return "{\"success\":true}";
            }
        }
        return "{\"success\":false, \"error\":\"Invalid request\"}";
    }

private:
    bool authenticate(AsyncWebServerRequest* request) {
        if (!config.enableAuth) return true;
        return request->authenticate(config.username.c_str(), config.password.c_str());
    }

    /**
     * @brief Print a string with JSON escaping to a response stream
     */
    void printJsonEscaped(AsyncResponseStream* response, const String& str) {
        for (size_t i = 0; i < str.length(); i++) {
            char c = str[i];
            switch (c) {
                case '"': response->print("\\\""); break;
                case '\\': response->print("\\\\"); break;
                case '\n': response->print("\\n"); break;
                case '\r': response->print("\\r"); break;
                case '\t': response->print("\\t"); break;
                default:
                    if (c < 0x20) {
                        // Control character
                        char buf[7];
                        snprintf(buf, sizeof(buf), "\\u00%02x", (unsigned char)c);
                        response->print(buf);
                    } else {
                        response->write(c);
                    }
                    break;
            }
        }
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
    
    void setupApiRoutes() {
        // System info API - optimized for ESP8266: use snprintf instead of String concatenation
        webServer->registerRoute("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest* request) {
            char sysInfo[128];
            snprintf(sysInfo, sizeof(sysInfo), "{\"uptime\":%u,\"heap\":%u,\"clients\":%d}",
                (unsigned)HAL::Platform::getMillis(), (unsigned)HAL::Platform::getFreeHeap(), getWebSocketClients());
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", sysInfo);
            addCorsHeaders(response);
            request->send(response);
        });
        
        // API components
        webServer->registerRoute("/api/components", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }
            
            AsyncResponseStream *response = request->beginResponseStream("application/json");
            addCorsHeaders(response);
            JsonDocument doc;
            registry->getComponentsList(doc);
            serializeJson(doc, *response);
            request->send(response);
        });

        // API components enable
        webServer->registerRoute("/api/components/enable", HTTP_POST, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }

            AsyncResponseStream *response = request->beginResponseStream("application/json");
            addCorsHeaders(response);
            
            String name;
            bool enabled = true;
            if (request->hasParam("name", true)) {
                name = request->getParam("name", true)->value();
            }
            if (request->hasParam("enabled", true)) {
                String v = request->getParam("enabled", true)->value();
                enabled = (v == "true" || v == "1" || v == "on");
            }

            auto result = registry->enableComponent(name, enabled);

            JsonDocument doc;
            doc["success"] = result.success;
            doc["name"] = result.name;
            doc["enabled"] = result.enabled;
            if (!result.warning.isEmpty()) {
                doc["warning"] = result.warning;
            }
            
            serializeJson(doc, *response);
            request->send(response);

            if (result.found) {
                webSocket->broadcastSchemaChange(name);
            }
        });

        // Context schema endpoint - loads full schema for a specific context
        webServer->registerRoute("/api/ui/context", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                return request->requestAuthentication();
            }

            if (!request->hasParam("id")) {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"error\":\"Missing 'id' parameter\"}");
                addCorsHeaders(response);
                request->send(response);
                return;
            }

            String contextId = request->getParam("id")->value();
            DLOG_I(LOG_WEB, "Loading context schema for: %s (heap: %u)", contextId.c_str(), HAL::Platform::getFreeHeap());

            IWebUIProvider* provider = registry->getProviderForContext(contextId);
            if (!provider) {
                AsyncWebServerResponse* response = request->beginResponse(404, "application/json",
                    "{\"error\":\"Context not found\"}");
                addCorsHeaders(response);
                request->send(response);
                return;
            }

            // Find the context from the provider
            WebUIContext foundContext;
            bool found = false;
            provider->forEachContext([&](const WebUIContext& ctx) {
                if (ctx.contextId == contextId) {
                    foundContext = ctx;
                    found = true;
                    return false; // Stop iteration
                }
                return true; // Continue
            });

            if (!found) {
                AsyncWebServerResponse* response = request->beginResponse(404, "application/json",
                    "{\"error\":\"Context not found in provider\"}");
                addCorsHeaders(response);
                request->send(response);
                return;
            }

            // Serialize the context
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            serializeContext(obj, foundContext);

            AsyncResponseStream *response = request->beginResponseStream("application/json");
            addCorsHeaders(response);
            serializeJson(doc, *response);
            request->send(response);

            DLOG_I(LOG_WEB, "Context schema sent for: %s (heap: %u)", contextId.c_str(), HAL::Platform::getFreeHeap());
        });

        // Schema endpoint - uses streaming serializer with static state
        // Memory-optimized: loads one context at a time using getContextAt()
        webServer->registerChunkedRoute("/api/ui/schema", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && !authenticate(request)) {
                request->requestAuthentication();
                return;
            }

            // Use static serializer state to minimize heap allocations
            static WebUI::ProviderRegistry::SchemaChunkState staticState;

            // Reset state - clear strings explicitly to release memory
            staticState.providers.clear();
            staticState.providers.shrink_to_fit();
            staticState.providerIndex = 0;
            staticState.contextIndexInProvider = 0;
            staticState.began = false;
            staticState.finished = false;
            staticState.needComma = false;
            staticState.serializingContext = false;
            // Clear context pointer
            staticState.currentContextPtr = nullptr;

            // Build provider list (just pointers, low memory)
            for (const auto& kv : registry->getContextProviders()) {
                if (kv.second && std::find(staticState.providers.begin(), staticState.providers.end(), kv.second) == staticState.providers.end()) {
                    staticState.providers.push_back(kv.second);
                }
            }

            DLOG_I(LOG_WEB, "Schema: %d providers, heap: %u", (int)staticState.providers.size(), HAL::Platform::getFreeHeap());

            auto* state = &staticState;

            AsyncWebServerResponse *response = request->beginChunkedResponse("application/json",
                [this, state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                    size_t written = 0;

                    if (state->finished) return 0;

                    if (!state->began) {
                        if (maxLen < 1) return 0;
                        buffer[written++] = '[';
                        state->began = true;
                    }

                    while (written < maxLen && !state->finished) {
                        if (state->serializingContext) {
                            size_t n = state->serializer.write(buffer + written, maxLen - written);
                            written += n;

                            if (state->serializer.isComplete()) {
                                state->serializingContext = false;
                                state->needComma = true;
                                // Clear context pointer
                                state->currentContextPtr = nullptr;
                            } else if (n == 0) {
                                break;
                            }
                            continue;
                        }

                        // Get next context from providers using indexed access
                        bool hasNext = false;
                        while (state->providerIndex < state->providers.size()) {
                            IWebUIProvider* provider = state->providers[state->providerIndex];
                            if (!provider || !provider->isWebUIEnabled()) {
                                state->providerIndex++;
                                state->contextIndexInProvider = 0;
                                continue;
                            }

                            // Get context pointer - ZERO COPY from cache
                            const WebUIContext* ctxPtr = provider->getContextAtRef(state->contextIndexInProvider);
                            if (ctxPtr) {
                                state->currentContextPtr = ctxPtr;
                                state->contextIndexInProvider++;
                                hasNext = true;
                                break;
                            }

                            // No more contexts in this provider
                            state->providerIndex++;
                            state->contextIndexInProvider = 0;
                        }

                        if (!hasNext) {
                            if (written < maxLen) {
                                buffer[written++] = ']';
                            }
                            state->finished = true;
                            // Clear state to release memory
                            state->providers.clear();
                            state->providers.shrink_to_fit();
                            state->currentContextPtr = nullptr;
                            DLOG_I(LOG_WEB, "Schema done, heap: %u", HAL::Platform::getFreeHeap());
                            return written;
                        }

                        if (!state->currentContextPtr || state->currentContextPtr->contextId.isEmpty()) continue;

                        if (state->needComma) {
                            if (written < maxLen) {
                                buffer[written++] = ',';
                            } else {
                                state->contextIndexInProvider--;
                                return written;
                            }
                        }

                        state->serializer.begin(*state->currentContextPtr);
                        state->serializingContext = true;

                        size_t n = state->serializer.write(buffer + written, maxLen - written);
                        written += n;

                        if (state->serializer.isComplete()) {
                            state->serializingContext = false;
                            state->needComma = true;
                            state->currentContextPtr = nullptr;
                        } else if (n == 0) {
                            break;
                        }
                    }

                    return written;
                });

            addCorsHeaders(response);
            request->send(response);
        });
    }
    
    // Duplicated helper to keep compilation working until I move it to a shared util or ProviderRegistry
    void serializeContext(JsonObject& obj, const WebUIContext& context) {
        obj["contextId"] = context.contextId;
        obj["title"] = context.title;
        obj["icon"] = context.icon;
        obj["location"] = (int)context.location;
        obj["presentation"] = (int)context.presentation;
        obj["priority"] = context.priority;
        obj["apiEndpoint"] = context.apiEndpoint;
        obj["alwaysInteractive"] = context.alwaysInteractive;
        
        if (!context.customHtml.isEmpty()) obj["customHtml"] = context.customHtml;
        if (!context.customCss.isEmpty()) obj["customCss"] = context.customCss;
        if (!context.customJs.isEmpty()) obj["customJs"] = context.customJs;

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
                for (const auto& opt : field.options) options.add(opt);
            }
            if (!field.optionLabels.empty()) {
                JsonObject labels = fieldObj["optionLabels"].to<JsonObject>();
                for (const auto& pair : field.optionLabels) labels[pair.first] = pair.second;
            }
        }
    }

    // Restored: send update to specific client on connect
    // Optimized for ESP8266: use static buffer instead of String concatenation
    void sendWebSocketUpdate(AsyncWebSocketClient* client) {
        if (!client || client->status() != WS_CONNECTED) return;

        // Reuse the same buffer as sendWebSocketUpdates to avoid extra heap allocation
        static char buffer[WEBUI_WS_BUFFER_SIZE];
        int pos = snprintf(buffer, sizeof(buffer),
            "{\"system\":{\"uptime\":%u,\"heap\":%u,\"clients\":%d,\"device_name\":\"%s\"},\"contexts\":{",
            (unsigned)HAL::Platform::getMillis(), (unsigned)HAL::Platform::getFreeHeap(), getWebSocketClients(), config.deviceName.c_str());

        if (pos < 0 || pos >= (int)sizeof(buffer)) return;

        int count = 0;
        for (const auto& p : registry->getContextProviders()) {
            if (pos > (int)sizeof(buffer) - 512) break;

            String data = p.second->getWebUIData(p.first);
            if (data.isEmpty() || data == "{}") continue;

            int needed = p.first.length() + data.length() + 5;
            if (pos + needed >= (int)sizeof(buffer) - 10) break;

            if (count++ > 0) buffer[pos++] = ',';
            int written = snprintf(buffer + pos, sizeof(buffer) - pos,
                "\"%s\":%s", p.first.c_str(), data.c_str());
            if (written > 0) pos += written;
        }

        if (pos < (int)sizeof(buffer) - 3) {
            buffer[pos++] = '}';
            buffer[pos++] = '}';
            buffer[pos] = '\0';
            if (client->canSend()) client->text(buffer);
        }
    }
    
    // Handle UI action from WebSocket
    void handleUIAction(const String& contextId, const String& field, const String& value) {
        IWebUIProvider* provider = registry->getProviderForContext(contextId);
        if (provider) {
            std::map<String, String> params;
            params["field"] = field;
            params["value"] = value;
            provider->handleWebUIRequest(contextId, "/", "POST", params);
            forceNextUpdate = true;
        }
    }

    void sendWebSocketUpdates() {
        // Use static buffer to avoid heap fragmentation on long-running devices
        // Size is platform-specific: ESP32=8KB, ESP8266=2KB, Native=4KB
        static char buffer[WEBUI_WS_BUFFER_SIZE];
        int pos = 0;
        
        // Write system info header
        pos = snprintf(buffer, sizeof(buffer),
            "{\"system\":{\"uptime\":%u,\"heap\":%u,\"clients\":%d,\"device_name\":\"%s\"},\"contexts\":{",
            (unsigned)HAL::Platform::getMillis(), (unsigned)HAL::Platform::getFreeHeap(), getWebSocketClients(), config.deviceName.c_str());
        
        if (pos < 0 || pos >= (int)sizeof(buffer)) {
            DLOG_E(LOG_WEB, "WS buffer overflow in header");
            return;
        }
        
        int contextCount = 0;
        auto contextProviders = registry->getContextProviders();
        
        for (const auto& pair : contextProviders) {
            // Leave room for context data + closing braces
            if (pos > (int)sizeof(buffer) - 512) {
                DLOG_W(LOG_WEB, "WS buffer nearly full, truncating contexts");
                break;
            }
            
            const String& contextId = pair.first;
            IWebUIProvider* provider = pair.second;
            
            // Delta check - skip unchanged data
            if (!forceNextUpdate && !provider->hasDataChanged(contextId)) continue;
            
            String contextData = provider->getWebUIData(contextId);
            if (contextData.isEmpty() || contextData == "{}") continue;
            
            // Calculate space needed: "contextId":data,
            int needed = contextId.length() + contextData.length() + 5;
            if (pos + needed >= (int)sizeof(buffer) - 10) {
                DLOG_W(LOG_WEB, "WS buffer full, skipping remaining contexts");
                break;
            }
            
            // Add comma separator if not first context
            if (contextCount > 0) {
                buffer[pos++] = ',';
            }
            
            // Write context to buffer
            int written = snprintf(buffer + pos, sizeof(buffer) - pos,
                "\"%s\":%s", contextId.c_str(), contextData.c_str());
            
            if (written > 0 && pos + written < (int)sizeof(buffer)) {
                pos += written;
                contextCount++;
            }
        }
        
        // Close JSON object
        if (pos < (int)sizeof(buffer) - 3) {
            buffer[pos++] = '}';
            buffer[pos++] = '}';
            buffer[pos] = '\0';
            
            webSocket->broadcast(String(buffer));
            forceNextUpdate = false;
        }
    }
};

} // namespace Components
} // namespace DomoticsCore
