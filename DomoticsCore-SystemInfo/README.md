# DomoticsCore-SystemInfo

System information component for DomoticsCore with an optional WebUI provider.

## Features

- Reports uptime, free heap, chip model, and CPU frequency
- WebUI provider to show live system metrics on dashboard and components tab
- Lightweight, no external deps beyond core

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

## License

MIT
