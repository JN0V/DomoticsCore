# Component Lifecycle

This document describes the lifecycle of DomoticsCore components.

## Lifecycle States

```
  ┌─────────────────┐
  │    Created      │  Component instantiated (std::make_unique<T>())
  └───────┬─────────┘
          │ core.addComponent()
          ▼
  ┌─────────────────┐
  │   Registered    │  Added to ComponentRegistry, registry injected
  └───────┬─────────┘
          │ Core::begin()
          ▼
  ┌─────────────────┐
  │    Sorted       │  Dependencies resolved via topological sort (Kahn's algorithm)
  └───────┬─────────┘
          │ EventBus + Core injected, then component->begin()
          ▼
  ┌─────────────────┐
  │    Active       │  setActive(true), EVENT_COMPONENT_READY published
  └───────┬─────────┘
          │ All components initialized
          ▼
  ┌─────────────────┐
  │ ComponentsReady │  onComponentsReady() called on each component
  └───────┬─────────┘
          │
          ▼
  ┌─────────────────┐
  │ AllReady        │  afterAllComponentsReady() called on each component
  └───────┬─────────┘
          │ Core::loop() drives component->loop() + eventBus.poll()
          ▼
  ┌─────────────────┐
  │   Running       │  loop() called each cycle for active components
  └───────┬─────────┘
          │ Core::shutdown()
          ▼
  ┌─────────────────┐
  │   Shutdown      │  component->shutdown() called in reverse order
  └─────────────────┘  EventBus subscriptions cleaned up
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
std::vector<Dependency> getDependencies() const override {
    return {{"Storage", false}, {"Wifi", false}};  // Optional dependencies
}
```

The `ComponentRegistry` uses topological sorting to determine initialization order.

## Lifecycle Events

Core lifecycle events (defined in `Events.h`):

| Event Constant | Topic | When Published |
|---------------|-------|----------------|
| `EVENT_COMPONENT_READY` | `component/ready` | After each successful `begin()` |
| `EVENT_COMPONENT_ERROR` | `component/error` | On component initialization failure |
| `EVENT_SYSTEM_READY` | `system/ready` | After ALL components initialized |
| `EVENT_SYSTEM_REBOOT` | `system/reboot` | Before system reboot |
| `EVENT_SHUTDOWN_START` | `shutdown/start` | Before shutdown begins |

Component-specific events (defined in respective `*Events.h` files):

| Event | Topic | When Published |
|-------|-------|----------------|
| Storage ready | `storage/ready` | When StorageComponent is initialized |
| Network ready | `network/ready` | When WifiComponent has connectivity |
| MQTT connected | `mqtt/connected` | After MQTT broker connection |

## Post-Initialization Hooks

After all `begin()` calls complete, the registry invokes two additional hooks in order:

1. **`onComponentsReady(registry)`** -- Components can discover other components in the registry
2. **`afterAllComponentsReady()`** -- Safe to access all dependencies (guaranteed available if declared in `getDependencies()`)

## Example

```cpp
class MyComponent : public IComponent {
public:
    MyComponent() {
        metadata.name = "MyComponent";
        metadata.version = "1.0.0";
    }

    std::vector<Dependency> getDependencies() const override {
        return {{"Storage", false}};  // Optional dependency
    }

    ComponentStatus begin() override {
        // Internal initialization only (GPIO, state, etc.)
        // Do NOT access other components here -- use afterAllComponentsReady()
        if (initFailed) {
            return ComponentStatus::HardwareError;
        }
        // Note: EVENT_COMPONENT_READY is published automatically by ComponentRegistry
        return ComponentStatus::Success;
    }

    void afterAllComponentsReady() override {
        // All dependencies guaranteed available here
        storage_ = getCore()->getComponent<StorageComponent>("Storage");
    }

    void loop() override {
        // Non-blocking work (< 10ms)
    }

    ComponentStatus shutdown() override {
        // Cleanup resources
        // Note: EventBus subscriptions are cleaned up automatically
        return ComponentStatus::Success;
    }
};
```
