# DomoticsCore-Core

Core runtime for DomoticsCore: component model, registry, lifecycle, configuration, logging, timers, and the event bus.

## Features

- Component base class and lifecycle (`begin`, `loop`, `shutdown`)
- Central `ComponentRegistry` for add/lookup by name and type
- `ComponentConfig` for strongly-typed parameters
- `EventBus` (publish/subscribe) for decoupled communication
- Utilities: `Logger`, `Timer` (non-blocking delay)

## Installation

Include headers with the prefix `DomoticsCore/` from this package in your PlatformIO project.

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

## Public Headers

- `Core.h`, `IComponent.h`, `ComponentRegistry.h`
- `ComponentConfig.h`, `EventBus.h`, `Timer.h`, `Logger.h`

## License

MIT
