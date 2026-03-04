# DomoticsCore-Storage -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

---

## Table of Contents

1. [StorageConfig](#storageconfig)
2. [StorageComponent](#storagecomponent)
3. [Typed Getters and Setters](#typed-getters-and-setters)
4. [Key Registration](#key-registration)
5. [Key Management](#key-management)
6. [Storage Information](#storage-information)
7. [Caching Layer](#caching-layer)
8. [Namespace Isolation](#namespace-isolation)
9. [Read-Only Mode](#read-only-mode)
10. [Auto-Commit](#auto-commit)
11. [Events](#events)
12. [WebUI Provider](#webui-provider)
13. [Hardware Abstraction Layer (HAL)](#hardware-abstraction-layer-hal)
14. [Platform Implementations](#platform-implementations)
15. [Supporting Types](#supporting-types)
16. [Lifecycle](#lifecycle)

---

## StorageConfig

Defined in `DomoticsCore/Storage.h`. Passed to the `StorageComponent` constructor.

```cpp
struct StorageConfig {
    String namespace_name = "domotics";  // NVS namespace (max 15 chars on ESP32)
    String componentName  = "";          // Custom component name for multi-instance support
    bool   readOnly       = false;       // Open namespace in read-only mode
    size_t maxEntries     = 100;         // Logical entry cap (1--500)
    bool   autoCommit     = true;        // Flush writes immediately
};
```

### Validation rules (enforced in `begin()`)

| Rule | Constraint | Status on failure |
|------|-----------|-------------------|
| Namespace non-empty | `namespace_name.isEmpty() == false` | `ConfigError` |
| Namespace length | `namespace_name.length() <= 15` | `ConfigError` |
| Entry limit range | `1 <= maxEntries <= 500` | `ConfigError` |

---

## StorageComponent

```cpp
class StorageComponent : public IComponent
```

Declared in `DomoticsCore::Components`. Inherits the standard component lifecycle (`begin`, `loop`, `shutdown`) from `IComponent`.

### Constructor

```cpp
StorageComponent(const StorageConfig& config = StorageConfig());
```

- Initializes metadata immediately so the component registry can resolve dependencies before `begin()` is called.
- If `config.componentName` is non-empty, the component registers under that name; otherwise it registers as `"Storage"`.
- Metadata version: `"1.4.2"`.
- Category: `"Storage"`. Tags: `storage`, `preferences`, `nvs`, `settings`, `config`.

---

## Typed Getters and Setters

All put/get methods return `false` (or the default value) if the storage is not open.

### String

```cpp
bool   putString(const String& key, const String& value);
String getString(const String& key, const String& defaultValue = "");
```

### Integer (int32_t)

```cpp
bool    putInt(const String& key, int32_t value);
int32_t getInt(const String& key, int32_t defaultValue = 0);
```

### Float

```cpp
bool  putFloat(const String& key, float value);
float getFloat(const String& key, float defaultValue = 0.0f);
```

### Boolean

```cpp
bool putBool(const String& key, bool value);
bool getBool(const String& key, bool defaultValue = false);
```

### Unsigned 64-bit Integer

```cpp
bool     putULong64(const String& key, uint64_t value);
uint64_t getULong64(const String& key, uint64_t defaultValue = 0);
```

Note: `putULong64` does not update the in-memory cache (unlike the other put methods).

### Binary Blob

```cpp
bool   putBlob(const String& key, const uint8_t* data, size_t length);
size_t getBlob(const String& key, uint8_t* buffer, size_t maxLength);
```

`putBlob` writes exactly `length` bytes through `HAL::PlatformStorage::putBytes`. Returns `true` only when all bytes are written. `getBlob` reads up to `maxLength` bytes into the caller-provided buffer and returns the number of bytes actually read. If the stored blob is larger than `maxLength`, it is silently truncated and a warning is logged.

---

## Key Registration

Components can register their storage keys with type metadata to enable introspection and the `dumpContents()` diagnostic method.

### StorageKeyDef

```cpp
struct StorageKeyDef {
    String key;
    char   type;         // 's'=string, 'b'=bool, 'i'=int, 'f'=float, 'u'=uint64
    String description;
};
```

### registerKeys

```cpp
void registerKeys(const String& componentName, const std::vector<StorageKeyDef>& keys);
```

Appends the given key definitions to the internal `registeredKeys` vector. Multiple components may register keys against the same `StorageComponent` instance.

### getStoredKeyCount

```cpp
size_t getStoredKeyCount();
```

Returns the number of registered keys that currently exist in the storage backend.

### dumpContents

```cpp
String dumpContents();
```

Returns a formatted multi-line string listing every registered key and its current value. Password-like keys (containing `"pass"`) are masked with `****`.

---

## Key Management

### exists

```cpp
bool exists(const String& key);
```

Returns `true` if the key exists in the backend (delegates to `HAL::PlatformStorage::isKey`).

### remove

```cpp
bool remove(const String& key);
```

Removes the key from the backend and evicts it from the in-memory cache. Returns `true` on success.

### clear

```cpp
bool clear();
```

Removes all entries in the current namespace from the backend and clears the in-memory cache.

### getKeys

```cpp
std::vector<String> getKeys();
```

Returns a vector of registered key names that currently exist in the backend. Unregistered keys are not included.

---

## Storage Information

```cpp
bool   isOpenStorage()  const;   // Whether the backend is open
size_t getEntryCount()  const;   // Number of entries in the cache
size_t getFreeEntries() const;   // maxEntries - getEntryCount()
String getNamespace()   const;   // Current namespace name
String getStorageInfo();          // Multi-line human-readable status string
```

---

## Caching Layer

`StorageComponent` maintains an in-memory `std::map<String, StorageEntry>` cache. Every successful `put*` call (except `putULong64`) writes through to the backend and simultaneously updates the cache. The cache is consulted for `getEntryCount()` and `getFreeEntries()` but is **not** consulted for `get*` reads -- those always go to the backend. The cache is cleared on `shutdown()`, `clear()`, and individual entries are evicted on `remove()`.

---

## Namespace Isolation

Each `StorageComponent` instance opens its own NVS namespace (ESP32) or JSON file (ESP8266). Two instances with different `namespace_name` values are fully isolated: keys in one namespace are invisible to the other.

The constitution (Section XI -- Centralized Storage) mandates that all persistent data go through the Storage component. Direct `Preferences` or `SPIFFS` access is forbidden.

When running multiple `StorageComponent` instances, set `StorageConfig::componentName` to a unique value so the component registry can distinguish them.

---

## Read-Only Mode

Set `StorageConfig::readOnly = true` to open the namespace without write permission. On ESP32, this maps directly to `Preferences::begin(name, true)`. All `put*` calls will fail because the HAL backend rejects writes when opened read-only.

---

## Auto-Commit

When `StorageConfig::autoCommit` is `true` (the default), the HAL backend flushes data to persistent storage on every write. On ESP32, NVS commits are inherent per-call. On ESP8266, the LittleFS JSON file is rewritten after every `put*` call when the dirty flag is set.

---

## Events

Defined in `DomoticsCore/StorageEvents.h` under `DomoticsCore::StorageEvents`.

| Constant | Value | Description |
|----------|-------|-------------|
| `EVENT_READY` | `"storage/ready"` | Emitted after `begin()` succeeds. Payload is the namespace name. |

Subscribe to this event to perform initialization that depends on storage being available.

---

## WebUI Provider

Defined in `DomoticsCore/StorageWebUI.h`. The `StorageWebUI` class extends `CachingWebUIProvider` and holds a non-owning pointer to a `StorageComponent`.

### Registration

```cpp
auto* webui   = core.getComponent<WebUIComponent>("WebUI");
auto* storage = core.getComponent<StorageComponent>("Storage");
if (webui && storage) {
    webui->registerProviderWithComponent(
        new DomoticsCore::Components::WebUI::StorageWebUI(storage), storage);
}
```

### Provided Contexts

| Context ID | Location | Fields |
|------------|----------|--------|
| `storage_component` | ComponentDetail | namespace, entries, free_entries (real-time, 5 s interval) |
| `storage_settings` | Settings | namespace (display only) |

### Data Endpoint

`getWebUIData(contextId)` returns a JSON object with the requested fields populated from the live `StorageComponent` instance.

---

## Hardware Abstraction Layer (HAL)

### IStorage Interface

Declared in `DomoticsCore/Storage_HAL.h` under `DomoticsCore::HAL`.

```cpp
class IStorage {
public:
    virtual bool     begin(const char* namespace_name, bool readOnly = false) = 0;
    virtual void     end() = 0;
    virtual bool     isKey(const char* key) = 0;

    virtual bool     putString(const char* key, const String& value) = 0;
    virtual String   getString(const char* key, const String& defaultValue = "") = 0;

    virtual bool     putInt(const char* key, int32_t value) = 0;
    virtual int32_t  getInt(const char* key, int32_t defaultValue = 0) = 0;

    virtual bool     putBool(const char* key, bool value) = 0;
    virtual bool     getBool(const char* key, bool defaultValue = false) = 0;

    virtual bool     putFloat(const char* key, float value) = 0;
    virtual float    getFloat(const char* key, float defaultValue = 0.0f) = 0;

    virtual bool     putULong64(const char* key, uint64_t value) = 0;
    virtual uint64_t getULong64(const char* key, uint64_t defaultValue = 0) = 0;

    virtual size_t   putBytes(const char* key, const uint8_t* data, size_t len) = 0;
    virtual size_t   getBytes(const char* key, uint8_t* buffer, size_t maxLen) = 0;
    virtual size_t   getBytesLength(const char* key) = 0;

    virtual bool     remove(const char* key) = 0;
    virtual bool     clear() = 0;
    virtual size_t   freeEntries() = 0;
};
```

### Platform Routing

`Storage_HAL.h` uses compile-time `#ifdef` guards (the only place such guards are permitted, per constitution Section IX) to select the concrete implementation:

| Macro | Implementation File | Concrete Class | Alias |
|-------|-------------------|----------------|-------|
| `DOMOTICS_PLATFORM_ESP32` | `Storage_ESP32.h` | `PreferencesStorage` | `PlatformStorage` |
| `DOMOTICS_PLATFORM_ESP8266` | `Storage_ESP8266.h` | `LittleFSStorage` | `PlatformStorage` |
| _(else)_ | `Storage_Stub.h` | `RAMOnlyStorage` | `PlatformStorage` |

`StorageComponent` uses `HAL::PlatformStorage` as its storage member, resolved at compile time.

---

## Platform Implementations

### ESP32 -- PreferencesStorage

Wraps the Arduino-ESP32 `Preferences` library. All calls delegate directly to the underlying `Preferences` object. NVS imposes a **15-character limit** on namespace names and individual key names. `freeEntries()` returns the actual NVS free entry count.

### ESP8266 -- LittleFSStorage

Stores each namespace as a JSON file at `/<namespace>.json` on LittleFS. Uses ArduinoJson 7.x with a `JsonDocument` limited to **2 KB** per namespace (FR-003c). Binary blobs are hex-encoded. Corrupted JSON files are silently cleared to defaults (FR-003d). `freeEntries()` returns a fixed 1000 since LittleFS has no entry limit.

### Stub -- RAMOnlyStorage

A fixed-size array of 32 entries stored in RAM. Keys are prefixed with `namespace:` for isolation. No persistence. Intended for native unit tests. `freeEntries()` returns `MAX_ENTRIES - count`.

---

## Supporting Types

### StorageValueType

```cpp
enum class StorageValueType { String, Integer, Float, Boolean, Blob };
```

### StorageEntry

```cpp
struct StorageEntry {
    String                key;
    StorageValueType      type;
    String                stringValue;
    int32_t               intValue;
    float                 floatValue;
    bool                  boolValue;
    std::vector<uint8_t>  blobValue;
    size_t                size;
};
```

Used internally by the caching layer. Each `put*` call constructs a `StorageEntry` and stores it in the cache map.

---

## Lifecycle

### begin()

1. Validates `StorageConfig` (namespace non-empty, length <= 15, maxEntries in range).
2. Calls `HAL::PlatformStorage::begin()` with the configured namespace and read-only flag.
3. On success, sets `isOpen = true`, updates storage info, and emits `StorageEvents::EVENT_READY`.
4. Returns `ComponentStatus::Success`, `ConfigError`, or `HardwareError`.

### loop()

1. Exits immediately if the last status is not `Success`.
2. Every 5 minutes (300 000 ms): updates storage info and reports status.
3. Every 5 minutes: runs maintenance (cache statistics, capacity warnings).

### shutdown()

1. Calls `HAL::PlatformStorage::end()` to close the backend.
2. Clears the in-memory cache.
3. Returns `ComponentStatus::Success`.
