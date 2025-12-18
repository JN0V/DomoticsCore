# Component Lifecycle

This document describes the lifecycle of DomoticsCore components.

## Lifecycle States

```
  ┌─────────────┐
  │  Created    │  Component instantiated
  └─────┬───────┘
        │ addComponent()
        ▼
  ┌─────────────┐
  │ Registered  │  Added to ComponentRegistry
  └─────┬───────┘
        │ Core::begin()
        ▼
  ┌─────────────┐
  │   Sorted    │  Dependencies resolved via topological sort
  └─────┬───────┘
        │ component->begin()
        ▼
  ┌─────────────┐
  │   Active    │  Running, loop() called each cycle
  └─────┬───────┘
        │ Core::shutdown()
        ▼
  ┌─────────────┐
  │  Shutdown   │  component->shutdown() called
  └─────────────┘
```

## Lifecycle Methods

### `begin()`
Called once when `Core::begin()` executes. Components are initialized in dependency order.

**Return**: `ComponentStatus` indicating success or failure type.

### `loop()`
Called repeatedly by `Core::loop()`. Must be non-blocking (< 10ms).

### `shutdown()`
Called when `Core::shutdown()` executes. Components shutdown in reverse dependency order.

## Dependency Resolution

Components declare dependencies via `getDependencies()`:

```cpp
std::vector<String> getDependencies() const override {
    return {"StorageComponent", "WifiComponent"};
}
```

The `ComponentRegistry` uses topological sorting to determine initialization order.

## Lifecycle Events

| Event | When Published |
|-------|----------------|
| `EVENT_COMPONENT_READY` | After successful `begin()` |
| `EVENT_SYSTEM_READY` | After all components initialized |
| `EVENT_STORAGE_READY` | When StorageComponent ready |
| `EVENT_NETWORK_READY` | When WifiComponent connected |
| `EVENT_SHUTDOWN_START` | Before shutdown begins |

## Example

```cpp
class MyComponent : public IComponent {
public:
    ComponentStatus begin() override {
        // Initialize hardware/state
        if (initFailed) {
            return ComponentStatus::HardwareError;
        }
        
        // Publish ready event
        if (__dc_eventBus) {
            __dc_eventBus->publish(EVENT_COMPONENT_READY, metadata.name);
        }
        
        return ComponentStatus::Success;
    }
    
    void loop() override {
        // Non-blocking work (< 10ms)
    }
    
    ComponentStatus shutdown() override {
        // Cleanup resources
        return ComponentStatus::Success;
    }
};
```
