# DomoticsCore-Core

Core runtime for DomoticsCore: component model, registry, lifecycle, configuration, logging, timers, and the event bus.

## Features

- Component base class and lifecycle (`begin`, `loop`, `shutdown`)
- Central `ComponentRegistry` for add/lookup by name and type
- `ComponentStatus` and `ComponentMetadata` shared by every component
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
- **Configuration**: each component takes a plain config struct with defaults, passed to its constructor and exposed through `getConfig()` / `setConfig()`.
- **Event bus**: publish/subscribe enables decoupled messaging between components.
- **Lifecycle events**: `EVENT_COMPONENT_READY`, `EVENT_SYSTEM_READY`, `EVENT_STORAGE_READY`, `EVENT_NETWORK_READY`.
- **Diagnostics**: `Logger` and `Timer` utilities help with non-blocking work and instrumentation.

## Public Headers

- `Core.h`, `IComponent.h`, `ComponentRegistry.h`
- `ComponentConfig.h` (`ComponentStatus`, `ComponentMetadata`), `EventBus.h`, `Timer.h`, `Logger.h`
- `StringParse.h` - `Utils::digitsOnly()` for user-typed numbers
- `MemoryManager.h` - Runtime memory adaptation
- `Testing/HeapTracker.h` - Memory leak detection for tests

## MemoryManager API

The `MemoryManager` classifies the heap available at boot into a profile and derives the WebSocket client limit from it. It detects the memory profile automatically during `Core::begin()` and logs the result.

```cpp
#include <DomoticsCore/MemoryManager.h>
using namespace DomoticsCore;

// Profile is auto-detected in Core::begin(), but you can query it anywhere:
auto& mm = MemoryManager::instance();
MemoryProfile profile = mm.getProfile();

// Get profile name for logging
DLOG_I("APP", "Memory profile: %s", mm.getProfileName());

// WebSocket client limit for the profile
uint8_t maxClients = mm.getMaxWsClients();

// Runtime low-memory checks
if (mm.isLowMemory()) {
    // Reduce operations
}
```

**Memory Profiles:**
| Profile | Free Heap | Max Clients |
|---------|-----------|-------------|
| FULL | > 30KB | 8 |
| STANDARD | 15-30KB | 4 |
| MINIMAL | 8-15KB | 2 |
| CRITICAL | < 8KB | 1 |

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
