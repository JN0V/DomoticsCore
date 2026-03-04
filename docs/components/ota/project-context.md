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
  src/
    OTA.cpp              -- OTAComponent implementation (state machine, download, upload, SHA-256)
  library.json           -- PlatformIO library manifest (v1.4.1)
  README.md              -- Component-level README with usage examples
```

### Line Counts (approximate)

| File | Lines | Notes |
|------|-------|-------|
| `OTA.h` | ~155 | Config struct + class declaration |
| `OTAEvents.h` | ~37 | Seven event topic constants |
| `OTAWebUI.h` | ~394 | Full WebUI provider with route registration |
| `Update_HAL.h` | ~26 | Platform routing only |
| `OTA.cpp` | ~608 | Core logic -- approaching 800-line constitution limit |

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
- Uses `LazyState<OTAState>` for change detection
- Registers both standard WebUI contexts and custom REST/upload routes

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

## ESP8266 Dual-Partition Caveat

On ESP8266, the Update library cannot write to flash while handling HTTP requests on the same thread. The OTA component addresses this with a buffered-write strategy:

1. `acceptUploadChunk()` calls `HAL::OTAUpdate::write()`, which buffers data internally on ESP8266 (direct write on ESP32).
2. `finalizeUpload()` calls `HAL::OTAUpdate::end(true)`, which on ESP8266 merely marks the buffer as "finalizing".
3. In `loop()`, when `HAL::OTAUpdate::hasPendingData()` returns `true`, the component calls `HAL::OTAUpdate::processBuffer()` to flush data to flash incrementally.
4. `processBuffer()` returns: `-1` for error, `0` for "continue next loop", `+1` for "done".
5. On ESP32, `requiresBuffering()` returns `false`, and all writes and finalization are immediate.

Additionally, `HAL::OTAUpdate::hasBufferOverflow()` detects when upload data arrives faster than flash writes can process, which produces a specific error message.

**AI agents must preserve this buffering flow when modifying upload logic.** Breaking the deferred-write pattern will cause ESP8266 crashes or corrupted firmware.

---

## Constitution Compliance Notes

| Principle | Status | Notes |
|-----------|--------|-------|
| SOLID / SRP | Compliant | `OTAComponent` handles update logic; `OTAWebUI` handles presentation; transport is injected |
| HAL Isolation (IX) | Compliant | All `#ifdef` confined to `Update_HAL.h` and platform headers |
| Non-Blocking (X) | Compliant | Deferred execution via pending flags; `loop()` processes work; `delay()` only used in 100ms pre-reboot pause |
| File Size (VII) | Warning | `OTA.cpp` is ~608 lines, within the 800-line hard limit but above the 500-line target. `OTAWebUI.h` is ~394 lines. Monitor growth. |
| EventBus (VI) | Compliant | All status and progress updates published via `emit<String>()` |
| Memory (XIV) | Compliant | Upload progress throttled; no String concatenation in hot paths; PROGMEM used for HTML |
| Versioning (XV) | Compliant | `library.json` version `1.4.1` matches `metadata.version` in constructor |
| Anti-Patterns (XIII) | Compliant | No singletons; centralized event constants in `OTAEvents.h`; dependencies declared |

### Potential Improvement Areas

- `OTA.cpp` line count (608) should be monitored. If new features are added, consider extracting manifest fetching or version comparison into a separate utility.
- `OTAWebUI.h` contains both context building and route registration. If it grows further, route registration could be extracted into a helper.
- The `signaturePublicKey` config field is declared but not yet used in the verification flow -- this is YAGNI-compliant for now but should be documented if implemented.
- **(C14)** `EVENT_START` and `EVENT_END` are defined in `OTAEvents.h` but never emitted by `OTAComponent`. Either add `emit()` calls at the appropriate points (beginning of download/upload and end of transfer before verification), or remove the unused constants to avoid misleading subscribers.
- **(C15)** `State::Applying` is defined in the enum and checked in `isBusy()` / `stateToString()` but is never entered via `transition()`. Either introduce a transition to `Applying` after download completes (before `finalizeUpdateOperation`), or remove the state from the enum to match actual runtime behavior.
