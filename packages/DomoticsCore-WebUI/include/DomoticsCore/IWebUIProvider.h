#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {

/**
 * Enhanced WebUI system with multi-context support
 * Allows components to display data in multiple locations with different presentations
 */

enum class WebUILocation {
    Dashboard,          // Main dashboard overview
    ComponentDetail,    // Detailed component view  
    HeaderStatus,       // Top-right status indicators
    QuickControls,      // Sidebar quick actions
    Settings           // Settings/configuration area
};

enum class WebUIPresentation {
    Card,              // Standard card layout
    Gauge,             // Circular gauge/meter
    Graph,             // Time-series chart
    StatusBadge,       // Small status indicator
    ProgressBar,       // Progress/percentage bar
    Table,             // Tabular data
    Toggle,            // On/off switch
    Slider,            // Range control
    Text,              // Simple text display
    Button             // Action button
};

enum class WebUIFieldType {
    Text,              // Text input/display
    Number,            // Number input/display
    Float,             // Float input/display
    Boolean,           // Checkbox/toggle
    Select,            // Dropdown selection
    Slider,            // Range slider
    Color,             // Color picker
    Button,            // Action button
    Display,           // Read-only display
    Chart,             // Chart data
    Status,            // Status indicator
    Progress           // Progress value
};

/**
 * Enhanced field definition with context-aware configuration
 */
struct WebUIField {
    String name;                    // Field identifier
    String label;                   // Display label
    WebUIFieldType type;            // Field type
    String value;                   // Current value
    String unit;                    // Unit of measurement
    bool readOnly;                  // Read-only flag
    
    // Constraints and options
    float minValue = 0;
    float maxValue = 100;
    std::vector<String> options;    // For select fields
    String endpoint;                // API endpoint for updates
    
    // Context-specific configuration
    JsonDocument config;            // Custom field configuration
    
    WebUIField(const String& n, const String& l, WebUIFieldType t, 
               const String& v = "", const String& u = "", bool ro = false)
        : name(n), label(l), type(t), value(v), unit(u), readOnly(ro) {}
    
    // Fluent interface
    WebUIField& range(float min, float max) { minValue = min; maxValue = max; return *this; }
    WebUIField& choices(const std::vector<String>& opts) { options = opts; return *this; }
    WebUIField& api(const String& ep) { endpoint = ep; return *this; }
    WebUIField& configure(const String& key, const JsonVariant& value) { 
        config[key] = value; 
        return *this; 
    }
};

/**
 * WebUI context - defines how component data appears in specific UI location
 */
struct WebUIContext {
    String contextId;               // Unique context identifier
    String title;                   // Context title
    String icon;                    // Icon class/name
    WebUILocation location;         // Where to display
    WebUIPresentation presentation; // How to display
    int priority = 0;               // Display order (higher = first)
    
    // Component-provided custom UI elements
    String customHtml;              // Custom HTML structure for this context
    String customCss;               // Custom CSS styling for this context  
    String customJs;                // Custom JavaScript behavior for this context
    
    std::vector<WebUIField> fields; // Context fields
    String apiEndpoint;             // API endpoint for this context
    bool realTime = false;          // Enable real-time updates
    int updateInterval = 5000;      // Update interval in ms
    
    // Context-specific configuration
    JsonDocument contextConfig;     // Custom presentation config
    
    // Constructors
    WebUIContext() = default;
    
    WebUIContext(const String& id, const String& t, const String& ic, 
                 WebUILocation loc, WebUIPresentation pres = WebUIPresentation::Card)
        : contextId(id), title(t), icon(ic), location(loc), presentation(pres) {}
    
    // Fluent interface
    WebUIContext& withField(const WebUIField& field) { 
        fields.push_back(field); 
        return *this; 
    }
    
    WebUIContext& withAPI(const String& endpoint) { 
        apiEndpoint = endpoint; 
        return *this; 
    }
    
    WebUIContext& withRealTime(int interval = 5000) { 
        realTime = true; 
        updateInterval = interval; 
        return *this; 
    }
    
    WebUIContext& withPriority(int p) { 
        priority = p; 
        return *this; 
    }
    
    WebUIContext& configure(const String& key, const JsonVariant& value) { 
        contextConfig[key] = value; 
        return *this; 
    }
    
    WebUIContext& withCustomHtml(const String& html) {
        customHtml = html;
        return *this;
    }
    
    WebUIContext& withCustomCss(const String& css) {
        customCss = css;
        return *this;
    }
    
    WebUIContext& withCustomJs(const String& js) {
        customJs = js;
        return *this;
    }
    
    // Factory methods for common contexts
    static WebUIContext dashboard(const String& id, const String& title, const String& icon = "fas fa-tachometer-alt") {
        return WebUIContext(id, title, icon, WebUILocation::Dashboard, WebUIPresentation::Card);
    }
    
    static WebUIContext gauge(const String& id, const String& title, const String& icon = "fas fa-gauge") {
        return WebUIContext(id, title, icon, WebUILocation::Dashboard, WebUIPresentation::Gauge);
    }
    
    static WebUIContext statusBadge(const String& id, const String& title, const String& icon = "fas fa-info") {
        return WebUIContext(id, title, icon, WebUILocation::HeaderStatus, WebUIPresentation::StatusBadge);
    }
    
    static WebUIContext graph(const String& id, const String& title, const String& icon = "fas fa-chart-line") {
        return WebUIContext(id, title, icon, WebUILocation::ComponentDetail, WebUIPresentation::Graph);
    }
    
    static WebUIContext quickControl(const String& id, const String& title, const String& icon = "fas fa-sliders") {
        return WebUIContext(id, title, icon, WebUILocation::QuickControls, WebUIPresentation::Toggle);
    }
    
    static WebUIContext settings(const String& id, const String& title, const String& icon = "fas fa-cog") {
        return WebUIContext(id, title, icon, WebUILocation::Settings, WebUIPresentation::Card);
    }
};

/**
 * WebUI Provider interface
 * Components implement this to provide multi-context UI integration
 */
class IWebUIProvider {
public:
    virtual ~IWebUIProvider() = default;
    
    /**
     * Get all WebUI contexts for this component
     * @return Vector of contexts defining how component appears in different UI locations
     */
    virtual std::vector<WebUIContext> getWebUIContexts() = 0;
    
    /**
     * Handle WebUI API requests for specific context
     * @param contextId The context identifier
     * @param endpoint The API endpoint being called
     * @param method HTTP method (GET, POST, PUT, DELETE)
     * @param params Request parameters
     * @return JSON response string
     */
    virtual String handleWebUIRequest(const String& contextId, const String& endpoint, 
                                    const String& method, const std::map<String, String>& params) = 0;
    
    /**
     * Get real-time data for specific context
     * @param contextId The context identifier
     * @return JSON string with current context data
     */
    virtual String getWebUIData(const String& contextId) { return "{}"; }

    // Methods to get component metadata directly for the UI
    virtual String getWebUIName() const = 0;
    virtual String getWebUIVersion() const = 0;
    
    /**
     * Get specific WebUI context by ID
     * @param contextId The context identifier
     * @return WebUIContext object for the specified ID
     */
    virtual WebUIContext getWebUIContext(const String& contextId) {
        auto contexts = getWebUIContexts();
        for (const auto& context : contexts) {
            if (context.contextId == contextId) {
                return context;
            }
        }
        return WebUIContext(); // Return empty context if not found
    }
    
    /**
     * Check if component should be visible in WebUI
     * @return true if component should appear in WebUI
     */
    virtual bool isWebUIEnabled() { return true; }
    
};

} // namespace Components
} // namespace DomoticsCore
