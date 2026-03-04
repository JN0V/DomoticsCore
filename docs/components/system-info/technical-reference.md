# DomoticsCore-SystemInfo -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document is the complete API reference for the DomoticsCore-SystemInfo component. It covers every public class, struct, method, and configuration option.

---

## Table of Contents

1. [Namespace Layout](#namespace-layout)
2. [SystemInfoConfig](#systeminfoconfig)
3. [BootDiagnostics](#bootdiagnostics)
4. [SystemInfoComponent](#systeminfocomponent)
   - [Constructor](#constructor)
   - [IComponent Lifecycle](#icomponent-lifecycle)
   - [Metrics Access](#metrics-access)
   - [Boot Diagnostics Access](#boot-diagnostics-access)
   - [Configuration Access](#configuration-access)
   - [Utility Methods](#utility-methods)
5. [SystemMetrics (Internal Struct)](#systemmetrics-internal-struct)
6. [CPU Load Estimation](#cpu-load-estimation)
7. [SystemInfoWebUI](#systeminfowebui)
   - [Constructor](#webui-constructor)
   - [WebUI Contexts](#webui-contexts)
   - [Data Methods](#data-methods)
   - [Device Name Persistence](#device-name-persistence)
   - [Change Detection](#change-detection)
8. [Header Includes](#header-includes)

---

## Namespace Layout

```
DomoticsCore::Components::SystemInfoComponent
DomoticsCore::Components::SystemInfoConfig
DomoticsCore::Components::BootDiagnostics
DomoticsCore::Components::WebUI::SystemInfoWebUI
```

All types reside under `DomoticsCore::Components`. The WebUI provider adds a nested `WebUI` namespace.

---

## SystemInfoConfig

**Header:** `DomoticsCore/SystemInfo.h`

Configuration struct passed to the `SystemInfoComponent` constructor.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `deviceName` | `String` | `"DomoticsCore Device"` | Human-readable device name. Typically overridden by `System` from `SystemConfig`. |
| `manufacturer` | `String` | `"DomoticsCore"` | Manufacturer string shown in UI. |
| `firmwareVersion` | `String` | `"1.0.0"` | Firmware version string shown in UI. |
| `enableDetailedInfo` | `bool` | `true` | Include detailed chip/flash information in metrics collection. |
| `enableMemoryInfo` | `bool` | `true` | Include memory statistics (heap, sketch size) in metrics collection. |
| `updateInterval` | `int` | `5000` | Minimum interval (in milliseconds) between metric refreshes in `loop()`. |
| `enableBootDiagnostics` | `bool` | `true` | Capture volatile boot diagnostics (reset reason, heap snapshot) during `begin()`. |

**Example:**

```cpp
SystemInfoConfig config;
config.deviceName = "MyDevice";
config.manufacturer = "Acme";
config.firmwareVersion = "2.1.0";
config.updateInterval = 10000;  // 10 seconds
config.enableBootDiagnostics = true;

SystemInfoComponent sysinfo(config);
```

---

## BootDiagnostics

**Header:** `DomoticsCore/SystemInfo.h`

Volatile data captured once at boot by `SystemInfoComponent::begin()`. The `bootCount` field is managed externally by the `System` component via `Storage` for persistence.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `bootCount` | `uint32_t` | `0` | Incrementing boot counter. Set by `System` via `Storage`, not by `SystemInfo` itself. |
| `resetReason` | `HAL::Platform::ResetReason` | `Unknown` | Enum representing the reason for the last reset. |
| `lastBootHeap` | `uint32_t` | `0` | Free heap (bytes) captured at boot. |
| `lastBootMinHeap` | `uint32_t` | `0` | Minimum free heap (bytes) captured at boot. |
| `valid` | `bool` | `false` | Set to `true` once boot diagnostics have been successfully captured. |

### Methods

#### `String getResetReasonString() const`

Returns a human-readable string for the stored `resetReason`. Delegates to `HAL::Platform::getResetReasonString()`.

**Returns:** A string such as `"Power-on"`, `"Software reset"`, `"Watchdog"`, or `"Unknown"`.

#### `bool wasUnexpectedReset() const`

Checks whether the last reset was unexpected (e.g., watchdog timeout, brownout, panic). Delegates to `HAL::Platform::wasUnexpectedReset()`.

**Returns:** `true` if the reset reason indicates an abnormal termination.

---

## SystemInfoComponent

**Header:** `DomoticsCore/SystemInfo.h`
**Base class:** `DomoticsCore::IComponent`
**Type key:** `"system_info"`
**Metadata name:** `"System Info"`
**Metadata version:** `"1.4.0"`

The core class of this component. Collects ESP32/ESP8266 system metrics at a configurable interval and exposes them through read-only accessors.

### Constructor

```cpp
SystemInfoComponent(const SystemInfoConfig& cfg = SystemInfoConfig());
```

Accepts an optional configuration struct. When omitted, all defaults from `SystemInfoConfig` apply. Sets the component metadata name to `"System Info"` and version to `"1.4.0"`.

### IComponent Lifecycle

#### `ComponentStatus begin()`

1. If `config.enableBootDiagnostics` is `true`, calls `initBootDiagnostics()` to capture reset reason and heap snapshot.
2. Calls `updateMetrics()` to populate the initial `SystemMetrics` cache.
3. Returns `ComponentStatus::Success`.

#### `void loop()`

Checks whether `config.updateInterval` milliseconds have elapsed since the last update. If so, calls `updateMetrics()` to refresh all cached metrics including the CPU load estimate.

The interval check uses `HAL::Platform::getMillis()` for non-blocking timing, in compliance with Constitution Principle X.

#### `ComponentStatus shutdown()`

No-op. Returns `ComponentStatus::Success`.

#### `const char* getTypeKey() const`

Returns `"system_info"`. Used by the component registry for lookup.

### Metrics Access

#### `const SystemMetrics& getMetrics() const`

Returns a const reference to the cached metrics struct. This is the primary way to read system data. The struct is refreshed on every successful `updateMetrics()` call.

See [SystemMetrics](#systemmetrics-internal-struct) for field details.

### Boot Diagnostics Access

#### `const BootDiagnostics& getBootDiagnostics() const`

Returns a const reference to the boot diagnostics struct. Data is valid only after `begin()` has executed with `enableBootDiagnostics = true`.

#### `void setBootCount(uint32_t count)`

Sets the `bootDiag.bootCount` value. This method is intended to be called by the `System` component after loading the persisted boot count from `Storage`. The `SystemInfo` component does not persist this value itself.

### Configuration Access

#### `const SystemInfoConfig& getConfig() const`

Returns a const reference to the current configuration.

#### `void setConfig(const SystemInfoConfig& cfg)`

Replaces the current configuration. Logs the updated device name, manufacturer, and firmware version at INFO level.

#### `int getUpdateInterval() const`

Returns `config.updateInterval` (milliseconds).

#### `bool isDetailedInfoEnabled() const`

Returns `config.enableDetailedInfo`.

#### `bool isMemoryInfoEnabled() const`

Returns `config.enableMemoryInfo`.

### Utility Methods

#### `String getFormattedUptimePublic()`

Returns the current uptime as a human-readable string. Format adapts to duration:

| Duration | Format Example |
|----------|----------------|
| < 1 hour | `"3m 42s"` |
| 1 hour -- 24 hours | `"5h 12m"` |
| >= 24 hours | `"2d 7h"` |

#### `String formatBytesPublic(uint32_t bytes)`

Formats a byte count into a human-readable string with automatic unit selection:

| Range | Unit | Example |
|-------|------|---------|
| < 1024 | B | `"512 B"` |
| 1024 -- 1048575 | KB | `"2.0 KB"` |
| >= 1048576 | MB | `"1.5 MB"` |

#### `void forceUpdateMetrics()`

Forces an immediate refresh of all metrics, bypassing the `updateInterval` timer. Useful for WebUI extensions that need fresh data on demand.

---

## SystemMetrics (Internal Struct)

Defined as a protected nested struct inside `SystemInfoComponent`. Accessible through `getMetrics()`.

| Field | Type | Description |
|-------|------|-------------|
| `freeHeap` | `uint32_t` | Currently available heap memory (bytes). |
| `totalHeap` | `uint32_t` | Total heap size (bytes). |
| `minFreeHeap` | `uint32_t` | Minimum free heap since boot (bytes). Useful for tracking fragmentation. |
| `maxAllocHeap` | `uint32_t` | Largest contiguous free block (bytes). |
| `cpuFreq` | `float` | CPU clock frequency in MHz. |
| `flashSize` | `uint32_t` | Total flash chip size (bytes). |
| `sketchSize` | `uint32_t` | Compiled firmware size (bytes). |
| `freeSketchSpace` | `uint32_t` | Remaining space for OTA updates (bytes). |
| `chipModel` | `String` | Chip model identifier (e.g., `"ESP32-D0WDQ6"`). |
| `chipRevision` | `uint8_t` | Silicon revision number. |
| `uptime` | `uint32_t` | Seconds since boot. |
| `cpuLoad` | `float` | Estimated CPU load percentage (0--100). |
| `valid` | `bool` | `true` once at least one `updateMetrics()` call has completed. |

---

## CPU Load Estimation

Direct CPU usage measurement is not available on ESP32 Arduino without special FreeRTOS configuration. The component uses a **heap-activity heuristic**:

1. On each `updateMetrics()` call, the absolute difference in free heap since the previous call is calculated.
2. This difference is converted to KB/s based on the elapsed time.
3. The KB/s value is mapped to a 0--100 range (10 KB/s maps to 100%).
4. An **exponential moving average** (EMA) with smoothing factor `alpha = 0.3` stabilises the reading.

The result is stored in `metrics.cpuLoad` and clamped to `[0, 100]` via `HAL::Platform::constrain()`.

**Note:** This is an activity estimator, not a true CPU utilisation metric. It correlates with allocation-heavy workloads but may underreport compute-bound tasks that do not allocate memory.

---

## SystemInfoWebUI

**Header:** `DomoticsCore/SystemInfoWebUI.h`
**Base class:** `CachingWebUIProvider`
**Namespace:** `DomoticsCore::Components::WebUI`

Composition-based WebUI provider that exposes `SystemInfoComponent` data on the web dashboard. It holds a **non-owning pointer** to its parent `SystemInfoComponent`.

### WebUI Constructor

```cpp
explicit SystemInfoWebUI(SystemInfoComponent* component);
```

Takes a raw, non-owning pointer to the `SystemInfoComponent` whose data will be rendered. The caller retains ownership of the component.

### WebUI Contexts

The provider registers three WebUI contexts via `buildContexts()`:

#### 1. `system_info` -- Device Information (Dashboard)

Static hardware information. Fields use `WebUIFieldType::Display` (read-only).

| Field ID | Label | Description |
|----------|-------|-------------|
| `manufacturer` | Manufacturer | Config manufacturer string |
| `firmware` | Firmware | Config firmware version |
| `chip` | Chip | `metrics.chipModel` |
| `revision` | Revision | `metrics.chipRevision` |
| `cpu_freq` | CPU Freq | CPU frequency with " MHz" suffix |
| `total_heap` | Total Heap | Total heap in KB |
| `mem_profile` | Mem Profile | Current `MemoryManager` profile name |

#### 2. `system_metrics` -- System Metrics (Dashboard, Real-Time)

Real-time metrics with chart visualisation. Configured with `withRealTime(2000)` for 2-second WebSocket push updates.

| Field ID | Label | Type | Unit |
|----------|-------|------|------|
| `cpu_load` | CPU Load | Chart | % |
| `heap_usage` | Memory Usage | Chart | % (calculated as used/total * 100) |

#### 3. `system_settings` -- Device Settings (Settings Page)

Editable device configuration.

| Field ID | Label | Type | Description |
|----------|-------|------|-------------|
| `device_name` | Device Name | Text | Editable device name. Changes are applied via `setConfig()` and optionally persisted through a callback. |

### Data Methods

#### `String getWebUIName() const`

Returns `sys->metadata.name` (typically `"System Info"`).

#### `String getWebUIVersion() const`

Returns `sys->metadata.version` (typically `"1.4.0"`).

#### `String getWebUIData(const String& contextId)`

Returns a JSON string with field values for the requested context:

- **`system_info`:** Static device/hardware fields as JSON object.
- **`system_metrics`:** `cpu_load` (float) and `heap_usage` (float, percentage).
- **`system_settings`:** `device_name` string.

Returns `"{}"` for unknown context IDs or if the component pointer is null.

#### `String handleWebUIRequest(const String& contextId, const String& endpoint, const String& method, const std::map<String, String>& params)`

Handles POST requests to `system_settings`. Expects `field` and `value` parameters. Currently supports only the `device_name` field.

On a successful device name change:
1. Reads current config via `getConfig()`.
2. Overrides the `deviceName` field.
3. Applies via `setConfig()` (Get-Override-Set pattern).
4. Invokes the optional `onDeviceNameChanged` callback for persistence.
5. Resets `LazyState` to trigger immediate WebSocket push.
6. Returns `{"success":true}`.

Returns `{"success":false}` for unrecognised fields or non-POST methods.

### Device Name Persistence

#### `void setDeviceNameCallback(std::function<void(const String&)> callback)`

Registers an optional callback that is invoked whenever the device name is changed through the WebUI. The callback receives the new name string and is responsible for persisting it (typically via the `Storage` component).

### Change Detection

#### `bool hasDataChanged(const String& contextId)`

Used by the WebUI framework to determine whether data should be pushed over WebSocket:

| Context | Behaviour |
|---------|-----------|
| `system_info` | Always returns `false` (static hardware data). |
| `system_settings` | Uses `LazyState<SystemInfoState>` to detect changes in `deviceName`, `manufacturer`, or `firmwareVersion`. |
| `system_metrics` | Always returns `true` (real-time data that changes every cycle). |

---

## Header Includes

### SystemInfo.h

```
DomoticsCore/IComponent.h        -- IComponent base class
DomoticsCore/Platform_HAL.h      -- Hardware abstraction (getMillis, getFreeHeap, etc.)
<cmath>                           -- fabsf for CPU load calculation
```

### SystemInfoWebUI.h

```
DomoticsCore/SystemInfo.h         -- SystemInfoComponent, SystemInfoConfig
DomoticsCore/IWebUIProvider.h     -- IWebUIProvider interface
DomoticsCore/BaseWebUIComponents.h -- CachingWebUIProvider, LazyState, WebUIContext, WebUIField
DomoticsCore/Platform_HAL.h       -- getMillis()
DomoticsCore/MemoryManager.h      -- Memory profile name
<ArduinoJson.h>                   -- JSON serialisation
```
