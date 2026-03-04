# DomoticsCore-OTA

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## What is DomoticsCore-OTA?

DomoticsCore-OTA is the secure over-the-air firmware update component for the DomoticsCore IoT framework. It provides HTTPS-capable firmware downloads, manifest-based version checking, manual file uploads via WebUI, real-time progress tracking, and semantic version management for ESP32 and ESP8266 devices.

The component registers as `OTAComponent` (type key `"ota"`) in the Core component registry and follows the standard `IComponent` lifecycle (`begin` / `loop` / `shutdown`). Unlike most DomoticsCore components, OTA includes a `.cpp` implementation file alongside its headers.

## Key Features

- **Secure Downloads** -- HTTPS/TLS with optional root CA certificate pinning and configurable `requireTLS` enforcement.
- **Manifest-Based Updates** -- fetch a JSON manifest providing version, URL, SHA-256, and signature metadata.
- **Direct URL Updates** -- download and install firmware from a known URL without a manifest.
- **Manual Upload** -- browser-based file upload via the WebUI provider (`/api/ota/upload`) with multipart/form-data support.
- **Progress Tracking** -- real-time percentage, byte counters, and state machine exposed through accessors and EventBus events.
- **Version Management** -- semantic version comparison with configurable downgrade prevention.
- **SHA-256 Verification** -- integrity check against manifest-provided hash after download completes.
- **Authentication** -- bearer token and HTTP basic-auth headers for protected firmware endpoints.
- **Network Agnostic** -- pluggable `ManifestFetcher` and `Downloader` callbacks keep HTTP client details out of the component.
- **Auto-Reboot Control** -- configurable automatic reboot after successful update, with a 2-second grace period for final UI updates.

## Quick Start

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/OTA.h>
#include <DomoticsCore/OTAWebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// 1. Configure
OTAConfig otaCfg;
otaCfg.updateUrl = "https://firmware.example.com/latest.bin";
otaCfg.autoReboot = true;
otaCfg.requireTLS = true;

// 2. Register component
core.addComponent(std::make_unique<OTAComponent>(otaCfg));

// 3. After core.begin(), wire up WebUI
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* ota   = core.getComponent<OTAComponent>("OTA");
if (webui && ota) {
    auto* otaWebUI = new DomoticsCore::Components::WebUI::OTAWebUI(ota);
    otaWebUI->init(webui);
    webui->registerProviderWithComponent(otaWebUI, ota);
}
```

See the [Technical Reference](technical-reference.md) for the full configuration struct, transport callbacks, and REST API details.

## Manifest Format

Optional JSON manifest returned by the `manifestUrl` endpoint:

```json
{
  "version": "1.5.0",
  "url": "https://firmware.example.com/v1.5.0.bin",
  "sha256": "a3f2c8...",
  "signature": "..."
}
```

## Further Reading

- [Technical Reference](technical-reference.md) -- full API documentation, OTAConfig fields, state machine, WebUI provider, and REST endpoints.
- [Project Context (AI Agent)](project-context.md) -- file inventory, dependencies, conventions, platform caveats, and constitution compliance notes.
- [DomoticsCore-OTA README](../../../DomoticsCore-OTA/README.md) -- upstream component README with code examples.

## Version

Current version: **1.4.1** (as declared in `library.json` and `metadata.version`).

## License

MIT
