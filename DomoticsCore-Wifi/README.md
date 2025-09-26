# DomoticsCore-Wifi

WiFi connectivity component for DomoticsCore with optional WebUI provider.

## Features

- Non-blocking STA connection management with reconnection
- Optional AP mode, or STA+AP concurrently
- Async WiFi scanning (non-blocking) with summarized results
- Clean API: `enableWifi`, `enableAP`, `disableAP`, `setCredentials`, `reconnect`
- WebUI provider (optional): STA/AP settings, connect, scan, live status and header badges

## Installation & Dependencies

- Requires Arduino-ESP32 core (`<WiFi.h>`)
- Optional WebUI integration depends on `DomoticsCore-WebUI` and its async server stack:
  - `ESP32Async/ESPAsyncWebServer @ ^3.8.1`
  - `ESP32Async/AsyncTCP @ ^3.4.8`

## Main behaviors

- If constructed with empty SSID, the component starts in AP-only mode and exposes a generated AP SSID (e.g., `DomoticsCore-XXXXXX`).
- STA connection attempts are non-blocking; reconnection is managed by the component.
- AP can be enabled/disabled independently from STA; STA+AP mode is supported.
- Async scanning avoids watchdog resets and updates results in the background.

## Usage

```cpp
#include <DomoticsCore/Wifi.h>
using namespace DomoticsCore::Components;

core.addComponent(std::make_unique<WifiComponent>("", "")); // AP-only on first boot

// Change credentials at runtime
auto* wifi = core.getComponent<WifiComponent>("Wifi");
if (wifi) {
  wifi->setCredentials("MySSID", "secret");
}
```

### With WebUI

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/WifiWebUI.h>

core.addComponent(std::make_unique<WebUIComponent>());
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* wifi  = core.getComponent<WifiComponent>("Wifi");
if (webui && wifi) {
  webui->registerProviderWithComponent(new DomoticsCore::Components::WebUI::WifiWebUI(wifi), wifi);
}
```

## API Highlights

- `bool enableWifi(bool enable=true)`
- `bool enableAP(const String& ssid, const String& password="", bool enable=true)`
- `bool disableAP()`
- `void setCredentials(const String& ssid, const String& password, bool reconnectNow=true)`
- `void startScanAsync()` and `String getLastScanSummary()`

## License

MIT
