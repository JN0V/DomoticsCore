# DomoticsCore-HomeAssistant

**Version:** 0.1.0  
**MQTT Discovery Integration for Home Assistant**

Provides automatic entity discovery and state management for seamless Home Assistant integration via MQTT.

---

## ✨ Features

- **Automatic Entity Discovery** - Devices appear in Home Assistant without manual configuration
- **Multiple Entity Types** - Sensors, switches, lights, buttons, and binary sensors
- **Device Registry** - Groups entities under a single device in Home Assistant
- **Availability Tracking** - Online/offline status with Last Will Testament (LWT)
- **State Management** - Automatic state publishing and command handling
- **WebUI Integration** - Optional web interface for configuration and monitoring

---

## 📦 Dependencies

**Required:**
- `DomoticsCore-Core` >= 0.2.5
- `DomoticsCore-MQTT` >= 0.1.0 (MQTT client component)
- `ArduinoJson` ^7.0.0 (JSON serialization for discovery payloads)

**Optional:**
- `DomoticsCore-WebUI` >= 0.1.0 (for web interface)

**External Libraries Used:**
- **ArduinoJson**: Industry-standard JSON library with efficient memory usage, perfect for constrained embedded systems. Used for building discovery payloads and parsing commands.

---

## 🚀 Quick Start

### 1. Basic Setup (No WebUI)

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/HomeAssistant.h>

using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

Core core;

void setup() {
    // MQTT component
    MQTTConfig mqttCfg;
    mqttCfg.broker = "192.168.1.100";
    mqttCfg.enableLWT = true;
    mqttCfg.lwtTopic = "homeassistant/esp32-demo/availability";
    mqttCfg.lwtMessage = "offline";
    
    auto mqtt = std::make_unique<MQTTComponent>(mqttCfg);
    auto* mqttPtr = mqtt.get();
    core.addComponent(std::move(mqtt));
    
    // Home Assistant component
    HAConfig haCfg;
    haCfg.nodeId = "esp32-demo";
    haCfg.deviceName = "ESP32 Demo Device";
    auto ha = std::make_unique<HomeAssistantComponent>(mqttPtr, haCfg);
    auto* haPtr = ha.get();
    
    // Add entities
    haPtr->addSensor("temperature", "Temperature", "°C", "temperature");
    haPtr->addSwitch("relay", "Relay", [](bool state) {
        digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    });
    
    core.addComponent(std::move(ha));
    core.begin();
}
```

### 2. Publish States

```cpp
void loop() {
    core.loop();
    
    // Update sensor values
    if (updateTimer.isReady()) {
        haPtr->publishState("temperature", readTemperature());
        haPtr->publishState("relay", digitalRead(RELAY_PIN) == HIGH);
    }
}
```

---

## 🎛️ Supported Entity Types

### Sensor (Read-Only Values)

```cpp
// Temperature sensor with unit and device class
ha->addSensor("temperature", "Temperature", "°C", "temperature", "mdi:thermometer");

// Publish numeric value
ha->publishState("temperature", 22.5);
```

**Device Classes:** temperature, humidity, pressure, power, energy, voltage, current, signal_strength, etc.

### Binary Sensor (On/Off States)

```cpp
// Motion detector
ha->addBinarySensor("motion", "Motion", "motion", "mdi:motion-sensor");

// Publish state
ha->publishState("motion", true);  // or false
```

**Device Classes:** motion, door, window, smoke, gas, etc.

### Switch (Controllable On/Off)

```cpp
// Relay with command callback
ha->addSwitch("relay", "Relay", [](bool state) {
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    // Publish state back to HA
    haPtr->publishState("relay", state);
}, "mdi:electric-switch");
```

### Light (With Brightness)

```cpp
// LED with brightness control
ha->addLight("led", "LED Strip", [](bool state, uint8_t brightness) {
    if (state) {
        analogWrite(LED_PIN, brightness);
    } else {
        digitalWrite(LED_PIN, LOW);
    }
    
    // Publish state as JSON
    JsonDocument doc;
    doc["state"] = state ? "ON" : "OFF";
    doc["brightness"] = brightness;
    haPtr->publishStateJson("led", doc);
});
```

### Button (Trigger Actions)

```cpp
// Restart button
ha->addButton("restart", "Restart", []() {
    delay(1000);
    ESP.restart();
}, "mdi:restart");
```

---

## 📡 MQTT Topics Structure

### Discovery Topics
```
homeassistant/sensor/{node_id}/{entity_id}/config
homeassistant/switch/{node_id}/{entity_id}/config
homeassistant/light/{node_id}/{entity_id}/config
```

### State Topics
```
homeassistant/sensor/{node_id}/{entity_id}/state
homeassistant/switch/{node_id}/{entity_id}/state
```

### Command Topics
```
homeassistant/switch/{node_id}/{entity_id}/set
homeassistant/light/{node_id}/{entity_id}/set
```

### Availability Topic
```
homeassistant/{node_id}/availability
```

---

## 🎨 WebUI Integration

### Enable Web Interface

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/HomeAssistantWebUI.h>

// After creating components...
webui->registerProviderWithComponent(new HomeAssistantWebUI(haPtr), haPtr);
```

### WebUI Features
- **Status Badge** - Shows entity count and connection status
- **Dashboard Card** - Entity overview and statistics
- **Settings** - Configure device info and discovery settings
- **Component Detail** - Full statistics and entity list

---

## 📊 Discovery Payload Example

When you add a temperature sensor, this JSON is published to `homeassistant/sensor/esp32-demo/temperature/config`:

```json
{
  "name": "Temperature",
  "unique_id": "esp32-demo_temperature",
  "state_topic": "homeassistant/sensor/esp32-demo/temperature/state",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "state_class": "measurement",
  "icon": "mdi:thermometer",
  "device": {
    "identifiers": ["esp32-demo"],
    "name": "ESP32 Demo Device",
    "model": "ESP32",
    "manufacturer": "DomoticsCore",
    "sw_version": "1.0.0"
  },
  "availability_topic": "homeassistant/esp32-demo/availability",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

---

## 🔧 Configuration Options

```cpp
struct HAConfig {
    String nodeId = "esp32";              // Unique device ID
    String deviceName = "ESP32 Device";   // Display name
    String manufacturer = "DomoticsCore";
    String model = "ESP32";
    String swVersion = "1.0.0";
    bool retainDiscovery = true;          // Retain discovery messages
    String discoveryPrefix = "homeassistant";  // MQTT discovery prefix
    String availabilityTopic = "";        // Auto-generated if empty
    String configUrl = "";                // Device config URL (e.g., http://192.168.1.100)
    String suggestedArea = "";            // Suggested area in HA (e.g., "Living Room")
};
```

---

## 📈 Statistics

Access component statistics:

```cpp
const auto& stats = ha->getStatistics();
DLOG_I(LOG_CORE, "Entities: %d, State Updates: %d, Commands: %d",
       stats.entityCount, stats.stateUpdates, stats.commandsReceived);
```

---

## 🗂️ Examples

### BasicHA
Minimal example demonstrating:
- WiFi + MQTT + HA integration
- Temperature, humidity, uptime sensors
- Controllable relay switch
- Restart button
- ~945KB Flash, 14.1% RAM

### HAWithWebUI
Full-featured example with:
- Web interface for configuration
- Real-time monitoring dashboard
- WebSocket-based updates
- 7 entities (5 sensors, 1 switch, 1 button)
- ~1,178KB Flash, 14.5% RAM

---

## 🏠 Home Assistant Setup

### 1. Enable MQTT Integration
In Home Assistant:
- Go to Settings → Devices & Services
- Add MQTT integration
- Configure broker (e.g., Mosquitto)

### 2. Configure Discovery
MQTT integration automatically enables discovery with prefix `homeassistant`

### 3. Deploy Your Device
- Flash firmware to ESP32
- Device connects to WiFi and MQTT
- Entities appear automatically in Home Assistant!

### 4. View Your Device
- Go to Settings → Devices & Services → MQTT
- Find your device (e.g., "ESP32 Demo Device")
- See all entities grouped together

---

## 🔍 Debugging

Enable detailed logging:

```cpp
build_flags = -D LOG_LEVEL=LOG_LEVEL_DEBUG
```

Monitor MQTT traffic:
```bash
# Subscribe to all HA discovery messages
mosquitto_sub -h 192.168.1.100 -t "homeassistant/#" -v

# Check availability
mosquitto_sub -h 192.168.1.100 -t "homeassistant/esp32-demo/availability" -v
```

---

## 🛠️ API Reference

### Entity Management
```cpp
void addSensor(const String& id, const String& name, 
               const String& unit = "", const String& deviceClass = "", 
               const String& icon = "");
void addBinarySensor(const String& id, const String& name,
                     const String& deviceClass = "", const String& icon = "");
void addSwitch(const String& id, const String& name,
               std::function<void(bool)> callback, const String& icon = "");
void addLight(const String& id, const String& name,
              std::function<void(bool, uint8_t)> callback);
void addButton(const String& id, const String& name,
               std::function<void()> callback, const String& icon = "");
```

### State Publishing
```cpp
void publishState(const String& id, const String& state);
void publishState(const String& id, float value);
void publishState(const String& id, bool state);
void publishStateJson(const String& id, const JsonDocument& doc);
void publishAttributes(const String& id, const JsonDocument& attributes);
```

### Availability
```cpp
void setAvailable(bool available);
```

### Discovery
```cpp
void publishDiscovery();
void removeDiscovery();
void republishEntity(const String& id);
```

---

## 📄 License

MIT License - See repository for details

---

## 🤝 Contributing

Part of the DomoticsCore ecosystem. See main repository for contribution guidelines.

---

## 🔗 Related Components

- **DomoticsCore-Core** - Core framework
- **DomoticsCore-MQTT** - MQTT client
- **DomoticsCore-WebUI** - Web interface
- **DomoticsCore-NTP** - Time synchronization

---

**Made with ❤️ by DomoticsCore Contributors**
