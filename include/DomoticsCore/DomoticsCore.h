#ifndef DOMOTICS_CORE_H
#define DOMOTICS_CORE_H

#include <Arduino.h>
#include <WiFi.h>
// Custom WiFi management system
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

// Use existing core modules
#include <DomoticsCore/LEDManager.h>
#include <DomoticsCore/SystemUtils.h>
#include <DomoticsCore/WebConfig.h>
#include <DomoticsCore/OTAManager.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/Storage.h>
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

  // Storage access for applications
  Storage& storage() { return storageManager; }

  // Version helpers
  String version() const { return cfg.firmwareVersion; }  // Application firmware version
  String fullVersion() const { return cfg.firmwareVersion + "+build." + String((uint64_t)BUILD_NUMBER_NUM); }
  String libraryVersion() const { return String(DOMOTICSCORE_VERSION); }  // DomoticsCore library version

  // Access configuration
  const CoreConfig& config() const { return cfg; }

protected:
  // MQTT message handler - can be overridden by derived classes
  virtual void onMQTTMessage(const String& topic, const String& message);
  
  // Public method to check AP mode status
  bool getIsInAPMode() const { return isInAPMode; }

private:
  // MQTT initialization and handling
  void initializeMQTT();
  void handleMQTT();
  
  // Custom WiFi management
  void startAPMode();
  void exitAPMode();
  
  // State tracking
  bool isInAPMode = false;

  CoreConfig cfg;
  // Core services
  // Custom WiFi management system
  AsyncWebServer server;
  Preferences preferences;

  // Modules
  LEDManager ledManager;
  WebConfig webConfig;
  OTAManager otaManager;

  // MQTT client for HA integration
  WiFiClient wifiClient;
  PubSubClient mqttClient;

  // Higher-level modules depending on MQTT
  HomeAssistantDiscovery homeAssistant;
  Storage storageManager;

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

  // (no additional members)
};

#endif // DOMOTICS_CORE_H
