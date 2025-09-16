#pragma once

#include <Arduino.h>
#include <vector>
#include <map>

// DOMOTICSCORE_WEBUI_ENABLED is defined by WebUI.h when WebUI component is included
// If not defined, WebUI features are disabled

namespace DomoticsCore {
namespace Components {

/**
 * WebUI field types for dynamic form generation
 */
enum class WebUIFieldType {
    Text,           // Text input
    Number,         // Number input
    Float,          // Float input with decimals
    Boolean,        // Checkbox/toggle
    Select,         // Dropdown selection
    Slider,         // Range slider
    Color,          // Color picker
    Button,         // Action button
    Display,        // Read-only display value
    Chart,          // Real-time chart data
    Status          // Status indicator (online/offline/error)
};

/**
 * WebUI field definition for component data
 */
struct WebUIField {
    String name;                    // Field identifier
    String label;                   // Display label
    WebUIFieldType type;            // Field type
    String value;                   // Current value
    String unit;                    // Unit of measurement (optional)
    bool readOnly;                  // Read-only flag
    
    // Constraints
    float minValue = 0;
    float maxValue = 100;
    std::vector<String> options;    // For select fields
    String endpoint;                // API endpoint for updates
    
    WebUIField(const String& n, const String& l, WebUIFieldType t, 
               const String& v = "", const String& u = "", bool ro = false)
        : name(n), label(l), type(t), value(v), unit(u), readOnly(ro) {}
    
    // Fluent interface for constraints
    WebUIField& range(float min, float max) { minValue = min; maxValue = max; return *this; }
    WebUIField& choices(const std::vector<String>& opts) { options = opts; return *this; }
    WebUIField& api(const String& ep) { endpoint = ep; return *this; }
};

/**
 * WebUI section for component representation
 */
struct WebUISection {
    String id;                      // Section identifier
    String title;                   // Section title
    String icon;                    // Icon class/name
    String category;                // Category (dashboard, devices, settings, system)
    std::vector<WebUIField> fields; // Section fields
    String apiEndpoint;             // Base API endpoint
    bool realTime;                  // Enable real-time updates
    int updateInterval;             // Update interval in ms (for real-time)
    
    // Default constructor
    WebUISection() : realTime(false), updateInterval(5000) {}
    
    WebUISection(const String& i, const String& t, const String& ic = "", 
                 const String& cat = "general") 
        : id(i), title(t), icon(ic), category(cat), realTime(false), updateInterval(5000) {}
    
    // Fluent interface
    WebUISection& withField(const WebUIField& field) { fields.push_back(field); return *this; }
    WebUISection& withAPI(const String& endpoint) { apiEndpoint = endpoint; return *this; }
    WebUISection& withRealTime(int interval = 5000) { realTime = true; updateInterval = interval; return *this; }
};

/**
 * Interface for components that provide WebUI integration
 * Always available but methods only work when WebUI is enabled
 */
class IWebUIProvider {
public:
    virtual ~IWebUIProvider() = default;
    
    /**
     * Get WebUI section definition for this component
     * @return WebUI section with fields and configuration
     */
    virtual WebUISection getWebUISection() = 0;
    
    /**
     * Handle WebUI API requests for this component
     * @param endpoint The API endpoint being called
     * @param method HTTP method (GET, POST, PUT, DELETE)
     * @param params Request parameters
     * @return JSON response string
     */
    virtual String handleWebUIRequest(const String& endpoint, const String& method, 
                                    const std::map<String, String>& params) = 0;
    
    /**
     * Get real-time data for WebSocket updates
     * @return JSON string with current component data
     */
    virtual String getWebUIData() { return "{}"; }
    
    /**
     * Check if component should be visible in WebUI
     * @return true if component should appear in WebUI
     */
    virtual bool isWebUIEnabled() { return true; }
};

} // namespace Components
} // namespace DomoticsCore
