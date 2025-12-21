#pragma once

/**
 * @file Core.h
 * @brief Declares the DomoticsCore::Core runtime responsible for component lifecycle and registry.
 */

#include <memory>
#include "Logger.h"
#include "Timer.h"
#include "ComponentRegistry.h"

namespace DomoticsCore {

/**
 * Minimal Core configuration
 */
struct CoreConfig {
    String deviceName = "DomoticsCore";
    String deviceId = "";
    uint8_t logLevel = 3; // INFO level
};

/**
 * @class DomoticsCore::Core
 * @brief Central runtime for the DomoticsCore framework.
 *
 * Manages configuration, logging, and registration/lifecycle of components stored in the
 * `Components::ComponentRegistry`. Provides convenience helpers for accessing components by name
 * or type and drives their `begin()`, `loop()`, and `shutdown()` methods.
 */
class Core {
private:
    CoreConfig config;
    bool initialized;
    Components::ComponentRegistry componentRegistry;
    
public:
    /**
     * @brief Construct a new Core runtime with default configuration.
     */
    Core();
    /**
     * @brief Clean up and ensure `shutdown()` has been called.
     */
    ~Core();
    
    // Core lifecycle
    /**
     * @brief Initialize logging, store configuration, and start all registered components.
     * @param cfg Configuration block (device name, ID, log level).
     * @return true on success, false if any component fails to initialize.
     */
    bool begin(const CoreConfig& cfg = CoreConfig());
    /**
     * @brief Drive the `loop()` method of each registered component.
     */
    void loop();
    /**
     * @brief Stop all components and release resources.
     */
    void shutdown();
    
    // Configuration access
    CoreConfig getConfiguration() const { return config; }
    void setConfiguration(const CoreConfig& cfg) { config = cfg; }
    
    // Device info
    String getDeviceId() const { return config.deviceId; }
    String getDeviceName() const { return config.deviceName; }
    
    // Component management
    /**
     * @brief Register a component with the internal registry.
     * @tparam T Concrete type deriving from `Components::IComponent`.
     * @param component Ownership of the component to register.
     * @return true if registration succeeds, false if a duplicate name exists.
     */
    template<typename T>
    bool addComponent(std::unique_ptr<T> component) {
        static_assert(std::is_base_of<Components::IComponent, T>::value, 
                      "Component must inherit from IComponent");
        return componentRegistry.registerComponent(std::move(component));
    }
    
    /**
     * @brief Fetch a component by name regardless of type.
     * @param name Registered component name.
     * @return Pointer to the component or nullptr if not found.
     */
    Components::IComponent* getComponent(const String& name) {
        return componentRegistry.getComponent(name);
    }
    
    template<typename T>
    /**
     * @brief Fetch a component by name and cast to the desired type.
     * @tparam T Target component type deriving from `IComponent`.
     * @param name Registered component name.
     * @return Pointer to component cast to T or nullptr if not found.
     */
    T* getComponent(const String& name) {
        auto* component = componentRegistry.getComponent(name);
        return component ? static_cast<T*>(component) : nullptr;
    }
    
    /**
     * @brief Current number of registered components.
     */
    size_t getComponentCount() const {
        return componentRegistry.getComponentCount();
    }
    
    // Runtime removal support
    /**
     * @brief Remove a component by name and invoke its `shutdown()`.
     * @param name Registered component name.
     * @return true if a component was removed.
     */
    bool removeComponent(const String& name) {
        return componentRegistry.removeComponent(name);
    }
    
    // Utility functions - convenience factory for timers
    /**
     * @brief Helper to create a `Utils::NonBlockingDelay` with the given interval.
     */
    static Utils::NonBlockingDelay createTimer(unsigned long intervalMs) {
        return Utils::NonBlockingDelay(intervalMs);
    }
    
    /**
     * @brief Get the EventBus for event-driven orchestration
     */
    Utils::EventBus& getEventBus() {
        return componentRegistry.getEventBus();
    }
    
    // EventBus convenience methods (same API as IComponent)
    /**
     * @brief Subscribe to a topic-based event
     * @param topic Event topic string
     * @param handler Callback function
     * @param replayLast If true and sticky event exists, handler called immediately
     * @return Subscription ID
     */
    template<typename PayloadT>
    uint32_t on(const String& topic, std::function<void(const PayloadT&)> handler, bool replayLast = false) {
        auto wrapper = [handler](const void* payload) {
            if (payload) {
                handler(*static_cast<const PayloadT*>(payload));
            }
        };
        return componentRegistry.getEventBus().subscribe(topic, wrapper, this, replayLast);
    }
    
    /**
     * @brief Emit/publish an event on a topic
     * @param topic Event topic string
     * @param payload Event payload data
     */
    template<typename PayloadT>
    void emit(const String& topic, const PayloadT& payload) {
        componentRegistry.getEventBus().publish(topic, payload);
    }
    
    /**
     * @brief Emit/publish an event on a topic without payload
     * @param topic Event topic string
     */
    void emit(const String& topic) {
        componentRegistry.getEventBus().publish(topic);
    }
};

} // namespace DomoticsCore
