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
  bool isMQTTConnected() const { return mqttConnected; }
  void reconnectMQTT(); // Force MQTT reconnection with new settings

  // Access to underlying web server for custom routes
  AsyncWebServer& webServer() { return server; }

  // Version helpers
  String version() const { return String(FIRMWARE_VERSION); }
  String fullVersion() const { return SystemUtils::getFullFirmwareVersion(); }

  // Access configuration
  const CoreConfig& config() const { return cfg; }

protected:
  // MQTT message handler - can be overridden by derived classes
  virtual void onMQTTMessage(const String& topic, const String& message);

private:
  // MQTT initialization and handling
  void initializeMQTT();
  void handleMQTT();

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

  // MQTT state
  bool mqttInitialized = false;
  bool mqttConnected = false;
  char mqttServerBuffer[64]; // Static buffer for MQTT server to avoid dangling pointers
  char mqttClientBuffer[64]; // Static buffer for MQTT client ID
  char mqttUserBuffer[64];   // Static buffer for MQTT user
  char mqttPassBuffer[64];   // Static buffer for MQTT password
  bool mqttServerIsIP = false; // True if server parsed as IP
  IPAddress mqttServerIp;      // Parsed IP if mqttServerIsIP

  // MQTT client for HA integration
  WiFiClient wifiClient;
  PubSubClient mqttClient;
};

#endif // DOMOTICS_CORE_H
