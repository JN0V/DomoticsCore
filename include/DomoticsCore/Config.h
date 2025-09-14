#ifndef DOMOTICS_CORE_CONFIG_H
#define DOMOTICS_CORE_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>

// Bridge to project-level configuration
// This allows library code to access DEVICE_NAME, MANUFACTURER, pins, ports, timeouts, etc.
#include "firmware-config.h"

struct CoreConfig {
  // Identity
  String deviceName;
  String manufacturer;
  String firmwareVersion;  // Application firmware version (overrides library default)

  // Hardware / services
  uint16_t webServerPort;
  uint8_t ledPin;

  // WiFi settings
  uint32_t wifiConnectTimeoutSec;
  uint32_t wifiReconnectTimeoutMs;
  uint32_t wifiReconnectIntervalMs;
  uint8_t wifiMaxReconnectAttempts;

  // Behavior flags
  bool strictNtpBeforeNormalOp;

  // MQTT defaults
  bool mqttEnabled;
  String mqttServer;
  uint16_t mqttPort;
  String mqttUser;
  String mqttPassword;
  String mqttClientId;

  // mDNS options
  bool mdnsEnabled;
  String mdnsHostname; // without .local
  
  // Home Assistant integration
  bool homeAssistantEnabled;
  String homeAssistantDiscoveryPrefix;

  CoreConfig() {
    String uniqueName = generateUniqueDeviceName();
    deviceName = uniqueName;
    manufacturer = MANUFACTURER;
    firmwareVersion = FIRMWARE_VERSION;
    webServerPort = WEB_SERVER_PORT;
    ledPin = LED_PIN;
    wifiConnectTimeoutSec = WIFI_CONNECT_TIMEOUT;
    wifiReconnectTimeoutMs = WIFI_RECONNECT_TIMEOUT;
    wifiReconnectIntervalMs = WIFI_RECONNECT_INTERVAL;
    wifiMaxReconnectAttempts = WIFI_MAX_RECONNECT_ATTEMPTS;
    strictNtpBeforeNormalOp = true;
    mqttEnabled = false;
    mqttServer = "";
    mqttPort = DEFAULT_MQTT_PORT;
    mqttUser = "";
    mqttPassword = "";
    mqttClientId = DEFAULT_MQTT_CLIENT_ID;
    mdnsEnabled = true;
    mdnsHostname = uniqueName;
    homeAssistantEnabled = false;
    homeAssistantDiscoveryPrefix = "homeassistant";
  }

private:
  String generateUniqueDeviceName() {
    String baseName = DEVICE_NAME;
    String macAddress = WiFi.macAddress();
    // Extract last 6 chars of MAC (without colons)
    String macSuffix = macAddress.substring(9, 11) + macAddress.substring(12, 14) + macAddress.substring(15, 17);
    macSuffix.toUpperCase();
    return baseName + "-" + macSuffix;
  }
};

// Placeholder for future library-level configuration API.
// For now, the library consumes project-level firmware-config.h.
// This header is provided to stabilize public includes: <DomoticsCore/Config.h>.

#endif // DOMOTICS_CORE_CONFIG_H
