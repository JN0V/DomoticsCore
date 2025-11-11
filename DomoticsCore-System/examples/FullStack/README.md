# Full Stack Example

**Complete IoT solution with MQTT and Home Assistant!**

## What You Get

This example provides **everything** for enterprise IoT:

- ✅ **WiFi** - Automatic connection with AP fallback
- ✅ **LED** - Automatic status visualization
- ✅ **RemoteConsole** - Telnet debugging (port 23)
- ✅ **WebUI** - Web interface (port 8080)
- ✅ **NTP** - Time synchronization
- ✅ **Storage** - Persistent configuration
- ✅ **MQTT** - Message broker integration
- ✅ **Home Assistant** - Auto-discovery
- ✅ **OTA** - Over-the-air updates

**Complete production-ready IoT solution!**

## Perfect For

- 🏠 Home automation
- 🏭 Industrial IoT
- 📡 MQTT-based systems
- 🔄 Remote updates
- 🎯 Home Assistant integration
- 🚀 Enterprise deployments

## Requirements

### External Services

1. **MQTT Broker** (required)
   - Mosquitto
   - HiveMQ
   - CloudMQTT
   - etc.

2. **Home Assistant** (optional)
   - For auto-discovery
   - Requires MQTT integration

### Configuration

Edit `src/main.cpp`:

```cpp
// WiFi
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// MQTT (required)
const char* MQTT_BROKER = "192.168.1.100";
const uint16_t MQTT_PORT = 1883;

// OTA (required for security)
const char* OTA_PASSWORD = "admin123";  // CHANGE THIS!
```

## Quick Start

### 1. Setup MQTT Broker

**Using Mosquitto:**
```bash
# Install
sudo apt-get install mosquitto mosquitto-clients

# Start
sudo systemctl start mosquitto

# Test
mosquitto_sub -h localhost -t test
```

### 2. Configure Device

Edit configuration in `src/main.cpp`

### 3. Build and Upload

```bash
pio run -t upload -t monitor
```

### 4. Verify MQTT

```bash
# Subscribe to device topics
mosquitto_sub -h 192.168.1.100 -t "homeassistant/#" -v
mosquitto_sub -h 192.168.1.100 -t "FullStackDevice/#" -v
```

## What's Included

### Everything from Standard

- WiFi, LED, Console
- WebUI, NTP, Storage

### Plus FullStack Features

#### MQTT Integration

**Features:**
- Auto-connect with reconnect
- QoS support
- Retained messages
- Last Will and Testament

**Usage:**
```cpp
auto mqtt = domotics.getCore().getComponent<MQTTComponent>();

// Publish
mqtt->publish("sensor/temperature", String(temp, 1));
mqtt->publish("sensor/humidity", String(humidity, 1), true);  // Retained

// Subscribe
mqtt->subscribe("command/relay", [](const String& topic, const String& payload) {
    if (payload == "ON") {
        digitalWrite(RELAY_PIN, HIGH);
    }
});
```

**Topics:**
```
FullStackDevice/status          → online/offline
FullStackDevice/sensor/temp     → 22.5
FullStackDevice/sensor/humidity → 65.0
FullStackDevice/command/relay   ← ON/OFF
```

#### Home Assistant Auto-Discovery

**Features:**
- Automatic device registration
- Entity creation
- State updates
- Device info

**Discovered Entities:**
- `sensor.fullstackdevice_temperature` - Simulated temperature sensor (°C)
- `sensor.fullstackdevice_uptime` - Device uptime (seconds)
- `sensor.fullstackdevice_free_heap` - Available memory (bytes)
- `sensor.fullstackdevice_wifi_signal` - WiFi signal strength (dBm)
- `switch.fullstackdevice_relay` - Cooling relay control
- `button.fullstackdevice_restart` - Device restart button

**Configuration:**
```yaml
# Automatically appears in Home Assistant!
# No manual configuration needed
```

**Device Card:**
```yaml
type: entities
title: Full Stack Device
entities:
  - sensor.fullstackdevice_temperature
  - sensor.fullstackdevice_humidity
  - switch.fullstackdevice_relay
```

#### OTA Updates

**Features:**
- Over-the-air firmware updates
- Password protection
- Arduino IDE integration
- PlatformIO integration

**Arduino IDE:**
1. Tools → Port → Network Ports
2. Select `FullStackDevice at 192.168.1.215`
3. Upload

**PlatformIO:**
```bash
pio run -t upload --upload-port 192.168.1.215
```

**Security:**
- Always use strong password
- Change default password!
- Consider disabling after deployment

## Code Structure

```cpp
// 1. Configure
SystemConfig config = SystemConfig::fullStack();
config.deviceName = "FullStackDevice";
config.mqttBroker = "192.168.1.100";
config.otaPassword = "secure123";

// 2. Create
System domotics(config);

// 3. Initialize
domotics.begin();

// 4. Use MQTT
auto mqtt = domotics.getCore().getComponent<MQTTComponent>();
mqtt->publish("sensor/temp", String(temp));
```

## MQTT Topics Structure

### Device Status

```
FullStackDevice/status → "online"
FullStackDevice/state  → "READY"
```

### Sensor Data

```
FullStackDevice/sensor/temperature → "22.5"
FullStackDevice/sensor/humidity    → "65.0"
FullStackDevice/sensor/pressure    → "1013.25"
```

### Commands

```
FullStackDevice/command/relay   ← "ON"/"OFF"
FullStackDevice/command/led     ← "ON"/"OFF"
FullStackDevice/command/restart ← "1"
```

### Home Assistant Discovery

```
homeassistant/sensor/FullStackDevice/temperature/config
homeassistant/sensor/FullStackDevice/humidity/config
homeassistant/switch/FullStackDevice/relay/config
```

## Home Assistant Integration

### Auto-Discovery Payload

```json
{
  "name": "Temperature",
  "device_class": "temperature",
  "state_topic": "FullStackDevice/sensor/temperature",
  "unit_of_measurement": "°C",
  "device": {
    "identifiers": ["FullStackDevice"],
    "name": "Full Stack Device",
    "model": "ESP32",
    "manufacturer": "DomoticsCore"
  }
}
```

### Manual Configuration (if needed)

```yaml
# configuration.yaml
sensor:
  - platform: mqtt
    name: "Device Temperature"
    state_topic: "FullStackDevice/sensor/temperature"
    unit_of_measurement: "°C"

switch:
  - platform: mqtt
    name: "Device Relay"
    command_topic: "FullStackDevice/command/relay"
    state_topic: "FullStackDevice/sensor/relay"
```

## Code Architecture Notes

### Home Assistant Component Namespace

The `HomeAssistantComponent` is in a nested namespace. Use the full qualified name:

```cpp
using namespace DomoticsCore::Components;

// Get the component with full namespace
HomeAssistant::HomeAssistantComponent* haPtr = 
    core.getComponent<HomeAssistant::HomeAssistantComponent>("HomeAssistant");
```

**Note:** Unlike other components, HomeAssistant uses `DomoticsCore::Components::HomeAssistant::` namespace for better organization of entity types (`HASensor`, `HASwitch`, etc.).

### Using WiFi RSSI

Always use the `WifiComponent` to get RSSI instead of accessing WiFi library directly:

```cpp
auto* wifiComp = domotics->getWiFi();
if (wifiComp && wifiComp->isSTAConnected()) {
    int32_t rssi = wifiComp->getRSSI();  // ✅ Use component
    // NOT: WiFi.RSSI()  // ❌ Don't access WiFi.h directly
}
```

This maintains proper encapsulation and component architecture.

## Complete Example

The FullStack example automatically publishes sensor data to Home Assistant every 5 seconds:

```cpp
void loop() {
    domotics->loop();
    
    // Automatic MQTT publishing to Home Assistant (every 5 seconds)
    if (mqttPublishTimer.isReady() && haPtr && haPtr->isMQTTConnected()) {
        float temp = readTemperature();
        uint32_t uptime = millis() / 1000;
        uint32_t freeHeap = ESP.getFreeHeap();
        
        // Publish sensor states (automatically routed to HA)
        haPtr->publishState("temperature", temp);
        haPtr->publishState("uptime", (float)uptime);
        haPtr->publishState("free_heap", (float)freeHeap);
        
        // Get WiFi signal from WifiComponent
        auto* wifiComp = domotics->getWiFi();
        if (wifiComp && wifiComp->isSTAConnected()) {
            haPtr->publishState("wifi_signal", (float)wifiComp->getRSSI());
        }
        
        DLOG_D(LOG_APP, "📡 Published to HA: Temp=%.1f°C, Uptime=%ds", temp, uptime);
    }
    
    // Automatic relay control based on temperature
    if (sensorTimer.isReady()) {
        float temp = readTemperature();
        if (temp > 25.0) {
            setRelay(true);  // Turn on cooling
        } else if (temp < 20.0) {
            setRelay(false);  // Turn off cooling
        }
    }
}
```

**Key Features:**
- ✅ Automatic sensor data publishing (5 second interval)
- ✅ Temperature-based relay control (10 second interval)
- ✅ Relay state changes published immediately
- ✅ Initial state published when HA is ready
- ✅ All entities auto-discovered in Home Assistant

## Build Results

```
RAM:   ~25-30% (with all features)
Flash: ~75-80% (with all features)
```

## Security Best Practices

### OTA Password

```cpp
// ❌ DON'T
config.otaPassword = "admin";

// ✅ DO
config.otaPassword = "MyStr0ng!P@ssw0rd";
```

### MQTT Credentials

```cpp
// Use authentication
config.mqttUser = "device_user";
config.mqttPassword = "secure_password";
```

### WiFi

```cpp
// Use WPA2
config.wifiPassword = "strong_wifi_password";
```

## Troubleshooting

### MQTT Not Connecting

- Check broker IP and port
- Verify broker is running: `mosquitto -v`
- Check firewall: `sudo ufw allow 1883`
- Test connection: `mosquitto_pub -h <broker> -t test -m "hello"`

### Home Assistant Not Discovering

- Check MQTT integration in HA
- Verify discovery prefix: `homeassistant`
- Check MQTT explorer for discovery messages
- Restart Home Assistant

### OTA Not Working

- Check password
- Verify same network
- Check firewall (port 3232)
- Try Arduino IDE network port

### High Memory Usage

- Disable unused features
- Reduce MQTT buffer size
- Optimize sensor reading frequency

## Comparison

| Feature | Minimal | Standard | FullStack |
|---------|---------|----------|-----------|
| WiFi | ✅ | ✅ | ✅ |
| LED | ✅ | ✅ | ✅ |
| Console | ✅ | ✅ | ✅ |
| WebUI | ❌ | ✅ | ✅ |
| NTP | ❌ | ✅ | ✅ |
| Storage | ❌ | ✅ | ✅ |
| MQTT | ❌ | ❌ | ✅ |
| Home Assistant | ❌ | ❌ | ✅ |
| OTA | ❌ | ❌ | ✅ |
| **External Services** | None | None | MQTT |
| **Use Case** | Learning | Production | Enterprise |

## Next Steps

**Too complex?**

- See: `../Standard/` - No MQTT needed
- See: `../Minimal/` - Absolute minimum

**Want to customize?**

- Add more sensors
- Create custom MQTT topics
- Integrate with other services
- Build custom Home Assistant cards

## License

MIT
