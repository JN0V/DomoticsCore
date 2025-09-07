/**
 * @file LEDManager.h
 * @brief LED status management for ESP32 domotics system (integrated in DomoticsCore)
 */

#ifndef DOMOTICS_CORE_LED_MANAGER_H
#define DOMOTICS_CORE_LED_MANAGER_H

#include <Arduino.h>

// WiFi connection status enumeration
enum WiFiStatus {
  WIFI_STARTING,
  WIFI_AP_MODE,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_RECONNECTING,
  WIFI_FAILED,
  WIFI_NORMAL_OPERATION
};

class LEDManager {
private:
  int ledPin;
  WiFiStatus currentStatus;
  unsigned long lastUpdate;
  bool ledState;
  int blinkCount;
  unsigned long startingSequenceStart;

public:
  explicit LEDManager(int pin = 2);
  void begin();
  void setStatus(WiFiStatus status);
  void update();
  void runSequence(WiFiStatus status, unsigned long duration);
  WiFiStatus getCurrentStatus();
};

#endif // DOMOTICS_CORE_LED_MANAGER_H
