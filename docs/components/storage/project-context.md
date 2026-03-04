# DomoticsCore-Storage -- Project Context (AI Reference)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides machine-readable context for AI assistants working on the DomoticsCore-Storage component. It covers identity, file inventory, architecture, dependencies, conventions, and constitution compliance.

---

## Identity

| Field | Value |
|-------|-------|
| Component name | DomoticsCore-Storage |
| Version | 1.4.2 |
| Metadata name | `"Storage"` (default) or custom via `StorageConfig::componentName` |
| Category | Storage |
| License | MIT |
| Author | JN0V (maintainer) |
| Frameworks | Arduino |
| Platforms | espressif32, espressif8266 |
| Repository | `https://github.com/JN0V/DomoticsCore.git` |

---

## File Inventory

All paths are relative to `DomoticsCore-Storage/`.

### Headers (`include/DomoticsCore/`)

| File | Purpose | Lines |
|------|---------|-------|
| `Storage.h` | Main component: `StorageComponent`, `StorageConfig`, `StorageEntry`, `StorageKeyDef`, `StorageValueType` | ~651 |
| `Storage_HAL.h` | HAL routing header: `IStorage` abstract interface, platform `#ifdef` dispatch | ~70 |
| `Storage_ESP32.h` | ESP32 implementation: `PreferencesStorage` wrapping `<Preferences.h>` | ~129 |
| `Storage_ESP8266.h` | ESP8266 implementation: `LittleFSStorage` using LittleFS + ArduinoJson | ~203 |
| `Storage_Stub.h` | Stub implementation: `RAMOnlyStorage` for native/test builds | ~158 |
| `StorageEvents.h` | Event constants: `EVENT_READY` | ~18 |
| `StorageWebUI.h` | WebUI provider: `StorageWebUI` extending `CachingWebUIProvider` | ~66 |

### Configuration

| File | Purpose |
|------|---------|
| `library.json` | PlatformIO library manifest (name, version, build dirs, dependencies) |
| `README.md` | Component-level README |

### Examples (`examples/`)

| Example | Description |
|---------|-------------|
| `BasicStorage/` | Headless demo: all data types, lifecycle, session counting, blob storage |
| `NamespaceDemo/` | Two `StorageComponent` instances with separate namespaces (`config`, `appdata`) |
| `StorageWithWebUI/` | Storage + WebUI provider registration for browser-based monitoring |

### Source (`src/`)

No `.cpp` source files. The component is header-only.

---

## Key Classes and Structs

### StorageComponent (`DomoticsCore::Components::StorageComponent`)

- Extends `IComponent`.
- Owns a `HAL::PlatformStorage` instance (compile-time selected).
- Maintains an in-memory `std::map<String, StorageEntry>` cache.
- Tracks registered keys via `std::vector<StorageKeyDef>`.
- Uses two `Utils::NonBlockingDelay` timers (status and maintenance, both 300 s).

### IStorage (`DomoticsCore::HAL::IStorage`)

- Pure virtual interface for key-value storage operations.
- Methods: `begin`, `end`, `isKey`, `putString`/`getString`, `putInt`/`getInt`, `putBool`/`getBool`, `putFloat`/`getFloat`, `putULong64`/`getULong64`, `putBytes`/`getBytes`/`getBytesLength`, `remove`, `clear`, `freeEntries`.

### Platform Implementations

| Class | Platform | Backend | `PlatformStorage` alias |
|-------|----------|---------|------------------------|
| `PreferencesStorage` | ESP32 | NVS via `<Preferences.h>` | Yes |
| `LittleFSStorage` | ESP8266 | LittleFS + ArduinoJson 7.x | Yes |
| `RAMOnlyStorage` | Native/other | Fixed array in RAM (32 entries) | Yes |

### StorageWebUI (`DomoticsCore::Components::WebUI::StorageWebUI`)

- Extends `CachingWebUIProvider`.
- Non-owning pointer to `StorageComponent`.
- Exposes two WebUI contexts: `storage_component` (card) and `storage_settings` (settings).

---

## Dependencies

### Compile-time (library.json declares no external deps)

| Dependency | Usage | Notes |
|------------|-------|-------|
| DomoticsCore (Core) | `IComponent`, `Timer`, `EventBus`, `Logger` macros | Peer dependency, always present |
| Arduino framework | `String`, `Serial`, basic types | Framework requirement |
| `<Preferences.h>` | ESP32 NVS backend | Available in arduino-esp32 core |
| `<LittleFS.h>` | ESP8266 filesystem backend | Available in ESP8266 Arduino core |
| `<ArduinoJson.h>` | ESP8266 JSON serialization; WebUI data serialization | Version 7.x |

### Runtime

- `EventBus` -- used to emit `storage/ready` on successful initialization.
- `WebUIComponent` -- optional; required only if `StorageWebUI` is registered.

---

## Conventions and Constraints

### NVS Key and Namespace Limits (ESP32)

- **Namespace name**: maximum 15 characters. Enforced in `begin()` with a `ConfigError` return.
- **Key name**: maximum 15 characters. This is an NVS hardware constraint on ESP32. Callers are responsible for keeping keys within this limit.
- Keys containing `"pass"` are masked in `dumpContents()` output.

### ESP8266 JSON Document Limit

- The `LittleFSStorage` implementation uses a single `JsonDocument` per namespace, limited to **2 KB** (FR-003c).
- Corrupted JSON files are silently cleared to defaults (FR-003d).
- Binary blobs are hex-encoded, doubling their storage cost in the JSON file.

### Stub Constraints (Native)

- Fixed maximum of 32 entries across all keys.
- Keys are namespace-prefixed (`namespace:key`) for isolation.
- `getBytes`/`getBytesLength` are no-ops (return 0).

### Component Naming

- A single `StorageComponent` with default config registers as `"Storage"`.
- When running multiple instances, set `StorageConfig::componentName` to a unique value.
- The component always registers under `metadata.name` for dependency resolution.

### Coding Patterns

- All inter-component communication uses the EventBus (constitution Section VI).
- Non-blocking timers only; `delay()` is forbidden (constitution Section X).
- `#ifdef` platform guards appear exclusively in HAL files (constitution Section IX).
- Header-only component (no `.cpp` files in `src/`).

---

## Constitution Compliance

This section maps DomoticsCore-Storage design decisions to specific constitution principles.

| Constitution Section | Requirement | How Storage Complies |
|---------------------|-------------|---------------------|
| I. SOLID | SRP, DIP, ISP | `StorageComponent` handles storage only; depends on `IStorage` abstraction; `IStorage` interface is minimal |
| V. Performance | Memory budget, no busy-wait | Cache uses `std::map` (bounded by `maxEntries`); timers are non-blocking |
| VI. EventBus | Decoupled communication | Emits `storage/ready` event; no direct references to other components |
| VII. File Size | < 800 lines per file | `Storage.h` is ~651 lines; all other files are well under 200 lines |
| IX. HAL Isolation | `#ifdef` only in HAL files | Platform guards in `Storage_HAL.h`, `Storage_ESP32.h`, `Storage_ESP8266.h`, `Storage_Stub.h` only |
| X. Non-Blocking Timer | No `delay()` | Uses `Utils::NonBlockingDelay` for status and maintenance timers |
| XI. Centralized Storage | All persistence via Storage component | This IS the centralized storage component; other components must use it |
| XII. Multi-Registry | PlatformIO + Arduino compatible | `library.json` present; `include/` and `examples/` follow standard layouts |
| XIV. Memory Leak Prevention | Heap stability | Cache bounded by `maxEntries`; `shrink_to_fit` should be applied after `clear()`/`remove()` in vectors |
| XV. Semantic Versioning | `library.json` matches `metadata.version` | Both set to `1.4.2` |

---

## Potential Improvement Areas

These observations are provided for AI assistants planning future work:

1. **Cache read-through**: `get*` methods currently bypass the cache and always read from the backend. A cache-first strategy could reduce HAL calls.
2. **putULong64 cache gap**: Unlike other `put*` methods, `putULong64` does not update the in-memory cache.
3. **getKeys scope**: `getKeys()` only returns registered keys that exist. Unregistered keys in the backend are invisible.
4. **WebUI write support**: `StorageWebUI::handleWebUIRequest` currently returns `{"success":false}` for all requests. The commented-out section in `Storage.h` shows intended CRUD operations.
5. **File size of Storage.h**: At ~651 lines it approaches the 800-line hard limit. If new features are added, consider extracting the cache or key-registration logic into separate headers.
