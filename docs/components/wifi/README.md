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

## Source Tree

```
DomoticsCore-Wifi/
├── include/DomoticsCore/
│   ├── Wifi.h                  # WifiConfig + WifiComponent (business logic)
│   ├── Wifi_HAL.h              # HAL routing: enums + inline delegates
│   ├── Wifi_ESP32.h            # ESP32 WiFiImpl (WiFi.h wrapper)
│   ├── Wifi_ESP8266.h          # ESP8266 WiFiImpl (ESP8266WiFi.h wrapper)
│   ├── Wifi_Stub.h             # Native test stub (no-ops, controllable flags)
│   ├── INetworkProvider.h      # Abstract network interface (DIP)
│   ├── WifiEvents.h            # Event topic constants
│   ├── WifiWebUI.h             # CachingWebUIProvider for browser config
│   ├── WiFiServer_HAL.h        # WiFiServer routing header
│   ├── WiFiServer_ESP32.h      # ESP32 WiFiServer type aliases
│   ├── WiFiServer_ESP8266.h    # ESP8266 WiFiServer type aliases
│   ├── WiFiServer_Stub.h       # Stub WiFiServer + WiFiClient with test helpers
│   ├── IPAddress_Stub.h        # Stub IPAddress for native tests
│   └── DocMainpage.h           # Doxygen mainpage (documentation only)
├── examples/
│   ├── BasicWifi/              # CLI-only demo (5 phases: connect, scan, AP, STA+AP, reconnect)
│   └── WifiWithWebUI/          # WebUI integration demo (AP mode + browser config)
├── test/
│   ├── test_wifi_component/    # Unit tests for WifiComponent logic
│   └── test_wifi_webui/        # Unit tests for WifiWebUI provider
├── library.json                # PlatformIO metadata (v1.4.1)
├── platformio.ini              # Build config (native test environment)
└── README.md                   # Component README
```

## Platform Notes

- **ESP32**: AP and STA use separate radios. Simultaneous AP+STA works on different channels without disruption.
- **ESP8266**: Single radio shared between AP and STA. The component handles channel synchronization by stopping the AP during STA connection, then restarting it on the STA channel.

## Examples

| Directory | Description |
|-----------|-------------|
| `DomoticsCore-Wifi/examples/BasicWifi/` | CLI-only WiFi control, status monitoring, scanning, AP mode switching |
| `DomoticsCore-Wifi/examples/WifiWithWebUI/` | WebUI integration with settings card, async scanning, live status badges |

## Further Reading

- [Technical Reference](./technical-reference.md) -- full API reference, connection modes, scanning, events, platform differences
- [Project Context](./project-context.md) -- file inventory, dependencies, key classes, and AI agent guidance
- [DomoticsCore Constitution](../../../.specify/memory/constitution.md) -- the non-negotiable principles governing all development
- [Component README](../../../DomoticsCore-Wifi/README.md) -- original component README with examples and integration tips

## License

MIT
