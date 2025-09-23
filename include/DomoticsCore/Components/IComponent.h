#pragma once

#include <Arduino.h>
#include <vector>
#include <memory>
#include "ComponentConfig.h"
#include "../Utils/EventBus.h"

// Forward declarations to avoid circular includes
namespace DomoticsCore { namespace Components { class ComponentRegistry; class IWebUIProvider; } }

namespace DomoticsCore {
namespace Components {

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
     * Get list of component dependencies
     * Dependencies will be initialized before this component
     * @return Vector of dependency component names
     */
    virtual std::vector<String> getDependencies() const { 
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

protected:
    /**
     * Set component status and update internal state
     */
    void setStatus(ComponentStatus status) {
        lastStatus = status;
        if (status != ComponentStatus::Success) {
            active = false;
        }
    }
    /**
     * Framework-injected access to EventBus. Valid after begin().
     */
    DomoticsCore::Utils::EventBus& eventBus() {
        return *__dc_eventBus;
    }
    
    /**
     * Mark component as active (called by ComponentRegistry)
     */
    void setActive(bool state) { 
        active = state; 
    }
    
    friend class ComponentRegistry;
    // Called by ComponentRegistry prior to begin()
    void __dc_setEventBus(DomoticsCore::Utils::EventBus* eb) { __dc_eventBus = eb; }
};

} // namespace Components
} // namespace DomoticsCore
