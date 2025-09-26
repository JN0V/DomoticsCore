# CoreOnly - Minimal DomoticsCore

This example demonstrates the minimal DomoticsCore setup with just the Core framework and logging.

## Features

- **Minimal Core**: Basic device configuration and logging
- **Custom Device Name**: Configurable device identification
- **Clean Architecture**: Foundation for step-by-step feature addition
- **Small Footprint**: Minimal dependencies and memory usage

## Configuration

```cpp
CoreConfig config;
config.deviceName = "MyESP32Device";  // Custom device name
config.logLevel = 3;                  // INFO level logging
```

## Usage

```bash
pio run -t upload -t monitor
```

## Next Steps

This minimal base can be extended step-by-step by adding:
1. Component system
2. WiFi functionality  
3. Web server
4. Individual components (LED, Storage, etc.)

All complex features have been moved to `../DomoticsCore-old/` directory for reference.
