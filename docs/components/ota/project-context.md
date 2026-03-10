# DomoticsCore-OTA -- Project Context (AI Agent)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides structured context for AI coding agents working on the DomoticsCore-OTA component. It covers identity, file inventory, class hierarchy, dependencies, coding conventions, platform caveats, and constitution compliance.

---

## Component Identity

| Field | Value |
|-------|-------|
| Library name | `DomoticsCore-OTA` |
| Version | **1.4.1** |
| `metadata.name` | `"OTA"` |
| `metadata.version` | `"1.4.1"` (must match `library.json`) |
| `metadata.category` | `"system"` |
| Type key | `"ota"` |
| Namespace | `DomoticsCore::Components` (core), `DomoticsCore::Components::WebUI` (provider), `DomoticsCore::OTAEvents` (events) |
| Platforms | `espressif32`, `espressif8266` |
| Frameworks | `arduino` |
| License | MIT |

---

## File Inventory

**Important**: Unlike most DomoticsCore components, OTA has a `.cpp` implementation file. This is not header-only.

```
DomoticsCore-OTA/
  include/
    DomoticsCore/
      OTA.h              -- OTAConfig struct, OTAComponent class declaration
      OTAEvents.h        -- Event topic constants (ota/start, ota/progress, etc.)
      OTAWebUI.h         -- OTAWebUI provider (header-only, CachingWebUIProvider)
      Update_HAL.h       -- Routing header: includes Update_ESP32.h, Update_ESP8266.h, or Update_Stub.h
      Update_ESP32.h     -- ESP32 HAL: direct flash write via ESP32 Update library
      Update_ESP8266.h   -- ESP8266 HAL: direct write using Update.runAsync(true)
      Update_Stub.h      -- No-op stub for native testing (all writes succeed, no flash)
  src/
    OTA.cpp              -- OTAComponent implementation (state machine, download, upload, SHA-256)
  examples/
    BasicOTA/            -- Minimal OTA with periodic URL checking, no WebUI
    OTAWithWebUI/        -- Full OTA with WebUI browser-based upload via Access Point
  test/
    test_ota_component/
      test_ota_component.cpp  -- 30 Unity tests: events, config, state, upload, lifecycle, integration
  library.json           -- PlatformIO library manifest (v1.4.1)
  platformio.ini         -- Native test environment (Unity, gnu++17)
  README.md              -- Component-level README with usage examples
```

### Line Counts

| File | Lines | Notes |
|------|-------|-------|
| `OTA.h` | 154 | Config struct (13 fields) + class declaration |
| `OTAEvents.h` | 36 | Seven event topic constants |
| `OTAWebUI.h` | 393 | Full WebUI provider with route registration |
| `Update_HAL.h` | 25 | Platform routing only |
| `Update_ESP32.h` | 100 | ESP32 HAL: direct flash write, no buffering |
| `Update_ESP8266.h` | 120 | ESP8266 HAL: `Update.runAsync(true)`, no buffering |
| `Update_Stub.h` | 48 | Stub HAL: all writes succeed silently |
| `OTA.cpp` | 607 | Core logic -- approaching 800-line constitution limit |
| **Total** | **1483** | All source files combined |

---

## Key Classes and Types

### `OTAConfig` (struct)

Plain configuration struct with 13 fields. No methods. Passed by value to constructor and `setConfig()`.

### `OTAComponent` (class)

- Inherits: `IComponent`
- State enum: `Idle`, `Checking`, `Downloading`, `Applying`, `RebootPending`, `Error` (**Note (C15)**: `Applying` is defined in the enum but never entered at runtime -- the code transitions directly from `Downloading` to `RebootPending` or `Idle`)
- Transport callbacks: `ManifestFetcher`, `Downloader` (both `std::function`, nullable)
- Internal structs: `ManifestInfo` (parsed manifest), `UploadSession` (upload state tracking)
- Key pattern: deferred execution -- `triggerImmediateCheck()` and `triggerUpdateFromUrl()` set pending flags; actual work happens in `loop()`

### `OTAWebUI` (class)

- Inherits: `CachingWebUIProvider`
- Holds non-owning pointers to `OTAComponent*` and `WebUIComponent*`
- Requires explicit `init(WebUIComponent*)` call after construction (route registration needs server access)
- Uses `LazyState<OTAState>` for change detection (tracks `state`, `progress`, `bytes`)
- Registers both standard WebUI contexts and custom REST/upload routes
- `handleWebUIRequest` accepts both `ota_unified` and `ota_manager` as context IDs
- Internal `UploadState` struct tracks active upload metadata (filename, total, success/error)
- **Bug (minor)**: `getWebUIVersion()` returns hardcoded `"1.4.0"` instead of `"1.4.1"` -- mismatch with `library.json` and `metadata.version`

### `OTAEvents` (namespace)

Seven `static constexpr const char*` constants. No classes or functions. **Note (C14)**: `EVENT_START` (`"ota/start"`) and `EVENT_END` (`"ota/end"`) are declared but never emitted by `OTAComponent`. Only `EVENT_PROGRESS`, `EVENT_ERROR`, `EVENT_INFO`, `EVENT_COMPLETE`, and `EVENT_COMPLETED` are actually emitted in `OTA.cpp`.

---

## Dependencies

### Build Dependencies (`library.json`)

| Dependency | Version | Purpose |
|------------|---------|---------|
| ArduinoJson | >= 7.0.0 | JSON manifest parsing, status event serialization |

### Runtime Dependencies (implicit via includes)

| Dependency | Headers Used | Purpose |
|------------|-------------|---------|
| DomoticsCore-Core | `IComponent.h`, `Logger.h`, `Events.h`, `Platform_HAL.h` | Base component interface, logging macros, EventBus, HAL platform utilities |
| DomoticsCore-WebUI | `IWebUIProvider.h`, `WebUI.h`, `BaseWebUIComponents.h` | WebUI provider interface, route registration, field types (only for OTAWebUI) |

OTA does **not** depend on DomoticsCore-Wifi, DomoticsCore-MQTT, or any other component. Network transport is fully delegated to application-injected callbacks.

### Test Dependencies

| Dependency | Purpose |
|------------|---------|
| Unity | Test framework (PlatformIO native) |
| DomoticsCore-Core | Provides `Core`, `IComponent`, stubs |

### Build Configuration

The native test environment (`platformio.ini`) uses `gnu++17`, `CORE_DEBUG_LEVEL=3`, and includes both Core and OTA headers. Tests link against `ArduinoJson@^7.0.0`.

---

## Conventions and Patterns

### Deferred Execution

All control methods (`triggerImmediateCheck`, `triggerUpdateFromUrl`) set `pending*` flags. The actual network and flash operations run inside `loop()`. This avoids blocking the caller and ensures operations execute in the main task context.

### Get-Override-Set for Config Updates

Both the component and WebUI use the pattern:
```cpp
OTAConfig cfg = ota->getConfig();
cfg.fieldName = newValue;
ota->setConfig(cfg);
```
This avoids exposing individual setters and keeps the config struct as the single source of truth.

### Event Publishing

All events are emitted as serialized JSON strings via `emit<String>(topic, payload, sticky)`. The `publishStatusEvent` helper automatically injects `state`, `progress`, and `lastResult` into every event payload.

### Progress Throttling

Upload progress events are throttled to once per second (`lastProgressPublishMillis`) to prevent EventBus queue overflow. Log messages are throttled to every 10% or every 256 KB.

### Platform HAL Isolation

All `#ifdef` platform branching is confined to `Update_HAL.h` and the platform-specific `Update_ESP32.h` / `Update_ESP8266.h` / `Update_Stub.h` headers. Business logic in `OTA.cpp` calls `HAL::OTAUpdate::*` and `HAL::Platform::*` functions without any conditional compilation.

---

## Testing

### Unit Tests (`test/test_ota_component/test_ota_component.cpp`)

30 Unity tests organized into 10 groups:

| Group | Tests | Coverage |
|-------|-------|----------|
| Event constants | 2 | All 7 `OTAEvents::EVENT_*` constants verified |
| Component creation | 3 | Default construction, config construction, type key |
| Config | 4 | Defaults, get/set, auth options, security options |
| State machine | 3 | Initial state, accessors, idle/busy |
| Triggers | 2 | `triggerImmediateCheck` and `triggerUpdateFromUrl` without providers |
| Upload session | 4 | Begin, chunk-before-begin, abort, finalize-without-begin |
| Lifecycle | 4 | begin, shutdown, loop crash safety (1000 iterations), full sequence |
| Providers | 2 | `setManifestFetcher`, `setDownloader` |
| Non-blocking | 1 | 1000 loops in <1s (validates no blocking) |
| Integration | 4 | Core registration, component lookup, check interval disabled/config |

Tests run on `native` platform using the `Update_Stub.h` HAL. No real firmware flashing occurs.

### Examples

| Example | Description | Key Concept |
|---------|-------------|-------------|
| `BasicOTA` | Minimal OTA with periodic URL checking, no WebUI. WiFi STA mode. | `OTAComponent` standalone, `checkIntervalMs` auto-check |
| `OTAWithWebUI` | Full OTA with WebUI browser upload, WiFi AP mode. | `OTAWebUI` provider, `init()` post-begin, `registerProviderWithComponent` |

---

## Platform HAL Details

### ESP32 (`Update_ESP32.h`)

ESP32 can write directly to flash from async context (no yield panic). Uses the standard ESP32 `Update` library. All buffering API functions return `false`/no-op.

### ESP8266 (`Update_ESP8266.h`)

On ESP8266, the OTA HAL uses `Update.runAsync(true)` to enable direct flash writes from AsyncWebServer callbacks without `__yield` panic. This eliminates the need for buffered writes. When `size == 0`, the HAL calculates available sketch space via `ESP.getFreeSketchSpace()`.

The buffering API functions still exist in the HAL for interface compatibility, but they are all no-ops:
- `requiresBuffering()` returns `false`.
- `hasPendingData()` returns `false`.
- `hasBufferOverflow()` returns `false`.
- `processBuffer()` is a no-op that returns `0`.

Both ESP32 and ESP8266 now perform direct writes via `HAL::OTAUpdate::write()` and immediate finalization via `HAL::OTAUpdate::end()`.

### Native Stub (`Update_Stub.h`)

Used for native unit tests. All writes succeed silently; `begin()` always returns `true`, `write()` accumulates byte count, `end()` returns `true`. `errorString()` returns `"Update not supported on this platform"`.

### Common HAL Interface

All three implementations expose the same function set in `DomoticsCore::HAL::OTAUpdate`:

| Function | Signature | Purpose |
|----------|-----------|---------|
| `begin` | `bool begin(size_t)` | Initialize flash partition |
| `write` | `size_t write(uint8_t*, size_t)` | Write firmware chunk |
| `end` | `bool end(bool)` | Finalize flash write |
| `abort` | `void abort()` | Cancel in-progress update |
| `errorString` | `String errorString()` | Last error description |
| `hasError` | `bool hasError()` | Error flag |
| `requiresBuffering` | `bool requiresBuffering()` | Always `false` on all platforms |
| `hasPendingData` | `bool hasPendingData()` | Always `false` on all platforms |
| `hasBufferOverflow` | `bool hasBufferOverflow()` | Always `false` on all platforms |
| `getBytesWritten` | `size_t getBytesWritten()` | Total bytes committed to flash |
| `processBuffer` | `int processBuffer(String&)` | No-op on all platforms (returns `0`) |

---

## Constitution Compliance Notes

| Principle | Status | Notes |
|-----------|--------|-------|
| SOLID / SRP | Compliant | `OTAComponent` handles update logic; `OTAWebUI` handles presentation; transport is injected |
| HAL Isolation (IX) | Compliant | All `#ifdef` confined to `Update_HAL.h` and platform headers |
| Non-Blocking (X) | Compliant | Deferred execution via pending flags; `loop()` processes work; `delay()` only used in 100ms pre-reboot pause |
| File Size (VII) | Warning | `OTA.cpp` is 607 lines, within the 800-line hard limit but above the 500-line target. `OTAWebUI.h` is 393 lines. Monitor growth. |
| EventBus (VI) | Compliant | All status and progress updates published via `emit<String>()` |
| Memory (XIV) | Compliant | Upload progress throttled; no String concatenation in hot paths; PROGMEM used for HTML |
| Versioning (XV) | Compliant | `library.json` version `1.4.1` matches `metadata.version` in constructor |
| Anti-Patterns (XIII) | Compliant | No singletons; centralized event constants in `OTAEvents.h`; dependencies declared |

### Potential Improvement Areas

- `OTA.cpp` line count (607) should be monitored. If new features are added, consider extracting manifest fetching or version comparison into a separate utility.
- `OTAWebUI.h` contains both context building and route registration. If it grows further, route registration could be extracted into a helper.
- The `signaturePublicKey` config field is declared but not yet used in the verification flow -- this is YAGNI-compliant for now but should be documented if implemented.
- **(C14)** `EVENT_START` and `EVENT_END` are defined in `OTAEvents.h` but never emitted by `OTAComponent`. Either add `emit()` calls at the appropriate points (beginning of download/upload and end of transfer before verification), or remove the unused constants to avoid misleading subscribers.
- **(C15)** `State::Applying` is defined in the enum and checked in `isBusy()` / `stateToString()` but is never entered via `transition()`. Either introduce a transition to `Applying` after download completes (before `finalizeUpdateOperation`), or remove the state from the enum to match actual runtime behavior.
- **(Bug)** `OTAWebUI::getWebUIVersion()` returns hardcoded `"1.4.0"` instead of reading `metadata.version` (which is `"1.4.1"`). Should be updated to match.
- **(Dead code)** `OTAComponent::broadcastProgress()` (OTA.cpp line 587) is defined but never called. The method constructs a JSON payload and emits it on `EVENT_PROGRESS`, but all actual progress broadcasting uses `publishStatusEvent()` instead. Consider removing or wiring it into the download path.
- **(Legacy code)** The `loop()` method still checks `HAL::OTAUpdate::hasPendingData()` and calls `HAL::OTAUpdate::processBuffer()`, which are no-ops on all platforms since the buffering strategy was removed. These code paths are dead but harmless.
