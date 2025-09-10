# Advanced Example

Comprehensive IoT device with sensor monitoring, relay control, persistent storage, MQTT, and Home Assistant integration.

## Features

- **Sensor Monitoring**: Analog sensor on pin A0 with configurable threshold detection
- **Relay Control**: Digital relay on pin 4 with REST API control and threshold automation
- **Persistent Storage**: Boot counter, sensor threshold, and device nickname storage
- **Multiple REST APIs**: Status, relay control, configuration, storage management, and system reboot endpoints
- **MQTT Integration**: Real-time data publishing every 30 seconds
- **Home Assistant**: Auto-discovery with 5 sensors including boot count
- **Specific Logging**: Dedicated logs for sensor, relay, and storage operations

## Hardware Setup

Connect your hardware:
- **Analog Pin A0**: Sensor signal (0-3.3V range)
- **Digital Pin 4**: Relay control (HIGH = ON, LOW = OFF)

## Storage Features

The example demonstrates persistent storage capabilities:
- **Boot Counter**: Tracks device restarts across power cycles
- **Sensor Threshold**: Configurable threshold for relay automation (default: 50%)
- **Device Nickname**: Custom device name for identification

Data is automatically loaded on startup and saved periodically or when changed via API.

## API Endpoints

### GET /api/status
Complete system status with sensor, relay, and storage data:
```json
{
  "device": "AdvancedExample",
  "nickname": "Living Room Sensor",
  "version": "2.1.0",
  "library_version": "0.2.0",
  "uptime": 12345,
  "boot_count": 42,
  "free_heap": 234567,
  "wifi_rssi": -45,
  "sensor_value": 75.2,
  "sensor_threshold": 60.0,
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

### POST /api/config
Configure sensor threshold and device nickname:
```bash
# Set sensor threshold to 70%
curl -X POST -d "threshold=70" http://device-ip/api/config

# Set device nickname
curl -X POST -d "nickname=Kitchen Sensor" http://device-ip/api/config

# Set both parameters
curl -X POST -d "threshold=65&nickname=Bedroom Sensor" http://device-ip/api/config
```

Response:
```json
{
  "sensor_threshold": 70.0,
  "device_nickname": "Kitchen Sensor",
  "status": "updated"
}
```

### GET /api/storage/stats
View storage statistics and key existence:
```json
{
  "free_entries": 498,
  "boot_count_exists": true,
  "threshold_exists": true,
  "nickname_exists": true
}
```

### POST /api/storage/clear
Clear all application storage data and reset to defaults:
```bash
curl -X POST http://device-ip/api/storage/clear
```

Response:
```json
{"status": "cleared"}
```

### POST /api/reboot
Restart the device:
```bash
curl -X POST http://device-ip/api/reboot
```

## Threshold-Based Automation

The example demonstrates intelligent automation:
- When sensor value **exceeds** the configured threshold → Relay turns **ON**
- When sensor value **drops below** the threshold → Relay turns **OFF**
- Threshold is configurable via `/api/config` and persists across reboots
- Default threshold: 50%

## MQTT & Home Assistant

When MQTT is configured, the device publishes:
- **Sensor reading** every 5 seconds (with change detection and threshold logging)
- **System metrics** every 30 seconds to Home Assistant
- **Auto-discovery** for 5 sensors: sensor_value, uptime, boot_count, free_heap, wifi_rssi

## Build

```bash
pio run --target upload
pio device monitor
```

## Usage

1. Configure WiFi via captive portal
2. Configure MQTT settings via web interface at `http://[device-ip]/`
3. Control relay: `POST /api/relay` with `state=on/off`
4. Configure threshold: `POST /api/config` with `threshold=75`
5. Set nickname: `POST /api/config` with `nickname=My Device`
6. Monitor sensor: `GET /api/status`
7. View storage stats: `GET /api/storage/stats`
8. View in Home Assistant (if MQTT configured)

## Logging

The example uses specific logging tags:
- `SENSOR`: Sensor value changes, threshold comparisons, and MQTT publishing
- `RELAY`: Relay state changes and API calls
- `STORAGE`: Data loading, saving, and configuration changes
- `HA`: Home Assistant setup and sensor publishing

## Storage Data Persistence

The example automatically:
- **Loads** stored data on startup (boot count, threshold, nickname)
- **Increments** boot counter on each restart
- **Saves** data when changed via API endpoints
- **Persists** data every 5 minutes during operation
- **Survives** power cycles and firmware updates
