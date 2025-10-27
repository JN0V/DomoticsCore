# HeadlessAPI Example

Pure REST API server without web interface - demonstrates API-only usage of DomoticsCore WebUI component.

## Features

- ✅ **Pure REST API** - No web interface, just JSON endpoints
- ✅ **Sensor Data API** - Read sensor values
- ✅ **Control API** - Control LED brightness
- ✅ **System Status** - Device information and health
- ✅ **API Key Authentication** - Secure protected endpoints
- ✅ **CORS Support** - Cross-origin requests enabled
- ✅ **Error Handling** - Proper HTTP status codes and error messages

## Hardware

- **LED:** GPIO 2 (built-in LED, PWM brightness control)
- **Sensors:** Simulated (replace with real sensors)

## Configuration

Edit `src/main.cpp`:

```cpp
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
const char* API_KEY = "your-secret-api-key";
```

## Build & Flash

```bash
pio run -d DomoticsCore-WebUI/examples/HeadlessAPI -t upload -t monitor
```

**Important:** API routes must be registered AFTER `core.begin()` because the WebUI component creates the server during initialization. The example demonstrates the correct order:

```cpp
// 1. Create and configure WebUI
auto webui = std::make_unique<WebUIComponent>(config);
auto* webuiPtr = webui.get();

// 2. Add to core and initialize
core.addComponent(std::move(webui));
core.begin();  // Server is created here

// 3. Now register API routes
webuiPtr->registerApiRoute("/api/health", HTTP_GET, handler);
```

## API Endpoints

### Public Endpoints (No Authentication)

#### GET /api/health
Health check endpoint.

**Response:**
```json
{
  "status": "ok",
  "uptime": 12345,
  "timestamp": 1234567890
}
```

**Example:**
```bash
curl http://192.168.1.100/api/health
```

---

#### GET /api/sensors
List all sensor values.

**Response:**
```json
{
  "sensors": [
    {
      "id": "temperature",
      "value": 22.5,
      "unit": "°C",
      "timestamp": 1234567890
    },
    {
      "id": "humidity",
      "value": 45.0,
      "unit": "%",
      "timestamp": 1234567890
    },
    {
      "id": "pressure",
      "value": 1013.25,
      "unit": "hPa",
      "timestamp": 1234567890
    }
  ],
  "count": 3
}
```

**Example:**
```bash
curl http://192.168.1.100/api/sensors
```

---

#### GET /api/sensor?id={id}
Get specific sensor value.

**Parameters:**
- `id` (required) - Sensor ID (temperature, humidity, pressure)

**Response:**
```json
{
  "id": "temperature",
  "value": 22.5,
  "unit": "°C",
  "timestamp": 1234567890
}
```

**Example:**
```bash
curl "http://192.168.1.100/api/sensor?id=temperature"
```

---

#### GET /api/status
System status and information.

**Response:**
```json
{
  "uptime": 12345,
  "free_heap": 234567,
  "chip_model": "ESP32",
  "chip_revision": 1,
  "cpu_freq": 240,
  "wifi": {
    "ssid": "MyNetwork",
    "ip": "192.168.1.100",
    "rssi": -45,
    "mac": "AA:BB:CC:DD:EE:FF"
  },
  "hardware": {
    "led_pin": 2
  }
}
```

**Example:**
```bash
curl http://192.168.1.100/api/status
```

---

### Protected Endpoints (Require API Key)

All protected endpoints require the `X-API-Key` header.

#### POST /api/led/set
Control LED brightness.

**Headers:**
- `X-API-Key: your-secret-api-key`

**Parameters:**
- `brightness` (required) - 0-255

**Response:**
```json
{
  "success": true,
  "led_brightness": 128,
  "led_state": "ON",
  "timestamp": 1234567890
}
```

**Example:**
```bash
curl -X POST http://192.168.1.100/api/led/set \
  -H "X-API-Key: your-secret-api-key" \
  -d "brightness=128"
```

---

## Error Responses

All errors return JSON with error message and HTTP status code:

```json
{
  "error": "Missing 'id' parameter",
  "code": 400
}
```

**Common Status Codes:**
- `200` - Success
- `400` - Bad Request (missing/invalid parameters)
- `401` - Unauthorized (invalid/missing API key)
- `404` - Not Found (endpoint or resource not found)

---

## CORS Support

CORS is enabled by default for cross-origin requests.

**Headers:**
- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type, X-API-Key`

---

## Client Examples

### cURL

```bash
# Health check
curl http://192.168.1.100/api/health

# Get all sensors
curl http://192.168.1.100/api/sensors

# Get specific sensor
curl "http://192.168.1.100/api/sensor?id=temperature"

# Set LED brightness
curl -X POST http://192.168.1.100/api/led/set \
  -H "X-API-Key: your-secret-api-key" \
  -d "brightness=200"

# System status
curl http://192.168.1.100/api/status
```

### Python

```python
import requests

BASE_URL = "http://192.168.1.100"
API_KEY = "your-secret-api-key"

# Get sensors
response = requests.get(f"{BASE_URL}/api/sensors")
sensors = response.json()
print(sensors)

# Set LED brightness
response = requests.post(
    f"{BASE_URL}/api/led/set",
    headers={"X-API-Key": API_KEY},
    data={"brightness": 128}
)
print(response.json())
```

### JavaScript (Browser/Node.js)

```javascript
const BASE_URL = 'http://192.168.1.100';
const API_KEY = 'your-secret-api-key';

// Get sensors
fetch(`${BASE_URL}/api/sensors`)
    .then(response => response.json())
    .then(data => console.log(data));

// Set LED brightness
fetch(`${BASE_URL}/api/led/set`, {
    method: 'POST',
    headers: {
        'X-API-Key': API_KEY,
        'Content-Type': 'application/x-www-form-urlencoded'
    },
    body: new URLSearchParams({brightness: 200})
})
    .then(response => response.json())
    .then(data => console.log(data));
```

---

## Integration Examples

### Home Assistant REST Sensor

```yaml
sensor:
  - platform: rest
    name: ESP32 Temperature
    resource: http://192.168.1.100/api/sensor?id=temperature
    value_template: '{{ value_json.value }}'
    unit_of_measurement: "°C"
    scan_interval: 30

  - platform: rest
    name: ESP32 Humidity
    resource: http://192.168.1.100/api/sensor?id=humidity
    value_template: '{{ value_json.value }}'
    unit_of_measurement: "%"
    scan_interval: 30
```

### Node-RED Flow

```json
[
    {
        "id": "http_request",
        "type": "http request",
        "method": "GET",
        "url": "http://192.168.1.100/api/sensors",
        "name": "Get Sensors"
    },
    {
        "id": "json_parse",
        "type": "json",
        "name": "Parse JSON"
    }
]
```

---

## Memory Usage

- **Flash:** ~350 KB (26.7%)
- **RAM:** ~35 KB (10.7%)

Very lightweight - no UI assets, just API logic!

---

## Customization

### Add Custom Endpoints

```cpp
// In setup(), after getting server instance:
server->on("/api/custom", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["message"] = "Custom endpoint";
    doc["data"] = getCustomData();
    
    String json;
    serializeJson(doc, json);
    sendJson(request, 200, json);
});
```

### Add Real Sensors

Replace simulated sensors with real hardware:

```cpp
#include <DHT.h>

DHT dht(DHT_PIN, DHT22);

void updateSensors() {
    sensors[0].value = dht.readTemperature();
    sensors[1].value = dht.readHumidity();
    // ...
}
```

### Change Authentication

Use different auth methods:

```cpp
// Bearer token
bool checkAuth(AsyncWebServerRequest* request) {
    if (!request->hasHeader("Authorization")) return false;
    String auth = request->header("Authorization");
    return auth == "Bearer your-token";
}

// Basic auth
bool checkAuth(AsyncWebServerRequest* request) {
    if (!request->authenticate("admin", "password")) {
        request->requestAuthentication();
        return false;
    }
    return true;
}
```

---

## Use Cases

- ✅ **IoT Sensor Nodes** - Headless data collection
- ✅ **Mobile App Backend** - REST API for mobile apps
- ✅ **Custom Dashboards** - Build your own frontend
- ✅ **Integration** - Connect to Home Assistant, Node-RED, etc.
- ✅ **M2M Communication** - Machine-to-machine data exchange
- ✅ **Microservices** - Part of larger IoT system

---

## Next Steps

- Add authentication database (users, tokens)
- Implement WebSocket for real-time updates
- Add data persistence (log sensor values)
- Create OpenAPI/Swagger documentation
- Add rate limiting
- Implement HTTPS (TLS/SSL)

---

## Related Examples

- **DomoticsCore-WebUI/examples/BasicWebUI** - Full web interface
- **DomoticsCore-MQTT/examples/BasicMQTT** - MQTT integration
- **DomoticsCore-HomeAssistant/examples/BasicHA** - Home Assistant discovery
