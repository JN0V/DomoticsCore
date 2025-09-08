#include <Arduino.h>
#include <DomoticsCore/DomoticsCore.h>
#include <DomoticsCore/Logger.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

CoreConfig cfg; // defaults from firmware-config.h
DomoticsCore* gCore = nullptr;

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

  // Example runtime overrides (keep minimal to let WebConfig take precedence)
  cfg.deviceName = "JNOV-ADV";
  cfg.firmwareVersion = "2.1.0";          // AdvancedApp firmware version
  cfg.strictNtpBeforeNormalOp = true;
  cfg.mqttEnabled = true; // allow MQTT; server/credentials managed via Web UI
  // Do not force server/port here; use Web UI or firmware defaults
  // Provide a fallback clientId only if none set in firmware defaults
  if (cfg.mqttClientId.length() == 0) {
    cfg.mqttClientId = String("adv-") + getChipId();
  }
  // cfg.webServerPort = 8080; // uncomment to change HTTP port

  gCore = new DomoticsCore(cfg);
  
  // Register routes BEFORE starting the server
  // Custom JSON status endpoint
  gCore->webServer().on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request){
    DynamicJsonDocument doc(256);
    doc["version_full"] = gCore->version();  // App firmware version
    doc["library_version"] = gCore->libraryVersion();  // DomoticsCore lib version
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
    
    DLOG_I(LOG_HA, "Auto-discovery entities published");
  }

  // MQTT setup is handled by DomoticsCore; we can still set a callback on its client if needed
  if (cfg.mqttEnabled && cfg.mqttServer.length()) {
    gCore->getMQTTClient().setCallback([](char* topic, byte* payload, unsigned int length){
      String msg; msg.reserve(length);
      for (unsigned int i=0;i<length;i++) msg += (char)payload[i];
      DLOG_I(LOG_MQTT, "%s => %s", topic, msg.c_str());
    });
  }
}

void loop() {
  if (gCore) gCore->loop();

  // MQTT demo loop using DomoticsCore's MQTT client
  if (cfg.mqttEnabled && cfg.mqttServer.length()) {
    PubSubClient& mqtt = gCore->getMQTTClient();
    mqtt.loop();

    // Subscribe/publish once connected
    static bool subscribed = false;
    if (gCore->isMQTTConnected()) {
      if (!subscribed) {
        mqtt.subscribe(topicIn().c_str());
        mqtt.publish(topicOut().c_str(), "online", true);
        subscribed = true;
      }

      // Heartbeat every 30s
      static unsigned long lastHeartbeat = 0;
      if (millis() - lastHeartbeat > 30000) {
        lastHeartbeat = millis();

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
        mqtt.publish(topic.c_str(), payload.c_str());

        if (gCore->isHomeAssistantEnabled()) {
          String deviceId = gCore->config().deviceName;
          mqtt.publish(("jnov/" + deviceId + "/uptime/state").c_str(), String(millis() / 1000).c_str());
          mqtt.publish(("jnov/" + deviceId + "/free_heap/state").c_str(), String(ESP.getFreeHeap()).c_str());
          mqtt.publish(("jnov/" + deviceId + "/wifi_rssi/state").c_str(), String(WiFi.RSSI()).c_str());
        }

        DLOG_I(LOG_MQTT, "Heartbeat sent: %s", payload.c_str());
      }
    } else {
      subscribed = false; // reset on disconnect
    }
  }
}
