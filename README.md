# DomoticsCore

ESP32 domotics framework with WiFi, MQTT, web interface, Home Assistant integration, and persistent storage.

## 🚀 Features

- **WiFi Management**: Custom WiFi management with web interface
- **Web Interface**: Configuration portal with responsive UI
- **MQTT Integration**: Full MQTT client with reconnection
- **Home Assistant**: Auto-discovery integration
- **OTA Updates**: Over-the-air firmware updates
- **Persistent Storage**: Application data storage with preferences separation
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
    https://github.com/JN0V/DomoticsCore.git#v0.2.0
```

### Local Development
```ini
lib_deps = 
    file:///path/to/DomoticsCore
```

## 🔧 Quick Start

### Basic Usage
```cpp
#include <DomoticsCore/DomoticsCore.h>

#define SENSOR_PIN A0

DomoticsCore* core = nullptr;
float sensorValue = 0.0;

void setup() {
  CoreConfig config;
  config.deviceName = "BasicExample";
  config.firmwareVersion = "1.0.0";
  
  core = new DomoticsCore(config);
  
  // Add REST API endpoint
  core->webServer().on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"sensor_value\":" + String(sensorValue) + "}";
    request->send(200, "application/json", json);
  });
  
  core->begin();
}

void loop() {
  core->loop();
  
  // Read sensor every 5 seconds
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 5000) {
    sensorValue = (analogRead(SENSOR_PIN) / 4095.0) * 100.0;
    lastRead = millis();
  }
}
```

### Advanced Usage
For MQTT, Home Assistant, storage, and relay control, see `examples/AdvancedApp/`.

## 🧩 EventBus (topic-based, decoupled)

DomoticsCore provides a lightweight, topic-based EventBus to enable cross-component communication without tight coupling.

- Publish/subscribe by topic strings (e.g., `"wifi.connected"`, `"storage.mounted"`).
- Payloads are plain structs defined by the publishing component.
- Dispatch is queued and processed in the main loop (non-ISR safe).

Basic usage:

```cpp
#include <DomoticsCore/Utils/EventBus.h>

// Define topics and payloads inside your component (recommended)
namespace WifiEvents {
  static constexpr const char* Connected = "wifi.connected";
  struct ConnectedPayload { String ssid; IPAddress ip; int rssi; };
}

// Publisher (e.g., WiFi component)
auto& bus = registry.getEventBus();
bus.publish(WifiEvents::Connected, WifiEvents::ConnectedPayload{ ssid, WiFi.localIP(), WiFi.RSSI() });

// Subscriber (e.g., LED component)
subscriptionId_ = registry.getEventBus().subscribe(
  WifiEvents::Connected,
  [this](const void* p){
    auto* payload = static_cast<const WifiEvents::ConnectedPayload*>(p);
    this->setOn(true);
  },
  this
);

// Cleanup on shutdown
registry.getEventBus().unsubscribeOwner(this);
```

Notes:

- The core no longer defines domain-specific events. Each component owns its event topics and payloads.
- A minimal enum `EventType::Custom` remains for rare global signals; most apps should prefer topics.
- The bus is available via `ComponentRegistry::getEventBus()` and is polled automatically in `loop()`.

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

### Persistent Storage System
The framework provides a robust storage system that separates system preferences from application data:

```cpp
// Store application data
core->storage().putULong("boot_count", bootCount);
core->storage().putFloat("sensor_threshold", 75.5);
core->storage().putString("device_nickname", "Living Room Sensor");

// Retrieve application data with defaults
unsigned long boots = core->storage().getULong("boot_count", 0);
float threshold = core->storage().getFloat("sensor_threshold", 50.0);
String nickname = core->storage().getString("device_nickname", "My Device");

// Check if keys exist
if (core->storage().isKey("calibration_offset")) {
    float offset = core->storage().getFloat("calibration_offset");
}

// Storage management
core->storage().remove("old_key");        // Remove specific key
core->storage().clear();                  // Clear all application data
size_t entries = core->storage().freeEntries(); // Get available space
```

**Supported Data Types:**
- `bool`, `uint8_t`, `int16_t`, `uint16_t`
- `int32_t`, `uint32_t`, `int64_t`, `uint64_t`
- `float`, `double`, `String`
- Binary data via `putBytes()`/`getBytes()`

**Storage Namespaces:**
- **System Preferences** (`esp32-config`): Used internally for WiFi, MQTT, and web config
- **Application Data** (`app-data`): Available for your application use
- Complete separation prevents conflicts between system and application data

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
`LOG_CORE`, `LOG_WIFI`, `LOG_MQTT`, `LOG_HTTP`, `LOG_HA`, `LOG_OTA`, `LOG_LED`, `LOG_SECURITY`, `LOG_WEB`, `LOG_SYSTEM`, `LOG_STORAGE`

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
│   ├── Storage.h             # Persistent storage system
│   ├── SystemUtils.h         # System utilities
│   └── WebConfig.h           # Web interface
├── src/                      # Implementation files
│   ├── DomoticsCore.cpp
│   ├── homeassistant/
│   ├── led/
│   ├── ota/
│   ├── storage/
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

## 📋 Examples

### BasicApp
Simple sensor monitoring with REST API (21 lines):
- Analog sensor reading on pin A0
- Single REST endpoint: `GET /api/sensor`
- Minimal configuration example

### AdvancedApp  
Comprehensive IoT device with hardware control and storage:
- Sensor monitoring with configurable threshold
- Persistent boot counter and device nickname
- Relay control via `POST /api/relay`
- Configuration endpoints (`POST /api/config`)
- Storage management (`GET /api/storage/stats`, `POST /api/storage/clear`)
- MQTT integration with 30-second updates
- Home Assistant auto-discovery (5 sensors including boot count)
- Threshold-based automation logic
- Specific logging for sensor/relay/storage operations

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
