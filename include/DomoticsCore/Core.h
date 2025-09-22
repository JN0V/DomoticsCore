#pragma once

#include <Arduino.h>
#include <memory>
#include "Logger.h"
#include "Utils/Timer.h"
#include "Components/ComponentRegistry.h"

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
 * DomoticsCore framework with modular component system
 * Provides logging, device configuration, and component management
 */
class Core {
private:
    CoreConfig config;
    bool initialized;
    Components::ComponentRegistry componentRegistry;
    
public:
    Core();
    ~Core();
    
    // Core lifecycle
    bool begin(const CoreConfig& cfg = CoreConfig());
    void loop();
    void shutdown();
    
    // Configuration access
    CoreConfig getConfiguration() const { return config; }
    void setConfiguration(const CoreConfig& cfg) { config = cfg; }
    
    // Device info
    String getDeviceId() const { return config.deviceId; }
    String getDeviceName() const { return config.deviceName; }
    
    // Component management
    template<typename T>
    bool addComponent(std::unique_ptr<T> component) {
        static_assert(std::is_base_of<Components::IComponent, T>::value, 
                      "Component must inherit from IComponent");
        return componentRegistry.registerComponent(std::move(component));
    }
    
    Components::IComponent* getComponent(const String& name) {
        return componentRegistry.getComponent(name);
    }
    
    template<typename T>
    T* getComponent(const String& name) {
        auto* component = componentRegistry.getComponent(name);
        return component ? static_cast<T*>(component) : nullptr;
    }
    
    size_t getComponentCount() const {
        return componentRegistry.getComponentCount();
    }
    
    // Runtime removal support
    bool removeComponent(const String& name) {
        return componentRegistry.removeComponent(name);
    }
    
    // Utility functions - convenience factory for timers
    static Utils::NonBlockingDelay createTimer(unsigned long intervalMs) {
        return Utils::NonBlockingDelay(intervalMs);
    }
};

} // namespace DomoticsCore
