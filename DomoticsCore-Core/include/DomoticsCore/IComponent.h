#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>
#include <memory>
#include "ComponentConfig.h"
#include "EventBus.h"

// Forward declarations to avoid circular includes
namespace DomoticsCore { 
    class Core;
    namespace Components { 
        class ComponentRegistry; 
        class IWebUIProvider; 
    } 
}

namespace DomoticsCore {
namespace Components {

/**
 * Dependency specification for component initialization ordering
 * Allows declaring both required and optional dependencies
 */
struct Dependency {
    String name;           // Component name
    bool required = true;  // If false, component will init even if dependency missing
    
    // Implicit conversion from String for backward compatibility
    Dependency(const String& n) : name(n), required(true) {}
    Dependency(const String& n, bool req) : name(n), required(req) {}
};

/**
 * Base interface for all DomoticsCore components
 * Provides lifecycle management, dependency resolution, and status reporting
 */
class IComponent {
protected:
    bool active = false;
    ComponentStatus lastStatus = ComponentStatus::Success;
    ComponentConfig config;
    ComponentMetadata metadata;
    // Set by ComponentRegistry before begin(); framework services
    DomoticsCore::Utils::EventBus* __dc_eventBus = nullptr; // preferred injection
    mutable DomoticsCore::Core* __dc_core = nullptr; // automatic Core injection (lazy)
    mutable DomoticsCore::Components::ComponentRegistry* __dc_registry = nullptr; // for lazy Core injection
    
public:
    virtual ~IComponent() = default;
    
    /**
     * Initialize the component
     * Called during Core.begin() after dependencies are resolved
     * @return ComponentStatus indicating success or specific error
     */
    virtual ComponentStatus begin() = 0;
    
    /**
     * Component main loop
     * Called during Core.loop() for active components
     */
    virtual void loop() = 0;
    
    /**
     * Shutdown the component
     * Called during Core.shutdown() or component removal
     * @return ComponentStatus indicating success or specific error
     */
    virtual ComponentStatus shutdown() = 0;
    
    /**
     * Get component name for identification and logging
     * @return Unique component name
     */
    virtual String getName() const = 0;
    
    /**
     * Get list of component dependencies with optional/required flags
     * Dependencies will be initialized before this component
     * @return Vector of Dependency objects with name and required flag
     * 
     * Examples:
     * ```cpp
     * // All required (default)
     * std::vector<Dependency> getDependencies() const override {
     *     return {"ComponentA", "ComponentB"};  // Implicit required=true
     * }
     * 
     * // Mix of required and optional
     * std::vector<Dependency> getDependencies() const override {
     *     return {
     *         {"Storage", false},      // Optional - won't fail if missing
     *         {"MQTT", false},         // Optional
     *         {"MyCustomComp", true}   // Required - init fails if missing
     *     };
     * }
     * ```
     */
    virtual std::vector<Dependency> getDependencies() const {
        return {};
    }
    
    /**
     * Check if component is currently active/running
     * @return true if active, false otherwise
     */
    virtual bool isActive() const { 
        return active; 
    }
    
    /**
     * Get last component status
     * @return Last ComponentStatus from lifecycle operations
     */
    virtual ComponentStatus getLastStatus() const {
        return lastStatus;
    }
    
    /**
     * Get component metadata
     * @return ComponentMetadata with version, author, description
     */
    virtual const ComponentMetadata& getMetadata() const {
        return metadata;
    }
    
    /**
     * Get component configuration
     * @return ComponentConfig for parameter access
     */
    virtual ComponentConfig& getConfig() {
        return config;
    }
    
    /**
     * Get component configuration (const)
     * @return ComponentConfig for parameter access
     */
    virtual const ComponentConfig& getConfig() const {
        return config;
    }
    
    /**
     * Validate component configuration
     * @return ValidationResult with detailed error information
     */
    virtual ValidationResult validateConfig() const {
        return config.validate();
    }
    
    /**
     * Get component version for compatibility checking
     * @return Version string (e.g., "1.0.0")
     */
    virtual String getVersion() const { 
        return metadata.version.isEmpty() ? "1.0.0" : metadata.version; 
    }

    /**
     * Optional: Stable type key to identify component kind (e.g., "system_info").
     * Used by WebUI to attach composition-based UI wrappers automatically.
     */
    virtual const char* getTypeKey() const { return ""; }

    /**
     * Optional: If this component also provides a WebUI, return the provider pointer.
     * Default returns nullptr (no WebUI).
     */
    virtual IWebUIProvider* getWebUIProvider() { return nullptr; }

    /**
     * Optional: Called by the registry after all components have been initialized.
     * Components may perform cross-component discovery here.
     */
    virtual void onComponentsReady(const ComponentRegistry& /*registry*/) {}
    
    /**
     * Optional: Called after ALL components (including built-ins) are ready
     * Use this for late initialization that depends on other components
     * All components declared in getDependenciesEx() are guaranteed available here
     * 
     * Lifecycle order:
     * 1. begin() - Internal initialization only (GPIO, state, etc.)
     * 2. afterAllComponentsReady() - Dependency setup (can access other components safely)
     * 3. loop() - Normal operation
     * 
     * Example:
     * ```cpp
     * ComponentStatus begin() override {
     *     pinMode(PIN, INPUT);  // Internal init only
     *     return ComponentStatus::Success;
     * }
     * 
     * void afterAllComponentsReady() override {
     *     // All components guaranteed available here
     *     storage_ = getCore()->getComponent<StorageComponent>("Storage");
     *     mqtt_ = getCore()->getComponent<MQTTComponent>("MQTT");
     *     // No null checks needed if declared in getDependenciesEx()
     * }
     * ```
     */
    virtual void afterAllComponentsReady() {}
    
    /**
     * Get access to the Core instance (injected automatically by framework)
     * Uses lazy injection - works even if component is registered after begin()
     * @return Pointer to Core, or nullptr if not available
     */
    DomoticsCore::Core* getCore() const;

protected:
    /**
     * Set component status and update internal state
     */
    void setStatus(ComponentStatus status) {
        lastStatus = status;
        // Components can override this for custom behavior
    }
    
public:
    /**
     * Mark component as active (can be called by ComponentRegistry or early init)
     */
    void setActive(bool state) { 
        active = state; 
    }
    
protected:
    
    friend class ComponentRegistry;
    // Called by ComponentRegistry prior to begin()
    void __dc_setEventBus(DomoticsCore::Utils::EventBus* eb) { __dc_eventBus = eb; }
    void __dc_setCore(DomoticsCore::Core* core) { __dc_core = core; }
    void __dc_setRegistry(DomoticsCore::Components::ComponentRegistry* reg) { __dc_registry = reg; }

public:
    // Typed helper: subscribe to a topic and receive a const T& payload. Owner is this component by default.
    template<typename T>
    uint32_t on(const String& topic, std::function<void(const T&)> cb, bool replayLast = false) {
        if (!__dc_eventBus || topic.length() == 0 || !cb) return 0;
        return __dc_eventBus->subscribe(topic, [cb](const void* p){
            const T* tp = static_cast<const T*>(p);
            if (tp) cb(*tp);
        }, this, replayLast);
    }

    // Typed helper: publish or publishSticky a payload of type T
    template<typename T>
    void emit(const String& topic, const T& payload, bool sticky = false) {
        if (!__dc_eventBus || topic.length() == 0) return;
        if (sticky) __dc_eventBus->publishSticky(topic, payload);
        else __dc_eventBus->publish(topic, payload);
    }
};

} // namespace Components
} // namespace DomoticsCore
