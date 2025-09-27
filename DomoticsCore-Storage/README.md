# DomoticsCore-Storage

Key-value storage component for DomoticsCore built on ESP32 Preferences/NVS with optional WebUI provider.

## Features

- Namespaced key-value storage (strings, numbers, booleans)
- Caching layer and periodic maintenance
- Simple API for get/set/remove/clear
- WebUI provider (optional): basic stats, CRUD helpers, settings

## Installation & Dependencies

- Requires Arduino-ESP32 core (`<Preferences.h>`).
- Include headers from this package with prefix `DomoticsCore/`:

```cpp
#include <DomoticsCore/Storage.h>
```

## Usage

```cpp
#include <DomoticsCore/Storage.h>
using namespace DomoticsCore::Components;

core.addComponent(std::make_unique<StorageComponent>());

auto* storage = core.getComponent<StorageComponent>("Storage");
if (storage) {
  storage->setString("wifi.ssid", "MySSID");
  String ssid = storage->getString("wifi.ssid");
}
```

## API Highlights

- `bool setString(const String& key, const String& value)`
- `String getString(const String& key, const String& defaultValue="")`
- `bool setNumber(const String& key, int value)` and typed variants
- `bool exists(const String& key)`
- `bool remove(const String& key)` / `void clearNamespace(const String& ns)`

Main behaviors:
- Opens ESP32 Preferences namespace defined in `StorageConfig` (default `domotics`).
- Maintains an in-memory cache and periodic maintenance timers.
- Respects `autoCommit` flag for automatic flush.
- Can be opened in read-only mode for diagnostics.

## Examples

- `DomoticsCore-Storage/examples/StorageNoWebUI` – headless storage usage.
- `DomoticsCore-Storage/examples/StorageWithWebUI` – adds the WebUI provider wrapper.

## License

MIT
