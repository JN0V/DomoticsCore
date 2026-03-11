# DomoticsCore-Core -- Project Context (AI Agent Reference)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides the context an AI coding agent needs to understand, navigate, and safely modify the DomoticsCore-Core component.

---

## 1. Component Identity

| Field | Value |
|-------|-------|
| **Name** | DomoticsCore-Core |
| **Version** | 1.5.2 (see `library.json`) |
| **Role** | Foundation library -- runtime engine, component model, EventBus, logging, timers, memory management |
| **Namespace** | `DomoticsCore` (top-level), `DomoticsCore::Components`, `DomoticsCore::Utils`, `DomoticsCore::Testing` |
| **License** | MIT |
| **Platforms** | espressif32, espressif8266 (Arduino framework) |
| **Repository** | `https://github.com/JN0V/DomoticsCore.git` |
| **Author** | JN0V |

---

## 2. File Inventory

### Header Files (`DomoticsCore-Core/include/DomoticsCore/`)

| File | Purpose |
|------|---------|
| `Core.h` | `Core` class -- central runtime, owns `ComponentRegistry`, drives lifecycle |
| `IComponent.h` | `IComponent` abstract base -- lifecycle hooks, dependency declaration, EventBus helpers, Core injection |
| `ComponentRegistry.h` | `ComponentRegistry` -- registration, dependency resolution (Kahn's algorithm), coordinated init/loop/shutdown |
| `ComponentConfig.h` | `ComponentStatus` enum, `ComponentMetadata`, `ConfigParam`, `ComponentConfig`, `ValidationResult` |
| `EventBus.h` | `EventBus` -- queued pub/sub with topic strings, wildcard matching, sticky events |
| `Events.h` | Core lifecycle event topic constants (`component/ready`, `system/ready`, `shutdown/start`, etc.) |
| `Logger.h` | Logging macros (`DLOG_E/W/I/D/V`), `LoggerCallbacks`, predefined component tags |
| `Timer.h` | `NonBlockingDelay` utility -- non-blocking interval timer using `HAL::getMillis()` |
| `MemoryManager.h` | `MemoryManager` singleton -- runtime heap profiling, adaptive buffer sizes, feature flags |
| `Platform_HAL.h` | Platform detection routing header -- includes `Platform_ESP32.h`, `Platform_ESP8266.h`, or `Platform_Stub.h` |
| `Platform_ESP32.h` | ESP32-specific HAL implementation |
| `Platform_ESP8266.h` | ESP8266-specific HAL implementation (PROGMEM-optimized logging) |
| `Platform_Stub.h` | Native/test stub implementation |
| `Platform_Arduino.h` | Shared Arduino utilities (time, string, GPIO, math, logging) included by `Platform_ESP32.h` and `Platform_ESP8266.h` to avoid code duplication |
| `Filesystem_HAL.h` | Filesystem HAL routing header |
| `Filesystem_ESP32.h` | ESP32 filesystem implementation |
| `Filesystem_ESP8266.h` | ESP8266 filesystem implementation |
| `Filesystem_Stub.h` | Native/test filesystem stub |
| `ArduinoJsonString.h` | ArduinoJson 7 converter functions for stub `String` class (native tests only; no-op on Arduino platforms) |
| `DocMainpage.h` | Doxygen main page documentation |
| `Testing.h` | Umbrella header for testing utilities (currently includes HeapTracker; StabilityTestRunner planned) |
| `Testing/HeapTracker.h` | `HeapTracker` class -- checkpoint-based heap monitoring for leak detection |
| `Testing/HeapTracker_HAL.h` | HeapTracker platform routing |
| `Testing/HeapTracker_Native.h` | Native platform heap tracking (mallinfo) |
| `Testing/HeapTracker_ESP32.h` | ESP32 heap tracking (heap_caps API) |
| `Testing/HeapTracker_ESP8266.h` | ESP8266 heap tracking (ESP.getFreeHeap) |

### Source Files (`DomoticsCore-Core/src/`)

| File | Purpose |
|------|---------|
| `Core.cpp` | `Core` implementation -- `begin()`, `loop()`, `shutdown()` |
| `IComponent.cpp` | `IComponent::getCore()` lazy injection implementation |

### Other Files

| File | Purpose |
|------|---------|
| `library.json` | PlatformIO library manifest |
| `README.md` | Component README |
| `LOGGING.md` | Comprehensive logging system guide |

---

## 3. Key Classes and Their Responsibilities

| Class | Responsibility | SRP Summary |
|-------|---------------|-------------|
| `Core` | Application entry point; owns registry; drives lifecycle | "Orchestrate the application" |
| `IComponent` | Abstract base for all components; defines lifecycle contract | "Define what a component is" |
| `ComponentRegistry` | Registration, dependency resolution, coordinated init/loop/shutdown | "Manage component collection and ordering" |
| `EventBus` | Queued publish/subscribe messaging | "Deliver messages between decoupled parties" |
| `ComponentConfig` | Typed configuration parameters with validation | "Define and validate configuration" |
| `MemoryManager` | Runtime memory profiling and adaptive sizing | "Adapt to available memory" |
| `NonBlockingDelay` | Interval timing without blocking | "Schedule periodic work" |
| `HeapTracker` | Checkpoint-based heap monitoring for tests | "Detect memory leaks" |
| `NativeAllocTracker` | Per-allocation heap tracking for native tests (singleton) | "Track individual allocations for leak diagnosis" |
| `ScopedAllocTracking` | RAII helper that enables/disables `NativeAllocTracker` around a test | "Scope allocation tracking to a test" |
| `AllocationRecord` | Record of a single allocation (ptr, size, source location, freed flag) | "Describe one allocation event" |
| `LoggerCallbacks` | Broadcast log messages to registered listeners (note: `removeCallback` currently clears ALL callbacks) | "Route log output" |
| `MemoryThresholds` | Configurable heap thresholds for profile detection | "Define profile boundaries" |
| `ProfileBufferSizes` | Buffer size configuration per profile | "Size buffers per profile" |
| `ProfileIntervals` | Timing intervals per profile (WS update, heap check) | "Configure profile-specific timing" |
| `ProfileLimits` | Resource limits per profile (WS clients, providers, chart points) | "Constrain resources per profile" |
| `HeapCheckpoint` | Named checkpoint wrapping a `HeapSnapshot` | "Label a heap measurement" |
| `MemoryTestResult` | Pass/fail result from heap stability assertions | "Report heap test outcome" |

---

## 4. Dependencies

**DomoticsCore-Core has ZERO library dependencies.** It is the foundational layer. It requires only the Arduino-ESP32 or Arduino-ESP8266 toolchain.

---

## 5. Dependents (Components That Depend on Core)

Every DomoticsCore component depends on Core. Based on `library.json` dependency declarations and `#include` usage:

| Component | Declared in library.json | Notes |
|-----------|------------------------|-------|
| DomoticsCore-MQTT | Yes (`>=1.0.0`) | Uses IComponent, EventBus, Logger |
| DomoticsCore-HomeAssistant | Yes (`>=1.4.0`) | Uses IComponent, EventBus, Core injection |
| DomoticsCore-LED | Yes | Uses IComponent, NonBlockingDelay |
| DomoticsCore-NTP | Yes (`>=1.0.0`) | Uses IComponent, EventBus |
| DomoticsCore-System | Yes (`>=1.0.0`) | Uses IComponent, EventBus, ComponentRegistry |
| DomoticsCore-RemoteConsole | Yes (`>=1.4.0`) | Uses IComponent, LoggerCallbacks, EventBus |
| DomoticsCore-Wifi | Implicit (via includes) | Uses IComponent, EventBus, Logger |
| DomoticsCore-Storage | Implicit (via includes) | Uses IComponent, EventBus, Logger |
| DomoticsCore-OTA | Implicit (via includes) | Uses IComponent, EventBus, Logger |
| DomoticsCore-WebUI | Implicit (via includes) | Uses IComponent, ComponentRegistry, IWebUIProvider |
| DomoticsCore-SystemInfo | Implicit (via includes) | Uses IComponent, EventBus, MemoryManager |

**Impact of changes to Core:** Any breaking change to Core's public API affects ALL components. Exercise extreme caution.

---

## 6. Important Conventions and Pitfalls

### Conventions

1. **Include prefix.** Always use `#include <DomoticsCore/Header.h>`, never bare `#include "Header.h"` from outside the library.

2. **Metadata in constructor.** Set `metadata` in the component constructor, not in `begin()`. The registry reads `metadata.name` at registration time.

3. **Internal init in begin(), cross-component in afterAllComponentsReady().** The `begin()` method must only do internal initialization (GPIO, state, timers). Accessing other components must happen in `afterAllComponentsReady()` where all dependencies are guaranteed available.

4. **Declare all dependencies.** Use `getDependencies()` to declare both required and optional dependencies. Never access another component without declaring the dependency.

5. **Sticky events for state.** When publishing state that late subscribers need (e.g., "wifi is connected"), use `publishSticky()` so components starting later can receive the current state.

6. **Subscription ownership.** The `on<T>()` helper on IComponent automatically sets `this` as the subscription owner. This enables automatic cleanup during `shutdown()`.

7. **Event topic naming.** Use hierarchical slash-separated topics: `"component/ready"`, `"wifi/sta/connected"`, `"mqtt/message"`. Constants must be defined in an `Events.h` or `*Events.h` file, not scattered as magic strings.

### Pitfalls

1. **Do NOT subscribe/unsubscribe during EventBus dispatch.** The EventBus asserts single-threaded access. Calling `subscribe()` or `unsubscribe()` inside an event handler will trigger an assert failure. Queue work for the next `loop()` iteration instead.

2. **Do NOT call `begin()` manually on components.** Let `ComponentRegistry::initializeAll()` handle initialization order. Manual early-init is an anti-pattern that breaks dependency resolution.

3. **EventBus queue cap is 32.** If your component publishes many events in a burst, older events will be silently dropped. Design for moderate publish rates.

4. **`poll()` processes max 8 events per call by default.** High-throughput event scenarios may need multiple loop iterations to clear the queue.

5. **`getCore()` returns nullptr before registration.** If you call `getCore()` in the component constructor, it will return `nullptr`. The registry reference is injected during `registerComponent()`, and the Core reference is injected during `initializeAll()`.

6. **HeapTracker checkpoints use `std::map<String, ...>`.** This allocates heap memory, so checkpoint creation itself affects heap measurements. Use coarse-grained checkpoints in real leak tests.

7. **MemoryManager is a singleton.** While the constitution warns against singleton abuse, `MemoryManager` is an intentional exception because memory profile must be globally accessible. Do not create additional global singletons for components.

8. **`static_cast` for component retrieval.** `Core::getComponent<T>(name)` uses `static_cast`, not `dynamic_cast`. If you request the wrong type, you get undefined behavior. Ensure the name matches the expected type.

9. **`Platform_Stub.h` `String` class.** The native stub `String` class mimics Arduino's `String` but has subtle differences (e.g., `toFloat()` uses `std::stof`, which may throw). Always test edge cases on the target platform too.

10. **`LoggerCallbacks` uses ID-based add/remove.** `addCallback()` returns a `CallbackId` (uint8_t) that must be passed to `removeCallback(id)` for targeted removal. This was fixed in v2.0.1 — the previous implementation cleared ALL callbacks on any remove call.

---

## 7. Constitution Compliance Reminders

The following constitution principles are especially relevant to Core development:

### TDD (Section II) -- NON-NEGOTIABLE
- Every new method, branch, and edge case in Core MUST have corresponding tests.
- Core tests run in the `native` environment using `Platform_Stub.h`.
- Use `HeapTracker` in tests to verify heap stability.

### Memory Leak Prevention (Section XIV) -- ABSOLUTE PRIORITY
- Core manages component ownership via `std::unique_ptr` -- never use raw `new` for components.
- `EventBus` copies payloads into `std::vector<uint8_t>` -- be mindful of large payload sizes.
- `ComponentRegistry::removeComponent()` cleans up EventBus subscriptions via `unsubscribeOwner()`.
- After removing items from vectors, call `shrink_to_fit()` per constitution.
- No `String` concatenation in hot paths (e.g., `loop()`, event handlers).

### HAL Isolation (Section IX) -- NON-NEGOTIABLE
- `#ifdef` for platform detection is FORBIDDEN outside `Platform_HAL.h`, `Platform_ESP32.h`, `Platform_ESP8266.h`, `Platform_Stub.h`, and the `Testing/HeapTracker_*.h` files.
- Business logic in `Core.cpp`, `IComponent.cpp`, `ComponentRegistry.h`, `EventBus.h` must be completely platform-agnostic.
- All platform-specific functions are accessed via the `DomoticsCore::HAL::` namespace.

### File Size Limits (Section VII)
- Target: 200-500 lines per file.
- Hard limit: 800 lines.
- `ComponentRegistry.h` is currently ~378 lines. Watch for growth.
- `EventBus.h` is currently ~283 lines.
- `ComponentConfig.h` is currently ~341 lines.
- `MemoryManager.h` is currently ~347 lines.
- `Platform_HAL.h` is currently ~368 lines.
- `Platform_Stub.h` is currently ~630 lines (includes full `String` class stub). Watch the 800-line limit.

### SOLID Principles (Section I)
- `IComponent` is the interface (ISP, DIP).
- `ComponentRegistry` has one job: manage component collection (SRP).
- `EventBus` has one job: message delivery (SRP).
- Keep inheritance depth to 3 levels maximum.

### Non-Blocking Timer Pattern (Section X)
- `delay()` is FORBIDDEN except in boot sequences.
- `loop()` must complete in < 10ms.
- Core's heartbeat uses `HAL::getMillis()` comparison, not `delay()`.

---

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

### Extending the ComponentConfig Validation

1. Add a new `ConfigType` value in `ComponentConfig.h`.
2. Add a private `validateNewType()` method to `ComponentConfig`.
3. Add a case to the `validateParameter()` switch statement.
4. Write tests for valid input, invalid input, and edge cases.

### Adding a New MemoryProfile-Aware Feature

1. Add a new enum value to `Feature` in `MemoryManager.h`.
2. Add the enable/disable logic to `MemoryManager::shouldEnable()`.
3. Use `MemoryManager::instance().shouldEnable(Feature::NewFeature)` in your component.
4. Document the profile thresholds.

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
