#include <Arduino.h>
#include <DomoticsCore/DomoticsCore.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

CoreConfig cfg; // defaults from firmware-config.h
DomoticsCore* gCore = nullptr;
WiFiClient gNet;
PubSubClient gMqtt(gNet);

static String topicIn() { return String("jnov/") + cfg.mqttClientId + "/in"; }
static String topicOut() { return String("jnov/") + cfg.mqttClientId + "/out"; }

static String getChipId() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[17];
  snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);
  return String(buf);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Example runtime overrides
  cfg.deviceName = "JNOV-ADV";
  cfg.strictNtpBeforeNormalOp = true;
  cfg.mqttEnabled = true;
  cfg.mqttServer = "192.168.1.10"; // change to your broker
  cfg.mqttPort = 1883;
  cfg.mqttClientId = String("adv-") + getChipId();
  // cfg.webServerPort = 8080; // uncomment to change HTTP port

  gCore = new DomoticsCore(cfg);
  
  // Register routes BEFORE starting the server
  // Custom JSON status endpoint
  gCore->webServer().on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request){
    DynamicJsonDocument doc(256);
    doc["version_full"] = gCore->fullVersion();
    doc["device"] = gCore->config().deviceName;
    doc["manufacturer"] = gCore->config().manufacturer;
    doc["heap"] = (uint32_t)ESP.getFreeHeap();
    doc["ip"] = WiFi.localIP().toString();
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // Simple ping endpoint
  gCore->webServer().on("/api/ping", HTTP_GET, [](AsyncWebServerRequest* request){
    String body;
    body.reserve(64);
    body = "pong: ";
    body += String(millis()/1000);
    body += "s";
    request->send(200, "text/plain", body);
  });

  // Reboot endpoint (use with care)
  gCore->webServer().on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request){
    request->send(200, "text/plain", "rebooting");
    delay(250);
    ESP.restart();
  });

  // Now start the core (starts server, WiFi, NTP, etc.)
  gCore->begin();

  // Setup Home Assistant auto-discovery if enabled
  if (gCore->isHomeAssistantEnabled()) {
    // Publish system sensors
    gCore->getHomeAssistant().publishSensor("uptime", "System Uptime", "s", "");
    gCore->getHomeAssistant().publishSensor("free_heap", "Free Heap Memory", "bytes", "");
    gCore->getHomeAssistant().publishSensor("wifi_rssi", "WiFi Signal Strength", "dBm", "signal_strength");
    
    // Publish device info
    gCore->getHomeAssistant().publishDevice();
    
    Serial.println("[HA] Auto-discovery entities published");
  }

  // MQTT setup (demo)
  if (cfg.mqttEnabled && cfg.mqttServer.length()) {
    gMqtt.setServer(cfg.mqttServer.c_str(), cfg.mqttPort);
    gMqtt.setCallback([](char* topic, byte* payload, unsigned int length){
      String msg; msg.reserve(length);
      for (unsigned int i=0;i<length;i++) msg += (char)payload[i];
      Serial.printf("[MQTT] %s => %s\n", topic, msg.c_str());
    });
  }
}

void loop() {
  if (gCore) gCore->loop();

  // MQTT demo loop
  static unsigned long lastPub = 0;
  if (cfg.mqttEnabled && cfg.mqttServer.length()) {
    if (!gMqtt.connected() && WiFi.isConnected()) {
      String clientId = cfg.mqttClientId.length() ? cfg.mqttClientId : String("jnov-") + String((uint32_t)ESP.getEfuseMac(), HEX);
      bool ok;
      if (cfg.mqttUser.length()) {
        ok = gMqtt.connect(clientId.c_str(), cfg.mqttUser.c_str(), cfg.mqttPassword.c_str());
      } else {
        ok = gMqtt.connect(clientId.c_str());
      }
      if (ok) {
        Serial.println("[MQTT] Connected");
        gMqtt.subscribe(topicIn().c_str());
        gMqtt.publish(topicOut().c_str(), "online");
      }
    }
    gMqtt.loop();

    // Example: Publish MQTT heartbeat every 30 seconds
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000) {
      lastHeartbeat = millis();
      
      if (gMqtt.connected()) {
        DynamicJsonDocument doc(256);
        doc["device"] = "JNOV-ESP32-Domotics";
        doc["status"] = "online";
        doc["uptime"] = millis() / 1000;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["wifi_rssi"] = WiFi.RSSI();
        
        String payload;
        serializeJson(doc, payload);
        
        String topic = "jnov/" + String(WiFi.macAddress()) + "/out";
        topic.replace(":", "");
        gMqtt.publish(topic.c_str(), payload.c_str());
        
        // Also publish individual HA sensor values if enabled
        if (gCore->isHomeAssistantEnabled()) {
          String deviceId = gCore->config().deviceName;
          gMqtt.publish(("jnov/" + deviceId + "/uptime/state").c_str(), String(millis() / 1000).c_str());
          gMqtt.publish(("jnov/" + deviceId + "/free_heap/state").c_str(), String(ESP.getFreeHeap()).c_str());
          gMqtt.publish(("jnov/" + deviceId + "/wifi_rssi/state").c_str(), String(WiFi.RSSI()).c_str());
        }
        
        Serial.println("MQTT heartbeat sent: " + payload);
      }
    }
  }
}
