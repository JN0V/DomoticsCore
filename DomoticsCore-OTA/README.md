# DomoticsCore-OTA

Over-the-Air (OTA) firmware update component for ESP32 devices in the DomoticsCore modular architecture.

## Features

- **Multiple Update Sources**: Direct URL, manifest-based, or manual upload
- **Progress Tracking**: Real-time progress reporting via WebSocket
- **Version Management**: Semantic versioning with upgrade/downgrade control
- **Integrity Check**: SHA-256 verification with rollback on mismatch
- **Authenticated WebUI Upload**: upload endpoints gated behind WebUI auth
- **Web UI Integration**: Rich upload interface with progress and reboot handling
- **Network Agnostic**: Pluggable transport layer (HTTP/HTTPS via app-provided callbacks)

## Installation

Add to `platformio.ini`:
```ini
lib_deps =
    file://../DomoticsCore-Core
    file://../DomoticsCore-WebUI
    file://../DomoticsCore-OTA
```

## Usage

### Basic Setup

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/OTA.h>
#include <DomoticsCore/OTAWebUI.h>

// Create OTA component
OTAConfig otaCfg;
otaCfg.updateUrl = "https://example.com/firmware.bin";
otaCfg.autoReboot = true;
core->addComponent(std::make_unique<OTAComponent>(otaCfg));

// Register WebUI provider (after core.begin())
auto* webui = core->getComponent<WebUIComponent>("WebUI");
auto* ota = core->getComponent<OTAComponent>("OTA");
if (webui && ota) {
    auto* otaWebUI = new DomoticsCore::Components::WebUI::OTAWebUI(ota);
    otaWebUI->init(webui); // Initialize routes for file upload
    webui->registerProviderWithComponent(otaWebUI, ota);
}
```

### Configuration

```cpp
OTAConfig cfg;
cfg.updateUrl = "https://firmware-server.com/latest.bin";
cfg.manifestUrl = "https://firmware-server.com/manifest.json"; // Optional
cfg.checkIntervalMs = 3600000; // Check every hour (0 = disabled)
cfg.autoReboot = true; // Reboot after successful update
cfg.allowDowngrades = false; // Prevent version downgrades
cfg.enableWebUIUpload = true; // Allow manual uploads via web interface
```

> **Transport security is yours to provide.** This component does not open
> sockets: it calls the fetcher and downloader callbacks you install (see
> *Network Transport* below). TLS, certificate pinning and any authorization
> header belong in those callbacks, where the HTTP client actually lives.
>
> Earlier versions declared `requireTLS`, `bearerToken`, `basicAuthUser`,
> `basicAuthPassword`, `rootCA` and `signaturePublicKey` here. **No code path
> ever read them**, so setting them bought nothing but the belief that it did.
> They have been removed rather than left to promise a protection that was
> never there.
>
> The WebUI upload endpoints are a separate matter, and they *are* enforced —
> see below.

### Network Transport

OTA component is network-agnostic. Provide HTTP client callbacks in your application:

```cpp
// Set manifest fetcher
ota->setManifestFetcher([](const String& url, String& outJson) -> bool {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, url);
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        outJson = http.getString();
        http.end();
        return true;
    }
    http.end();
    return false;
});

// Set downloader
ota->setDownloader([](const String& url, size_t& totalSize, 
                      OTAComponent::DownloadCallback onChunk) -> bool {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    http.begin(client, url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) { http.end(); return false; }
    
    totalSize = http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[2048];
    
    while (stream->available()) {
        size_t len = stream->readBytes(buf, sizeof(buf));
        if (!onChunk(buf, len)) { http.end(); return false; }
    }
    http.end();
    return true;
});
```

## Web UI

The OTA WebUI provides:
- **Header Badge**: Quick status indicator (idle/downloading/applying/error)
- **Remote Updates**: Configure firmware URL and trigger updates
- **Manual Upload**: File selection and upload with progress tracking
- **Auto-Refresh**: Page automatically refreshes after device reboots

### Custom Routes

OTA requires custom REST endpoints for file uploads:
- `/api/ota/status` - GET status
- `/api/ota/check` - Trigger manifest check
- `/api/ota/update` - Start update from URL
- `/api/ota/upload` - Upload firmware file (multipart/form-data) — **authenticated**
- `/ota/upload` - Upload form page — **authenticated**

These are registered via `init()` method after WebUI server is ready.

The two upload routes require WebUI credentials whenever `WebUIConfig::enableAuth`
is set. Authentication is checked on the **first chunk**, before any byte reaches
flash, and a failed check aborts the update rather than letting the remaining
chunks through. With `enableAuth` false the endpoints are open, exactly like the
rest of the WebUI — that is a deployment decision, not an OTA one.

## Manifest Format

Optional JSON manifest for version checking:

```json
{
  "version": "1.2.0",
  "url": "https://example.com/firmware-v1.2.0.bin",
  "sha256": "abc123...",
  "signature": "..."
}
```

## Architecture

- **OTAComponent**: Core logic (state machine, upload, validation)
- **OTAWebUI**: Web interface provider with custom upload routes
- **Transport Callbacks**: App-provided HTTP/HTTPS client implementation
- **Event System**: WebSocket updates for real-time progress

See `examples/OTAWithWebUI` for complete working example.

## License

MIT
