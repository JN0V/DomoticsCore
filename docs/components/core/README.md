# DomoticsCore-Core

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## What is DomoticsCore-Core?

DomoticsCore-Core is the foundation library of the DomoticsCore IoT framework. It provides the runtime engine that every other DomoticsCore component builds upon. Think of it as the kernel of your embedded application: it manages the component lifecycle, provides inter-component communication, handles logging, offers non-blocking timers, and adapts to available memory at runtime.

Core has **zero external dependencies** -- it only requires the Arduino-ESP32 (or ESP8266) toolchain.

## What Does It Provide?

| Feature | Description |
|---------|-------------|
| **Component Model** | `IComponent` base class with `begin()`, `loop()`, `shutdown()` lifecycle |
| **Component Registry** | Central registry for registering, resolving, and managing components by name or type |
| **Dependency Resolution** | Automatic topological sort of component initialization order via `getDependencies()` |
| **EventBus** | Publish/subscribe message bus with topic-based routing, sticky events, and wildcard matching |
| **Logger** | Platform-agnostic macro-based logging (`DLOG_E`, `DLOG_W`, `DLOG_I`, `DLOG_D`, `DLOG_V`) with component tags |
| **Timer** | `NonBlockingDelay` utility for scheduling periodic work without blocking `loop()` |
| **MemoryManager** | Runtime memory profiling that auto-detects heap profile (FULL / STANDARD / MINIMAL / CRITICAL) |
| **HeapTracker** | Testing utility for detecting memory leaks across checkpoints |
| **Platform HAL** | Hardware Abstraction Layer routing so business code never contains `#ifdef` |
| **Platform Arduino** | Shared Arduino utilities (time, string, GPIO, math) factored out of ESP32/ESP8266 HAL files |
| **Filesystem HAL** | Platform-agnostic filesystem access (`begin`, `exists`, `getFS`, `format`, `totalBytes`, `usedBytes`) |
| **Configuration** | `ComponentConfig` class with typed parameters and built-in validation |

## Quick Start

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// 1. Define your component
class BlinkComponent : public IComponent {
public:
    BlinkComponent() {
        metadata = {"Blink", "1.0.0", "Me", "Blinks the LED"};
    }

    ComponentStatus begin() override {
        // Internal initialization only
        return ComponentStatus::Success;
    }

    void loop() override {
        if (timer.isReady()) {
            // toggle LED
        }
    }

    ComponentStatus shutdown() override {
        return ComponentStatus::Success;
    }

private:
    Utils::NonBlockingDelay timer{1000};
};

// 2. Wire it up in your sketch
Core core;

void setup() {
    core.addComponent(std::make_unique<BlinkComponent>());

    CoreConfig cfg;
    cfg.deviceName = "MyDevice";
    cfg.logLevel = 3;  // INFO
    core.begin(cfg);
}

void loop() {
    core.loop();
}
```

## Using the EventBus

Components communicate through topics without knowing about each other:

```cpp
// Publisher (e.g., a sensor component)
emit("sensor/temperature", 23.5f, /*sticky=*/true);

// Subscriber (e.g., a display component)
on<float>("sensor/temperature", [](const float& temp) {
    // React to temperature update
}, /*replayLast=*/true);
```

## Using the Logger

```cpp
#include <DomoticsCore/Logger.h>

#define LOG_MY_APP "MYAPP"

DLOG_I(LOG_MY_APP, "Startup complete, heap: %u", HAL::getFreeHeap());
DLOG_W(LOG_MY_APP, "Sensor reading out of range: %.2f", value);
DLOG_E(LOG_MY_APP, "Failed to connect after %d retries", retries);
```

## Memory-Aware Code

```cpp
#include <DomoticsCore/MemoryManager.h>

auto& mm = MemoryManager::instance();
if (mm.shouldEnable(Feature::ChartHistory)) {
    // Allocate chart history -- only on devices with enough RAM
}
size_t bufSize = mm.getBufferSize(BufferType::JsonDocument);
```

## Further Reading

- [Technical Reference](./technical-reference.md) -- full API reference, lifecycle details, dependency algorithm, EventBus internals
- [Project Context](./project-context.md) -- file inventory, key classes, conventions, and modification patterns for AI agents
- [DomoticsCore Constitution](../../../.specify/memory/constitution.md) -- the non-negotiable principles governing all development
- [Logging Guide](../../../DomoticsCore-Core/LOGGING.md) -- comprehensive logging best practices

## License

MIT
