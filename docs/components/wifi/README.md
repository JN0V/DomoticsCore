# DomoticsCore-Wifi

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## What is DomoticsCore-Wifi?

DomoticsCore-Wifi is the WiFi connectivity component for the DomoticsCore IoT framework. It provides non-blocking STA (station) and AP (access point) management with automatic reconnection, async network scanning, and runtime credential changes. The component uses a Hardware Abstraction Layer (HAL) to support both ESP32 and ESP8266 platforms transparently.

When no SSID is configured, the device automatically starts in AP mode with an auto-generated SSID (e.g., `DomoticsCore-A1B2C3`), making first-time provisioning effortless.

## Key Features

| Feature | Description |
|---------|-------------|
| **STA Mode** | Non-blocking connection to a WiFi network with automatic reconnection |
| **AP Mode** | Create an access point for direct device configuration |
| **STA+AP Mode** | Run both modes simultaneously (dual radio on ESP32, channel-synced on ESP8266) |
| **Async Scanning** | Non-blocking network scan that avoids watchdog resets |
| **Runtime Credentials** | Change SSID/password at runtime without reboot |
| **AP Auto-SSID** | Generated from MAC address when no AP name is configured |
| **INetworkProvider** | Abstraction interface allowing other components to depend on network connectivity generically |
| **WebUI Provider** | Optional `WifiWebUI` for browser-based STA/AP settings, scanning, and live status badges |
| **Heap-Aware** | Automatically adapts WiFi mode based on available heap, with fallback and reboot strategies for ESP8266 |

## Quick Start

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core core;

void setup() {
    // AP-only on first boot (empty SSID triggers AP mode)
    core.addComponent(std::make_unique<WifiComponent>("", ""));
    core.begin();
}

void loop() {
    core.loop();
}
```

### Setting Credentials at Runtime

```cpp
auto* wifi = core.getComponent<WifiComponent>("Wifi");
if (wifi) {
    wifi->setCredentials("MyNetwork", "MyPassword");
}
```

### With WebUI

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/WifiWebUI.h>

auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* wifi  = core.getComponent<WifiComponent>("Wifi");
if (webui && wifi) {
    webui->registerProviderWithComponent(
        new DomoticsCore::Components::WebUI::WifiWebUI(wifi), wifi);
}
```

## Platform Notes

- **ESP32**: AP and STA use separate radios. Simultaneous AP+STA works on different channels without disruption.
- **ESP8266**: Single radio shared between AP and STA. The component handles channel synchronization by stopping the AP during STA connection, then restarting it on the STA channel.

## Further Reading

- [Technical Reference](./technical-reference.md) -- full API reference, connection modes, scanning, events, platform differences
- [Project Context](./project-context.md) -- file inventory, dependencies, key classes, and AI agent guidance
- [DomoticsCore Constitution](../../../.specify/memory/constitution.md) -- the non-negotiable principles governing all development
- [Component README](../../../DomoticsCore-Wifi/README.md) -- original component README with examples and integration tips

## License

MIT
