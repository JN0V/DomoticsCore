#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include <memory>
#include "IComponent.h"
#include "ComponentConfig.h"
#include "../Logger.h"

namespace DomoticsCore {
namespace Components {

/**
 * Component registry for managing component lifecycle and dependencies
 * Handles registration, dependency resolution, and coordinated initialization
 */
class ComponentRegistry {
private:
    std::vector<std::unique_ptr<IComponent>> components;
    std::map<String, IComponent*> componentMap;
    std::vector<IComponent*> initializationOrder;
    bool initialized = false;
    
public:
    /**
     * Register a component with the registry
     * @param component Unique pointer to component instance
     * @return true if registration successful, false if duplicate name
     */
    bool registerComponent(std::unique_ptr<IComponent> component) {
        if (!component) {
            DLOG_E(LOG_CORE, "Cannot register null component");
            return false;
        }
        
        String name = component->getName();
        if (componentMap.find(name) != componentMap.end()) {
            DLOG_E(LOG_CORE, "Component '%s' already registered", name.c_str());
            return false;
        }
        
        IComponent* ptr = component.get();
        componentMap[name] = ptr;
        components.push_back(std::move(component));
        
        DLOG_I(LOG_CORE, "Registered component: %s v%s", 
               name.c_str(), ptr->getVersion().c_str());
        return true;
    }
    
    /**
     * Initialize all registered components in dependency order
     * @return ComponentStatus indicating overall initialization result
     */
    ComponentStatus initializeAll() {
        if (initialized) {
            DLOG_W(LOG_CORE, "Components already initialized");
            return ComponentStatus::Success;
        }
        
        // Resolve dependency order
        if (!resolveDependencies()) {
            DLOG_E(LOG_CORE, "Failed to resolve component dependencies");
            return ComponentStatus::DependencyError;
        }
        
        // Validate all component configurations first
        for (const auto& component : initializationOrder) {
            auto validation = component->validateConfig();
            if (!validation.isValid()) {
                DLOG_E(LOG_CORE, "Component %s config validation failed: %s", 
                       component->getName().c_str(), validation.toString().c_str());
                return ComponentStatus::ConfigError;
            }
        }
        
        // Initialize components in dependency order
        for (auto* component : initializationOrder) {
            DLOG_I(LOG_CORE, "Initializing component: %s", component->getName().c_str());
            
            ComponentStatus status = component->begin();
            if (status != ComponentStatus::Success) {
                DLOG_E(LOG_CORE, "Failed to initialize component %s: %s", 
                       component->getName().c_str(), statusToString(status));
                return status;
            }
            
            component->setActive(true);
            DLOG_I(LOG_CORE, "Component initialized: %s", component->getName().c_str());
        }
        
        initialized = true;
        DLOG_I(LOG_CORE, "All components initialized successfully (%d components)", initializationOrder.size());
        return ComponentStatus::Success;
    }
    
    /**
     * Call loop() on all active components
     */
    void loopAll() {
        if (!initialized) return;
        
        for (auto* component : initializationOrder) {
            if (component->isActive()) {
                component->loop();
            }
        }
    }
    
    /**
     * Shutdown all components in reverse order
     */
    void shutdownAll() {
        if (!initialized) return;
        
        // Shutdown in reverse order
        for (auto it = initializationOrder.rbegin(); it != initializationOrder.rend(); ++it) {
            auto* component = *it;
            if (component->isActive()) {
                DLOG_I(LOG_CORE, "Shutting down component: %s", component->getName().c_str());
                ComponentStatus status = component->shutdown();
                if (status != ComponentStatus::Success) {
                    DLOG_W(LOG_CORE, "Component %s shutdown warning: %s", 
                           component->getName().c_str(), statusToString(status));
                }
                component->setActive(false);
            }
        }
        
        initialized = false;
        DLOG_I(LOG_CORE, "All components shut down");
    }
    
    /**
     * Get component by name
     * @param name Component name
     * @return Pointer to component or nullptr if not found
     */
    IComponent* getComponent(const String& name) {
        auto it = componentMap.find(name);
        return (it != componentMap.end()) ? it->second : nullptr;
    }
    
    /**
     * Get number of registered components
     * @return Component count
     */
    size_t getComponentCount() const {
        return components.size();
    }
    
    /**
     * Check if components are initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const {
        return initialized;
    }

private:
    /**
     * Resolve component dependencies using topological sort
     * @return true if dependencies resolved successfully, false if circular dependency
     */
    bool resolveDependencies() {
        initializationOrder.clear();
        std::map<String, int> inDegree;
        std::map<String, std::vector<String>> dependents;
        
        // Initialize in-degree count
        for (const auto& comp : components) {
            String name = comp->getName();
            inDegree[name] = 0;
            dependents[name] = {};
        }
        
        // Build dependency graph
        for (const auto& comp : components) {
            String name = comp->getName();
            auto deps = comp->getDependencies();
            
            for (const String& dep : deps) {
                if (componentMap.find(dep) == componentMap.end()) {
                    DLOG_E(LOG_CORE, "Component '%s' depends on unregistered component '%s'", 
                           name.c_str(), dep.c_str());
                    return false;
                }
                
                dependents[dep].push_back(name);
                inDegree[name]++;
            }
        }
        
        // Topological sort using Kahn's algorithm
        std::vector<String> queue;
        for (const auto& pair : inDegree) {
            if (pair.second == 0) {
                queue.push_back(pair.first);
            }
        }
        
        while (!queue.empty()) {
            String current = queue.back();
            queue.pop_back();
            
            initializationOrder.push_back(componentMap[current]);
            
            for (const String& dependent : dependents[current]) {
                inDegree[dependent]--;
                if (inDegree[dependent] == 0) {
                    queue.push_back(dependent);
                }
            }
        }
        
        // Check for circular dependencies
        if (initializationOrder.size() != components.size()) {
            DLOG_E(LOG_CORE, "Circular dependency detected in components");
            return false;
        }
        
        return true;
    }
};

} // namespace Components
} // namespace DomoticsCore
