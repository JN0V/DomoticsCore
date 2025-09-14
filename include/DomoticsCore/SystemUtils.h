/**
 * @file SystemUtils.h
 * @brief System utilities for ESP32 domotics core (integrated under DomoticsCore)
 */

#ifndef DOMOTICS_CORE_SYSTEM_UTILS_H
#define DOMOTICS_CORE_SYSTEM_UTILS_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

class SystemUtils {
private:
  static bool timeInitialized;
  static const char* ntpServer;
  static const long gmtOffset_sec;
  static const int daylightOffset_sec;

public:
  static void displaySystemInfo();
  static void initializeNTP();
  static bool isTimeInitialized();
  static void setTimeInitialized(bool initialized);
  static String getCurrentTimeString();
  static void watchdogSafeDelay(unsigned long milliseconds);
};

#endif // DOMOTICS_CORE_SYSTEM_UTILS_H
