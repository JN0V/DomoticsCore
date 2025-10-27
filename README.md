# DomoticsCore

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/JN0V/DomoticsCore/releases/tag/v1.0.0)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-orange.svg)](https://platformio.org/)

**Production-ready ESP32 framework for IoT applications** with modular architecture, automatic error handling, and visual status indicators.

> **🎉 Version 1.0.0 Released!** First stable release with complete modular architecture, LED error indicators, chunked HTTP responses, and production-ready components. See [CHANGELOG.md](CHANGELOG.md) for details.

## ✨ What Makes DomoticsCore Different

- **🔌 Truly Modular**: Only include what you need - from a 300KB minimal core to full-featured IoT system
- **🚨 Visual Debugging**: LED status indicators work even when system fails - perfect for headless devices
- **🛡️ Production Ready**: Comprehensive error handling, component health monitoring, and graceful degradation
- **🎯 Developer Friendly**: Header-only components, automatic dependency resolution, extensive examples
- **🔧 IoT Complete**: WiFi, MQTT, Home Assistant, OTA, WebUI, Storage - everything integrated and tested

## 🚀 Quick Start (3 Minutes)

### Option 1: Full System (Recommended for beginners)

Everything automatic: WiFi, LED status, remote console, error recovery!

```cpp
#include <DomoticsCore/System.h>

using namespace DomoticsCore;

System* domotics = nullptr;

void setup() {
    Serial.begin(115200);
    
    // Full-stack configuration
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "MyDevice";
    config.wifiSSID = "YOUR_WIFI";
    config.wifiPassword = "YOUR_PASSWORD";
    config.ledPin = 2;  // Visual status on GPIO 2
    
    domotics = new System(config);
    
    // Add custom console commands
    domotics->registerCommand("hello", [](const String& args) {
        return String("Hello from DomoticsCore!\n");
    });
    
    // Initialize - automatic WiFi, LED, Console, error handling
    if (!domotics->begin()) {
        DLOG_E(LOG_APP, "System initialization failed!");
        while (1) {
            domotics->loop();  // Keep LED error animation running
            yield();
        }
    }
    
    DLOG_I(LOG_APP, "System ready!");
}

void loop() {
    domotics->loop();  // Handles everything automatically
    
    // Your application code here
}
```

**That's it!** LED patterns show system state, telnet console on port 23, error recovery built-in.

**LED States:**
- 🔵 Fast blink (200ms): Booting
- 🟡 Slow blink (1000ms): WiFi connecting  
- 🟢 Pulse (2000ms): Connected, services starting
- 🟢 Breathing (3000ms): System ready
- 🔴 **Fast blink (300ms): ERROR** (LED works even in error state!)

### Option 2: Minimal Core (Advanced users)

Use only what you need - build your own orchestration:

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/LED.h>
#include <DomoticsCore/Wifi.h>

using namespace DomoticsCore;

Core core;

void setup() {
    // Add only the components you need
    core.addComponent(std::make_unique<Components::LEDComponent>());
    core.addComponent(std::make_unique<Components::WifiComponent>("SSID", "password"));
    
    // Initialize - automatic dependency resolution
    CoreConfig config;
    config.deviceName = "MinimalDevice";
    core.begin(config);
}

void loop() {
    core.loop();
}
```

Binary size: **~300KB** (vs 1MB+ for full system)

## 📦 Installation

### PlatformIO (Recommended)

Add to your `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0
```

### Specific Components Only

```ini
lib_deps = 
    symlink://path/to/DomoticsCore/DomoticsCore-Core
    symlink://path/to/DomoticsCore/DomoticsCore-LED
    symlink://path/to/DomoticsCore/DomoticsCore-Wifi
```

## 🧩 Available Components

| Component | Description | Size | Status |
|-----------|-------------|------|--------|
| **Core** | Essential framework, component registry, event bus | ~50KB | ✅ Stable |
| **System** | High-level orchestration (batteries included) | ~100KB | ✅ Stable |
| **WiFi** | Network connectivity with AP fallback | ~40KB | ✅ Stable |
| **LED** | Visual status indicators (6 effects) | ~20KB | ✅ Stable |
| **Storage** | NVS persistent data | ~30KB | ✅ Stable |
| **RemoteConsole** | Telnet debugging console | ~25KB | ✅ Stable |
| **WebUI** | Modern web interface with WebSocket | ~150KB | ✅ Stable |
| **MQTT** | Message broker with auto-reconnect | ~40KB | ✅ Stable |
| **NTP** | Time synchronization | ~15KB | ✅ Stable |
| **OTA** | Over-the-air updates | ~30KB | ✅ Stable |
| **HomeAssistant** | Auto-discovery integration | ~20KB | ✅ Stable |
| **SystemInfo** | Real-time monitoring with charts | ~25KB | ✅ Stable |

**Total with everything:** ~545KB flash, ~50KB RAM

## 📖 Documentation

- **[GETTING_STARTED.md](GETTING_STARTED.md)** - Comprehensive tutorial
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Design decisions and patterns
- **[CHANGELOG.md](CHANGELOG.md)** - Version history
- **Component READMEs** - See each `DomoticsCore-*/README.md`
- **Examples** - 20+ working examples in component directories

### Key Documentation

- **LED Effects**: [DomoticsCore-LED/README.md](DomoticsCore-LED/README.md)
- **Event Bus**: [DomoticsCore-Core/README.md](DomoticsCore-Core/README.md#event-bus)
- **WebUI Development**: [docs/WebUI-Developer-Guide.md](docs/WebUI-Developer-Guide.md)
- **Storage API**: [DomoticsCore-Storage/README.md](DomoticsCore-Storage/README.md)

## 📁 Project Structure

```
DomoticsCore/                      # Monorepo with 12 component packages
├── DomoticsCore-Core/             # Essential framework
├── DomoticsCore-System/           # High-level orchestration (batteries included)
├── DomoticsCore-WiFi/             # Network connectivity
├── DomoticsCore-LED/              # Visual status indicators
├── DomoticsCore-Storage/          # Persistent data (NVS)
├── DomoticsCore-RemoteConsole/    # Telnet debugging console
├── DomoticsCore-WebUI/            # Web interface with WebSocket
├── DomoticsCore-MQTT/             # Message broker client
├── DomoticsCore-NTP/              # Time synchronization
├── DomoticsCore-OTA/              # Firmware updates
├── DomoticsCore-HomeAssistant/    # Auto-discovery integration
└── DomoticsCore-SystemInfo/       # System monitoring

Each component has:
├── include/                       # Public headers
├── src/                           # Implementation (if needed)
├── examples/                      # Working examples
├── README.md                      # Component documentation
└── library.json                   # Package metadata (v1.0.0)
```

## 💡 Examples

### Full-Featured Application

**Location:** `DomoticsCore-System/examples/FullStack/`

Complete IoT device with:
- ✅ WiFi with AP fallback
- ✅ LED status indicators
- ✅ Telnet console (port 23)
- ✅ Web interface (port 80)
- ✅ MQTT with Home Assistant discovery
- ✅ OTA updates
- ✅ NTP time sync
- ✅ Persistent storage
- ✅ Custom sensor integration

**Binary:** ~900KB flash, ~50KB RAM

### Minimal Core

**Location:** `DomoticsCore-Core/examples/01-CoreOnly/`

Bare minimum:
- ✅ Component registry
- ✅ Logging system
- ✅ Non-blocking timers

**Binary:** ~250KB flash, ~15KB RAM

### LED Status Patterns

**Location:** `DomoticsCore-LED/examples/BasicLED/`

Demonstrates all LED effects:
- Solid on/off
- Blink (configurable speed)
- Fade in/out
- Pulse/heartbeat
- Breathing
- Rainbow cycle

### Component Development

**Location:** `DomoticsCore-Core/examples/02-CoreWithDummyComponent/`

Learn to build custom components:
- Component lifecycle (begin/loop/shutdown)
- Dependency declaration
- Configuration management
- Health monitoring

### Event Bus Communication

**Location:** `DomoticsCore-Core/examples/03-EventBusBasics/`

Inter-component messaging:
- Publish/subscribe pattern
- Sticky events
- Type-safe payloads
- Event coordination

## 🔧 Key Features Deep Dive

### Error Recovery

System continues running even when components fail:

```cpp
if (!domotics->begin()) {
    DLOG_E(LOG_APP, "Init failed!");
    while (1) {
        domotics->loop();  // LED shows ERROR, console still accessible
        yield();
    }
}
```

LED fast-blinks (300ms) to indicate error state. Telnet console remains available for debugging.

### Automatic Dependency Resolution

Components declare dependencies, framework initializes in correct order:

```cpp
class MyComponent : public IComponent {
    std::vector<String> getDependencies() const override {
        return {"Storage", "Wifi"};  // Will init after these
    }
};
```

### Visual Status Indicators

LED shows system state without serial console:

- **BOOTING** → Fast blink (200ms)
- **WIFI_CONNECTING** → Slow blink (1000ms)
- **WIFI_CONNECTED** → Pulse (2000ms)
- **READY** → Breathing (3000ms)
- **ERROR** → Fast blink (300ms)
- **OTA_UPDATE** → Solid on

### Event Bus

Decouple components with topic-based messaging:

```cpp
// Publisher
struct TempData { float celsius; };
eventBus().publish("sensor.temperature", TempData{22.5});

// Subscriber
eventBus().subscribe("sensor.temperature", [](const void* data) {
    auto* temp = static_cast<const TempData*>(data);
    Serial.printf("Temp: %.1f°C\n", temp->celsius);
}, this);
```

### Chunked HTTP Responses

WebUI handles large responses (>40KB) automatically:

```cpp
// Automatically uses chunked transfer encoding for large schemas
webUI->serveSchema();  // Works even with 50KB+ JSON
```

## 🤝 Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow existing code style
4. Add tests if applicable
5. Update documentation
6. Submit a pull request

See [ARCHITECTURE.md](ARCHITECTURE.md) for design patterns.

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

Built on top of excellent ESP32 ecosystem:
- **Arduino Core for ESP32**
- **ESPAsyncWebServer** (3.x)
- **AsyncTCP** (3.x)
- **PubSubClient** (MQTT)
- **ArduinoJson** (7.x)

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/JN0V/DomoticsCore/issues)
- **Discussions**: [GitHub Discussions](https://github.com/JN0V/DomoticsCore/discussions)
- **Documentation**: See `docs/` folder and component READMEs

## 🗺️ Roadmap

See [docs/ROADMAP.md](docs/ROADMAP.md) for planned features and improvements.

### Current Priorities
- PlatformIO Registry publication
- Additional component examples
- Performance optimization
- Extended Home Assistant integration

---

**Made with ❤️ for the ESP32 community**
