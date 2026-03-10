# DomoticsCore-NTP -- AI Project Context

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides structured context for AI assistants working on the DomoticsCore-NTP component.

---

## Component Identity

| Property | Value |
|---|---|
| Name | DomoticsCore-NTP |
| Registered name | `"NTP"` |
| Version | 1.3.0 |
| Author | JN0V (DomoticsCore) |
| License | MIT |
| Architecture | Header-only, HAL-isolated |
| Platforms | `espressif32`, `espressif8266` |
| Frameworks | Arduino |

---

## File Inventory

```
DomoticsCore-NTP/
  library.json                              # PlatformIO package manifest (v1.3.0)
  platformio.ini                            # Build configuration for component tests
  README.md                                 # Component README with usage examples
  SPECIFICATIONS.md                         # Design specifications document (v0.1.0, partially outdated)
  include/DomoticsCore/
    NTP.h                      (524 lines)  # NTPConfig, NTPStatistics, NTPComponent class
    NTPEvents.h                 (21 lines)  # Event constants: ntp/synced, ntp/sync_failed
    NTP_HAL.h                   (95 lines)  # HAL routing header (selects platform impl)
    NTP_ESP32.h                 (49 lines)  # ESP32 implementation using esp_sntp
    NTP_ESP8266.h               (46 lines)  # ESP8266 implementation using configTime/sntp
    NTP_Stub.h                  (27 lines)  # No-op stubs for native/test builds
    NTPWebUI.h                 (338 lines)  # WebUI provider, timezone lookup table
  test/
    test_ntp_component/
      test_ntp_component.cpp   (571 lines)  # 33 Unity tests (events, lifecycle, config, memory)
  examples/
    BasicNTP/                               # Minimal time sync example
    NTPWithWebUI/                           # Full WebUI integration example
```

### Documentation Files

```
docs/components/ntp/
  README.md                                 # Lightweight overview and quick start
  technical-reference.md                    # Full API reference
  project-context.md                        # This file (AI context)
```

### Related Test Files

```
tests/mocks/
  MockNTPClient.h                           # Mock for native unit testing
```

---

## Key Classes and Structures

### `DomoticsCore::Components::NTPComponent`

The main component class. Inherits `IComponent`. Registered as `"NTP"` in the component registry.

- **Lifecycle**: `begin()` initializes the SNTP client via HAL; `loop()` detects sync completion or timeout; `shutdown()` stops the client.
- **State**: Tracks `synced` (bool), `syncInProgress` (bool), `bootTime` (uint32_t).
- **Timer**: Uses `Utils::NonBlockingDelay` for sync timeout tracking (constitution Principle X).
- **Events**: Emits `ntp/synced` and `ntp/sync_failed` via `IComponent::emit()`.
- **Type alias**: `SyncCallback = std::function<void(bool success)>`.

### `DomoticsCore::Components::NTPConfig`

Plain struct for configuration. Fields: `enabled`, `servers` (vector, max 3), `syncInterval` (seconds), `timezone` (POSIX TZ), `timeoutMs`.

### `DomoticsCore::Components::NTPStatistics`

Plain struct for runtime statistics. Fields: `syncCount`, `syncErrors`, `lastSyncTime`, `lastSyncDuration`, `lastFailTime`, `consecutiveFailures`.

### `DomoticsCore::Components::Timezones`

Namespace containing 11 `constexpr const char*` timezone presets: UTC, EST, CST, MST, PST, CET, GMT, JST, AEST, IST, NZST.

### `DomoticsCore::NTPEvents`

Namespace containing two `static constexpr const char*` event topic constants: `EVENT_SYNCED` and `EVENT_SYNC_FAILED`.

### `DomoticsCore::HAL::NTP`

HAL namespace providing platform-independent inline functions: `init`, `setTimezone`, `setSyncInterval`, `stop`, `forceSync`, `isSynced`, `getTime`, `getFormattedTime`.

### `DomoticsCore::Components::WebUI::NTPWebUI`

WebUI provider extending `CachingWebUIProvider`. Holds a non-owning pointer to `NTPComponent`. Exposes two UI contexts (`ntp_time`, `ntp_settings`) and registers the `/api/ntp/timezones` endpoint. Contains a 29-entry constexpr timezone lookup table stored in flash.

**Note**: The class docstring in `NTPWebUI.h` (lines 67-71) references four UI contexts (`ntp_status`, `ntp_dashboard`, `ntp_settings`, `ntp_detail`), but only two are implemented in `buildContexts()`. The docstring is stale and inherited from the original `SPECIFICATIONS.md` design.

### `DomoticsCore::Components::WebUI::TimezoneLookupEntry`

Plain struct with two `const char*` fields (`posix`, `friendly`) used in the flash-stored `TIMEZONE_LOOKUP` constexpr array (29 entries).

---

## Dependencies

### Required

| Dependency | Minimum Version | What It Provides |
|---|---|---|
| DomoticsCore-Core | >= 1.0.0 | `IComponent`, `Logger`, `Timer` (NonBlockingDelay), `Platform_HAL`, EventBus |

### Optional (for WebUI integration)

| Dependency | Minimum Version | What It Provides |
|---|---|---|
| DomoticsCore-WebUI | >= 0.1.0 | `CachingWebUIProvider`, `WebUIComponent`, `IWebUIProvider`, `LazyState<T>` |
| ArduinoJson | (transitive via WebUI) | JSON serialization in `NTPWebUI` |

### System Libraries (provided by platform SDK)

- `<time.h>`, `<sys/time.h>` -- standard C time functions
- `<esp_sntp.h>` -- ESP32 SNTP API
- `<sntp.h>` -- ESP8266 SNTP API

---

## Conventions and Patterns

### Naming

- Component class: `NTPComponent` (PascalCase, suffixed with `Component`).
- Config struct: `NTPConfig`.
- Statistics struct: `NTPStatistics`.
- Events namespace: `NTPEvents` with `EVENT_` prefixed constants.
- HAL files: `NTP_HAL.h` (router), `NTP_ESP32.h`, `NTP_ESP8266.h`, `NTP_Stub.h`.
- WebUI provider: `NTPWebUI`.
- Log tag: `LOG_NTP`.

### Component Registration

The component registers itself with `metadata.name = "NTP"`, `metadata.version = "1.3.0"`, `metadata.author = "DomoticsCore"`, and `metadata.description = "Network Time Protocol synchronization component"` in the constructor. Version must match `library.json` per constitution Principle XV.

### Event Topics

Follow the `<component>/<action>` hierarchical pattern: `ntp/synced`, `ntp/sync_failed`.

### Sync Detection Heuristic

Time is considered "synced" when `time(nullptr) > 1000000000` (approximately 2001-09-09). The HAL uses a slightly different threshold: `time(nullptr) > 1577836800` (2020-01-01).

### WebUI State Tracking

Uses `LazyState<T>` helper for change detection to avoid unnecessary JSON serialization, following the `CachingWebUIProvider` pattern.

---

## Constitution Compliance

This section maps the NTP component's design to specific constitution principles.

| Principle | Status | Implementation Notes |
|---|---|---|
| I. SOLID | Compliant | `NTPComponent` has single responsibility (time sync). Depends on `IComponent` abstraction. `NTPWebUI` is a separate class (ISP). |
| III. KISS | Compliant | Header-only design. Minimal API surface. Simple sync-detection heuristic. |
| IV. YAGNI | Compliant | No speculative features. Only exposes what consumers need. |
| V. Performance First | Compliant | Flash-stored timezone table saves ~3-4 KB RAM. `NonBlockingDelay` for timeouts. Minimal `loop()` overhead. |
| VI. Modular EventBus | Compliant | Emits `ntp/synced` and `ntp/sync_failed` events. No direct references to other components. |
| VII. File Size Limits | Compliant | `NTP.h` is ~525 lines. `NTPWebUI.h` is ~338 lines. All under the 800-line hard limit. |
| IX. HAL Isolation | Compliant | All `#ifdef` platform directives are confined to `NTP_HAL.h`, `NTP_ESP32.h`, `NTP_ESP8266.h`, `NTP_Stub.h`. Zero platform conditionals in business logic. |
| X. Non-Blocking Timer | Compliant | Uses `Utils::NonBlockingDelay` for sync timeout. No `delay()` calls. `loop()` completes in well under 10 ms. |
| XIII. Anti-Pattern Avoidance | Compliant | No singletons. No magic strings (events are centralized in `NTPEvents.h`). No blocking callbacks. |
| XIV. Memory Leak Prevention | Compliant | No dynamic allocations in `loop()`. Constexpr lookup table avoids runtime allocations. `LazyState` avoids repeated string building. |
| XV. Semantic Versioning | Compliant | `library.json` version `1.3.0` matches `metadata.version` in `NTPComponent` constructor. |

### Areas for Future Attention

- **Principle II (TDD)**: The component has 33 native unit tests (`test/test_ntp_component/test_ntp_component.cpp`) covering events, lifecycle, config, sync status, timezones, callbacks, uptime, edge cases, and memory stability via `HeapTracker`. The mock file `tests/mocks/MockNTPClient.h` also exists. However, `NTPWebUI` has no dedicated unit tests.
- **Principle XI (Centralized Storage)**: Configuration persistence is handled via a callback (`setConfigSaveCallback`), which is the correct decoupled pattern. The callback should delegate to the Storage component, not directly to Preferences.
- **Stale code comment**: `NTPWebUI` class docstring lists four UI contexts but only two (`ntp_time`, `ntp_settings`) are implemented.
- **Version fallback**: `NTPWebUI::getWebUIVersion()` returns `"1.0.2"` as a fallback when the NTP pointer is null, which does not match the current component version `1.3.0`. This is cosmetic but should be updated.

---

## Quick Reference for Common Tasks

### Adding a new timezone preset

Add a new `constexpr const char*` entry to the `Timezones` namespace in `NTP.h` and a corresponding `TimezoneLookupEntry` to the `TIMEZONE_LOOKUP` array in `NTPWebUI.h`.

### Adding a new event

Add a new `static constexpr const char*` constant to `NTPEvents.h` and emit it via `this->emit()` in `NTPComponent`.

### Supporting a new platform

Create a new `NTP_<Platform>.h` file implementing the `DomoticsCore::HAL::NTPImpl` namespace with inline functions: `init`, `setTimezone`, `setSyncInterval`, `stop`, `forceSync`. Add a new `#elif` branch in `NTP_HAL.h`.

### Changing the version

Use `tools/bump_version.py NTP <major|minor|patch>` per constitution Principle XV. Do not manually edit version strings.
