# DomoticsCore

ESP32 domotics framework with WiFi, MQTT, web interface and Home Assistant integration.

## 🚀 Features

- **WiFi Management**: Auto-configuration with WiFiManager
- **Web Interface**: Configuration portal with responsive UI
- **MQTT Integration**: Full MQTT client with reconnection
- **Home Assistant**: Auto-discovery integration
- **OTA Updates**: Over-the-air firmware updates
- **LED Status**: Visual feedback system
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
    https://github.com/seb/DomoticsCore.git
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
    core.getHomeAssistant().publishSensor("temperature", "Temperature", "°C");
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

- ArduinoJson (>=6.21.0)
- PubSubClient (>=2.8.0)
- ESP Async WebServer (>=1.2.3)
- AsyncTCP (>=1.1.1)
- WiFiManager (>=2.0.17)

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

MIT License - see LICENSE file for details.

## 🆘 Support

- GitHub Issues: Report bugs and request features
- Documentation: Check the examples and source code
- Community: Share your projects and get help

---

**Created with ❤️ using Cascade AI**
