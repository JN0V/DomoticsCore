#ifndef DOMOTICS_CORE_H
#define DOMOTICS_CORE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

// Use existing core modules
#include <DomoticsCore/LEDManager.h>
#include <DomoticsCore/SystemUtils.h>
#include <DomoticsCore/WebConfig.h>
#include <DomoticsCore/OTAManager.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/Config.h>
#include <PubSubClient.h>

class DomoticsCore {
public:
  explicit DomoticsCore(const CoreConfig& cfg = CoreConfig());

  // Initialize all modules and start services
  void begin();

  // Main loop processing (WiFi reconnection, LED update, logs)
  void loop();

  // Home Assistant integration
  HomeAssistantDiscovery& getHomeAssistant() { return homeAssistant; }
  bool isHomeAssistantEnabled() const { return cfg.homeAssistantEnabled; }

  // MQTT client access
  PubSubClient& getMQTTClient() { return mqttClient; }
  bool isMQTTConnected() const { return const_cast<PubSubClient&>(mqttClient).connected(); }

  // Access to underlying web server for custom routes
  AsyncWebServer& webServer() { return server; }

  // Version helpers
  String version() const { return String(FIRMWARE_VERSION); }
  String fullVersion() const { return SystemUtils::getFullFirmwareVersion(); }

  // Access configuration
  const CoreConfig& config() const { return cfg; }

private:
  CoreConfig cfg;
  // Core services
  WiFiManager wifiManager;
  AsyncWebServer server;
  Preferences preferences;

  // Modules
  LEDManager ledManager;
  WebConfig webConfig;
  OTAManager otaManager;
  HomeAssistantDiscovery homeAssistant;

  // State
  volatile bool shouldReboot = false;
  // WiFi reconnection state
  bool wifiReconnecting = false;
  unsigned long wifiLostTime = 0;
  uint8_t reconnectAttempts = 0;

  // MQTT client for HA integration
  WiFiClient wifiClient;
  PubSubClient mqttClient;
};

#endif // DOMOTICS_CORE_H
