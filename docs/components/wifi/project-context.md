# DomoticsCore-Wifi -- Project Context (AI Agent Reference)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides structured context for AI coding agents working on the DomoticsCore-Wifi component. It contains the file inventory, key classes, dependency graph, known caveats, and constitution compliance notes.

---

## Component Identity

| Field | Value |
|-------|-------|
| **Name** | DomoticsCore-Wifi |
| **Version** | 1.4.1 |
| **metadata.name** | `"Wifi"` |
| **metadata.version** | `"1.4.1"` |
| **metadata.description** | `"Wifi connectivity management component"` |
| **Namespace** | `DomoticsCore::Components` (component), `DomoticsCore::HAL::WiFiHAL` (HAL) |
| **License** | MIT |
| **Platforms** | espressif32, espressif8266 |
| **Frameworks** | Arduino |
| **metadata.author** | `"DomoticsCore"` (in-code), `"JN0V"` (library.json) |

---

## File Inventory

All files are under `DomoticsCore-Wifi/`.

### Headers (`include/DomoticsCore/`)

| File | Purpose | Key Classes/Namespaces |
|------|---------|----------------------|
| `Wifi.h` | Main component header | `WifiConfig`, `WifiComponent` |
| `Wifi_HAL.h` | HAL routing header + backward-compatible API | `DomoticsCore::HAL::WiFiHAL` (enums + inline delegates) |
| `Wifi_ESP32.h` | ESP32 WiFi implementation | `DomoticsCore::HAL::WiFiImpl`, `NetworkClient`, `SecureNetworkClient` |
| `Wifi_ESP8266.h` | ESP8266 WiFi implementation | `DomoticsCore::HAL::WiFiImpl`, `NetworkClient`, `SecureNetworkClient` |
| `Wifi_Stub.h` | Stub for native tests | `DomoticsCore::HAL::WiFiImpl` (no-ops), `NetworkClient`, `SecureNetworkClient` |
| `INetworkProvider.h` | Network abstraction interface | `DomoticsCore::Components::INetworkProvider` |
| `WifiEvents.h` | Event topic constants | `DomoticsCore::WifiEvents` |
| `WifiWebUI.h` | WebUI provider | `DomoticsCore::Components::WebUI::WifiWebUI` |
| `WiFiServer_HAL.h` | WiFiServer routing header | Includes platform-specific WiFiServer |
| `WiFiServer_ESP32.h` | ESP32 WiFiServer type aliases | `DomoticsCore::HAL::WiFiServer`, `WiFiClient`, `IPAddress` |
| `WiFiServer_ESP8266.h` | ESP8266 WiFiServer type aliases | `DomoticsCore::HAL::WiFiServer`, `WiFiClient`, `IPAddress` |
| `WiFiServer_Stub.h` | Stub WiFiServer with test helpers | `DomoticsCore::HAL::WiFiServer`, `WiFiClient` (full stubs) |
| `IPAddress_Stub.h` | Stub IPAddress for native tests | `DomoticsCore::HAL::IPAddress` |
| `DocMainpage.h` | Doxygen mainpage | Documentation only |

### Configuration

| File | Purpose |
|------|---------|
| `library.json` | PlatformIO library metadata (version 1.4.1) |
| `platformio.ini` | Build configuration |
| `README.md` | Component README with usage, platform notes, examples |

### Examples

| Directory | Description |
|-----------|-------------|
| `examples/BasicWifi/` | CLI-only WiFi control and logging |
| `examples/WifiWithWebUI/` | WebUI integration with settings card, scanning, status badges |

### Tests

| Directory | Description |
|-----------|-------------|
| `test/test_wifi_component/` | Unit tests for `WifiComponent` logic |
| `test/test_wifi_webui/` | Unit tests for `WifiWebUI` provider |

Mock support is provided by `tests/mocks/MockWifiHAL.h` in the project root `tests/` directory.

---

## Key Classes

### WifiComponent

- **Inherits**: `IComponent`, `INetworkProvider`
- **Responsibilities**: WiFi STA/AP lifecycle, non-blocking connection, reconnection, async scanning, mode switching
- **State**: `wifiEnabled`, `apEnabled`, `ssid`, `password`, `apSSID_`, `apPassword_`, `scanInProgress`, `isConnecting`, `shouldConnect`
- **Timers**: `reconnectTimer` (5s), `statusTimer` (30s), `connectionTimer` (100ms), `staFallbackTimer_` (30s), `rebootTimer_` (1.5s)
- **Deferred flags**: `pendingModeUpdate_`, `pendingConfigSave_`, `pendingReboot_`

### WifiConfig

- **Type**: Plain struct
- **Fields**: ssid, password, autoConnect, enableAP, apSSID, apPassword, reconnectInterval, connectionTimeout
- **Note**: Allocates 6+ Strings; avoid constructing in low-heap HTTP handler paths. Use `setSTACredentials()` instead.

### INetworkProvider

- **Type**: Abstract interface
- **Purpose**: Decouples network consumers (MQTT, WebUI) from WiFi-specific implementation
- **Pure virtuals**: `isConnected()`, `getLocalIP()`, `getNetworkType()`, `getConnectionStatus()`, `getNetworkInfo()`

### WifiWebUI

- **Inherits**: `CachingWebUIProvider`
- **Purpose**: Exposes WiFi settings via browser interface
- **State tracking**: Uses `LazyState<T>` for efficient delta-based WebSocket updates
- **Context IDs**: `wifi_status`, `wifi_component`, `wifi_settings`
- **Additional state**: `pendingSsid`, `pendingPassword`, `pendingApSsid`, `lastScanSummary`
- **Optional callback**: `setConfigSaveCallback()` for unified config persistence
- **Accessors**: `getWebUIName()` returns `metadata.name`, `getWebUIVersion()` returns `metadata.version`

---

## Dependencies

### Required by DomoticsCore-Wifi

| Dependency | Type | Purpose |
|-----------|------|---------|
| **DomoticsCore-Core** | Library | `IComponent`, `Logger`, `Timer` (`NonBlockingDelay`), `Platform_HAL`, `EventBus` |
| **ArduinoJson** | Library (>= 7.0.0) | JSON serialization for `getNetworkInfo()`, `getAPInfo()`, WebUI data |
| **DomoticsCore-WebUI** | Library (optional) | Required only for `WifiWebUI` (`IWebUIProvider`, `CachingWebUIProvider`) |

### Dependents (components that use DomoticsCore-Wifi)

| Component | How It Depends |
|-----------|---------------|
| **DomoticsCore-MQTT** | Depends on `INetworkProvider` for connection state; subscribes to `network/ready` event |
| **DomoticsCore-System** | Orchestrates WiFi setup, sets AP SSID from device name, provides config persistence callback |
| **DomoticsCore-OTA** | Needs network connectivity (via `INetworkProvider`) for OTA updates |
| **DomoticsCore-WebUI** | Needs network connectivity to serve web interface |
| **DomoticsCore-RemoteConsole** | Needs network connectivity for remote serial access |

---

## ESP8266 Channel Sync Caveat

This is the most important platform-specific behavior to understand when modifying this component.

**Problem:** ESP8266 has a single radio. When running in AP+STA mode, both AP and STA MUST operate on the same WiFi channel. If the target router is on channel 6 but the AP was started on channel 1, calling `WiFi.begin()` for STA will silently fail with `WL_IDLE_STATUS`.

**Solution implemented in `updateWifiMode()`:**

1. Stop the AP (frees the radio from the AP channel).
2. Set mode to `Station` only.
3. Begin STA connection (radio tunes to the router's channel).
4. Activate `staFallbackTimer_` (30 seconds).
5. In `loop()`, when STA connects: restart AP in `StationAndAP` mode (AP inherits STA's channel).
6. If STA times out (30s): restore AP-only mode, save config with `autoConnect=false` to prevent boot loops.

**Key constraint:** During step 1-3, the AP is unavailable. Any browser connected via AP will lose connectivity temporarily. If heap is too low to restart AP after STA connects, the AP stays off and the device runs STA-only.

---

## Constitution Compliance

This section maps DomoticsCore-Wifi patterns to specific constitution principles.

| Principle | Compliance |
|-----------|------------|
| **I. SOLID** | SRP: WifiComponent handles connectivity; WifiWebUI handles UI; INetworkProvider provides DIP abstraction |
| **III. KISS** | Simple enable/disable API hides complex mode switching logic |
| **V. Performance First** | Heap-aware mode switching; deferred operations avoid OOM during HTTP handling |
| **VI. EventBus** | Uses `emit()` for `wifi/sta/connected`, `wifi/ap/enabled`, `network/ready` -- no direct component references |
| **IX. HAL Isolation** | All `#ifdef` platform code is in `Wifi_ESP32.h`, `Wifi_ESP8266.h`, `Wifi_Stub.h` -- zero `#ifdef` in business logic |
| **X. Non-Blocking Timers** | All timers use `NonBlockingDelay`; no `delay()` calls; `loop()` completes in < 10ms |
| **XI. Centralized Storage** | WiFi config persistence uses `configSaveCallback_` (injected by System), not direct NVS access |
| **XII. Multi-Registry** | Has both `library.json` (PlatformIO) format; Arduino-compatible `include/` structure |
| **XIII. Anti-Patterns** | Uses EventBus (not direct references), centralized constants in `WifiEvents.h`, no singletons |
| **XIV. Memory Leaks** | Deferred NVS writes, `shrink_to_fit()` awareness, lightweight credential setters for low-heap paths |
| **XV. Semantic Versioning** | Version 1.4.1 in both `library.json` and `metadata.version` in constructor |

---

## Common Modification Patterns

### Adding a New WiFi Event

1. Add the event constant to `WifiEvents.h`.
2. Call `emit()` at the appropriate point in `WifiComponent`.
3. Subscribers use `on<PayloadType>(topic, callback)` in their own `begin()` or `afterAllComponentsReady()`.

### Adding a New WebUI Field

1. Add the field in `buildContexts()` within the appropriate context.
2. Handle the POST in `handleWebUIRequest()`.
3. Return the data in `getWebUIData()`.
4. Add state tracking via a new `LazyState<>` member in `hasDataChanged()`.

### Changing Connection Behavior

All connection logic flows through `updateWifiMode()`. The method has guards followed by four branches:
- **Stale config guard**: If `wifiEnabled` but `ssid` is empty, disables STA (prevents useless connection attempts from corrupted config)
- **Ultra-low heap guard** (< 2KB): Disables STA entirely, saves config with `autoConnect=false`
- `wifiEnabled && apEnabled` -- dual mode with heap-aware strategies (10KB+: direct, 3.5-10KB: channel sync, < 3.5KB: reboot-to-STA)
- `wifiEnabled && !apEnabled` -- STA only
- `!wifiEnabled && apEnabled` -- AP only (skips restart if AP already active with correct SSID)
- `!wifiEnabled && !apEnabled` -- all off

Reconnection is driven by `loop()` polling. Never add blocking waits.

### Adding a New Platform

1. Create `Wifi_{Platform}.h` in `include/DomoticsCore/`.
2. Implement all functions in `DomoticsCore::HAL::WiFiImpl` namespace.
3. Add `#elif` branch in `Wifi_HAL.h`.
4. Create matching `WiFiServer_{Platform}.h` and add to `WiFiServer_HAL.h`.
5. Zero changes to `Wifi.h`, `WifiWebUI.h`, or any business logic.

---

## Version History (Recent)

| Version | Changes |
|---------|---------|
| 1.4.1 | Current release |
| 1.4.0 | Unified WebUI settings card (STA+AP in one context), LazyState change tracking |
| 1.3.x | Deferred config save, reboot-to-STA strategy, heap guards |
