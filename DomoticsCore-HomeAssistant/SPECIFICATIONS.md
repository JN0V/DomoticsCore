# DomoticsCore-HomeAssistant Component Specifications

**Version:** 0.1.0  
**Component:** DomoticsCore-HomeAssistant  
**Date:** 2025-10-02

---

## 1. Overview

Home Assistant MQTT Discovery integration component providing automatic entity registration and state management.

### 1.1 Purpose
- Automatic entity discovery via MQTT
- Device registry (grouping entities)
- State reporting and command handling
- Availability tracking
- Entity configuration UI

### 1.2 Dependencies
- **Required:** 
  - DomoticsCore-Core >= 0.2.5
  - DomoticsCore-MQTT >= 0.1.0
  - ArduinoJson >= 7.0.0
- **Optional:** 
  - DomoticsCore-WebUI >= 0.1.0

---

## 2. Core Features

### 2.1 MQTT Discovery Protocol
- Discovery topic format: `homeassistant/{component}/{node_id}/{object_id}/config`
- JSON payload with entity configuration
- Automatic cleanup on component removal
- Unique ID per entity
- Device registry grouping

### 2.2 Supported Entity Types
- **Sensor**: Read-only numeric/text values ✅
- **Binary Sensor**: On/off states (motion, door, etc.) ✅
- **Switch**: Toggle control ✅
- **Light**: Brightness control ✅
- **Button**: Trigger actions ✅
- **Number**: Numeric input (sliders) ⏳ Future
- **Select**: Dropdown selection ⏳ Future
- **Text**: Text input ⏳ Future

### 2.3 Device Information
- Device name, model, manufacturer
- Software version
- Hardware identifiers (MAC address)
- Configuration URL
- Suggested area

### 2.4 Availability
- Online/offline status
- LWT (Last Will Testament)
- Per-entity or device-level

---

## 3. API Design

### 3.1 Configuration

```cpp
struct HAConfig {
    String nodeId = "esp32";            // Unique device ID
    String deviceName = "ESP32 Device";
    String manufacturer = "DomoticsCore";
    String model = "ESP32";
    String swVersion = "1.0.0";
    bool retainDiscovery = true;
    String discoveryPrefix = "homeassistant";
    String availabilityTopic = "";      // Auto-generated if empty
    String configUrl = "";              // Device configuration URL
    String suggestedArea = "";          // Suggested area in HA
};
```

### 3.2 Component Interface

```cpp
class HomeAssistantComponent : public IComponent {
public:
    HomeAssistantComponent(MQTTComponent* mqtt, const HAConfig& cfg = HAConfig());
    
    // IComponent interface
    String getName() const override { return "HomeAssistant"; }
    String getVersion() const override { return "0.1.0"; }
    ComponentStatus begin() override;
    void loop() override;
    ComponentStatus shutdown() override;
    
    // Entity Management
    void addSensor(const String& id, const String& name, const String& unit = "", 
                   const String& deviceClass = "", const String& icon = "");
    void addBinarySensor(const String& id, const String& name, 
                         const String& deviceClass = "", const String& icon = "");
    void addSwitch(const String& id, const String& name,
                   std::function<void(bool)> commandCallback, const String& icon = "");
    void addLight(const String& id, const String& name, 
                  std::function<void(bool, uint8_t)> commandCallback);
    void addButton(const String& id, const String& name,
                   std::function<void()> pressCallback, const String& icon = "");
    
    // State Publishing
    void publishState(const String& id, const String& state);
    void publishState(const String& id, float value);
    void publishState(const String& id, bool state);
    void publishStateJson(const String& id, const JsonDocument& doc);
    void publishAttributes(const String& id, const JsonDocument& attributes);
    
    // Availability
    void setAvailable(bool available);
    void setEntityAvailable(const String& id, bool available);
    
    // Discovery
    void publishDiscovery();
    void removeDiscovery();
    void republishEntity(const String& id);
    
    // Configuration
    void setConfig(const HAConfig& cfg);
    const HAConfig& getHAConfig() const { return config; }
    
    // Device Info
    void setDeviceInfo(const String& name, const String& model, 
                       const String& manufacturer, const String& swVersion);
    
    // Statistics
    struct HAStatistics {
        uint32_t entityCount = 0;
        uint32_t discoveryCount = 0;
        uint32_t stateUpdates = 0;
        uint32_t commandsReceived = 0;
    };
    const HAStatistics& getStatistics() const { return stats; }
};
```

### 3.3 Entity Classes

```cpp
class HAEntity {
public:
    String id;                  // Unique ID
    String name;                // Display name
    String icon;                // mdi:icon-name
    String deviceClass;         // HA device class
    bool retained = true;
    
    virtual String getDiscoveryTopic() const = 0;
    virtual JsonDocument getDiscoveryPayload() const = 0;
    virtual String getStateTopic() const = 0;
};

class HASensor : public HAEntity {
public:
    String unit;                // °C, %, etc.
    String stateClass;          // measurement, total, total_increasing
    float expireAfter = 0;      // seconds (0 = never)
    
    JsonDocument getDiscoveryPayload() const override;
};

class HASwitch : public HAEntity {
public:
    String commandTopic;
    std::function<void(bool)> callback;
    bool optimistic = false;
    
    JsonDocument getDiscoveryPayload() const override;
    void handleCommand(const String& payload);
};
```

---

## 4. MQTT Topics Structure

### 4.1 Discovery Topics
```
homeassistant/sensor/{node_id}/{object_id}/config
homeassistant/binary_sensor/{node_id}/{object_id}/config
homeassistant/switch/{node_id}/{object_id}/config
homeassistant/light/{node_id}/{object_id}/config
```

### 4.2 State Topics
```
homeassistant/sensor/{node_id}/{object_id}/state
homeassistant/switch/{node_id}/{object_id}/state
```

### 4.3 Command Topics
```
homeassistant/switch/{node_id}/{object_id}/set
homeassistant/light/{node_id}/{object_id}/set
```

### 4.4 Availability
```
homeassistant/{node_id}/availability
```

---

## 5. Usage Examples

### 5.1 Basic Sensor Discovery

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/HomeAssistant.h>

Core core;

void setup() {
    // MQTT component
    MQTTConfig mqttCfg;
    mqttCfg.broker = "192.168.1.100";
    auto mqtt = std::make_unique<MQTTComponent>(mqttCfg);
    auto* mqttPtr = mqtt.get();
    core.addComponent(std::move(mqtt));
    
    // Home Assistant component
    HAConfig haCfg;
    haCfg.nodeId = "esp32-sensor";
    haCfg.deviceName = "Living Room Sensor";
    auto ha = std::make_unique<HomeAssistantComponent>(mqttPtr, haCfg);
    auto* haPtr = ha.get();
    
    // Add temperature sensor
    haPtr->addSensor("temperature", "Temperature", "°C", "temperature");
    haPtr->addSensor("humidity", "Humidity", "%", "humidity");
    
    core.addComponent(std::move(ha));
    core.begin();
}

void loop() {
    core.loop();
    
    // Update sensor values
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 60000) {  // Every minute
        auto* ha = core.getComponent<HomeAssistantComponent>("HomeAssistant");
        if (ha) {
            ha->publishState("temperature", readTemperature());
            ha->publishState("humidity", readHumidity());
        }
        lastUpdate = millis();
    }
}
```

### 5.2 Switch with Callback

```cpp
// Add controllable relay
haPtr->addSwitch("relay1", "Relay 1", [](bool state) {
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    DLOG_I(LOG_CORE, "Relay 1 set to: %s", state ? "ON" : "OFF");
});

// Publish initial state
haPtr->publishState("relay1", digitalRead(RELAY_PIN) == HIGH);
```

### 5.3 LED as Light

```cpp
haPtr->addLight("led_strip", "LED Strip", [](bool state, uint8_t brightness) {
    if (state) {
        ledStrip.setBrightness(brightness);
        ledStrip.show();
    } else {
        ledStrip.clear();
        ledStrip.show();
    }
});
```

---

## 6. Discovery Payload Examples

### 6.1 Sensor (Temperature)

```json
{
  "name": "Temperature",
  "unique_id": "esp32-sensor_temperature",
  "state_topic": "homeassistant/sensor/esp32-sensor/temperature/state",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "state_class": "measurement",
  "icon": "mdi:thermometer",
  "device": {
    "identifiers": ["esp32-sensor"],
    "name": "Living Room Sensor",
    "model": "ESP32",
    "manufacturer": "DomoticsCore",
    "sw_version": "1.0.0"
  },
  "availability_topic": "homeassistant/esp32-sensor/availability",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

### 6.2 Switch (Relay)

```json
{
  "name": "Relay 1",
  "unique_id": "esp32-sensor_relay1",
  "state_topic": "homeassistant/switch/esp32-sensor/relay1/state",
  "command_topic": "homeassistant/switch/esp32-sensor/relay1/set",
  "payload_on": "ON",
  "payload_off": "OFF",
  "state_on": "ON",
  "state_off": "OFF",
  "icon": "mdi:electric-switch",
  "device": { ... },
  "availability_topic": "homeassistant/esp32-sensor/availability"
}
```

---

## 7. WebUI Integration

### 7.1 Contexts

**Status Badge:**
- Shows: Entity count, connection status
- Icon: `mdi:home-assistant`

**Dashboard Card:**
- Shows: Registered entities list
- Quick actions: Republish discovery, set availability

**Settings:**
- Device configuration
- Discovery prefix
- Node ID
- Entity management

**Component Detail:**
- All entities with types
- State values
- Last command received
- Statistics

---

## 8. Implementation Notes

### 8.1 Discovery on Connect
```cpp
mqttPtr->onConnect([haPtr]() {
    haPtr->setAvailable(true);
    haPtr->publishDiscovery();
});
```

### 8.2 LWT Configuration
```cpp
// Configure MQTT LWT for availability
mqttCfg.enableLWT = true;
mqttCfg.lwtTopic = "homeassistant/esp32-sensor/availability";
mqttCfg.lwtMessage = "offline";
mqttCfg.lwtQoS = 1;
mqttCfg.lwtRetain = true;
```

### 8.3 Command Handling
```cpp
mqttPtr->onMessage("homeassistant/+/+/+/set", [haPtr](const String& topic, const String& payload) {
    haPtr->handleCommand(topic, payload);
});
```

---

## 9. File Structure

```
DomoticsCore-HomeAssistant/
├── library.json
├── README.md
├── SPECIFICATIONS.md (this file)
├── include/
│   └── DomoticsCore/
│       ├── HomeAssistant.h      # Main component
│       ├── HAEntity.h           # Base entity
│       ├── HASensor.h           # Sensor entity
│       ├── HASwitch.h           # Switch entity
│       ├── HALight.h            # Light entity
│       ├── HABinarySensor.h     # Binary sensor
│       └── HomeAssistantWebUI.h # WebUI provider
└── examples/
    ├── BasicHA/
    │   ├── platformio.ini
    │   ├── src/main.cpp
    │   └── README.md
    └── HAWithWebUI/
        ├── platformio.ini
        ├── src/main.cpp
        └── README.md
```

---

