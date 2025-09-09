# Advanced Example

Comprehensive IoT device with sensor monitoring, relay control, MQTT, and Home Assistant integration.

## Features

- **Sensor Monitoring**: Analog sensor on pin A0 with change detection
- **Relay Control**: Digital relay on pin 4 with REST API control
- **Multiple REST APIs**: Status, relay control, and system reboot endpoints
- **MQTT Integration**: Real-time data publishing every 30 seconds
- **Home Assistant**: Auto-discovery with 4 sensors
- **Specific Logging**: Dedicated logs for sensor and relay operations

## Hardware Setup

Connect your hardware:
- **Analog Pin A0**: Sensor signal (0-3.3V range)
- **Digital Pin 4**: Relay control (HIGH = ON, LOW = OFF)

## API Endpoints

### GET /api/status
Complete system status with sensor and relay data:
```json
{
  "device": "AdvancedExample",
  "version": "2.0.0",
  "library_version": "0.1.5",
  "uptime": 12345,
  "free_heap": 234567,
  "wifi_rssi": -45,
  "sensor_value": 75.2,
  "relay_state": true
}
```

### POST /api/relay
Control relay state with `state` parameter:
```bash
# Turn relay ON
curl -X POST -d "state=on" http://device-ip/api/relay

# Turn relay OFF  
curl -X POST -d "state=off" http://device-ip/api/relay
```

Response:
```json
{"relay_state": true}
```

### POST /api/reboot
Restart the device:
```bash
curl -X POST http://device-ip/api/reboot
```

## MQTT & Home Assistant

When MQTT is configured, the device publishes:
- **Sensor reading** every 5 seconds (with change detection)
- **System metrics** every 30 seconds to Home Assistant
- **Auto-discovery** for 4 sensors: sensor_value, uptime, free_heap, wifi_rssi

## Build

```bash
pio run --target upload
pio device monitor
```

## Usage

1. Configure WiFi via captive portal
2. Configure MQTT settings via web interface at `http://[device-ip]/`
3. Control relay: `POST /api/relay` with `state=on/off`
4. Monitor sensor: `GET /api/status`
5. View in Home Assistant (if MQTT configured)

## Logging

The example uses specific logging tags:
- `SENSOR`: Sensor value changes and MQTT publishing
- `RELAY`: Relay state changes and API calls
- `HA`: Home Assistant setup and sensor publishing
