#include <DomoticsCore/DomoticsCore.h>
#include <DomoticsCore/Logger.h>
#include <ArduinoJson.h>

#define SENSOR_PIN A0
#define RELAY_PIN 4

#define LOG_SENSOR "SENSOR"
#define LOG_RELAY "RELAY"
#define LOG_STORAGE "STORAGE"

DomoticsCore* core = nullptr;
float sensorValue = 0.0;
bool relayState = false;

// Storage demonstration variables
unsigned long bootCount = 0;
float sensorThreshold = 50.0;
String deviceNickname = "";

void loadStorageData() {
  // Load persistent data from storage
  bootCount = core->storage().getULong("boot_count", 0);
  sensorThreshold = core->storage().getFloat("sensor_threshold", 50.0);
  deviceNickname = core->storage().getString("device_nickname", "My Device");
  
  DLOG_I(LOG_STORAGE, "Loaded from storage - Boot count: %lu, Threshold: %.1f, Nickname: %s", 
         bootCount, sensorThreshold, deviceNickname.c_str());
}

void saveStorageData() {
  // Save current data to storage
  core->storage().putULong("boot_count", bootCount);
  core->storage().putFloat("sensor_threshold", sensorThreshold);
  core->storage().putString("device_nickname", deviceNickname);
  
  DLOG_D(LOG_STORAGE, "Saved to storage - Boot count: %lu, Threshold: %.1f, Nickname: %s", 
         bootCount, sensorThreshold, deviceNickname.c_str());
}

void setup() {
  // Configure device
  CoreConfig config;
  config.deviceName = "AdvancedExample";
  config.firmwareVersion = "2.1.0";
  config.mqttEnabled = true;
  config.homeAssistantEnabled = true;

  core = new DomoticsCore(config);
  
  // Initialize hardware
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  DLOG_I(LOG_RELAY, "Relay initialized on pin %d", RELAY_PIN);
  
  // Load persistent data after core initialization
  loadStorageData();
  
  // Increment boot counter and save
  bootCount++;
  saveStorageData();
  DLOG_I(LOG_STORAGE, "Device boot #%lu", bootCount);
  
  // Enhanced REST API endpoints with storage integration
  core->webServer().on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request){
    DynamicJsonDocument doc(1024);
    doc["device"] = core->config().deviceName;
    doc["nickname"] = deviceNickname;
    doc["version"] = core->version();
    doc["library_version"] = core->libraryVersion();
    doc["uptime"] = millis() / 1000;
    doc["boot_count"] = bootCount;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["sensor_value"] = sensorValue;
    doc["sensor_threshold"] = sensorThreshold;
    doc["relay_state"] = relayState;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  core->webServer().on("/api/relay", HTTP_POST, [](AsyncWebServerRequest* request){
    if (request->hasParam("state", true)) {
      String state = request->getParam("state", true)->value();
      relayState = (state == "on" || state == "1" || state == "true");
      digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
      DLOG_I(LOG_RELAY, "Relay turned %s via API", relayState ? "ON" : "OFF");
      request->send(200, "application/json", "{\"relay_state\":" + String(relayState ? "true" : "false") + "}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing state parameter\"}");
    }
  });

  // New endpoint to configure sensor threshold
  core->webServer().on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request){
    bool updated = false;
    DynamicJsonDocument response(512);
    
    if (request->hasParam("threshold", true)) {
      float newThreshold = request->getParam("threshold", true)->value().toFloat();
      if (newThreshold > 0 && newThreshold <= 100) {
        sensorThreshold = newThreshold;
        updated = true;
        response["sensor_threshold"] = sensorThreshold;
        DLOG_I(LOG_STORAGE, "Sensor threshold updated to %.1f", sensorThreshold);
      } else {
        response["error"] = "Invalid threshold value (must be 0-100)";
      }
    }
    
    if (request->hasParam("nickname", true)) {
      deviceNickname = request->getParam("nickname", true)->value();
      updated = true;
      response["device_nickname"] = deviceNickname;
      DLOG_I(LOG_STORAGE, "Device nickname updated to: %s", deviceNickname.c_str());
    }
    
    if (updated) {
      saveStorageData();
      response["status"] = "updated";
    } else {
      response["status"] = "no_changes";
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    request->send(200, "application/json", responseStr);
  });

  // Storage management endpoints
  core->webServer().on("/api/storage/stats", HTTP_GET, [](AsyncWebServerRequest* request){
    DynamicJsonDocument doc(512);
    doc["free_entries"] = core->storage().freeEntries();
    doc["boot_count_exists"] = core->storage().isKey("boot_count");
    doc["threshold_exists"] = core->storage().isKey("sensor_threshold");
    doc["nickname_exists"] = core->storage().isKey("device_nickname");
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  core->webServer().on("/api/storage/clear", HTTP_POST, [](AsyncWebServerRequest* request){
    if (core->storage().clear()) {
      // Reset to defaults after clearing
      bootCount = 0;
      sensorThreshold = 50.0;
      deviceNickname = "My Device";
      saveStorageData();
      DLOG_I(LOG_STORAGE, "Storage cleared and reset to defaults");
      request->send(200, "application/json", "{\"status\":\"cleared\"}");
    } else {
      request->send(500, "application/json", "{\"error\":\"Failed to clear storage\"}");
    }
  });

  core->webServer().on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request){
    request->send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
  });

  core->begin();

  // Setup Home Assistant sensors and controls with storage data
  if (core->isHomeAssistantEnabled()) {
    core->getHomeAssistant().publishSensor("sensor_value", "Sensor Reading", "%", "");
    core->getHomeAssistant().publishSensor("uptime", "System Uptime", "s", "duration");
    core->getHomeAssistant().publishSensor("boot_count", "Boot Count", "", "");
    core->getHomeAssistant().publishSensor("free_heap", "Free Heap", "bytes", "data_size");
    core->getHomeAssistant().publishSensor("wifi_rssi", "WiFi Signal", "dBm", "signal_strength");
    DLOG_I("HA", "Published %d sensors to Home Assistant", 5);
  }
}

void loop() {
  core->loop();

  // Read sensor every 5 seconds
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 5000) {
    lastSensorRead = millis();
    
    float newValue = (analogRead(SENSOR_PIN) / 4095.0) * 100.0;
    if (abs(newValue - sensorValue) > 2.0) {
      DLOG_I(LOG_SENSOR, "Sensor value changed: %.1f%% -> %.1f%% (threshold: %.1f%%)", 
             sensorValue, newValue, sensorThreshold);
      
      // Demonstrate threshold-based logic using stored configuration
      if (newValue > sensorThreshold && sensorValue <= sensorThreshold) {
        DLOG_W(LOG_SENSOR, "Sensor exceeded threshold! Triggering relay...");
        relayState = true;
        digitalWrite(RELAY_PIN, HIGH);
      } else if (newValue <= sensorThreshold && sensorValue > sensorThreshold) {
        DLOG_I(LOG_SENSOR, "Sensor below threshold, turning off relay");
        relayState = false;
        digitalWrite(RELAY_PIN, LOW);
      }
    }
    sensorValue = newValue;
  }

  // Publish sensor data every 30 seconds
  static unsigned long lastMqttUpdate = 0;
  if (millis() - lastMqttUpdate > 30000) {
    lastMqttUpdate = millis();
    
    if (core->isMQTTConnected() && core->isHomeAssistantEnabled()) {
      String deviceId = core->config().deviceName;
      core->getMQTTClient().publish(("jnov/" + deviceId + "/sensor_value/state").c_str(), 
                                   String(sensorValue).c_str());
      core->getMQTTClient().publish(("jnov/" + deviceId + "/uptime/state").c_str(), 
                                   String(millis() / 1000).c_str());
      core->getMQTTClient().publish(("jnov/" + deviceId + "/boot_count/state").c_str(), 
                                   String(bootCount).c_str());
      core->getMQTTClient().publish(("jnov/" + deviceId + "/free_heap/state").c_str(), 
                                   String(ESP.getFreeHeap()).c_str());
      core->getMQTTClient().publish(("jnov/" + deviceId + "/wifi_rssi/state").c_str(), 
                                   String(WiFi.RSSI()).c_str());
      DLOG_D(LOG_SENSOR, "Published sensor data: %.1f%% (boot #%lu)", sensorValue, bootCount);
    }
  }

  // Save storage data every 5 minutes to persist any runtime changes
  static unsigned long lastStorageSave = 0;
  if (millis() - lastStorageSave > 300000) {
    lastStorageSave = millis();
    saveStorageData();
  }
}
