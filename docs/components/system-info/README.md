# DomoticsCore-SystemInfo

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## Overview

DomoticsCore-SystemInfo is a lightweight diagnostic component that collects runtime system metrics on ESP32 and ESP8266 devices. It reports heap memory usage, uptime, CPU frequency, chip model, estimated CPU load, flash usage, and boot diagnostics -- all through the standard `IComponent` lifecycle.

When paired with the optional `SystemInfoWebUI` provider, these metrics are exposed as real-time dashboard widgets and WebSocket updates through the DomoticsCore WebUI framework.

## Key Metrics

| Metric | Description |
|--------|-------------|
| Free / Total / Min Heap | RAM usage and fragmentation tracking |
| Max Alloc Heap | Largest contiguous free block (fragmentation indicator) |
| Uptime | Seconds since boot, with human-readable formatting |
| CPU Frequency | Clock speed in MHz |
| CPU Load (estimated) | Heuristic based on heap activity, smoothed with EMA |
| Chip Model / Revision | Hardware identification |
| Flash / Sketch / Free Sketch Space | Firmware storage and OTA update capacity |
| Boot Diagnostics | Reset reason, boot count, heap at boot |

## Quick Start

### Minimal (no WebUI)

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/SystemInfo.h>
using namespace DomoticsCore::Components;

core.addComponent(std::make_unique<SystemInfoComponent>());
```

### With WebUI Dashboard

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/SystemInfo.h>
#include <DomoticsCore/SystemInfoWebUI.h>
using namespace DomoticsCore::Components;

core.addComponent(std::make_unique<WebUIComponent>(webCfg));
core.addComponent(std::make_unique<SystemInfoComponent>());

auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* sys   = core.getComponent<SystemInfoComponent>("System Info");
if (webui && sys) {
    webui->registerProviderWithComponent(
        new DomoticsCore::Components::WebUI::SystemInfoWebUI(sys), sys);
}
```

## Platform Support

| Platform | Status |
|----------|--------|
| ESP32 | Primary target |
| ESP8266 | Secondary target |
| ESP32-S2/S3/C3 | Compatible |

All platform-specific calls are routed through the HAL (`Platform_HAL.h`), so the component contains zero `#ifdef` directives, in compliance with Constitution Principle IX.

## Further Reading

- [Technical Reference](technical-reference.md) -- full API documentation for every class and method.
- [Project Context](project-context.md) -- AI-oriented context file with identity, dependencies, and conventions.
- [Component README (library root)](../../../DomoticsCore-SystemInfo/README.md) -- original library README.

## License

MIT
