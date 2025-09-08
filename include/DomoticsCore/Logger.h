#ifndef DOMOTICS_CORE_LOGGER_H
#define DOMOTICS_CORE_LOGGER_H

#include <Arduino.h>

// Unified logging macros using Arduino ESP32 logging
// Format: [timestamp] [LEVEL] [COMPONENT] message
// Timestamp is handled automatically by Arduino logging (millis before NTP, time after NTP)
// Each component declares its own tag string

#define DLOG_E(tag, format, ...) log_e("[%s] " format, tag, ##__VA_ARGS__)
#define DLOG_W(tag, format, ...) log_w("[%s] " format, tag, ##__VA_ARGS__)
#define DLOG_I(tag, format, ...) log_i("[%s] " format, tag, ##__VA_ARGS__)
#define DLOG_D(tag, format, ...) log_d("[%s] " format, tag, ##__VA_ARGS__)
#define DLOG_V(tag, format, ...) log_v("[%s] " format, tag, ##__VA_ARGS__)

// Convenience macros for simple string messages (no formatting)
#define DLOG_ES(tag, message) DLOG_E(tag, "%s", message)
#define DLOG_WS(tag, message) DLOG_W(tag, "%s", message)
#define DLOG_IS(tag, message) DLOG_I(tag, "%s", message)
#define DLOG_DS(tag, message) DLOG_D(tag, "%s", message)
#define DLOG_VS(tag, message) DLOG_V(tag, "%s", message)

// Standard component tags (can be used by library and apps)
#define LOG_CORE     "CORE"
#define LOG_WIFI     "WIFI"
#define LOG_MQTT     "MQTT"
#define LOG_HTTP     "HTTP"
#define LOG_HA       "HA"
#define LOG_OTA      "OTA"
#define LOG_LED      "LED"
#define LOG_SECURITY "SECURITY"
#define LOG_WEB      "WEB"
#define LOG_SYSTEM   "SYSTEM"

// Log levels (for reference - controlled by CORE_DEBUG_LEVEL):
// 0 = None
// 1 = Error only
// 2 = Error + Warning
// 3 = Error + Warning + Info (default)
// 4 = Error + Warning + Info + Debug
// 5 = All (including Verbose)

#endif // DOMOTICS_CORE_LOGGER_H
