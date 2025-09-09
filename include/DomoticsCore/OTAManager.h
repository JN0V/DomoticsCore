/**
 * @file OTAManager.h
 * @brief Over-The-Air update management for ESP32 domotics system (integrated under DomoticsCore)
 */

#ifndef DOMOTICS_CORE_OTA_MANAGER_H
#define DOMOTICS_CORE_OTA_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

class WebConfig; // Forward declaration

class OTAManager {
private:
  AsyncWebServer* server;
  WebConfig* webConfig;
  String otaError;

public:
  explicit OTAManager(AsyncWebServer* srv, WebConfig* webCfg);
  void begin();
  void setupRoutes();
  String getOTAError() const { return otaError; }
  void clearError() { otaError = ""; }
};

#endif // DOMOTICS_CORE_OTA_MANAGER_H
