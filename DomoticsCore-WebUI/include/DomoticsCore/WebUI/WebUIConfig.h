#pragma once

#include <DomoticsCore/Platform_HAL.h>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * Optimized WebUI Component Configuration
 */
struct WebUIConfig {
    // User-configurable fields (exposed in WebUI settings)
    String deviceName = "DomoticsCore Device";  // Device display name
    String theme = "auto";                      // UI theme: dark, light, auto (system detection)
    
    // Advanced fields (configured at compile-time, not exposed in WebUI)
    uint16_t port = 80;                         // HTTP server port
    bool enableWebSocket = true;                // Enable WebSocket for live updates
    int wsUpdateInterval = 5000;                // WebSocket update interval in ms
    bool useFileSystem = false;                 // Use SPIFFS/LittleFS for content
    String staticPath = "/webui";               // Path for static files
    String primaryColor = "#007acc";            // Primary UI accent color
    bool enableAuth = false;                    // Enable basic authentication
    String username = "admin";                  // Auth username
    String password = "";                       // Auth password
    int maxWebSocketClients = 3;                // Max concurrent WebSocket connections
    int apiTimeout = 5000;              // API request timeout in ms
    bool enableCompression = true;      // Enable gzip compression
    bool enableCaching = true;          // Enable browser caching
    bool enableCORS = false;            // Enable CORS headers
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
