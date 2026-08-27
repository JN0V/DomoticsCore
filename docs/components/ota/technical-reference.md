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
14. [Internal Helpers (Private)](#internal-helpers-private)
15. [Known Issues and Warnings](#known-issues-and-warnings)

---

## OTAConfig Struct

Defined in `DomoticsCore/OTA.h` within `DomoticsCore::Components`.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `updateUrl` | `String` | `""` | Direct firmware binary URL. Used when no manifest is configured. |
| `manifestUrl` | `String` | `""` | Optional JSON manifest endpoint providing version, URL, SHA-256, and signature. |
| `checkIntervalMs` | `uint32_t` | `3600000` | Automatic periodic check interval in milliseconds. Set to `0` to disable periodic checks. |
| `allowDowngrades` | `bool` | `false` | When `true`, permit installing firmware with a lower semantic version. |
| `autoReboot` | `bool` | `true` | When `true`, reboot automatically 2 seconds after a successful update. |
| `maxDownloadSize` | `size_t` | `0` | Ceiling on an incoming firmware image, in bytes. `0` means unlimited. Applies to **downloads and uploads alike** since SEC-8, and is checked twice on each: once against the size the sender announces, and again against the bytes that actually arrive — the announced figure is one the sender chose, and on an upload it is optional. On the upload path the announced-size refusal lands before flash is erased. |
| `enableWebUIUpload` | `bool` | `true` | When `true`, expose manual firmware upload routes and WebUI file-upload controls. |
| `requireUploadHash` | `bool` | `false` | When `true`, refuse any upload that arrives without an expected SHA-256 (SEC-7). The refusal happens before a flash sector is erased. Note this also rejects the built-in `/ota/upload` browser form, which cannot send one — see below. |

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
| `loop()` | `void` | Processes pending URL updates, pending manifest checks, periodic auto-checks, and auto-reboot countdown. |
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
| `getTotalBytes()` | `size_t` | **While a transfer runs**, the size that was announced — `Content-Length` on a download, the multipart body on an upload — or 0 if none was. **Once it finishes**, on both paths, the bytes actually counted. SEC-9 and TEST-8: an announced size is a number the other end chose, and on the browser path it is the whole multipart body, ~220 bytes larger than the firmware. It survives only as `ota/progress`'s denominator, where it is the only figure there is; `ota/end` and `ota/completed` report what arrived. |
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

## Partition Requirements (ESP32)

OTA needs **two application slots**. It writes the new firmware into the one that
is not running, then points the bootloader at it. With a single slot there is
nowhere to write.

A single-slot table does not fail politely. `esp_ota_get_next_update_partition()`
returns **the running partition** rather than `NULL` — there is no other OTA slot
to cycle to — so `Update` tries to erase the code it is executing from, and
ESP-IDF calls `abort()` inside `spi_flash` with no message. What you see is a
panic and a backtrace several frames deep in flash-driver internals, with nothing
pointing at the partition table.

Several stock tables are single-slot, `huge_app.csv` among them, and some boards
select one by default — the `esp32cam` board does. Check before assuming:

```ini
board_build.partitions = default.csv     ; app0 + app1, 1.25 MB each
; or min_spiffs.csv                      ; app0 + app1, 1.92 MB each
```

The slots must be equal in size; OTA does not dictate what that size is. If you
are close to the ceiling, `min_spiffs.csv` buys the most room while keeping two
slots — see CI-9 in `docs/CODE-ROADMAP.md`.

`DomoticsCore-OTA/test/test_ota_esp32/` asserts this as its first test, so a
misconfigured table reports itself instead of panicking later.

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

**Watchdog safety**: During remote downloads, the `installFromUrl` chunk callback calls `HAL::Platform::yield()` after each chunk write to prevent watchdog timer resets on long transfers.

---

## TLS and Transport Security

**`OTAConfig` carries no security fields, and never usefully did.**

Until v2.1.0 it declared `requireTLS`, `rootCA`, `bearerToken`, `basicAuthUser`,
`basicAuthPassword` and `signaturePublicKey`, and this page described them as
working — `requireTLS` as rejecting non-HTTPS URLs, the others as credentials
"passed to the HTTP client". **No code path ever read any of them.** Nothing
rejected an HTTP URL; nothing attached an `Authorization` header; nothing
validated a certificate or a signature. The fields have been removed rather than
left to promise a protection that was never implemented.

The component performs no HTTP requests of its own. It calls the
`ManifestFetcher` and `Downloader` callbacks the application installs, and
**transport security lives entirely inside those callbacks** — that is where the
HTTP client is, so that is where TLS, certificate pinning and any authorization
header belong:

```cpp
ota->setDownloader([](const String& url, ChunkCallback onChunk) -> bool {
    WiFiClientSecure client;
    client.setCACert(MY_ROOT_CA);          // certificate validation, here
    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Authorization", "Bearer " + myToken);   // and auth, here
    // ...
});
```

The WebUI upload endpoints are a separate matter and *are* enforced: they require
WebUI credentials when `WebUIConfig::enableAuth` is set, checked on the first
chunk before any byte reaches flash.

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
bool beginUpload(size_t expectedSize = 0, const String& expectedSha256 = "");
```

Initializes the HAL update subsystem for the given size (or `UPDATE_SIZE_UNKNOWN` if 0). Resets progress counters and transitions to `Downloading` state. Returns `false` if an upload is already active, if `HAL::OTAUpdate::begin()` fails, or if `requireUploadHash` is set and `expectedSha256` is empty.

`expectedSha256` is the hex digest the uploaded image must match. Leave it empty and the image is committed unverified — which is what this endpoint did for every caller before SEC-7. The check itself happens in `finalizeUpload()`; what `beginUpload()` decides is whether to start at all.

**Do not discard the return value.** A refusal here means nothing was written to flash. Ignoring it makes the failure resurface one chunk later as `"Upload not active"`, which says nothing about the cause.

### `acceptUploadChunk`

```cpp
bool acceptUploadChunk(const uint8_t* data, size_t length);
```

Writes a chunk of firmware data to the HAL. On both ESP32 and ESP8266, this writes directly to flash. (ESP8266 uses `Update.runAsync(true)` to enable direct writes from async callbacks.) Returns `false` on write failure. Each chunk is also fed to the session's running SHA-256, hashing what was written rather than what was offered.

### `finalizeUpload`

```cpp
bool finalizeUpload();
```

Finishes the session's digest and, when an expected SHA-256 was supplied, verifies it **before** calling `HAL::OTAUpdate::end()`. Returns `false` if no upload is active, if the digest does not match (`lastError` is `"SHA256 mismatch"`), or if `end()` fails.

The ordering is the security property, not a style choice. `end()` is the point of no return — it switches the ESP32 boot partition and stages the ESP8266 eboot copy — and no Arduino core lets the application undo it, so a rejected image must never reach it. See the contract in `Update_HAL.h`, and SEC-2 in the roadmap for what it cost to learn.

### `abortUpload`

```cpp
void abortUpload(const String& reason);
```

Cancels the current upload, calls `HAL::OTAUpdate::abort()`, transitions to `Error`, and publishes an error event.

---

## Event System (OTAEvents)

Defined in `DomoticsCore/OTAEvents.h`. Namespace: `DomoticsCore::OTAEvents`.

All events are emitted via the Core EventBus with JSON string payloads. Each payload includes `state`, `progress`, and `lastResult` fields in addition to event-specific data.

> **Do not read these payloads yet (BUG-30).** `publishStatusEvent()` publishes a
> `String` through `EventBus::publish(topic, PayloadT)`, which byte-copies the
> object — pointer, length, capacity — into a queue that dispatches after the
> publisher's local has been destroyed. A subscriber registered with
> `core.on<String>("ota/start", …)` gets a `String` reporting the right length
> over freed heap; measured on 2026-08-27 as 114 characters of garbage. The
> guard that would have caught this exists on the sibling `EventType` overload
> (BUG-1) and not on this one.
>
> **Subscribe to the topics, not the payloads.** The topic and its timing carry
> the lifecycle information — see the `EVENT_END` note below — and are safe:
> `core.getEventBus().subscribe("ota/end", [](const void*) { … })` ignores the
> payload pointer and works today. The columns below describe what the payload
> *contains*, and will describe what it *delivers* once BUG-30 is fixed.

| Constant | Topic String | When Emitted | Additional Payload Fields |
|----------|-------------|--------------|---------------------------|
| `EVENT_START` | `"ota/start"` | Transfer opened — after the state reaches `Downloading`, on both the download and the upload path | `source`, plus `url` (download) or `total` (upload) |
| `EVENT_PROGRESS` | `"ota/progress"` | Periodically during transfer (throttled to 1/s for uploads) | `bytes`, `total`, `source` |
| `EVENT_END` | `"ota/end"` | Transfer finished, **before** the SHA-256 verdict and before the commit | `source`, `bytes`, `total` |
| `EVENT_ERROR` | `"ota/error"` | Error encountered | `error`, `source` |
| `EVENT_INFO` | `"ota/info"` | Informational message (e.g., upload started) | `message`, `source` |
| `EVENT_COMPLETED` | `"ota/completed"` | Completion, with reboot status (sticky) | `source`, `autoReboot`, `bytes`, `total`, `progress`, `message` |

> **Fixed in v2.3.0 (BUG-21)**: `EVENT_START` and `EVENT_END` were declared from
> the first release and emitted by nothing. Three documents said so and told
> readers not to subscribe; the constants are now emitted on both the download and
> the upload path.
>
> `EVENT_END` fires **before** verification, which is what makes it worth having:
> between `ota/end` and the next event a subscriber knows the transfer completed
> and the verdict is still out. A transfer that dies produces `ota/start` and
> `ota/error` with no `ota/end` in between; an image that arrives whole and fails
> its hash produces `ota/start`, `ota/end`, `ota/error`. Nothing else distinguishes
> the two.
>
> `EVENT_INFO` still fires at the start of an upload. This reference named it as
> the upload-start signal, and the library is installed by version — `ota/start`
> is added alongside it, not in its place.

> **Removed in v2.1.0 (DC-6)**: `EVENT_COMPLETE` (`"ota/complete"`). It fired
> immediately before `EVENT_COMPLETED`, carrying the progress fields while the
> latter carried the reboot decision — two topics one letter apart. They are
> consolidated onto `EVENT_COMPLETED`, whose payload now carries both sets.
>
> **A subscriber to `"ota/complete"` stops receiving without any error.** The
> constant is gone, so C++ code referencing it fails to compile — but code
> subscribing by string literal compiles, runs, and silently never fires.
> Search your project for `"ota/complete"` as a string, not just for the
> constant.

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

The provider builds a unified settings card using the `ota_unified` context ID. The `handleWebUIRequest` method also accepts `ota_manager` as an alias for the same context, allowing either context ID to be used interchangeably for WebUI interactions.

The card includes the following fields:

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

### Internal Utility

- `respondJson(request, fn)` -- static template helper that creates an `AsyncResponseStream`, builds a `JsonDocument` via the provided lambda, serializes, and sends. Used by all REST endpoint handlers.
- `stateToString(state)` -- static method converting `OTAComponent::State` enum to lowercase strings (`"idle"`, `"checking"`, `"downloading"`, `"applying"`, `"reboot_pending"`, `"error"`). Note: a separate free function with the same name exists in the anonymous namespace of `OTA.cpp`.

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
| `POST` | `/api/ota/upload` | Accepts `multipart/form-data` firmware upload (only when `enableWebUIUpload` is `true`). Returns `{"success": true/false}`. Optionally carries the expected digest as an `X-Firmware-SHA256` header, or a `?sha256=` query parameter for clients that cannot set headers. |

#### Supplying the upload digest (SEC-7)

```bash
curl -u admin:secret \
     -H "X-Firmware-SHA256: $(sha256sum firmware.bin | cut -d' ' -f1)" \
     -F 'firmware=@firmware.bin' \
     http://device.local/api/ota/upload
```

The digest must arrive as a header or a query parameter, not as a multipart form field: both are parsed before the body, so both are available when the upload starts, whereas a form field arrives wherever it happens to sit in the body.

The built-in `/ota/upload` browser form sends no digest and keeps working — verification is opt-in per upload. It cannot be made to send one: setting a header needs JavaScript, and computing SHA-256 in a browser needs `crypto.subtle`, which is unavailable outside a secure context. A device answering plain HTTP on a LAN is not one. Set `requireUploadHash` if that trade is unacceptable for your deployment, and accept that the browser form stops working.

---

## Update HAL

Defined in `DomoticsCore/Update_HAL.h`. This is a routing header that includes the appropriate platform implementation:

| Platform | Implementation Header | Notes |
|----------|-----------------------|-------|
| ESP32 | `Update_ESP32.h` (100 lines) | Direct flash write via ESP32 `Update` library |
| ESP8266 | `Update_ESP8266.h` (120 lines) | Direct flash write using `Update.runAsync(true)` for async-safe writes |
| Other | `Update_Stub.h` (48 lines) | No-op stub for native testing (all writes succeed) |

### Key HAL Functions Used

All three platform implementations expose the same interface in `DomoticsCore::HAL::OTAUpdate`:

| Function | Description |
|----------|-------------|
| `HAL::OTAUpdate::begin(size)` | Initialize flash partition for writing |
| `HAL::OTAUpdate::write(data, len)` | Write firmware chunk |
| `HAL::OTAUpdate::end(true)` | Finalize flash write |
| `HAL::OTAUpdate::abort()` | Cancel in-progress update |
| `HAL::OTAUpdate::errorString()` | Last error description |
| `HAL::OTAUpdate::hasError()` | Error flag |
| `HAL::OTAUpdate::hasPendingData()` | Always returns `false` (legacy buffering API, now unused on all platforms) |
| `HAL::OTAUpdate::processBuffer(error)` | No-op on all platforms (returns `0`). Retained for interface compatibility. |
| `HAL::OTAUpdate::requiresBuffering()` | Returns `false` on all platforms (ESP8266 now uses `Update.runAsync(true)` for direct writes) |
| `HAL::OTAUpdate::hasBufferOverflow()` | Always returns `false` (no buffering on any platform) |
| `HAL::OTAUpdate::getBytesWritten()` | Total bytes committed to flash |
| `HAL::SHA256` | SHA-256 context for download integrity verification |

### Platform-Specific Notes

- **ESP32**: Uses the standard `Update` library. `UPDATE_SIZE_UNKNOWN` is `0xFFFFFFFF`.
- **ESP8266**: `Update.runAsync(true)` disables `yield()` inside `Update.write()`, preventing `__yield` panic in async callbacks. When size is `0`, calculates available space via `ESP.getFreeSketchSpace()`. `UPDATE_SIZE_UNKNOWN` is `0`. Abort explicitly calls `Update.runAsync(false)` and `Update.clearError()`.
- **Stub**: All operations succeed. `errorString()` returns `"Update not supported on this platform"`. `UPDATE_SIZE_UNKNOWN` is `0xFFFFFFFF`.

---

## Internal Helpers (Private)

These private methods of `OTAComponent` are documented for maintainers and AI agents.

| Method | Signature | Purpose |
|--------|-----------|---------|
| `scheduleNextCheck` | `void scheduleNextCheck(uint32_t delayMs = 0)` | Sets `nextCheckMillis` for periodic auto-check. Uses `config.checkIntervalMs` if `delayMs` is 0. No-op if both are 0. |
| `transition` | `void transition(State next, const String& reason = "")` | Changes state, resets `stateChangeMillis` and `lastProgressPublishMillis`, logs the transition. |
| `shouldCheckNow` | `bool shouldCheckNow() const` | Returns `true` if `checkIntervalMs > 0` and the timer has elapsed. |
| `performCheck` | `bool performCheck(bool force)` | Orchestrates manifest fetch, version comparison, and `installFromUrl`. |
| `fetchManifest` | `ManifestInfo fetchManifest()` | Calls `manifestFetcher` callback, parses JSON into `ManifestInfo`. |
| `installFromUrl` | `bool installFromUrl(const String& url, const String& expectedSha256, bool allowDowngrade)` | Downloads firmware via `downloader` callback, writes chunks through HAL, computes SHA-256, calls `finalizeUpdateOperation`. |
| `finalizeUpdateOperation` | `bool finalizeUpdateOperation(const String& source, bool autoRebootPending)` | Sets progress to 100%, emits `EVENT_COMPLETE` and `EVENT_COMPLETED` (sticky), transitions to `RebootPending` or `Idle`. |
| `verifySha256` | `bool verifySha256(const uint8_t* digest, const String& expectedHex)` | Compares computed digest against expected hex (case-insensitive, strips spaces and colons). |
| `isNewerVersion` | `bool isNewerVersion(const String& candidate) const` | Semantic version comparison (major.minor.patch) against `metadata.version`. |
| `broadcastProgress` | `void broadcastProgress()` | **Dead code**: defined but never called. Emits a progress event with `percent`, `downloaded`, `total`, `state`. All actual progress events use `publishStatusEvent()` instead. |
| `publishStatusEvent` | `void publishStatusEvent(const String& topic, fn, bool sticky)` | Builds JSON payload via callback, auto-injects `state`, `progress`, `lastResult`, serializes and emits via EventBus. |

---

## Known Issues and Warnings

| ID | Severity | Description |
|----|----------|-------------|
| C14 | ~~Minor~~ | **Resolved (BUG-21).** `EVENT_START` and `EVENT_END` are emitted on both the download and the upload path. See the event table above. |
| C15 | Minor | `State::Applying` exists in the enum but `transition(State::Applying, ...)` is never called. |
| BUG-1 | Minor | `OTAWebUI::getWebUIVersion()` returns hardcoded `"1.4.0"` instead of `"1.4.1"`. |
| DEAD-1 | Cosmetic | `broadcastProgress()` is defined but never called (607 lines in OTA.cpp). |
| LEGACY-1 | Cosmetic | `loop()` checks `hasPendingData()` / `processBuffer()` which are no-ops on all platforms since buffering was removed. |
