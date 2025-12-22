#pragma once

#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include "IComponent.h"
#include "ComponentConfig.h"
#include "Logger.h"
#include "EventBus.h"
#include "Events.h"

namespace DomoticsCore {
    class Core; // Forward declaration
}

namespace DomoticsCore {
namespace Components {

/**
 * Component registry for managing component lifecycle and dependencies
 * Handles registration, dependency resolution, and coordinated initialization
 */
class ComponentRegistry {
public:
    // Forward declaration of nested listener interface
    class IComponentLifecycleListener;

private:
    std::vector<std::unique_ptr<IComponent>> components;
    std::map<String, IComponent*> componentMap;
    std::vector<IComponent*> initializationOrder;
    bool initialized = false;
    // Lifecycle listeners
    std::vector<IComponentLifecycleListener*> listeners;
    // Event bus for cross-component communication
    DomoticsCore::Utils::EventBus eventBus;
    // Core reference for automatic injection to components
    DomoticsCore::Core* core_ = nullptr;
    
    
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
        
        String name = component->metadata.name;
        if (componentMap.find(name) != componentMap.end()) {
            DLOG_E(LOG_CORE, "Component '%s' already registered", name.c_str());
            return false;
        }
        
        IComponent* ptr = component.get();
        
        // Inject registry immediately for lazy Core access
        ptr->__dc_setRegistry(this);
        
        componentMap[name] = ptr;
        components.push_back(std::move(component));
        
        DLOG_I(LOG_CORE, "Registered component: %s v%s", 
               name.c_str(), ptr->metadata.version.c_str());
        // Notify listeners about addition
        for (auto* l : listeners) {
            if (l) l->onComponentAdded(ptr);
        }
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
        
        // Initialize components in dependency order
        for (auto* component : initializationOrder) {
            // Skip if already initialized (e.g., early init by System)
            if (component->getLastStatus() == ComponentStatus::Success && component->isActive()) {
                DLOG_I(LOG_CORE, "Component already initialized, skipping: %s", component->metadata.name.c_str());
                continue;
            }
            
            DLOG_I(LOG_CORE, "Initializing component: %s", component->metadata.name.c_str());
            
            // Provide framework services (EventBus, Core) to the component before begin()
            if (component) {
                component->__dc_setEventBus(&eventBus);
                component->__dc_setCore(core_);
            }

            ComponentStatus status = component->begin();
            if (status != ComponentStatus::Success) {
                DLOG_E(LOG_CORE, "Failed to initialize component %s: %s", 
                       component->metadata.name.c_str(), statusToString(status));
                return status;
            }
            
            component->setActive(true);
            DLOG_I(LOG_CORE, "Component initialized: %s", component->metadata.name.c_str());

            // Publish component ready event
            eventBus.publish(Events::EVENT_COMPONENT_READY, component->metadata.name);
        }

        initialized = true;
        DLOG_I(LOG_CORE, "All components initialized successfully (%zu components)", initializationOrder.size());

        // Publish system ready event
        eventBus.publish(Events::EVENT_SYSTEM_READY, String(""));

        // Post-initialization hooks for components
        for (auto* component : initializationOrder) {
            component->onComponentsReady(*this);
        }
        
        // v1.1: Late initialization hook - all components guaranteed available
        DLOG_D(LOG_CORE, "Calling afterAllComponentsReady() hooks...");
        for (auto* component : initializationOrder) {
            component->afterAllComponentsReady();
        }
        
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
        // Dispatch queued events
        eventBus.poll();
    }
    
    /**
     * Shutdown all components in reverse order
     */
    void shutdownAll() {
        if (!initialized) return;

        // Publish shutdown start event and dispatch immediately
        // so listeners can react before components are shut down
        eventBus.publish(Events::EVENT_SHUTDOWN_START, String(""));
        eventBus.poll();

        // Shutdown in reverse order
        for (auto it = initializationOrder.rbegin(); it != initializationOrder.rend(); ++it) {
            auto* component = *it;
            if (component->isActive()) {
                DLOG_I(LOG_CORE, "Shutting down component: %s", component->metadata.name.c_str());
                ComponentStatus status = component->shutdown();
                if (status != ComponentStatus::Success) {
                    DLOG_W(LOG_CORE, "Component %s shutdown warning: %s", 
                           component->metadata.name.c_str(), statusToString(status));
                }
                // Clean up EventBus subscriptions for this component
                eventBus.unsubscribeOwner(component);
                component->setActive(false);
            }
        }
        
        initialized = false;
        DLOG_I(LOG_CORE, "All components shut down");
    }

    /**
     * Remove a component by name at runtime. Will shutdown the component and notify listeners.
     */
    bool removeComponent(const String& name) {
        auto itMap = componentMap.find(name);
        if (itMap == componentMap.end()) return false;
        IComponent* comp = itMap->second;
        // Shutdown if active
        if (comp->isActive()) {
            DLOG_I(LOG_CORE, "Shutting down component (remove): %s", name.c_str());
            comp->shutdown();
            // Clean up EventBus subscriptions for this component
            eventBus.unsubscribeOwner(comp);
            comp->setActive(false);
        }
        // Notify listeners
        for (auto* l : listeners) {
            if (l) l->onComponentRemoved(comp);
        }
        // Erase from containers
        componentMap.erase(itMap);
        // Remove from initialization order
        initializationOrder.erase(std::remove(initializationOrder.begin(), initializationOrder.end(), comp), initializationOrder.end());
        // Remove from components vector
        for (auto it = components.begin(); it != components.end(); ++it) {
            if (it->get() == comp) {
                components.erase(it);
                break;
            }
        }
        DLOG_I(LOG_CORE, "Component removed: %s", name.c_str());
        return true;
    }

    /**
     * Return raw pointers to all registered components.
     */
    std::vector<IComponent*> getAllComponents() const {
        std::vector<IComponent*> out;
        out.reserve(components.size());
        for (const auto& up : components) out.push_back(up.get());
        return out;
    }
    
    /**
     * Set Core reference for automatic injection to components
     * Called by Core before initializing components
     */
    void setCore(DomoticsCore::Core* core) {
        core_ = core;
    }

public:
    /**
     * Listener interface to observe component lifecycle events.
     */
    class IComponentLifecycleListener {
    public:
        virtual ~IComponentLifecycleListener() = default;
        virtual void onComponentAdded(IComponent* /*comp*/) {}
        virtual void onComponentRemoved(IComponent* /*comp*/) {}
    };

    void addListener(IComponentLifecycleListener* listener) {
        if (listener) listeners.push_back(listener);
    }
    
    void removeListener(IComponentLifecycleListener* listener) {
        listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
    }
    
    /**
     * @brief Get the EventBus for event-driven orchestration
     */
    DomoticsCore::Utils::EventBus& getEventBus() {
        return eventBus;
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
    
    /**
     * Get Core reference for lazy injection
     * Used by IComponent::getCore() for lazy injection
     * @return Pointer to Core, or nullptr if not set
     */
    DomoticsCore::Core* getCore() const {
        return core_;
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
            String name = comp->metadata.name;
            inDegree[name] = 0;
            dependents[name] = {};
        }
        
        // Build dependency graph
        for (const auto& comp : components) {
            String name = comp->metadata.name;
            auto deps = comp->getDependencies();
            
            for (const auto& dep : deps) {
                if (componentMap.find(dep.name) == componentMap.end()) {
                    if (dep.required) {
                        DLOG_E(LOG_CORE, "Component '%s' depends on unregistered required component '%s'", 
                               name.c_str(), dep.name.c_str());
                        return false;
                    } else {
                        DLOG_I(LOG_CORE, "Component '%s' optional dependency '%s' not available (OK)", 
                               name.c_str(), dep.name.c_str());
                        continue;  // Skip optional missing dependency
                    }
                }
                
                dependents[dep.name].push_back(name);
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
