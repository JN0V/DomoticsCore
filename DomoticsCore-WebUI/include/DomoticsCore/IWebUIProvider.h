#pragma once

#include <DomoticsCore/Platform_HAL.h>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {

/**
 * @brief Helper for lazy state initialization and change tracking
 * 
 * This template class provides timing-independent state tracking for WebUI providers.
 * It handles the common pattern where providers may be created before components
 * are fully initialized.
 * 
 * Usage:
 * @code
 * LazyState<bool> connectedState;
 * 
 * // In hasDataChanged():
 * return connectedState.hasChanged(wifi->isConnected());
 * @endcode
 */
template<typename T>
struct LazyState {
    T value;
    bool initialized = false;
    
    /**
     * @brief Initialize state on first access
     * @param initializer Function that returns the initial value
     * @return Reference to the stored value
     */
    T& get(std::function<T()> initializer) {
        if (!initialized) {
            value = initializer();
            initialized = true;
        }
        return value;
    }
    
    /**
     * @brief Check if value has changed, update internal state
     * @param current Current value to compare against
     * @return true if value changed OR first call (to ensure initial state is sent)
     * 
     * On first call (uninitialized): stores value and returns TRUE (initial state needs to be sent)
     * On subsequent calls: compares with stored value, updates, returns true if changed
     */
    bool hasChanged(const T& current) {
        if (!initialized) {
            value = current;
            initialized = true;
            return true;  // First check returns true to ensure initial state is sent
        }
        bool changed = (current != value);
        value = current;
        return changed;
    }
    
    /**
     * @brief Get current stored value
     * @return Stored value (undefined if not initialized)
     */
    const T& getValue() const {
        return value;
    }
    
    /**
     * @brief Check if state has been initialized
     * @return true if initialized
     */
    bool isInitialized() const {
        return initialized;
    }
    
    /**
     * @brief Reset state to uninitialized
     */
    void reset() {
        initialized = false;
    }
};

/**
 * Enhanced WebUI system with multi-context support
 * Allows components to display data in multiple locations with different presentations
 */

enum class WebUILocation {
    Dashboard,          // Main dashboard overview
    ComponentDetail,    // Detailed component view  
    HeaderStatus,       // Top-right status indicators
    QuickControls,      // Sidebar quick actions
    Settings,           // Settings/configuration area
    HeaderInfo          // Main header info zone (time, uptime, etc.) - NEW: Added at end to preserve existing enum values
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
    Chart,             // Chart data (auto-rendered by frontend with history)
    Status,            // Status indicator
    Progress,          // Progress value
    Password,          // Password input
    File               // File upload input
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
    std::vector<String> options;    // For select fields (option values)
    std::map<String, String> optionLabels;  // Option value -> label mapping
    String endpoint;                // API endpoint for updates

    // Context-specific configuration
    // Use pointer to avoid large JsonDocument allocation on stack/heap for every field
    // Only allocated when configure() is called - most fields don't need custom config
    std::unique_ptr<JsonDocument> config;  // Custom field configuration (optional)

    WebUIField(const String& n, const String& l, WebUIFieldType t,
               const String& v = "", const String& u = "", bool ro = false)
        : name(n), label(l), type(t), value(v), unit(u), readOnly(ro) {}

    // Copy constructor - deep copy of config if present
    WebUIField(const WebUIField& other)
        : name(other.name), label(other.label), type(other.type), value(other.value),
          unit(other.unit), readOnly(other.readOnly), minValue(other.minValue),
          maxValue(other.maxValue), options(other.options), optionLabels(other.optionLabels),
          endpoint(other.endpoint) {
        if (other.config) {
            config = std::make_unique<JsonDocument>(*other.config);
        }
    }

    // Copy assignment
    WebUIField& operator=(const WebUIField& other) {
        if (this != &other) {
            name = other.name;
            label = other.label;
            type = other.type;
            value = other.value;
            unit = other.unit;
            readOnly = other.readOnly;
            minValue = other.minValue;
            maxValue = other.maxValue;
            options = other.options;
            optionLabels = other.optionLabels;
            endpoint = other.endpoint;
            if (other.config) {
                config = std::make_unique<JsonDocument>(*other.config);
            } else {
                config.reset();
            }
        }
        return *this;
    }

    // Move constructor and assignment are defaulted (unique_ptr handles move correctly)
    WebUIField(WebUIField&&) = default;
    WebUIField& operator=(WebUIField&&) = default;

    // Fluent interface
    WebUIField& range(float min, float max) { minValue = min; maxValue = max; return *this; }
    WebUIField& choices(const std::vector<String>& opts) { options = opts; return *this; }
    WebUIField& addOption(const String& value, const String& label) {
        options.push_back(value);
        optionLabels[value] = label;
        return *this;
    }
    WebUIField& api(const String& ep) { endpoint = ep; return *this; }
    WebUIField& configure(const String& key, const JsonVariant& value) {
        if (!config) {
            config = std::make_unique<JsonDocument>();
        }
        (*config)[key] = value;
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
    bool alwaysInteractive = false; // If true, controls are always enabled (bypassing Settings lock)

    // Context-specific configuration
    // Use pointer to avoid large JsonDocument allocation on stack/heap for every context
    // Only allocated when configure() is called - most contexts don't need custom config
    std::unique_ptr<JsonDocument> contextConfig;  // Custom presentation config (optional)

    // Constructors
    WebUIContext() = default;

    WebUIContext(const String& id, const String& t, const String& ic,
                 WebUILocation loc, WebUIPresentation pres = WebUIPresentation::Card)
        : contextId(id), title(t), icon(ic), location(loc), presentation(pres) {}

    // Copy constructor - deep copy of contextConfig if present
    WebUIContext(const WebUIContext& other)
        : contextId(other.contextId), title(other.title), icon(other.icon),
          location(other.location), presentation(other.presentation), priority(other.priority),
          customHtml(other.customHtml), customCss(other.customCss), customJs(other.customJs),
          fields(other.fields), apiEndpoint(other.apiEndpoint), realTime(other.realTime),
          updateInterval(other.updateInterval), alwaysInteractive(other.alwaysInteractive) {
        if (other.contextConfig) {
            contextConfig = std::make_unique<JsonDocument>(*other.contextConfig);
        }
    }

    // Copy assignment
    WebUIContext& operator=(const WebUIContext& other) {
        if (this != &other) {
            contextId = other.contextId;
            title = other.title;
            icon = other.icon;
            location = other.location;
            presentation = other.presentation;
            priority = other.priority;
            customHtml = other.customHtml;
            customCss = other.customCss;
            customJs = other.customJs;
            fields = other.fields;
            apiEndpoint = other.apiEndpoint;
            realTime = other.realTime;
            updateInterval = other.updateInterval;
            alwaysInteractive = other.alwaysInteractive;
            if (other.contextConfig) {
                contextConfig = std::make_unique<JsonDocument>(*other.contextConfig);
            } else {
                contextConfig.reset();
            }
        }
        return *this;
    }

    // Move constructor and assignment are defaulted (unique_ptr handles move correctly)
    WebUIContext(WebUIContext&&) = default;
    WebUIContext& operator=(WebUIContext&&) = default;
    
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

    WebUIContext& withAlwaysInteractive(bool interactive = true) {
        alwaysInteractive = interactive;
        return *this;
    }
    
    WebUIContext& withPriority(int p) {
        priority = p;
        return *this;
    }

    WebUIContext& configure(const String& key, const JsonVariant& value) {
        if (!contextConfig) {
            contextConfig = std::make_unique<JsonDocument>();
        }
        (*contextConfig)[key] = value;
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
    
    static WebUIContext statusBadge(const String& id, const String& title, const String& icon = "dc-info") {
        WebUIContext ctx(id, title, icon, WebUILocation::HeaderStatus, WebUIPresentation::StatusBadge);
        
        // Add SVG rendering by default for custom icons (dc-mqtt, dc-wifi, etc.)
        ctx.withCustomHtml(String(R"(<svg class="icon status-icon" viewBox="0 0 1024 1024"><use href="#)") + icon + R"("/></svg>)");
        ctx.withCustomCss(R"(
            .status-icon {
                width: 1.2rem;
                height: 1.2rem;
                fill: currentColor;
            }
            .status-badge {
                display: flex;
                align-items: center;
                gap: 0.5rem;
            }
        )");
        
        return ctx;
    }
    
    /**
     * Factory for header info items (time, uptime, etc.)
     * Appears in main header as injectable info zone
     */
    static WebUIContext headerInfo(const String& id, const String& label, const String& icon = "dc-info") {
        WebUIContext ctx(id, label, icon, WebUILocation::HeaderInfo, WebUIPresentation::Text);
        return ctx;
    }
    
    static WebUIContext graph(const String& id, const String& title, const String& icon = "dc-chart") {
        return WebUIContext(id, title, icon, WebUILocation::ComponentDetail, WebUIPresentation::Graph);
    }
    
    static WebUIContext quickControl(const String& id, const String& title, const String& icon = "dc-settings") {
        return WebUIContext(id, title, icon, WebUILocation::QuickControls, WebUIPresentation::Toggle);
    }
    
    static WebUIContext settings(const String& id, const String& title, const String& icon = "dc-cog") {
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
    
    /**
     * Check if context data has changed since last query (for delta updates)
     * @param contextId The context identifier
     * @return true if data has changed and should be sent to clients
     * @note Default implementation always returns true (always send updates)
     *       Override to optimize bandwidth by only sending when data changes
     */
    virtual bool hasDataChanged(const String& contextId) { return true; }

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
    
    /**
     * Iterate over contexts without creating temporary vector
     * @param callback Function called for each context, return false to stop
     */
    virtual void forEachContext(std::function<bool(const WebUIContext&)> callback) {
        auto contexts = getWebUIContexts();
        for (const auto& ctx : contexts) {
            if (!callback(ctx)) break;
        }
    }
    
    /**
     * Get number of contexts
     */
    virtual size_t getContextCount() {
        return getWebUIContexts().size();
    }
    
    /**
     * Get context at specific index
     * @param index Context index
     * @param outContext Output context
     * @return true if found
     */
    virtual bool getContextAt(size_t index, WebUIContext& outContext) {
        auto contexts = getWebUIContexts();
        if (index < contexts.size()) {
            outContext = contexts[index];
            return true;
        }
        return false;
    }
};

/**
 * @brief Base class for WebUI providers that caches contexts
 * 
 * This class prevents memory leaks by caching WebUIContext objects after
 * their initial creation. Subclasses implement buildContexts() instead of
 * getWebUIContexts().
 * 
 * IMPORTANT: On ESP8266 with ~30KB free heap, repeated context creation
 * causes heap fragmentation. This class ensures contexts are built once
 * and reused.
 * 
 * Usage:
 * @code
 * class MyWebUI : public CachingWebUIProvider {
 * protected:
 *     void buildContexts(std::vector<WebUIContext>& contexts) override {
 *         contexts.push_back(WebUIContext::dashboard("my_ctx", "My Context"));
 *     }
 * public:
 *     String getWebUIName() const override { return "MyComponent"; }
 *     String getWebUIVersion() const override { return "1.0.0"; }
 *     String handleWebUIRequest(...) override { return "{}"; }
 * };
 * @endcode
 */
class CachingWebUIProvider : public IWebUIProvider {
protected:
    mutable std::vector<WebUIContext> cachedContexts_;
    mutable bool contextsCached_ = false;

    /**
     * Build contexts - called once, results are cached.
     * Subclasses implement this instead of getWebUIContexts().
     * @param contexts Vector to populate with contexts
     */
    virtual void buildContexts(std::vector<WebUIContext>& contexts) = 0;

    /**
     * Ensure contexts are cached
     */
    void ensureContextsCached() const {
        if (!contextsCached_) {
            auto* self = const_cast<CachingWebUIProvider*>(this);
            self->buildContexts(self->cachedContexts_);
            self->contextsCached_ = true;
        }
    }

public:
    /**
     * Invalidate cached contexts (call when config changes)
     */
    void invalidateContextCache() {
        cachedContexts_.clear();
        contextsCached_ = false;
    }

    // IWebUIProvider implementation with caching

    std::vector<WebUIContext> getWebUIContexts() override {
        ensureContextsCached();
        return cachedContexts_;
    }

    void forEachContext(std::function<bool(const WebUIContext&)> callback) override {
        ensureContextsCached();
        for (const auto& ctx : cachedContexts_) {
            if (!callback(ctx)) break;
        }
    }

    size_t getContextCount() override {
        ensureContextsCached();
        return cachedContexts_.size();
    }

    bool getContextAt(size_t index, WebUIContext& outContext) override {
        ensureContextsCached();
        if (index < cachedContexts_.size()) {
            outContext = cachedContexts_[index];
            return true;
        }
        return false;
    }

    WebUIContext getWebUIContext(const String& contextId) override {
        ensureContextsCached();
        for (const auto& ctx : cachedContexts_) {
            if (ctx.contextId == contextId) {
                return ctx;
            }
        }
        return WebUIContext();
    }
};

} // namespace Components
} // namespace DomoticsCore
