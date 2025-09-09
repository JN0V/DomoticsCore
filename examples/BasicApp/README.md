# Basic Example

Simple sensor monitoring with DomoticsCore and REST API.

## Hardware

Connect sensor to pin A0 (0-3.3V range).

## API

**GET /api/sensor**
```json
{"sensor_value": 75.2}
```

## Build

```bash
pio run --target upload
```

## Usage

1. Configure WiFi via captive portal
2. Access REST API at `http://[device-ip]/api/sensor`
