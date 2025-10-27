# DomoticsCore-SystemInfo

System information component for DomoticsCore with an optional WebUI provider.

## Features

- Reports uptime, free heap, chip model, and CPU frequency
- WebUI provider to show live system metrics on dashboard and components tab
- Lightweight, no external deps beyond core

## Installation & Dependencies

- No external libraries beyond Arduino-ESP32 core.
- Include headers with prefix `DomoticsCore/`:

```cpp
#include <DomoticsCore/SystemInfo.h>
```

## Usage

```cpp
#include <DomoticsCore/SystemInfo.h>
using namespace DomoticsCore::Components;

core.addComponent(std::make_unique<SystemInfoComponent>());
```

### With WebUI

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/SystemInfoWebUI.h>

core.addComponent(std::make_unique<WebUIComponent>());
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* sys   = core.getComponent<SystemInfoComponent>("System Info");
if (webui && sys) {
  webui->registerProviderWithComponent(new DomoticsCore::Components::WebUI::SystemInfoWebUI(sys), sys);
}
```

## Main behaviors

- Collects ESP-IDF system metrics during `loop()` and pushes via `getWebUIData()` when registered with WebUI.
- Exposes read-only fields: uptime, free heap, chip model, CPU frequency, SDK version.
- Estimates CPU usage (percentage) via heap-activity heuristic and smooths it with an exponential moving average.
- No persistent configuration required; begins immediately once registered.

## Examples

- `DomoticsCore-SystemInfo/examples/SystemInfoNoWebUI` – console logging only.
- `DomoticsCore-SystemInfo/examples/SystemInfoWithWebUI` – dashboard integration demo.

## License

MIT
