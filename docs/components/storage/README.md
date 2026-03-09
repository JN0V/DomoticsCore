# DomoticsCore-Storage

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## Overview

DomoticsCore-Storage is the centralized persistent key-value storage component for the DomoticsCore IoT framework. It abstracts platform-specific storage backends behind a Hardware Abstraction Layer (HAL), providing a unified API for reading and writing typed data across ESP32, ESP8266, and native/test environments.

On ESP32, data is persisted to NVS (Non-Volatile Storage) via the Preferences library. On ESP8266, LittleFS with JSON serialization is used. On other platforms (native unit tests), a RAM-only stub provides the same interface without persistence.

## Key Features

- **Typed key-value storage** -- String, int32, float, bool, uint64 (UInt64), and binary blob support
- **Namespace isolation** -- each StorageComponent instance operates within its own namespace, preventing key collisions between components
- **In-memory cache** -- write-through cache keeps recently stored entries in RAM for fast reads
- **Auto-commit** -- changes are flushed to the backend immediately by default
- **Read-only mode** -- open a namespace for diagnostic reads without risk of accidental writes
- **Key registration** -- components declare their storage keys with type and description metadata
- **Change notifications** -- emits `storage/changed` events (with a `StorageChangedEvent` POD struct containing the key name) on every `put*`, `remove`, or `clear` operation
- **Periodic maintenance** -- background timers report storage health and usage statistics
- **WebUI provider** -- optional `StorageWebUI` wrapper exposes namespace stats and settings to the DomoticsCore WebUI
- **Multi-platform HAL** -- ESP32 (Preferences/NVS), ESP8266 (LittleFS+JSON), Stub (RAM-only)

## Quick Start

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core core;

void setup() {
    StorageConfig cfg;
    cfg.namespace_name = "myapp";   // max 15 characters on ESP32
    cfg.maxEntries = 100;
    cfg.autoCommit = true;

    core.addComponent(std::make_unique<StorageComponent>(cfg));

    // Note: CoreConfig::deviceName is optional and only meaningful
    // when using Core directly. If using the System orchestrator,
    // configure via SystemConfig::deviceName instead.
    CoreConfig coreCfg;
    coreCfg.deviceName = "MyDevice";
    core.begin(coreCfg);

    auto* storage = core.getComponent<StorageComponent>("Storage");
    if (storage) {
        storage->putString("wifi_ssid", "MyNetwork");
        storage->putInt("boot_count", storage->getInt("boot_count", 0) + 1);
        storage->putBool("debug", true);
    }
}

void loop() {
    core.loop();
}
```

## Multiple Namespaces

Create separate `StorageComponent` instances with distinct namespaces to isolate configuration from application data:

```cpp
StorageConfig configStorage;
configStorage.namespace_name = "config";

StorageConfig dataStorage;
dataStorage.namespace_name = "appdata";

core.addComponent(std::make_unique<StorageComponent>(configStorage));
core.addComponent(std::make_unique<StorageComponent>(dataStorage));
```

## Platform Support

| Platform | Backend | Persistence | Notes |
|----------|---------|-------------|-------|
| ESP32 | Preferences (NVS) | Flash | 15-char namespace limit, hardware-backed |
| ESP8266 | LittleFS + JSON | Flash | 2 KB JSON document limit per namespace |
| Native/Test | RAM-only stub | None | 32-entry limit, for unit testing |

## Examples

- **BasicStorage** -- headless demo of all data types and lifecycle management
- **NamespaceDemo** -- two namespaces isolating config from app data
- **StorageWithWebUI** -- Storage component with the WebUI provider registered

## Related Documentation

- [Technical Reference](technical-reference.md) -- full API, HAL details, configuration options
- [Project Context](project-context.md) -- AI context, file inventory, dependency map, constitution compliance
