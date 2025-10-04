# Web API Endpoints Without UI

**DomoticsCore WebUI Component**  
**Version:** 2.0.0  
**Date:** 2025-10-03

---

## Overview

**YES**, the DomoticsCore WebUI component **fully supports** creating pure API endpoints without any UI components. This allows you to build headless services, REST APIs, or custom integrations.

---

## Core Capability: Direct AsyncWebServer Access

The `WebUIComponent` exposes the underlying `AsyncWebServer` instance, allowing direct endpoint registration.

### Method 1: Using `registerEndpoint()`

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>

using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

Core core;

void setup() {
    WebUIConfig config;
    config.port = 80;
    
    auto webui = std::make_unique<WebUIComponent>(config);
    auto* webuiPtr = webui.get();
    
    // Register pure API endpoint (no UI)
    webuiPtr->registerEndpoint("/api/temperature", HTTP_GET, 
        [](AsyncWebServerRequest* request) {
            float temp = readTemperatureSensor();
            String json = "{\"temperature\":" + String(temp, 2) + ",\"unit\":\"C\"}";
            request->send(200, "application/json", json);
        });
    
    // POST endpoint for control
    webuiPtr->registerEndpoint("/api/relay/set", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            if (request->hasParam("state", true)) {
                String state = request->getParam("state", true)->value();
                bool relayState = (state == "ON" || state == "1");
                digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
                
                request->send(200, "application/json", 
                    "{\"success\":true,\"state\":\"" + state + "\"}");
            } else {
                request->send(400, "application/json", 
                    "{\"error\":\"Missing 'state' parameter\"}");
            }
        });
    
    core.addComponent(std::move(webui));
    core.begin();
}
```

### Method 2: Direct Server Access (Advanced)

```cpp
auto webui = std::make_unique<WebUIComponent>(config);
auto* webuiPtr = webui.get();

// Get raw server instance
AsyncWebServer* server = webuiPtr->getServer();

// Register endpoints directly
server->on("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    // Custom handler
});

// With URL parameters
server->on("/api/device/config", HTTP_GET, 
    [](AsyncWebServerRequest* request) {
        if (request->hasParam("id")) {
            String deviceId = request->getParam("id")->value();
            String json = getDeviceConfig(deviceId);
            request->send(200, "application/json", json);
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing id\"}");
        }
    });

// JSON body parsing
server->on("/api/config", HTTP_POST, 
    [](AsyncWebServerRequest* request) {}, 
    NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, 
       size_t index, size_t total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (!error) {
            String name = doc["name"].as<String>();
            int value = doc["value"].as<int>();
            // Process configuration...
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    });
```

---

## Complete Example: Headless REST API

### Scenario: IoT Sensor API (No Web UI)

```cpp
/**
 * @file headless_api.cpp
 * @brief Pure REST API without WebUI
 * 
 * Endpoints:
 * - GET  /api/sensors         - List all sensors
 * - GET  /api/sensor/{id}     - Get sensor reading
 * - POST /api/relay/{id}/set  - Control relay
 * - GET  /api/status          - System status
 */

#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <ArduinoJson.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components::WebUI;

Core core;

// Sensor data structure
struct SensorData {
    String id;
    float value;
    String unit;
    uint32_t timestamp;
};

std::vector<SensorData> sensors = {
    {"temp_1", 22.5, "°C", 0},
    {"humidity_1", 45.0, "%", 0},
    {"pressure_1", 1013.25, "hPa", 0}
};

void updateSensors() {
    // Update sensor readings
    sensors[0].value = 20.0 + (random(0, 100) / 10.0);
    sensors[1].value = 40.0 + (random(0, 200) / 10.0);
    sensors[2].value = 1000.0 + (random(0, 50));
    
    for (auto& s : sensors) {
        s.timestamp = millis();
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin("YourWifiSSID", "YourPassword");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    // WebUI config (API only, no UI assets)
    WebUIConfig config;
    config.port = 80;
    config.useFileSystem = false;  // No UI files needed
    
    auto webui = std::make_unique<WebUIComponent>(config);
    auto* webuiPtr = webui.get();
    AsyncWebServer* server = webuiPtr->getServer();
    
    // ========== API Endpoints ==========
    
    // GET /api/sensors - List all sensors
    server->on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc["sensors"].to<JsonArray>();
        
        for (const auto& sensor : sensors) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = sensor.id;
            obj["value"] = sensor.value;
            obj["unit"] = sensor.unit;
            obj["timestamp"] = sensor.timestamp;
        }
        
        AsyncResponseStream* response = request->beginResponseStream("application/json");
        serializeJson(doc, *response);
        request->send(response);
    });
    
    // GET /api/sensor/{id} - Get specific sensor
    server->on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) {
            return request->send(400, "application/json", 
                "{\"error\":\"Missing 'id' parameter\"}");
        }
        
        String id = request->getParam("id")->value();
        
        for (const auto& sensor : sensors) {
            if (sensor.id == id) {
                JsonDocument doc;
                doc["id"] = sensor.id;
                doc["value"] = sensor.value;
                doc["unit"] = sensor.unit;
                doc["timestamp"] = sensor.timestamp;
                
                String json;
                serializeJson(doc, json);
                return request->send(200, "application/json", json);
            }
        }
        
        request->send(404, "application/json", 
            "{\"error\":\"Sensor not found\"}");
    });
    
    // POST /api/relay/set - Control relay
    server->on("/api/relay/set", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("state", true)) {
            return request->send(400, "application/json", 
                "{\"error\":\"Missing 'state' parameter\"}");
        }
        
        String state = request->getParam("state", true)->value();
        bool relayOn = (state == "ON" || state == "1" || state == "true");
        
        digitalWrite(2, relayOn ? HIGH : LOW);
        
        JsonDocument doc;
        doc["success"] = true;
        doc["relay_state"] = relayOn ? "ON" : "OFF";
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
    
    // GET /api/status - System status
    server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["uptime"] = millis() / 1000;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["wifi_ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
    
    // Catch-all for unknown endpoints
    server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "application/json", 
            "{\"error\":\"Endpoint not found\"}");
    });
    
    core.addComponent(std::move(webui));
    core.begin();
    
    Serial.printf("API Server started: http://%s\n", WiFi.localIP().toString().c_str());
}

void loop() {
    core.loop();
    
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        updateSensors();
        lastUpdate = millis();
    }
}
```

---

## Advanced Patterns

### 1. RESTful CRUD API

```cpp
// Create
server->on("/api/device", HTTP_POST, handleCreate);

// Read (single)
server->on("/api/device", HTTP_GET, handleRead);

// Read (all)
server->on("/api/devices", HTTP_GET, handleReadAll);

// Update
server->on("/api/device", HTTP_PUT, handleUpdate);

// Delete
server->on("/api/device", HTTP_DELETE, handleDelete);
```

### 2. Authentication Middleware

```cpp
bool checkApiKey(AsyncWebServerRequest* request) {
    if (!request->hasHeader("X-API-Key")) {
        return false;
    }
    
    String apiKey = request->header("X-API-Key");
    return (apiKey == "your-secret-key");
}

server->on("/api/protected", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!checkApiKey(request)) {
        return request->send(401, "application/json", 
            "{\"error\":\"Unauthorized\"}");
    }
    
    // Handle authenticated request
    request->send(200, "application/json", "{\"data\":\"secret\"}");
});
```

### 3. CORS Support

```cpp
server->on("/api/data", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
    request->send(response);
});

server->on("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, 
        "application/json", "{\"data\":\"example\"}");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
});
```

### 4. WebSocket API (Real-time)

```cpp
AsyncWebSocket* ws = new AsyncWebSocket("/ws");

ws->onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        // Handle incoming message
        String message = String((char*)data).substring(0, len);
        JsonDocument doc;
        deserializeJson(doc, message);
        
        // Process and respond
        String response = processCommand(doc);
        client->text(response);
    }
});

webuiPtr->getServer()->addHandler(ws);
```

---

## Benefits of API-Only Mode

✅ **Lightweight** - No UI assets, minimal memory
✅ **Headless Operation** - Perfect for sensor nodes
✅ **Integration Friendly** - Easy to consume from:
  - Mobile apps
  - Web dashboards
  - Home Assistant
  - Node-RED
  - Python scripts

✅ **Performance** - No rendering overhead
✅ **Security** - Simpler attack surface
✅ **Flexibility** - Build any UI separately

---

## Example API Clients

### cURL
```bash
# Get sensors
curl http://192.168.1.100/api/sensors

# Get specific sensor
curl http://192.168.1.100/api/sensor?id=temp_1

# Control relay
curl -X POST http://192.168.1.100/api/relay/set -d "state=ON"

# System status
curl http://192.168.1.100/api/status
```

### Python
```python
import requests

# Get sensor data
response = requests.get('http://192.168.1.100/api/sensors')
data = response.json()
print(data['sensors'])

# Control relay
requests.post('http://192.168.1.100/api/relay/set', data={'state': 'ON'})
```

### JavaScript (Node.js/Browser)
```javascript
// Fetch sensor data
fetch('http://192.168.1.100/api/sensors')
    .then(response => response.json())
    .then(data => console.log(data.sensors));

// Control relay
fetch('http://192.168.1.100/api/relay/set', {
    method: 'POST',
    body: new URLSearchParams({state: 'ON'})
});
```

---

## Summary

**Answer:** YES, DomoticsCore fully supports API-only endpoints without UI.

**How:**
1. Use `WebUIComponent` with `useFileSystem = false`
2. Register endpoints via `registerEndpoint()` or direct `server->on()`
3. Build pure REST APIs, WebSocket services, or custom protocols
4. No UI overhead, maximum flexibility

**Use Cases:**
- ✅ Headless sensor nodes
- ✅ REST APIs for integration
- ✅ Custom dashboards (separate frontend)
- ✅ Mobile app backends
- ✅ Machine-to-machine communication
