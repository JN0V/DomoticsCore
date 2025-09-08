#include <DomoticsCore/Logger.h>

const char* getComponentTag(LogComponent component) {
  switch (component) {
    case LOG_CORE:     return "CORE";
    case LOG_WIFI:     return "WIFI";
    case LOG_MQTT:     return "MQTT";
    case LOG_HTTP:     return "HTTP";
    case LOG_HA:       return "HA";
    case LOG_OTA:      return "OTA";
    case LOG_LED:      return "LED";
    case LOG_SECURITY: return "SECURITY";
    case LOG_WEB:      return "WEB";
    case LOG_SYSTEM:   return "SYSTEM";
    default:           return "UNKNOWN";
  }
}
