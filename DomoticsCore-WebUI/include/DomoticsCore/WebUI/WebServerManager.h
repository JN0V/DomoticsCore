#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include "WebUIConfig.h"
#include "DomoticsCore/Generated/WebUIAssets.h"
#include "DomoticsCore/Logger.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @class WebServerManager
 * @brief Manages HTTP server and static file serving.
 */
class WebServerManager {
private:
    WebUIConfig config;
    AsyncWebServer* server = nullptr;
    
    // Helper for authentication
    std::function<bool(AsyncWebServerRequest*)> authHandler;

public:
    WebServerManager(const WebUIConfig& cfg) : config(cfg) {}

    ~WebServerManager() {
        if (server) {
            delete server;
            server = nullptr;
        }
    }

    void begin() {
        server = new AsyncWebServer(config.port);
        setupStaticRoutes();
    }

    void start() {
        if (server) server->begin();
    }

    void stop() {
        if (server) server->end();
    }

    AsyncWebServer* getServer() {
        return server;
    }

    void setAuthHandler(std::function<bool(AsyncWebServerRequest*)> handler) {
        authHandler = handler;
    }
    
    // Expose route registration
    void registerRoute(const String& uri, WebRequestMethod method, ArRequestHandlerFunction handler) {
        if (server) server->on(uri.c_str(), method, handler);
    }

    void registerChunkedRoute(const String& uri, WebRequestMethod method, std::function<void(AsyncWebServerRequest*)> handler) {
        if (server) server->on(uri.c_str(), method, handler);
    }
    
    void registerUploadRoute(const String& uri, ArRequestHandlerFunction handler, ArUploadHandlerFunction uploadHandler) {
        if (server) server->on(uri.c_str(), HTTP_POST, handler, uploadHandler);
    }

    // Default static file serving
    void setupStaticRoutes() {
        if (!server) return;

        // Serve main HTML page
        server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.enableAuth && authHandler && !authHandler(request)) {
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
                // Disable caching so updated UI logic is always loaded after firmware updates
                resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
                request->send(resp);
            }
        });
    }

private:
    void serveFromFileSystem(AsyncWebServerRequest* request, const String& path, const String& contentType) {
        if (LittleFS.exists(path)) {
            request->send(LittleFS, path, contentType);
        } else if (SPIFFS.exists(path)) {
            request->send(SPIFFS, path, contentType);
        } else {
            request->send(404, "text/plain", "File not found");
        }
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
