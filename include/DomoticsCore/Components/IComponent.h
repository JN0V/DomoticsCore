#pragma once

#include <Arduino.h>
#include <vector>

namespace DomoticsCore {
namespace Components {

/**
 * Base interface for all DomoticsCore components
 * Components are modular, optional features that can be added to Core
 */
class IComponent {
public:
    virtual ~IComponent() = default;
    
    /**
     * Initialize the component
     * Called during Core.begin() after dependencies are resolved
     * @return true if initialization successful, false otherwise
     */
    virtual bool begin() = 0;
    
    /**
     * Component main loop
     * Called during Core.loop() for active components
     */
    virtual void loop() = 0;
    
    /**
     * Shutdown the component
     * Called during Core.shutdown() or component removal
     */
    virtual void shutdown() = 0;
    
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
     * Get component version for compatibility checking
     * @return Version string (e.g., "1.0.0")
     */
    virtual String getVersion() const { 
        return "1.0.0"; 
    }

protected:
    bool active = false;
    
    /**
     * Mark component as active (called by ComponentRegistry)
     */
    void setActive(bool state) { 
        active = state; 
    }
    
    friend class ComponentRegistry;
};

} // namespace Components
} // namespace DomoticsCore
