# HAWithWebUI Example

Full-featured Home Assistant integration with web interface for configuration and monitoring.

## Features

- ✅ All BasicHA features
- ✅ Web interface for device configuration
- ✅ Real-time monitoring dashboard
- ✅ Configuration management via WebUI
- ✅ Live statistics and entity overview
- ✅ WebSocket-based real-time updates

## Requirements

- ESP32 development board
- WiFi network
- MQTT broker (e.g., Mosquitto)
- Home Assistant with MQTT integration enabled

## Hardware

- Built-in LED on GPIO 2 (controlled by relay switch)
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
pio run -d DomoticsCore-HomeAssistant/examples/HAWithWebUI -t upload -t monitor
```

## Access Points

After deployment:

- **Web Interface**: `http://[device-ip]`
- **Home Assistant**: Settings → Devices & Services → MQTT

## Web Interface Features

### Status Badge
- Shows entity count in header
- Connection status indicator
- Home Assistant icon

### Dashboard Card
- Entity overview
- Discovery count
- State updates counter
- Commands received counter

### Settings Panel
- Configure Node ID
- Set device name and manufacturer
- Change discovery prefix
- Set suggested area
- Auto-republish discovery on save

### Component Detail
- Full statistics breakdown
- Availability topic
- Configuration URL
- Real-time updates

## Entities Registered

| Entity ID | Type | Description |
|-----------|------|-------------|
| temperature | Sensor | Simulated temperature (°C) |
| humidity | Sensor | Simulated humidity (%) |
| uptime | Sensor | Device uptime (seconds) |
| wifi_signal | Sensor | WiFi signal strength (dBm) |
| free_heap | Sensor | Available memory (bytes) |
| relay | Switch | Controllable relay (GPIO 2, built-in LED) |
| restart | Button | Restart device |

**Total: 7 entities** (5 sensors, 1 switch, 1 button)

## Memory Usage

- Flash: ~1,172 KB (89.5%)
- RAM: ~47 KB (14.6%)

## Usage

### Control from Home Assistant

1. Open Home Assistant
2. Navigate to device page
3. Control relay switch (turns built-in LED on/off)
4. Press restart button to reboot device
5. Monitor sensor values in real-time

### Configure via WebUI

1. Open `http://[device-ip]` in browser
2. Go to Settings
3. Update device configuration
4. Changes applied immediately
5. Discovery automatically republished

## Next Steps

- Replace simulated sensors with real hardware
- Add custom entities for your use case
- Customize areas and device classes
- Integrate with Home Assistant automations

## Related Examples

- **BasicHA** - Minimal example without WebUI
