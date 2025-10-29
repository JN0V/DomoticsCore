# Installing DomoticsCore from GitHub

This example demonstrates how simple it is to use DomoticsCore directly from GitHub.

## Installation

1. Create your project with this `platformio.ini`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
board_build.partitions = min_spiffs.csv

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0

build_unflags = 
    -std=gnu++11

build_flags = 
    -std=gnu++14
```

2. Include and use:
```cpp
#include <DomoticsCore/System.h>
```

3. Build and upload:
```bash
pio run -t upload -t monitor
```

That's it! No manual configuration needed.

## What you get automatically
- All DomoticsCore components
- Correct include paths
- All dependencies (ArduinoJson, ESPAsyncWebServer, etc.)
- WebUI assets generated at compile time

## Troubleshooting
If compilation fails, ensure you're using DomoticsCore v1.0.0 or later:
```ini
lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0
```
