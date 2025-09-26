# DomoticsCore-Storage

Key-value storage component for DomoticsCore built on ESP32 Preferences/NVS with optional WebUI provider.

## Features

- Namespaced key-value storage (strings, numbers, booleans)
- Caching layer and periodic maintenance
- Simple API for get/set/remove/clear
- WebUI provider (optional): basic stats, CRUD helpers, settings

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

## License

MIT
