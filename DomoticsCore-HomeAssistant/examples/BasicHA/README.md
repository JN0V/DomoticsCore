# BasicHA Example

Basic Home Assistant MQTT Discovery example demonstrating automatic entity registration and state management.

## Features

- ✅ Automatic entity discovery in Home Assistant
- ✅ Multiple sensor types (temperature, humidity, uptime, WiFi signal, free heap)
- ✅ Controllable switch (relay)
- ✅ Action button (restart)
- ✅ Device grouping and availability tracking
- ✅ Simulated sensor readings (replace with real sensors)

## Requirements

- ESP32 development board
- WiFi network
- MQTT broker (e.g., Mosquitto)
- Home Assistant with MQTT integration enabled

## Hardware

- Built-in LED on GPIO 2 (relay example)
- No external sensors required (simulated values)

## Configuration

Edit the following in `src/main.cpp`:

```cpp
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
const char* MQTT_BROKER = "192.168.1.100";
const uint16_t MQTT_PORT = 1883;
```

## Build & Flash

```bash
pio run -d DomoticsCore-HomeAssistant/examples/BasicHA -t upload -t monitor
```

## Home Assistant Setup

1. Ensure MQTT integration is configured in Home Assistant
2. Device will auto-discover on first connection
3. Find device in: Settings → Devices & Services → MQTT → Devices
4. Device name: "ESP32 Demo Device"

## Entities Registered

| Entity ID | Type | Description |
|-----------|------|-------------|
| temperature | Sensor | Simulated temperature (°C) |
| humidity | Sensor | Simulated humidity (%) |
| uptime | Sensor | Device uptime (seconds) |
| wifi_signal | Sensor | WiFi signal strength (dBm) |
| free_heap | Sensor | Available memory (bytes) |
| relay | Switch | Controllable relay (GPIO 2) |
| restart | Button | Restart device |

## Memory Usage

- Flash: ~945 KB (72.1%)
- RAM: ~46 KB (14.1%)

## Next Steps

- Replace simulated sensors with real hardware (DHT22, BME280, etc.)
- Add more entity types (lights, binary sensors, etc.)
- Customize device information and areas
- Add WebUI interface (see HAWithWebUI example)

## Related Examples

- **HAWithWebUI** - Full example with web interface
