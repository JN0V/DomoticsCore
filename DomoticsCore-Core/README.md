# DomoticsCore-Core

Core runtime for DomoticsCore: component model, registry, lifecycle, configuration, logging, timers, and the event bus.

## Features

- Component base class and lifecycle (`begin`, `loop`, `shutdown`)
- Central `ComponentRegistry` for add/lookup by name and type
- `ComponentConfig` for strongly-typed parameters
- `EventBus` (publish/subscribe) for decoupled communication
- Utilities: `Logger`, `Timer` (non-blocking delay)

## Installation

- Designed for the Arduino-ESP32 toolchain (C++14). No additional libraries beyond the ESP32 core are required.
- Include headers with the prefix `DomoticsCore/` from this package in your PlatformIO project:

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>
```

## Quickstart

```cpp
#include <DomoticsCore/Core.h>
using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core core;

void setup() {
  core.addComponent(std::make_unique<MyComponent>());
  CoreConfig cfg; cfg.deviceName = "MyDevice"; cfg.logLevel = 3;
  core.begin(cfg);
}

void loop() {
  core.loop();
}
```

Main behaviors:
- **Component lifecycle**: each component gets `begin()`, `loop()`, `shutdown()` calls from the `Core`.
- **Dependency resolution**: components declare dependencies via `getDependencies()`, initialized in topological order.
- **Registry**: components can be resolved by type or name via `ComponentRegistry`.
- **Configuration**: components declare typed configuration parameters through `ComponentConfig`.
- **Event bus**: publish/subscribe enables decoupled messaging between components.
- **Lifecycle events**: `EVENT_COMPONENT_READY`, `EVENT_SYSTEM_READY`, `EVENT_STORAGE_READY`, `EVENT_NETWORK_READY`.
- **Diagnostics**: `Logger` and `Timer` utilities help with non-blocking work and instrumentation.

## Public Headers

- `Core.h`, `IComponent.h`, `ComponentRegistry.h`
- `ComponentConfig.h`, `EventBus.h`, `Timer.h`, `Logger.h`
- `Testing/HeapTracker.h` - Memory leak detection for tests

## HeapTracker API (Testing)

The `HeapTracker` provides platform-agnostic memory tracking for detecting leaks in unit tests.

```cpp
#include <DomoticsCore/Testing/HeapTracker.h>
using namespace DomoticsCore::Testing;

HeapTracker tracker;

// Take snapshots
tracker.checkpoint("before");
// ... code under test ...
tracker.checkpoint("after");

// Check for leaks
int64_t delta = tracker.getDelta("before", "after");
TEST_ASSERT_TRUE(delta >= -100 && delta <= 100); // Allow 100 bytes tolerance

// Or use macros
HEAP_ASSERT_STABLE(tracker, "before", "after", 100);
HEAP_ASSERT_NO_GROWTH(tracker, "before");
```

**Platform Support:**
- **Native**: Real malloc/free tracking via mallinfo
- **ESP32**: heap_caps API
- **ESP8266**: ESP.getFreeHeap()

## Examples

Example projects live under `DomoticsCore-Core/examples/`:
- `01-CoreOnly` – minimal setup with timers.
- `02-CoreWithDummyComponent` – demonstrates registering custom components.
- `03-05` series – event bus basics, coordinators, and testing patterns.

## License

MIT
