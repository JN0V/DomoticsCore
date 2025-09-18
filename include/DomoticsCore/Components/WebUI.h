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

/**
 * Optimized WebUI Content Storage using PROGMEM
 * Stores static web content in flash memory to save RAM
 */
class WebUIContent {
public:
    // HTML Content stored in PROGMEM
    static const char HTML_CONTENT[] PROGMEM;
    
    // CSS Content stored in PROGMEM  
    static const char CSS_CONTENT[] PROGMEM;
    
    // JavaScript Content stored in PROGMEM
    static const char JS_CONTENT[] PROGMEM;
    
    // Stream content directly from PROGMEM (most efficient)
    static void streamHTML(AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse_P(
            200, "text/html", (const uint8_t*)HTML_CONTENT, strlen_P(HTML_CONTENT)
        );
        response->addHeader("Cache-Control", "public, max-age=3600");
        request->send(response);
    }
    
    static void streamCSS(AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse_P(
            200, "text/css", (const uint8_t*)CSS_CONTENT, strlen_P(CSS_CONTENT)
        );
        response->addHeader("Cache-Control", "public, max-age=86400");
        request->send(response);
    }
    
    static void streamJS(AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse_P(
            200, "application/javascript", (const uint8_t*)JS_CONTENT, strlen_P(JS_CONTENT)
        );
        response->addHeader("Cache-Control", "public, max-age=86400");
        request->send(response);
    }
};

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
                WebUIContent::streamHTML(request);
            }
        });
        
        // Serve CSS
        server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.useFileSystem) {
                serveFromFileSystem(request, "/webui/style.css", "text/css");
            } else {
                WebUIContent::streamCSS(request);
            }
        });
        
        // Serve JavaScript
        server->on("/app.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (config.useFileSystem) {
                serveFromFileSystem(request, "/webui/app.js", "application/javascript");
            } else {
                WebUIContent::streamJS(request);
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
                    WebUIContent::streamHTML(request);
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

// HTML Content Definition
const char WebUIContent::HTML_CONTENT[] PROGMEM = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DomoticsCore Dashboard</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="app-container">
        <header class="header">
            <div class="header-left">
                <button class="menu-toggle" id="menuToggle"><span class="icon-bars">☰</span></button>
                <h1 class="device-name" id="deviceName">DomoticsCore Device</h1>
            </div>
            <div class="header-center">
                <div class="datetime" id="datetime"></div>
            </div>
            <div class="header-right">
                <div class="status-indicators">
                    <span class="status-indicator" id="wifiStatus"><span class="icon-wifi">📶</span></span>
                    <span class="status-indicator" id="wsStatus"><span class="icon-connection">🔌</span></span>
                </div>
            </div>
        </header>
        
        <div class="content-wrapper">
            <nav class="sidebar" id="sidebar">
                <ul class="nav-menu">
                    <li><a href="#dashboard" class="nav-link active" data-section="dashboard">
                        <span class="icon-dashboard">📊</span> Dashboard
                    </a></li>
                    <li><a href="#components" class="nav-link" data-section="components">
                        <span class="icon-components">🧩</span> Components
                    </a></li>
                    <li><a href="#settings" class="nav-link" data-section="settings">
                        <span class="icon-settings">⚙️</span> Settings
                    </a></li>
                </ul>
            </nav>
            
            <main class="main-content">
            <section id="dashboard-section" class="content-section active">
                <h2>Dashboard</h2>
                <div id="dashboardGrid" class="dashboard-grid">
                    <div class="loading-message">Loading dashboard...</div>
                </div>
            </section>
            
            <section id="components-section" class="content-section">
                <h2>Components</h2>
                <div id="componentsGrid" class="components-grid">
                    <div class="loading-message">Loading components...</div>
                </div>
            </section>
            
            <section id="settings-section" class="content-section">
                <h2>System Settings</h2>
                <div id="settingsGrid" class="settings-grid">
                    <div class="loading-message">Loading settings...</div>
                </div>
            </section>
            </main>
        </div>
        
        <footer class="footer">
            <div class="footer-content">
                <span class="footer-text" id="footerText">Loading...</span>
            </div>
        </footer>
    </div>
    <script src="/app.js"></script>
</body>
</html>)";

// CSS Content Definition (with footer, mobile fixes, and embedded icons)
const char WebUIContent::CSS_CONTENT[] PROGMEM = R"(:root{--primary-color:#007acc;--bg-primary:#1a1a1a;--bg-secondary:#2d2d2d;--bg-tertiary:#3d3d3d;--text-primary:#ffffff;--text-secondary:#cccccc;--border-color:#444444;--success-color:#28a745;--warning-color:#ffc107;--danger-color:#dc3545;--info-color:#17a2b8;--footer-height:50px}*{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background-color:var(--bg-primary);color:var(--text-primary);line-height:1.6}.app-container{display:flex;flex-direction:column;height:100vh}.header{background-color:var(--bg-secondary);border-bottom:1px solid var(--border-color);padding:1rem;display:flex;justify-content:space-between;align-items:center;position:fixed;top:0;left:0;right:0;z-index:100;height:80px}.header-left{display:flex;align-items:center;gap:1rem}.menu-toggle{background:none;border:none;color:var(--text-primary);font-size:1.5rem;cursor:pointer;padding:0.75rem;display:block;border-radius:4px;transition:background-color 0.2s ease}.menu-toggle:hover{background-color:var(--bg-tertiary)}.menu-toggle:active{background-color:var(--primary-color)}@media (min-width:768px){.menu-toggle{display:none}}.device-name{font-size:1.5rem;font-weight:600}@media (max-width:767px){.device-name{font-size:1.2rem}}.header-center{flex:1;text-align:center}.datetime{font-size:0.9rem;color:var(--text-secondary)}.header-right{display:flex;align-items:center;gap:1rem}.status-indicators{display:flex;gap:0.5rem}.status-indicator{padding:0.5rem;border-radius:50%;background-color:var(--bg-tertiary);color:var(--text-secondary)}.status-indicator.connected{color:var(--success-color)}.content-wrapper{display:flex;margin-top:80px;height:calc(100vh - 80px - var(--footer-height))}.sidebar{background-color:var(--bg-secondary);border-right:1px solid var(--border-color);width:250px;position:fixed;left:-250px;top:80px;height:calc(100vh - 80px - var(--footer-height));transition:left 0.3s ease;z-index:99;overflow-y:auto}@media (min-width:768px){.sidebar{position:static;left:0;transform:none}}.sidebar.open{left:0}.nav-menu{list-style:none;padding:1rem 0}.nav-menu li{margin:0.5rem 0}.nav-link{display:flex;align-items:center;gap:0.75rem;padding:0.75rem 1.5rem;color:var(--text-secondary);text-decoration:none;transition:all 0.2s ease}.nav-link:hover,.nav-link.active{background-color:var(--bg-tertiary);color:var(--primary-color)}.main-content{flex:1;padding:2rem;margin-left:0;overflow-y:auto;padding-bottom:1rem}@media (min-width:768px){.main-content{margin-left:0px}}.content-section{display:none}.content-section.active{display:block}.dashboard-grid,.components-grid,.settings-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:1.5rem;margin-top:1.5rem}.card{background-color:var(--bg-secondary);border:1px solid var(--border-color);border-radius:8px;padding:1.5rem;transition:transform 0.2s ease}.card:hover{transform:translateY(-2px)}.card-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:1rem}.card-title{font-size:1.2rem;font-weight:600}.card-status{padding:0.25rem 0.75rem;border-radius:20px;font-size:0.8rem;font-weight:500}.status-success{background-color:var(--success-color);color:white}.status-warning{background-color:var(--warning-color);color:black}.status-error{background-color:var(--danger-color);color:white}.card-content{margin-top:1rem}.field-row{display:flex;justify-content:space-between;align-items:center;margin:0.5rem 0;padding:0.5rem 0;border-bottom:1px solid var(--border-color)}.field-row:last-child{border-bottom:none}.field-label{font-weight:500;color:var(--text-secondary)}.field-value{color:var(--text-primary);font-weight:600}.loading-message{text-align:center;color:var(--text-secondary);padding:2rem}.footer{background-color:var(--bg-secondary);border-top:1px solid var(--border-color);position:fixed;bottom:0;left:0;right:0;height:var(--footer-height);z-index:98;display:flex;align-items:center;justify-content:center}.footer-content{text-align:center}.footer-text{font-size:0.8rem;color:var(--text-secondary)}.icon-bars,.icon-wifi,.icon-connection,.icon-dashboard,.icon-components,.icon-settings{display:inline-block;font-style:normal;font-size:1em;line-height:1}.icon-bars{font-size:1.2em}.status-indicator .icon-wifi,.status-indicator .icon-connection{font-size:0.9em})";

// JavaScript Content Definition (improved functionality)
const char WebUIContent::JS_CONTENT[] PROGMEM = R"(class DomoticsApp{constructor(){this.ws=null;this.wsReconnectInterval=null;this.systemStartTime=Date.now();this.init()}init(){this.setupEventListeners();this.setupWebSocket();this.updateDateTime();setInterval(()=>this.updateDateTime(),1000)}setupEventListeners(){const menuToggle=document.getElementById('menuToggle');const sidebar=document.getElementById('sidebar');const navLinks=document.querySelectorAll('.nav-link');menuToggle?.addEventListener('click',()=>{sidebar.classList.toggle('open')});navLinks.forEach(link=>{link.addEventListener('click',(e)=>{e.preventDefault();const section=link.dataset.section;this.showSection(section);navLinks.forEach(l=>l.classList.remove('active'));link.classList.add('active')})});document.addEventListener('click',(e)=>{if(!sidebar.contains(e.target)&&!menuToggle.contains(e.target)){sidebar.classList.remove('open')}})}setupWebSocket(){const protocol=window.location.protocol==='https:'?'wss':'ws';const wsUrl=`${protocol}://${window.location.host}/ws`;this.ws=new WebSocket(wsUrl);this.ws.onopen=()=>{console.log('WebSocket connected');this.updateConnectionStatus(true);this.clearReconnectInterval()};this.ws.onmessage=(event)=>{try{const data=JSON.parse(event.data);this.handleWebSocketMessage(data)}catch(e){console.error('Failed to parse WebSocket message:',e)}};this.ws.onclose=()=>{console.log('WebSocket disconnected');this.updateConnectionStatus(false);this.scheduleReconnect()};this.ws.onerror=(error)=>{console.error('WebSocket error:',error)}}scheduleReconnect(){if(this.wsReconnectInterval)return;this.wsReconnectInterval=setInterval(()=>{if(this.ws.readyState===WebSocket.CLOSED){this.setupWebSocket()}},5000)}clearReconnectInterval(){if(this.wsReconnectInterval){clearInterval(this.wsReconnectInterval);this.wsReconnectInterval=null}}handleWebSocketMessage(data){if(data.system){this.updateSystemInfo(data.system)}if(data.contexts){this.updateContexts(data.contexts)}}updateSystemInfo(system){if(system.uptime){this.systemStartTime=Date.now()-system.uptime}if(system.device_name){document.getElementById('deviceName').textContent=system.device_name}if(system.manufacturer&&system.version){const footerText=document.getElementById('footerText');if(footerText){footerText.textContent=`${system.manufacturer} ${system.version} | ESP32 Framework`;}}}updateContexts(contexts){const dashboardGrid=document.getElementById('dashboardGrid');if(!dashboardGrid)return;dashboardGrid.innerHTML='';Object.entries(contexts).forEach(([id,context])=>{const card=this.createContextCard(id,context);dashboardGrid.appendChild(card)})}createContextCard(id,context){const card=document.createElement('div');card.className='card';let content='';if(typeof context==='object'){Object.entries(context).forEach(([key,value])=>{if(key!=='name'&&key!=='id'){content+=`<div class="field-row"><span class="field-label">${this.formatLabel(key)}:</span><span class="field-value">${this.formatValue(value)}</span></div>`}})}else{content=`<div class="field-row"><span class="field-value">${context}</span></div>`}card.innerHTML=`<div class="card-header"><h3 class="card-title">${this.formatTitle(id)}</h3><span class="card-status status-success">Active</span></div><div class="card-content">${content}</div>`;return card}formatTitle(id){return id.replace(/_/g,' ').replace(/\b\w/g,l=>l.toUpperCase())}formatLabel(key){return key.replace(/_/g,' ').replace(/\b\w/g,l=>l.toUpperCase())}formatValue(value){if(typeof value==='boolean'){return value?'On':'Off'}if(typeof value==='number'&&value>1000000){return(value/1000000).toFixed(1)+'M'}if(typeof value==='number'&&value>1000){return(value/1000).toFixed(1)+'K'}return String(value)}updateConnectionStatus(connected){const wsStatus=document.getElementById('wsStatus');if(wsStatus){wsStatus.classList.toggle('connected',connected)}}updateDateTime(){const now=new Date();const uptime=Math.floor((now.getTime()-this.systemStartTime)/1000);const hours=Math.floor(uptime/3600);const minutes=Math.floor((uptime%3600)/60);const seconds=uptime%60;const uptimeStr=`${hours.toString().padStart(2,'0')}:${minutes.toString().padStart(2,'0')}:${seconds.toString().padStart(2,'0')}`;document.getElementById('datetime').textContent=`Uptime: ${uptimeStr}`}showSection(sectionName){document.querySelectorAll('.content-section').forEach(section=>{section.classList.remove('active')});const targetSection=document.getElementById(`${sectionName}-section`);if(targetSection){targetSection.classList.add('active')}if(sectionName==='components'){this.loadComponents()}else if(sectionName==='settings'){this.loadSettings()}}loadComponents(){const grid=document.getElementById('componentsGrid');grid.innerHTML='<div class="card"><div class="card-header"><h3 class="card-title">System Components</h3></div><div class="card-content"><div class="field-row"><span class="field-label">WebUI Component:</span><span class="field-value">Active</span></div><div class="field-row"><span class="field-label">Demo LED Controller:</span><span class="field-value">Active</span></div><div class="field-row"><span class="field-label">Demo System Info:</span><span class="field-value">Active</span></div></div></div>'}loadSettings(){const grid=document.getElementById('settingsGrid');grid.innerHTML='<div class="card"><div class="card-header"><h3 class="card-title">System Information</h3></div><div class="card-content"><div class="field-row"><span class="field-label">Device:</span><span class="field-value">ESP32</span></div><div class="field-row"><span class="field-label">Framework:</span><span class="field-value">DomoticsCore v2.0</span></div><div class="field-row"><span class="field-label">WebUI Port:</span><span class="field-value">80</span></div></div></div>'}}document.addEventListener('DOMContentLoaded',()=>{new DomoticsApp()});)";

} // namespace Components
} // namespace DomoticsCore
