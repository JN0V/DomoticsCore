# DomoticsCore-OTA -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides the complete API surface, configuration options, state machine, event system, WebUI provider, and REST endpoint details for the DomoticsCore-OTA component.

---

## Table of Contents

1. [OTAConfig Struct](#otaconfig-struct)
2. [OTAComponent Class](#otacomponent-class)
3. [State Machine](#state-machine)
4. [Update Sources](#update-sources)
5. [Transport Providers](#transport-providers)
6. [TLS and Certificate Configuration](#tls-and-certificate-configuration)
7. [Progress Tracking](#progress-tracking)
8. [Version Management](#version-management)
9. [Manual Upload API](#manual-upload-api)
10. [Event System (OTAEvents)](#event-system-otaevents)
11. [OTAWebUI Provider](#otawebui-provider)
12. [REST Endpoints](#rest-endpoints)
13. [Update HAL](#update-hal)

---

## OTAConfig Struct

Defined in `DomoticsCore/OTA.h` within `DomoticsCore::Components`.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `updateUrl` | `String` | `""` | Direct firmware binary URL. Used when no manifest is configured. |
| `manifestUrl` | `String` | `""` | Optional JSON manifest endpoint providing version, URL, SHA-256, and signature. |
| `bearerToken` | `String` | `""` | Optional HTTP `Authorization: Bearer <token>` header value. |
| `basicAuthUser` | `String` | `""` | Optional HTTP basic-auth username. |
| `basicAuthPassword` | `String` | `""` | Optional HTTP basic-auth password. |
| `rootCA` | `String` | `""` | Optional PEM-encoded root CA certificate for TLS validation. |
| `signaturePublicKey` | `String` | `""` | Optional PEM-encoded public key for firmware signature validation. |
| `checkIntervalMs` | `uint32_t` | `3600000` | Automatic periodic check interval in milliseconds. Set to `0` to disable periodic checks. |
| `requireTLS` | `bool` | `true` | When `true`, reject non-HTTPS URLs. |
| `allowDowngrades` | `bool` | `false` | When `true`, permit installing firmware with a lower semantic version. |
| `autoReboot` | `bool` | `true` | When `true`, reboot automatically 2 seconds after a successful update. |
| `maxDownloadSize` | `size_t` | `0` | Maximum acceptable firmware binary size in bytes. `0` means unlimited. |
| `enableWebUIUpload` | `bool` | `true` | When `true`, expose manual firmware upload routes and WebUI file-upload controls. |

### Runtime Configuration Update

Configuration can be modified at runtime using the get/set pattern:

```cpp
OTAConfig cfg = ota->getConfig();
cfg.updateUrl = "https://new-server.example.com/firmware.bin";
cfg.autoReboot = false;
ota->setConfig(cfg);
```

---

## OTAComponent Class

Defined in `DomoticsCore/OTA.h`. Namespace: `DomoticsCore::Components`.

Inherits from `IComponent`. Type key: `"ota"`. Metadata name: `"OTA"`, version: `"1.4.1"`.

### Constructor

```cpp
OTAComponent(const OTAConfig& cfg = OTAConfig());
```

Creates the component with the given configuration. Default-constructed `OTAConfig` has no URLs configured, so the component starts idle until URLs are provided via `setConfig()` or the WebUI.

### IComponent Lifecycle

| Method | Returns | Description |
|--------|---------|-------------|
| `begin()` | `ComponentStatus` | Initializes internal state, resets timers and upload session. Returns `ComponentStatus::Success`. |
| `loop()` | `void` | Processes pending URL updates, pending manifest checks, periodic auto-checks, upload buffer processing (ESP8266), and auto-reboot countdown. |
| `shutdown()` | `ComponentStatus` | Aborts any active upload session and resets state to `Idle`. |
| `getTypeKey()` | `const char*` | Returns `"ota"`. |

### Control Methods

#### `triggerImmediateCheck`

```cpp
bool triggerImmediateCheck(bool force = false);
```

Schedules a manifest/URL check to execute on the next `loop()` iteration. When `force` is `true`, the check proceeds even if the manifest version indicates the firmware is already up to date.

Returns `true` (always succeeds in scheduling).

#### `triggerUpdateFromUrl`

```cpp
bool triggerUpdateFromUrl(const String& url, bool force = false);
```

Schedules a firmware download-and-install from the given URL on the next `loop()` iteration. When `force` is `true`, version and downgrade checks are skipped.

Returns `false` only if `url` is empty.

### State Accessors

| Method | Return Type | Description |
|--------|-------------|-------------|
| `isIdle()` | `bool` | `true` when state is `Idle` or `Error`. |
| `isBusy()` | `bool` | `true` when state is `Checking`, `Downloading`, or `Applying`. (Note: `Applying` is checked in the code but never entered at runtime -- see C15 warning in State Machine section.) |
| `getState()` | `State` | Current state machine value. |
| `getProgress()` | `float` | Download/upload progress as a percentage (0.0 -- 100.0). |
| `getDownloadedBytes()` | `size_t` | Bytes written to flash so far. |
| `getTotalBytes()` | `size_t` | Expected total firmware size (0 if unknown). |
| `getLastResult()` | `const String&` | Human-readable status message (e.g., `"Idle"`, `"Uploading firmware"`, `"Update complete - rebooting in 2s"`). |
| `getLastError()` | `const String&` | Last error message, or empty if no error. |
| `getLastVersion()` | `const String&` | Last firmware version seen in a manifest response. |
| `getConfig()` | `const OTAConfig&` | Current configuration (const reference). |

---

## State Machine

The `OTAComponent::State` enum defines the following states:

```
Idle --> Checking --> Downloading --> RebootPending (or Idle)
  ^                       |
  |                       v
  +<----- Error <---------+
```

> **Note (C15)**: The `Applying` state exists in the enum but is never entered in the current implementation. The actual flow goes directly from `Downloading` to `RebootPending` (if `autoReboot` is true) or `Idle` (if `autoReboot` is false) when the update completes via `finalizeUpdateOperation()`.

| State | Description |
|-------|-------------|
| `Idle` | No update in progress. Ready for new checks or uploads. |
| `Checking` | Fetching the manifest endpoint and evaluating version information. |
| `Downloading` | Firmware binary is being downloaded (remote) or uploaded (manual). |
| `Applying` | Firmware data has been received; HAL is finalizing the flash write. **Warning (C15): this state is defined in the `State` enum and referenced by `isBusy()` and `stateToString()`, but `transition(State::Applying, ...)` is never called in the current implementation (v1.4.1). The code transitions directly from `Downloading` to `RebootPending` or `Idle` via `finalizeUpdateOperation()`. The state is retained in the enum for forward compatibility but is not entered at runtime.** |
| `RebootPending` | Update applied successfully. If `autoReboot` is `true`, the device reboots after a 2-second grace period. |
| `Error` | Something went wrong. `getLastError()` contains the reason. The component returns to accepting new operations. |

---

## Update Sources

### 1. Manifest-Based Update

When `manifestUrl` is configured, the component fetches JSON from that endpoint:

```json
{
  "version": "1.5.0",
  "url": "https://firmware.example.com/v1.5.0.bin",
  "sha256": "a3f2c8de9b...",
  "signature": "..."
}
```

The internal `ManifestInfo` struct captures `version`, `url`, `sha256`, `signature`, and a `valid` flag. If the manifest version is not newer than the current `metadata.version` (and `allowDowngrades` is `false` and the check is not forced), the component returns to `Idle` with `"No update needed"`.

### 2. Direct URL Update

When only `updateUrl` is configured (no manifest), `performCheck()` uses the URL directly without version comparison.

### 3. Manual Upload

Browser-based file upload through the WebUI. The upload flow uses `beginUpload()` / `acceptUploadChunk()` / `finalizeUpload()` (see below).

---

## Transport Providers

OTA is network-agnostic. The application must inject HTTP client logic via two callback types:

### ManifestFetcher

```cpp
using ManifestFetcher = std::function<bool(const String& manifestUrl, String& outJson)>;
```

Fetches the manifest JSON from the given URL and writes it into `outJson`. Returns `true` on success.

```cpp
ota->setManifestFetcher([](const String& url, String& outJson) -> bool {
    // Perform HTTP GET on url, write response body to outJson
    return true;
});
```

### Downloader

```cpp
using DownloadCallback = std::function<bool(const uint8_t* data, size_t len)>;
using Downloader = std::function<bool(const String& url, size_t& totalSize, DownloadCallback onChunk)>;
```

Downloads firmware from the given URL. The implementation must set `totalSize` to the content length (or 0 if unknown) and call `onChunk` repeatedly with data buffers. Returns `true` on success.

```cpp
ota->setDownloader([](const String& url, size_t& totalSize,
                      OTAComponent::DownloadCallback onChunk) -> bool {
    // HTTP GET, set totalSize, stream chunks via onChunk
    return true;
});
```

If either provider is not set, the corresponding network features log an error and fail gracefully.

---

## TLS and Certificate Configuration

| Config Field | Purpose |
|-------------|---------|
| `requireTLS` | When `true` (default), non-HTTPS URLs are rejected. |
| `rootCA` | PEM-encoded root CA certificate string. Passed to the HTTP client in the application-provided transport callbacks. |
| `signaturePublicKey` | PEM-encoded public key for firmware signature validation (reserved; verification logic is application-side). |
| `bearerToken` | Bearer token included in HTTP `Authorization` headers. |
| `basicAuthUser` / `basicAuthPassword` | HTTP basic authentication credentials. |

The OTA component itself does not perform HTTP requests -- TLS setup is the responsibility of the injected `ManifestFetcher` and `Downloader` callbacks. The configuration fields serve as a structured way to pass credentials and certificates from the application configuration to those callbacks.

---

## Progress Tracking

Progress information is available through three channels:

### 1. State Accessors

Call `getProgress()`, `getDownloadedBytes()`, `getTotalBytes()`, and `getState()` from application code.

### 2. EventBus Events

The component emits progress events (see [Event System](#event-system-otaevents) below). Progress broadcasts during upload are throttled to once per second to prevent EventBus queue overflow.

### 3. WebUI Real-Time Updates

The `OTAWebUI` provider polls the component state every 2 seconds (configurable via `withRealTime(2000)`) and pushes JSON updates to connected browsers.

### Progress Logging

Internal logging occurs every 10% progress change or every 256 KB received, whichever comes first, to avoid excessive serial output during large transfers.

---

## Version Management

The component uses semantic versioning (`major.minor.patch`) for update decisions.

### `isNewerVersion(candidate)`

Compares the `candidate` version string against `metadata.version` using numeric major/minor/patch comparison. Returns `true` only if the candidate is strictly newer.

### Downgrade Prevention

When `allowDowngrades` is `false` (default) and a manifest provides a version that is not newer, the check returns without downloading. Setting `allowDowngrades = true` or passing `force = true` to `triggerImmediateCheck()` bypasses this check.

### SHA-256 Verification

After a remote download completes, if the manifest provided a `sha256` field, the component compares the SHA-256 digest of the downloaded data against the expected hex string. Mismatches transition the state to `Error` with `"SHA256 mismatch"`.

---

## Manual Upload API

These methods are called by the `OTAWebUI` provider (or custom upload tooling) to handle browser-based firmware uploads.

### `beginUpload`

```cpp
bool beginUpload(size_t expectedSize = 0);
```

Initializes the HAL update subsystem for the given size (or `UPDATE_SIZE_UNKNOWN` if 0). Resets progress counters and transitions to `Downloading` state. Returns `false` if an upload is already active or if `HAL::OTAUpdate::begin()` fails.

### `acceptUploadChunk`

```cpp
bool acceptUploadChunk(const uint8_t* data, size_t length);
```

Writes a chunk of firmware data to the HAL. On ESP32, this writes directly to flash. On ESP8266, data is buffered internally and flushed in `loop()` to work within the dual-partition constraints. Returns `false` on write failure or buffer overflow.

### `finalizeUpload`

```cpp
bool finalizeUpload();
```

Signals the HAL that all data has been received. On ESP32, finalization is immediate. On ESP8266, the component enters a buffered-finalization mode and completes in subsequent `loop()` calls. Returns `false` if no upload is active or if `HAL::OTAUpdate::end()` fails.

### `abortUpload`

```cpp
void abortUpload(const String& reason);
```

Cancels the current upload, calls `HAL::OTAUpdate::abort()`, transitions to `Error`, and publishes an error event.

---

## Event System (OTAEvents)

Defined in `DomoticsCore/OTAEvents.h`. Namespace: `DomoticsCore::OTAEvents`.

All events are emitted via the Core EventBus with JSON string payloads. Each payload includes `state`, `progress`, and `lastResult` fields in addition to event-specific data.

| Constant | Topic String | When Emitted | Additional Payload Fields |
|----------|-------------|--------------|---------------------------|
| `EVENT_START` | `"ota/start"` | **Declared but NOT emitted by `OTAComponent` as of v1.4.1.** Defined in `OTAEvents.h` for forward compatibility or application-level use, but no call to `emit()` with this topic exists in `OTA.cpp`. | -- |
| `EVENT_PROGRESS` | `"ota/progress"` | Periodically during transfer (throttled to 1/s for uploads) | `bytes`, `total`, `source` |
| `EVENT_END` | `"ota/end"` | **Declared but NOT emitted by `OTAComponent` as of v1.4.1.** Defined in `OTAEvents.h` for forward compatibility or application-level use, but no call to `emit()` with this topic exists in `OTA.cpp`. | -- |
| `EVENT_ERROR` | `"ota/error"` | Error encountered | `error`, `source` |
| `EVENT_INFO` | `"ota/info"` | Informational message (e.g., upload started) | `message`, `source` |
| `EVENT_COMPLETE` | `"ota/complete"` | Intermediate completion (before reboot decision) | `progress`, `bytes`, `total` |
| `EVENT_COMPLETED` | `"ota/completed"` | Final completion with reboot status (sticky) | `source`, `autoReboot`, `bytes`, `message` |

> **Warning (C14)**: `EVENT_START` and `EVENT_END` are defined as constants in `OTAEvents.h` but are **not emitted** anywhere in the current `OTA.cpp` implementation. Do not subscribe to these events expecting them to fire. The actual lifecycle events emitted are: `EVENT_INFO` (start of upload), `EVENT_PROGRESS`, `EVENT_ERROR`, `EVENT_COMPLETE`, and `EVENT_COMPLETED`.

The `EVENT_COMPLETED` event is published as **sticky**, so late subscribers (e.g., WebUI clients reconnecting) receive the last update status.

---

## OTAWebUI Provider

Defined in `DomoticsCore/OTAWebUI.h`. Namespace: `DomoticsCore::Components::WebUI`.

Class `OTAWebUI` extends `CachingWebUIProvider` and bridges `OTAComponent` with the WebUI system.

### Construction and Initialization

```cpp
auto* otaWebUI = new OTAWebUI(otaComponent);
otaWebUI->init(webuiComponent);  // Must be called after WebUI server is ready
webui->registerProviderWithComponent(otaWebUI, otaComponent);
```

The `init()` call is required because OTA registers custom REST routes for file upload (`multipart/form-data`), which differs from simpler components that only use standard WebUI field interactions.

### WebUI Card

The provider builds a unified settings card (`ota_unified`) with the following fields:

| Field ID | Type | Description |
|----------|------|-------------|
| `status` | Display | Current state (idle/downloading/applying/error) |
| `progress` | Progress | Upload/download percentage |
| `update_url` | Text | Firmware URL (editable) |
| `check_now` | Button | Trigger manifest check |
| `start_update` | Button | Download and install from configured URL |
| `firmware` | File | Manual firmware upload (.bin, .bin.gz) |
| `auto_reboot` | Boolean | Toggle auto-reboot after update |

When `enableWebUIUpload` is `false`, the card omits the file upload field and uses a remote-only layout.

### Change Detection

`OTAWebUI` uses `LazyState<OTAState>` to track `(state, progress, bytes)`. WebSocket updates are only pushed when these values change, reducing unnecessary traffic.

### Supported WebUI Interactions (POST)

| Field | Action |
|-------|--------|
| `update_url` | Updates `OTAConfig::updateUrl` via get/override/set pattern |
| `check_now` | Calls `triggerImmediateCheck(true)` |
| `start_update` | Calls `triggerUpdateFromUrl()` with the configured or provided URL |
| `auto_reboot` | Updates `OTAConfig::autoReboot` via get/override/set pattern |

---

## REST Endpoints

All endpoints are registered by `OTAWebUI::registerRoutes()` after `init()` is called.

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/ota/unified` | Returns JSON with `state`, `message`, `progress`, `bytes`, `total`, `update_url`, `auto_reboot`. |
| `POST` | `/api/ota/unified` | Returns current state (same fields as GET). |
| `GET` | `/api/ota/status` | Returns JSON with `state`, `progress`, `downloaded`, `total`, `lastResult`, `lastVersion`, `autoReboot`. |
| `POST` | `/api/ota/check` | Triggers an immediate manifest check. Returns `{"success": true}`. |
| `POST` | `/api/ota/update` | Starts a firmware download. Accepts `url` and `force` parameters. Without parameters, returns current field values. |
| `GET` | `/ota/upload` | Serves a minimal HTML firmware upload page (only when `enableWebUIUpload` is `true`). |
| `POST` | `/api/ota/upload` | Accepts `multipart/form-data` firmware upload (only when `enableWebUIUpload` is `true`). Returns `{"success": true/false}`. |

---

## Update HAL

Defined in `DomoticsCore/Update_HAL.h`. This is a routing header that includes the appropriate platform implementation:

| Platform | Implementation Header | Notes |
|----------|-----------------------|-------|
| ESP32 | `Update_ESP32.h` | Direct flash write via ESP32 Update library |
| ESP8266 | `Update_ESP8266.h` | Buffered writes required due to dual-partition constraints |
| Other | `Update_Stub.h` | No-op stub for native testing |

### Key HAL Functions Used

| Function | Description |
|----------|-------------|
| `HAL::OTAUpdate::begin(size)` | Initialize flash partition for writing |
| `HAL::OTAUpdate::write(data, len)` | Write firmware chunk |
| `HAL::OTAUpdate::end(true)` | Finalize flash write |
| `HAL::OTAUpdate::abort()` | Cancel in-progress update |
| `HAL::OTAUpdate::errorString()` | Last error description |
| `HAL::OTAUpdate::hasPendingData()` | ESP8266 buffering: check if data awaits processing |
| `HAL::OTAUpdate::processBuffer(error)` | ESP8266 buffering: flush pending data to flash |
| `HAL::OTAUpdate::requiresBuffering()` | `true` on ESP8266, `false` on ESP32 |
| `HAL::OTAUpdate::hasBufferOverflow()` | Check if write buffer was exceeded |
| `HAL::OTAUpdate::getBytesWritten()` | Total bytes committed to flash |
| `HAL::SHA256` | SHA-256 context for download integrity verification |
