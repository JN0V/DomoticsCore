# DomoticsCore-System -- Project Context (AI Agent Reference)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document is optimized for AI coding agents. It provides the file inventory, key classes, dependency graph, naming conventions, and constitution compliance notes needed to work on the DomoticsCore-System component effectively.

**Last verified against source:** 2026-03-10

---

## Identity

| Field | Value |
|-------|-------|
| **Library name** | `DomoticsCore-System` |
| **Version** | `1.4.1` |
| **Description** | Complete ready-to-use system with automatic component orchestration |
| **Namespace** | `DomoticsCore` (main), `DomoticsCore::SystemHelpers` (internal helpers) |
| **Platforms** | `espressif32`, `espressif8266` |
| **Framework** | Arduino |
| **License** | MIT |
| **Repository** | `https://github.com/JN0V/DomoticsCore.git` |
| **Build type** | Header-only (no `src/` directory; all code is in `include/`) |

---

## File Inventory

### Headers (`include/DomoticsCore/`)

| File | Purpose |
|------|---------|
| `System.h` | Main entry point. Defines the `System` class with `begin()`, `loop()`, accessors, state management, event orchestration, LED pattern mapping, boot diagnostics, and all component registration logic. |
| `SystemConfig.h` | Defines the `SystemState` enum, `systemStateToString()` converter, and the `SystemConfig` struct with all configuration fields and preset factory methods (`minimal()`, `standard()`, `fullStack()`). |
| `SystemPersistence.h` | Defines `SystemHelpers::registerStorageKeys()`, individual config loaders (`loadDeviceName`, `loadWifiConfig`, `loadWebUIConfig`, `loadNTPConfig`, `loadMQTTConfig`, `loadHomeAssistantConfig`), and the aggregate `loadAllConfigs()` function. |
| `SystemWebUISetup.h` | Defines `SystemHelpers::WebUIProviders` struct and `setupWebUIProviders()` function. Handles dynamic creation, registration, and persistence callback wiring for all WebUI providers. |

### Examples (`examples/`)

| Directory | Preset | Description |
|-----------|--------|-------------|
| `examples/Minimal/` | `SystemConfig::minimal()` | WiFi + LED + Console only. Simulated temp sensor and relay. |
| `examples/Standard/` | `SystemConfig::standard()` | Adds WebUI, NTP, and Storage. Standalone device. |
| `examples/FullStack/` | `SystemConfig::fullStack()` | All components including MQTT, Home Assistant, OTA, and SystemInfo. Shows HA entity creation and state publishing. |

### Configuration

| File | Purpose |
|------|---------|
| `library.json` | PlatformIO library manifest. Declares name, version, dependencies, build directories, supported platforms. |

---

## Key Classes and Types

| Type | Header | Role |
|------|--------|------|
| `System` | `System.h` | Top-level orchestrator class. Owns a `Core`, creates all components, manages lifecycle. |
| `SystemConfig` | `SystemConfig.h` | POD-like config struct with all toggles, credentials, and preset factories. |
| `SystemState` | `SystemConfig.h` | Enum class with 8 states: `BOOTING`, `WIFI_CONNECTING`, `WIFI_CONNECTED`, `SERVICES_STARTING`, `READY`, `ERROR`, `OTA_UPDATE`, `SHUTDOWN`. |
| `SystemHelpers::WebUIProviders` | `SystemWebUISetup.h` | Aggregate struct holding raw pointers to all WebUI providers for lifetime management. |
| `systemStateToString()` | `SystemConfig.h` | Free function converting `SystemState` to a C-string. |

---

## Dependencies

### Required (declared in `library.json`)

| Dependency | Minimum Version | What It Provides |
|------------|----------------|------------------|
| `DomoticsCore-Core` | `>=1.0.0` | `Core`, `IComponent`, `EventBus`, `Logger`, `NonBlockingDelay`, `MemoryManager`, `Platform_HAL` |
| `DomoticsCore-LED` | `>=1.0.0` | `LEDComponent`, `LEDEffect`, `LEDColor` |
| `DomoticsCore-Wifi` | `>=1.0.0` | `WifiComponent`, `WifiConfig`, `WifiEvents` |
| `DomoticsCore-RemoteConsole` | `>=0.1.0` | `RemoteConsoleComponent`, `RemoteConsoleConfig` |

### Optional (detected via `__has_include`)

| Dependency | Header Checked | Config Flag | Used For |
|------------|---------------|-------------|----------|
| `DomoticsCore-Storage` | `<DomoticsCore/Storage.h>` | `enableStorage` | NVS key-value persistence |
| `DomoticsCore-WebUI` | `<DomoticsCore/WebUI.h>` | `enableWebUI` | Browser-based control panel |
| `DomoticsCore-NTP` | `<DomoticsCore/NTP.h>` | `enableNTP` | Network time synchronization |
| `DomoticsCore-MQTT` | `<DomoticsCore/MQTT.h>` | `enableMQTT` | MQTT pub/sub messaging |
| `DomoticsCore-HomeAssistant` | `<DomoticsCore/HomeAssistant.h>` | `enableHomeAssistant` | HA auto-discovery and entity management |
| `DomoticsCore-OTA` | `<DomoticsCore/OTA.h>` | `enableOTA` | Over-the-air firmware updates |
| `DomoticsCore-SystemInfo` | `<DomoticsCore/SystemInfo.h>` | `enableSystemInfo` | Boot diagnostics, chip info, uptime |

### Optional WebUI Providers (detected via `__has_include`)

These are only relevant when `DomoticsCore-WebUI` is also installed:

| Provider Header | Compile Guard Define |
|-----------------|---------------------|
| `<DomoticsCore/WifiWebUI.h>` | `WEBUI_SETUP_HAS_WIFI_WEBUI` |
| `<DomoticsCore/NTPWebUI.h>` | `WEBUI_SETUP_HAS_NTP_WEBUI` |
| `<DomoticsCore/MQTTWebUI.h>` | `WEBUI_SETUP_HAS_MQTT_WEBUI` |
| `<DomoticsCore/OTAWebUI.h>` | `WEBUI_SETUP_HAS_OTA_WEBUI` |
| `<DomoticsCore/SystemInfoWebUI.h>` | `WEBUI_SETUP_HAS_SYSINFO_WEBUI` |
| `<DomoticsCore/RemoteConsoleWebUI.h>` | `WEBUI_SETUP_HAS_CONSOLE_WEBUI` |
| `<DomoticsCore/HomeAssistantWebUI.h>` | `WEBUI_SETUP_HAS_HA_WEBUI` |

---

## Conventions

### Naming

- **Namespace:** `DomoticsCore` for all public API; `DomoticsCore::SystemHelpers` for internal helper functions and structs.
- **Log tags:** `LOG_SYSTEM` (`"SYSTEM"`), `LOG_PERSISTENCE` (`"PERSIST"`), `LOG_WEBUI_SETUP` (`"WEBUI_SETUP"`).
- **Include guard pattern:** `DOMOTICS_CORE_SYSTEM_H`, `DOMOTICS_CORE_SYSTEM_CONFIG_H`, etc.
- **Config struct fields:** camelCase, grouped by component area. Boolean toggles prefixed with `enable`.

### Architecture

- **Header-only:** The entire System component is implemented in headers. There is no `src/` directory.
- **Compile-time feature detection:** All optional integrations use `__has_include()` preprocessor directives, not runtime checks.
- **Heap-guarded initialization:** Post-Core-init steps are skipped when free heap drops below `MIN_HEAP_POST_INIT` (3072 bytes). WebUI provider registration uses `MIN_HEAP_PER_PROVIDER` (6500 bytes).
- **Component ownership:** Components are created via `std::make_unique` and moved into Core. System retains raw observation pointers (`led`, `wifi`, `console`) for direct access.
- **State pattern:** `SystemState` enum with `setState()` orchestrating LED updates and callback dispatch.
- **Preset hierarchy:** `fullStack()` calls `standard()`, which calls `minimal()`. Each adds incremental features.

### Coding Style

- Inline functions in headers (e.g., all `SystemHelpers` functions are `inline`).
- Logging via `DLOG_E`, `DLOG_W`, `DLOG_I`, `DLOG_D`, `DLOG_V` macros with component tag as the first argument.
- Lambda callbacks for persistence (captured variables include `storage` pointer and, where needed, `config` reference).
- Early returns for disabled features (`if (!config.enableX) return;`).

---

## Constitution Compliance

The following aspects of DomoticsCore-System align with the project constitution:

- **Component model adherence:** System creates components using `std::make_unique<>` and registers them via `core.addComponent()`, following the standard `IComponent` lifecycle.
- **EventBus integration:** Inter-component communication (WiFi -> MQTT, NTP sync, HA discovery) uses the Core `EventBus` publish/subscribe pattern rather than direct coupling.
- **Platform HAL usage:** All hardware access goes through `HAL::` functions (`getChipId()`, `getFreeHeap()`, `getChipModel()`), avoiding direct `#ifdef` in business logic.
- **Header-only build:** Consistent with the framework convention of header-only component libraries.
- **Namespace isolation:** All code resides within the `DomoticsCore` namespace hierarchy.
- **Graceful degradation:** Missing optional libraries produce warnings (not errors). Heap exhaustion skips non-critical steps rather than crashing.
- **Storage key registration:** All NVS keys are registered with `registerKeys()` before use, enabling enumeration and documentation.

---

## Removed Config Fields (v1.4.1 cleanup)

The following fields were removed from `SystemConfig` and no longer exist in the source code. They should not be referenced in new code:

- `haDiscoveryPrefix` -- HA discovery prefix is now set exclusively via `HAConfig.discoveryPrefix` (default `"homeassistant"`), loaded from Storage key `ha_disc_prefix`.
- `webUIEnableAPI` -- The WebUI API is always enabled when WebUI is active; there is no toggle.
- `wifiTimeout` -- WiFi connection timeout is managed internally by `WifiComponent`; no user-facing config field exists in `SystemConfig`.

---

## Common Modification Patterns

### Adding a new optional component

1. Add an `enable*` boolean and related config fields to `SystemConfig`.
2. Add a `register*Component()` private method to `System` with `__has_include` guard.
3. Call the new registration method from `registerOptionalComponents()`.
4. Add persistence keys in `SystemPersistence.h` (`registerStorageKeys` and a new `load*Config` function).
5. Add the loader call to `loadAllConfigs()`.
6. If WebUI is relevant, add provider registration in `SystemWebUISetup.h`.
7. Update `fullStack()` (or the appropriate preset) to enable the new component.

### Adding a new console command

Call `console->registerCommand()` in `registerConsoleComponent()` or via `system.registerCommand()` from application code after `begin()`.

### Adding a new system state

1. Add the value to the `SystemState` enum in `SystemConfig.h`.
2. Add its string representation to `systemStateToString()`.
3. Add the LED pattern mapping in `System::updateLEDPattern()`.
