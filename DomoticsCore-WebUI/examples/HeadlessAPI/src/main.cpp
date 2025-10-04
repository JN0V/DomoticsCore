/**
 * @file main.cpp
 * @brief Headless API Example - Pure REST API without WebUI
 * 
 * Demonstrates:
 * - Pure REST API endpoints (no web interface)
 * - Sensor data API
 * - Control API (relay, LED)
 * - System status API
 * - Authentication
 * - CORS support
 * - JSON responses
 * 
 * Access:
 * - GET  /api/sensors         - List all sensors
 * - GET  /api/sensor/{id}     - Get specific sensor
 * - POST /api/relay/set       - Control relay
 * - POST /api/led/set         - Control LED
 * - GET  /api/status          - System status
 * - GET  /api/health          - Health check
 */

#include <WiFi.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/Timer.h>
#include <ArduinoJson.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// Custom application log tag
#define LOG_APP "APP"

// ========== Configuration ==========

const char* WIFI_SSID = "YourWifiSSID";
const char* WIFI_PASSWORD = "YourWifiPassword";

// API Configuration
const uint16_t API_PORT = 80;
const char* API_KEY = "your-secret-api-key";  // For protected endpoints

// Hardware
const int LED_PIN = 2;  // Built-in LED
const int SENSOR_UPDATE_INTERVAL = 5000;  // Update sensors every 5 seconds

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay sensorTimer(SENSOR_UPDATE_INTERVAL);

// Sensor data structure
struct SensorData {
    String id;
    float value;
    String unit;
    uint32_t timestamp;
};

std::vector<SensorData> sensors = {
    {"temperature", 22.5, "°C", 0},
    {"humidity", 45.0, "%", 0},
    {"pressure", 1013.25, "hPa", 0}
};

// ========== Helper Functions ==========

// Simulated sensor readings (replace with real sensors)
void updateSensors() {
    sensors[0].value = 20.0 + (random(0, 100) / 10.0);  // 20-30°C
    sensors[1].value = 40.0 + (random(0, 200) / 10.0);  // 40-60%
    sensors[2].value = 1000.0 + (random(0, 50));        // 1000-1050 hPa
    
    uint32_t now = millis();
    for (auto& s : sensors) {
        s.timestamp = now;
    }
}

// Check API key authentication
bool checkApiKey(AsyncWebServerRequest* request) {
    if (!request->hasHeader("X-API-Key")) {
        return false;
    }
    return request->header("X-API-Key") == API_KEY;
}

// Send JSON response
void sendJson(AsyncWebServerRequest* request, int code, const String& json) {
    request->send(code, "application/json", json);
}

// Send error response
void sendError(AsyncWebServerRequest* request, int code, const String& message) {
    JsonDocument doc;
    doc["error"] = message;
    doc["code"] = code;
    String json;
    serializeJson(doc, json);
    sendJson(request, code, json);
}

// ========== Setup ==========

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - Headless API Example");
    DLOG_I(LOG_APP, "========================================");
    
    // Initialize hardware
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Connect to WiFi
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        DLOG_E(LOG_APP, "WiFi connection failed!");
        while (1) delay(1000);
    }
    
    DLOG_I(LOG_APP, "WiFi connected: %s", WiFi.localIP().toString().c_str());
    
    // Configure WebUI component (API only, no UI assets)
    WebUIConfig config;
    config.port = API_PORT;
    config.deviceName = "ESP32 API Server";
    config.useFileSystem = false;  // No UI files needed
    config.enableAuth = false;     // Using custom API key auth
    config.enableCORS = true;      // Enable CORS for cross-origin requests
    
    auto webui = std::make_unique<WebUIComponent>(config);
    auto* webuiPtr = webui.get();
    
    // Add component to core
    core.addComponent(std::move(webui));
    
    // Initialize core (this creates the server)
    if (!core.begin()) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        while (1) delay(1000);
    }
    
    // ========== API Endpoints ==========
    // Register routes AFTER core.begin() so server exists
    
    // GET /api/health - Health check (no auth required)
    webuiPtr->registerApiRoute("/api/health", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["status"] = "ok";
        doc["uptime"] = millis() / 1000;
        doc["timestamp"] = millis();
        
        String json;
        serializeJson(doc, json);
        sendJson(request, 200, json);
    });
    
    // GET /api/sensors - List all sensors
    webuiPtr->registerApiRoute("/api/sensors", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray arr = doc["sensors"].to<JsonArray>();
        
        for (const auto& sensor : sensors) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = sensor.id;
            obj["value"] = sensor.value;
            obj["unit"] = sensor.unit;
            obj["timestamp"] = sensor.timestamp;
        }
        
        doc["count"] = sensors.size();
        
        String json;
        serializeJson(doc, json);
        sendJson(request, 200, json);
    });
    
    // GET /api/sensor?id={id} - Get specific sensor
    webuiPtr->registerApiRoute("/api/sensor", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("id")) {
            return sendError(request, 400, "Missing 'id' parameter");
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
                return sendJson(request, 200, json);
            }
        }
        
        sendError(request, 404, "Sensor not found: " + id);
    });
    
    
    // POST /api/led/set - Control LED brightness (requires API key)
    webuiPtr->registerApiRoute("/api/led/set", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!checkApiKey(request)) {
            return sendError(request, 401, "Unauthorized - Invalid or missing API key");
        }
        
        if (!request->hasParam("brightness", true)) {
            return sendError(request, 400, "Missing 'brightness' parameter (0-255)");
        }
        
        int brightness = request->getParam("brightness", true)->value().toInt();
        
        if (brightness < 0 || brightness > 255) {
            return sendError(request, 400, "Brightness must be 0-255");
        }
        
        analogWrite(LED_PIN, brightness);
        
        DLOG_I(LOG_APP, "LED brightness set to: %d", brightness);
        
        JsonDocument doc;
        doc["success"] = true;
        doc["led_brightness"] = brightness;
        doc["led_state"] = brightness > 0 ? "ON" : "OFF";
        doc["timestamp"] = millis();
        
        String json;
        serializeJson(doc, json);
        sendJson(request, 200, json);
    });
    
    // GET /api/status - System status
    webuiPtr->registerApiRoute("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        
        // System info
        doc["uptime"] = millis() / 1000;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["chip_model"] = ESP.getChipModel();
        doc["chip_revision"] = ESP.getChipRevision();
        doc["cpu_freq"] = ESP.getCpuFreqMHz();
        
        // WiFi info
        JsonObject wifi = doc["wifi"].to<JsonObject>();
        wifi["ssid"] = WiFi.SSID();
        wifi["ip"] = WiFi.localIP().toString();
        wifi["rssi"] = WiFi.RSSI();
        wifi["mac"] = WiFi.macAddress();
        
        // Hardware state
        JsonObject hardware = doc["hardware"].to<JsonObject>();
        hardware["led_pin"] = LED_PIN;
        
        String json;
        serializeJson(doc, json);
        sendJson(request, 200, json);
    });
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "API Server ready!");
    DLOG_I(LOG_APP, "Base URL: http://%s", WiFi.localIP().toString().c_str());
    DLOG_I(LOG_APP, "API Key: %s", API_KEY);
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "");
    DLOG_I(LOG_APP, "Available Endpoints:");
    DLOG_I(LOG_APP, "  GET  /api/health          - Health check");
    DLOG_I(LOG_APP, "  GET  /api/sensors         - List all sensors");
    DLOG_I(LOG_APP, "  GET  /api/sensor?id={id}  - Get specific sensor");
    DLOG_I(LOG_APP, "  POST /api/led/set         - Control LED (requires API key)");
    DLOG_I(LOG_APP, "  GET  /api/status          - System status");
    DLOG_I(LOG_APP, "========================================");
    
    // Initial sensor update
    updateSensors();
}

// ========== Loop ==========

void loop() {
    core.loop();
    
    // Update sensor values periodically
    if (sensorTimer.isReady()) {
        updateSensors();
        DLOG_D(LOG_APP, "Sensors updated: Temp=%.1f°C, Humidity=%.1f%%, Pressure=%.1fhPa",
               sensors[0].value, sensors[1].value, sensors[2].value);
    }
}
