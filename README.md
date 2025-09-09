# DomoticsCore

ESP32 domotics framework with WiFi, MQTT, web interface and Home Assistant integration.

## 🚀 Features

- **WiFi Management**: Auto-configuration with WiFiManager
- **Web Interface**: Configuration portal with responsive UI
- **MQTT Integration**: Full MQTT client with reconnection
- **Home Assistant**: Auto-discovery integration
- **OTA Updates**: Over-the-air firmware updates
- **LED Status**: Visual feedback system
- **Unified Logging**: Decentralized, extensible logging system
- **Modular Design**: Clean, extensible architecture

## 📦 Installation

### PlatformIO Library Manager
```bash
pio lib install "DomoticsCore"
```

### Git Dependency
Add to your `platformio.ini`:
```ini
lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v0.1.5
```

### Local Development
```ini
lib_deps = 
    file:///path/to/DomoticsCore
```

## 🔧 Quick Start

```cpp
#include <DomoticsCore/DomoticsCore.h>

CoreConfig config;
DomoticsCore core(config);

void setup() {
  // Configure your project
  config.deviceName = "MyDevice";
  config.mqttEnabled = true;
  config.homeAssistantEnabled = true;
  
  // Start the framework
  core.begin();
  
  // Add Home Assistant entities
  if (core.isHomeAssistantEnabled()) {
    core.getHomeAssistant().publishSensor("temperature", "Temperature", "°C", "temperature");
  }
}

void loop() {
  core.loop();
  
  // Your application code here
}
```

## 📖 Documentation

### Core Configuration
```cpp
CoreConfig config;
config.deviceName = "MyESP32";           // Device identifier
config.manufacturer = "MyCompany";        // Manufacturer name
config.webServerPort = 80;               // Web server port
config.ledPin = 2;                       // Status LED pin

// MQTT Configuration
config.mqttEnabled = true;
config.mqttServer = "192.168.1.100";
config.mqttPort = 1883;

// Home Assistant Integration
config.homeAssistantEnabled = true;
config.homeAssistantDiscoveryPrefix = "homeassistant";
```

### Web Interface
The framework provides a complete web interface accessible at `http://[device-ip]/`:
- WiFi configuration
- MQTT settings
- Home Assistant integration
- System information
- OTA updates

### Home Assistant Integration
```cpp
// Publish sensors
core.getHomeAssistant().publishSensor("temperature", "Temperature Sensor", "°C", "temperature");
core.getHomeAssistant().publishSensor("humidity", "Humidity Sensor", "%", "humidity");

// Publish switches
core.getHomeAssistant().publishSwitch("relay1", "Main Relay");

// Publish binary sensors
core.getHomeAssistant().publishBinarySensor("motion", "Motion Sensor", "motion");

// Publish values via MQTT
String deviceId = core.config().deviceName;
core.getMQTTClient().publish(("jnov/" + deviceId + "/temperature/state").c_str(), "23.5");
```

### Unified Logging System
The framework provides a decentralized logging system with component-based tagging:

```cpp
#include <DomoticsCore/Logger.h>

// Use predefined library tags
DLOG_I(LOG_CORE, "System initialized");
DLOG_W(LOG_WIFI, "Connection unstable, RSSI: %d", WiFi.RSSI());
DLOG_E(LOG_MQTT, "Broker connection failed");

// Define custom application tags
#define LOG_SENSOR "SENSOR"
#define LOG_PUMP   "PUMP"

DLOG_I(LOG_SENSOR, "Temperature: %.2f°C", temperature);
DLOG_E(LOG_PUMP, "Motor failure detected");

// Or use inline custom tags
DLOG_D("CUSTOM", "My component message");
```

**Available Log Levels:**
- `DLOG_E` - Error messages
- `DLOG_W` - Warning messages  
- `DLOG_I` - Information messages
- `DLOG_D` - Debug messages
- `DLOG_V` - Verbose messages

**Predefined Component Tags:**
`LOG_CORE`, `LOG_WIFI`, `LOG_MQTT`, `LOG_HTTP`, `LOG_HA`, `LOG_OTA`, `LOG_LED`, `LOG_SECURITY`, `LOG_WEB`, `LOG_SYSTEM`

**Log Level Control:**
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=3  ; 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose
```

### LED Status System
The framework includes a comprehensive LED status system:
- **Solid ON (3s)**: System starting
- **Slow blink (1s)**: AP configuration mode
- **Medium blink (500ms)**: WiFi connecting
- **Fast blink (200ms)**: WiFi reconnecting
- **Very fast blink (100ms)**: Connection failed
- **Heartbeat (2s)**: Normal operation

## 📁 Project Structure

```
DomoticsCore/
├── library.json              # Library metadata
├── README.md                 # This file
├── include/DomoticsCore/     # Public headers
│   ├── Config.h              # Configuration structures
│   ├── DomoticsCore.h        # Main framework class
│   ├── HomeAssistant.h       # HA integration
│   ├── LEDManager.h          # LED status management
│   ├── Logger.h              # Unified logging system
│   ├── OTAManager.h          # OTA updates
│   ├── SystemUtils.h         # System utilities
│   └── WebConfig.h           # Web interface
├── src/                      # Implementation files
│   ├── DomoticsCore.cpp
│   ├── homeassistant/
│   ├── led/
│   ├── ota/
│   ├── system/
│   └── web/
└── examples/                 # Example projects
    ├── AdvancedApp/
    └── BasicApp/
```

## 🔗 Dependencies

- ArduinoJson (>=6.21.0) - MIT License
- PubSubClient (>=2.8.0) - MIT License
- ESP Async WebServer (>=1.2.3) - LGPL 2.1 License
- AsyncTCP (>=1.1.1) - LGPL 3.0 License
- WiFiManager (>=2.0.17) - MIT License

## 📋 Examples

### Basic Usage
See `examples/BasicApp/` for a minimal implementation.

### Advanced Usage
See `examples/AdvancedApp/` for a complete example with:
- Custom web routes
- MQTT messaging
- Home Assistant integration
- System monitoring

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see LICENSE file for details.

### Third-Party Licenses

This project uses several third-party libraries with different licenses:

- **ArduinoJson** by Benoit Blanchon - MIT License
- **PubSubClient** by Nick O'Leary - MIT License  
- **WiFiManager** by tzapu - MIT License
- **ESP Async WebServer** by Hristo Gochkov - LGPL 2.1 License
- **AsyncTCP** by me-no-dev - LGPL 3.0 License

**Important**: The ESP Async WebServer and AsyncTCP libraries are licensed under LGPL, which means:
- You can use this library in commercial projects
- If you modify the LGPL-licensed components, you must make those modifications available under LGPL
- Your application code remains under your chosen license
- Static linking is allowed, but you must provide a way for users to replace the LGPL components

For full license compliance, please review the individual license files of each dependency.

## 🆘 Support

- GitHub Issues: Report bugs and request features
- Documentation: Check the examples and source code
- Community: Share your projects and get help

---

**Created with ❤️ using Cascade AI**
