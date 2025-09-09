#include <DomoticsCore/DomoticsCore.h>
#include <DomoticsCore/Logger.h>
#include <ArduinoJson.h>

#define SENSOR_PIN A0
#define RELAY_PIN 4

#define LOG_SENSOR "SENSOR"
#define LOG_RELAY "RELAY"

DomoticsCore* core = nullptr;
float sensorValue = 0.0;
bool relayState = false;

void setup() {
  // Configure device
  CoreConfig config;
  config.deviceName = "AdvancedExample";
  config.firmwareVersion = "2.0.0";
  config.mqttEnabled = true;
  config.homeAssistantEnabled = true;

  core = new DomoticsCore(config);
  
  // Initialize hardware
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  DLOG_I(LOG_RELAY, "Relay initialized on pin %d", RELAY_PIN);
  
  // Multiple REST API endpoints
  core->webServer().on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request){
    DynamicJsonDocument doc(512);
    doc["device"] = core->config().deviceName;
    doc["version"] = core->version();
    doc["library_version"] = core->libraryVersion();
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["sensor_value"] = sensorValue;
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

  core->webServer().on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request){
    request->send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
  });

  core->begin();

  // Setup Home Assistant sensors and controls
  if (core->isHomeAssistantEnabled()) {
    core->getHomeAssistant().publishSensor("sensor_value", "Sensor Reading", "%", "");
    core->getHomeAssistant().publishSensor("uptime", "System Uptime", "s", "duration");
    core->getHomeAssistant().publishSensor("free_heap", "Free Heap", "bytes", "data_size");
    core->getHomeAssistant().publishSensor("wifi_rssi", "WiFi Signal", "dBm", "signal_strength");
    DLOG_I("HA", "Published %d sensors to Home Assistant", 4);
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
      DLOG_I(LOG_SENSOR, "Sensor value changed: %.1f%% -> %.1f%%", sensorValue, newValue);
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
      core->getMQTTClient().publish(("jnov/" + deviceId + "/free_heap/state").c_str(), 
                                   String(ESP.getFreeHeap()).c_str());
      core->getMQTTClient().publish(("jnov/" + deviceId + "/wifi_rssi/state").c_str(), 
                                   String(WiFi.RSSI()).c_str());
      DLOG_D(LOG_SENSOR, "Published sensor data: %.1f%%", sensorValue);
    }
  }
}
