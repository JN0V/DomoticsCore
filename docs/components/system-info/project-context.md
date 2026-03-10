# DomoticsCore-SystemInfo -- Project Context

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This file provides structured context for AI assistants and developers working on the DomoticsCore-SystemInfo component. It captures identity, file inventory, key abstractions, dependencies, conventions, and constitution compliance status.

---

## Component Identity

| Property | Value |
|----------|-------|
| **Name** | DomoticsCore-SystemInfo |
| **Version** | 1.4.0 |
| **Type Key** | `system_info` |
| **Metadata Name** | `"System Info"` |
| **Description** | Runtime diagnostic component: system metrics, CPU/heap, uptime, boot diagnostics |
| **License** | MIT |
| **Author** | JN0V |
| **Platforms** | espressif32, espressif8266 |
| **Frameworks** | Arduino |
| **Repository** | https://github.com/JN0V/DomoticsCore.git |

---

## File Inventory

### Headers

| File | Purpose |
|------|---------|
| `DomoticsCore-SystemInfo/include/DomoticsCore/SystemInfo.h` | Main header. Defines `SystemInfoConfig`, `BootDiagnostics`, `SystemInfoComponent`. Header-only (no .cpp). |
| `DomoticsCore-SystemInfo/include/DomoticsCore/SystemInfoWebUI.h` | WebUI provider. Defines `SystemInfoWebUI`. Header-only (no .cpp). |

### Library Metadata

| File | Purpose |
|------|---------|
| `DomoticsCore-SystemInfo/library.json` | PlatformIO library descriptor (name, version, dependencies, build config). |
| `DomoticsCore-SystemInfo/platformio.ini` | Build configuration for the library itself. |

### Examples

| File | Purpose |
|------|---------|
| `DomoticsCore-SystemInfo/examples/BasicSystemInfo/src/main.cpp` | Console-only demo. Logs metrics every 5 seconds via `DLOG_I`. |
| `DomoticsCore-SystemInfo/examples/BasicSystemInfo/platformio.ini` | Build config for the basic example. |
| `DomoticsCore-SystemInfo/examples/SystemInfoWithWebUI/src/main.cpp` | Full WebUI integration demo. Starts AP, registers WebUI provider, serves dashboard. |
| `DomoticsCore-SystemInfo/examples/SystemInfoWithWebUI/platformio.ini` | Build config for the WebUI example. |

### Tests

| File | Coverage Area |
|------|---------------|
| `DomoticsCore-SystemInfo/test/test_systeminfo_api/test_systeminfo_api.cpp` | Component creation, configuration, lifecycle (begin/loop/shutdown), boot count. 12 tests. |
| `DomoticsCore-SystemInfo/test/test_systeminfo_boot/test_systeminfo_boot.cpp` | Boot diagnostics, reset reason, boot count persistence, boot heap capture, struct defaults. 18 tests. |
| `DomoticsCore-SystemInfo/test/test_systeminfo_metrics/test_systeminfo_metrics.cpp` | Metrics collection, format helpers, update interval, HAL integration. 15 tests. |

---

## Key Classes and Structures

### `SystemInfoComponent` (public class)

- Inherits from `IComponent`.
- Core responsibility: collect and cache system metrics at a configurable interval.
- Header-only implementation; all logic resides in `SystemInfo.h`.
- Protected nested struct `SystemMetrics` stores cached values.
- Protected EMA state: `lastHeapCheck`, `lastHeapValue`, `cpuLoadEma` support the CPU load heuristic.
- CPU load estimated via heap-activity heuristic with EMA smoothing (alpha = 0.3).
- Boot diagnostics (volatile) captured in `begin()`; boot count set externally by `System`.
- Private methods: `initBootDiagnostics()` (boot-time capture) and `updateMetrics()` (periodic refresh).

### `SystemInfoConfig` (struct)

- Plain data struct with device identity fields and diagnostic toggles.
- Populated by `System` component from `SystemConfig` at runtime.
- `updateInterval` controls the non-blocking refresh cadence in `loop()`.

### `BootDiagnostics` (struct)

- Captures volatile boot-time data: reset reason, free heap, min free heap.
- `bootCount` managed by `System` via `Storage` (not by `SystemInfo`).
- Helper methods delegate to `HAL::Platform` for reset reason interpretation.

### `SystemInfoWebUI` (public class)

- Inherits from `CachingWebUIProvider`.
- Composition-based: holds a non-owning pointer to `SystemInfoComponent`.
- Exposes three WebUI contexts: `system_info` (static), `system_metrics` (real-time), `system_settings` (editable).
- Uses `LazyState<SystemInfoState>` for efficient change detection on settings.
- Supports an optional device-name-changed callback for persistence.

---

## Dependencies

### Build Dependencies (library.json)

The `library.json` declares **no external dependencies** (`"dependencies": []`). All required interfaces are provided by DomoticsCore-Core at the framework level.

### Implicit Framework Dependencies

| Dependency | Usage |
|------------|-------|
| `DomoticsCore-Core` | `IComponent`, `Platform_HAL`, `DLOG_*` logging macros, `ComponentStatus` |
| `DomoticsCore-Core` (WebUI only) | `IWebUIProvider`, `CachingWebUIProvider`, `BaseWebUIComponents`, `LazyState`, `WebUIContext`, `WebUIField` |
| `DomoticsCore-Core` (WebUI only) | `MemoryManager` (for memory profile name) |
| `ArduinoJson` (WebUI only) | JSON serialisation in `getWebUIData()` and `handleWebUIRequest()` |

### Runtime Collaborators

| Component | Relationship |
|-----------|-------------|
| `System` | Populates `SystemInfoConfig` fields (deviceName, manufacturer, firmwareVersion) and sets `bootCount` from Storage. |
| `Storage` | Persists boot count. `SystemInfo` does not access Storage directly. |
| `WebUI` | Hosts the `SystemInfoWebUI` provider. Optional -- `SystemInfo` works without it. |

---

## Conventions

### Namespace

All types live under `DomoticsCore::Components`. The WebUI provider adds a nested `DomoticsCore::Components::WebUI` namespace.

### Include Path

Headers use the `DomoticsCore/` prefix:

```cpp
#include <DomoticsCore/SystemInfo.h>
#include <DomoticsCore/SystemInfoWebUI.h>
```

### Header-Only

Both `SystemInfo.h` and `SystemInfoWebUI.h` are fully header-only. There is no `src/` directory with `.cpp` files. The `library.json` declares `"lib_archive": false` accordingly.

### Component Registration

The component registers with metadata name `"System Info"` (with space) and type key `"system_info"` (with underscore). Use the metadata name for `getComponent<>()` lookups:

```cpp
auto* sys = core.getComponent<SystemInfoComponent>("System Info");
```

### Configuration Pattern

Uses the **Get-Override-Set** pattern for configuration updates, as demonstrated in `SystemInfoWebUI::handleWebUIRequest()`:

```cpp
SystemInfoConfig cfg = sys->getConfig();
cfg.deviceName = newValue;
sys->setConfig(cfg);
```

### Non-Blocking Timing

The `loop()` method uses `HAL::Platform::getMillis()` with interval comparison. No `delay()` calls anywhere in the component.

---

## Constitution Compliance

| Principle | Status | Notes |
|-----------|--------|-------|
| I. SOLID | Compliant | SRP (metrics only), DIP (depends on IComponent abstraction), ISP (minimal public API). |
| II. TDD | Compliant | 45 unit tests across 3 test suites covering API, boot diagnostics, and metrics. |
| III. KISS | Compliant | Header-only, zero external dependencies, simple struct-based data model. |
| IV. YAGNI | Compliant | No speculative features. Only metrics that are actually used. |
| V. Performance First | Compliant | Cached metrics with configurable interval. EMA avoids allocation. `formatBytes` uses stack-local Strings. |
| VI. EventBus | N/A | No inter-component events emitted (read-only diagnostic data). |
| VII. File Size | Compliant | `SystemInfo.h` is 264 lines. `SystemInfoWebUI.h` is 165 lines. Both well under the 800-line limit. |
| VIII. Progressive Refactoring | Compliant | Evolved from v1.0 to v1.4.0 with incremental additions (boot diagnostics, CPU load, WebUI). |
| IX. HAL Isolation | Compliant | Zero `#ifdef` in component code. All platform calls go through `HAL::Platform::*`. |
| X. Non-Blocking Timer | Compliant | Uses `HAL::Platform::getMillis()` interval check. No `delay()` calls. |
| XI. Centralized Storage | Compliant | Does not access Storage directly. Boot count set externally by `System`. |
| XII. Multi-Registry | Compliant | `library.json` present with correct structure. |
| XIII. Anti-Pattern Avoidance | Compliant | No singletons, no god objects, no circular dependencies. |
| XIV. Memory Leak Prevention | Compliant | No dynamic allocation in hot path. Metrics struct is pre-allocated. String formatting uses local scope. |
| XV. Semantic Versioning | Compliant | `library.json` version `1.4.0` matches `metadata.version` in constructor. |

---

## Version History (Summary)

| Version | Key Changes |
|---------|-------------|
| 1.0.0 | Initial release: uptime, heap, chip model, CPU frequency. |
| 1.2.1 | WebUI provider added (SystemInfoWebUI). |
| 1.4.0 | Boot diagnostics, CPU load estimation with EMA, HAL abstraction, MemoryManager integration. |
