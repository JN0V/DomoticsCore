<!-- workline
sources: [DomoticsCore-Core/include/DomoticsCore/Core.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h, DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h, DomoticsCore-Core/include/DomoticsCore/ComponentConfig.h, DomoticsCore-Core/include/DomoticsCore/EventBus.h, DomoticsCore-Core/include/DomoticsCore/Events.h, DomoticsCore-Core/include/DomoticsCore/Platform_HAL.h, DomoticsCore-Core/include/DomoticsCore/Platform_Arduino.h, DomoticsCore-Core/include/DomoticsCore/Platform_Stub.h, DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker_Native.h, DomoticsCore-Core/src/Core.cpp]
checked: e6ad62e
verified: agent:claude-code
-->
# DomoticsCore-Core -- Common Modification Patterns and Testing Notes

Part of the [DomoticsCore-Core Project Context](project-context.md) (AI agent reference).

## 8. Common Modification Patterns

### Adding a New Core Lifecycle Event

1. Open `DomoticsCore-Core/include/DomoticsCore/Events.h`.
2. Add a new `static constexpr const char*` constant in the `DomoticsCore::Events` namespace:
   ```cpp
   static constexpr const char* EVENT_MY_EVENT = "my/event";
   ```
3. Publish it at the appropriate point in `ComponentRegistry::initializeAll()`, `shutdownAll()`, or `Core::begin()`.
4. Write tests verifying the event is published with the correct payload.

### Adding a New Method to IComponent

1. Add the virtual method declaration to `IComponent` in `IComponent.h` with a default implementation (to avoid breaking all existing components).
2. If it needs to be called by the registry, add the call site in `ComponentRegistry::initializeAll()` or `loopAll()`.
3. Update tests for both the base class default and at least one concrete override.
4. Update this documentation.

### Adding a New HAL Function

1. If the function is common to Arduino platforms (ESP32 + ESP8266), implement it once in `Platform_Arduino.h` in the `DomoticsCore::HAL::Platform` namespace.
2. If the function is platform-specific (e.g., uses `esp_*` API), implement it separately in `Platform_ESP32.h` and `Platform_ESP8266.h`.
3. Always add a stub in `Platform_Stub.h`.
4. Add a forwarding inline function in `Platform_HAL.h` in the `DomoticsCore::HAL` namespace.
5. Never add `#ifdef` outside of HAL files.
6. Write native tests using the stub implementation.

### Creating a New Component (Consumer of Core)

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

class MyComponent : public DomoticsCore::Components::IComponent {
public:
    MyComponent() {
        metadata = {"MyComponent", "1.0.0", "Author", "Description"};
    }

    std::vector<Dependency> getDependencies() const override {
        return {
            {"Storage", false},  // optional
            {"MQTT", true}       // required
        };
    }

    ComponentStatus begin() override {
        // Internal init ONLY
        return ComponentStatus::Success;
    }

    void afterAllComponentsReady() override {
        // Safe to access dependencies here
        mqtt_ = getCore()->getComponent<MQTTComponent>("MQTT");
    }

    void loop() override {
        // Non-blocking work
    }

    ComponentStatus shutdown() override {
        return ComponentStatus::Success;
    }
};
```

Register it:
```cpp
core.addComponent(std::make_unique<MyComponent>());
```

---

## 9. Testing Notes

- Core tests use the `native` PlatformIO environment with `Platform_Stub.h` providing mock HAL functions.
- `HeapTracker_Native.h` uses `mallinfo()` for real heap tracking in native tests.
- Event handler tests should call `eventBus.poll()` explicitly to process queued events (no automatic dispatch in tests).
- Component lifecycle tests should verify the full sequence: register -> begin -> loop -> shutdown, and check that `isActive()` transitions correctly.
- Dependency cycle tests should verify that `resolveDependencies()` returns `false` and logs an error.
