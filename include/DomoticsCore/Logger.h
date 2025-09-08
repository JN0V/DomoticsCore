#ifndef DOMOTICS_CORE_LOGGER_H
#define DOMOTICS_CORE_LOGGER_H

#include <Arduino.h>

// Log components for consistent tagging
enum LogComponent {
  LOG_CORE,      // DomoticsCore main system
  LOG_WIFI,      // WiFiManager and connections
  LOG_MQTT,      // MQTT client
  LOG_HTTP,      // Web server and HTTP requests
  LOG_HA,        // Home Assistant integration
  LOG_OTA,       // OTA updates
  LOG_LED,       // LED manager
  LOG_SECURITY,  // Authentication and security
  LOG_WEB,       // Web configuration
  LOG_SYSTEM     // System utilities
};

// Get component tag string
const char* getComponentTag(LogComponent component);

// Unified logging macros using Arduino ESP32 logging
// Format: [timestamp] [LEVEL] [COMPONENT] message
// Timestamp is handled automatically by Arduino logging (millis before NTP, time after NTP)
// Arduino log macros expect: log_level(format, ...) - tag is handled internally

#define DLOG_E(component, format, ...) log_e("[%s] " format, getComponentTag(component), ##__VA_ARGS__)
#define DLOG_W(component, format, ...) log_w("[%s] " format, getComponentTag(component), ##__VA_ARGS__)
#define DLOG_I(component, format, ...) log_i("[%s] " format, getComponentTag(component), ##__VA_ARGS__)
#define DLOG_D(component, format, ...) log_d("[%s] " format, getComponentTag(component), ##__VA_ARGS__)
#define DLOG_V(component, format, ...) log_v("[%s] " format, getComponentTag(component), ##__VA_ARGS__)

// Convenience macros for simple string messages (no formatting)
#define DLOG_ES(component, message) DLOG_E(component, "%s", message)
#define DLOG_WS(component, message) DLOG_W(component, "%s", message)
#define DLOG_IS(component, message) DLOG_I(component, "%s", message)
#define DLOG_DS(component, message) DLOG_D(component, "%s", message)
#define DLOG_VS(component, message) DLOG_V(component, "%s", message)

// Log levels (for reference - controlled by CORE_DEBUG_LEVEL):
// 0 = None
// 1 = Error only
// 2 = Error + Warning
// 3 = Error + Warning + Info (default)
// 4 = Error + Warning + Info + Debug
// 5 = All (including Verbose)

#endif // DOMOTICS_CORE_LOGGER_H
