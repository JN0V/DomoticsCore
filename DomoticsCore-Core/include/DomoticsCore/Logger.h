#ifndef DOMOTICS_CORE_LOGGER_H
#define DOMOTICS_CORE_LOGGER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "DomoticsCore/Platform_HAL.h"

// Log levels enum
enum LogLevel {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_VERBOSE = 5
};

// Logger callback system for RemoteConsole and other listeners
class LoggerCallbacks {
public:
    static void addCallback(std::function<void(LogLevel, const char*, const char*)> cb) {
        getCallbacks().push_back(cb);
    }
    
    static void removeCallback(std::function<void(LogLevel, const char*, const char*)> cb) {
        // Note: This is a simplified version. For production, use a handle/ID system
        getCallbacks().clear();
    }
    
    static void broadcast(LogLevel level, const char* tag, const char* message) {
        for (auto& cb : getCallbacks()) {
            if (cb) {
                cb(level, tag, message);
            }
        }
    }
    
private:
    // Use Meyer's singleton to avoid static initialization order issues
    static std::vector<std::function<void(LogLevel, const char*, const char*)>>& getCallbacks() {
        static std::vector<std::function<void(LogLevel, const char*, const char*)>> callbacks;
        return callbacks;
    }
};

// Unified logging macros using Arduino ESP32 logging
// Format: [timestamp] [LEVEL] [COMPONENT] message
// Timestamp is handled automatically by Arduino logging (millis before NTP, time after NTP)
// Each component declares its own tag string

#define DLOG_E(tag, format, ...) \
    do { \
        char _log_buf[256]; \
        snprintf(_log_buf, sizeof(_log_buf), format, ##__VA_ARGS__); \
        log_e("[%s] %s", tag, _log_buf); \
        LoggerCallbacks::broadcast(LOG_LEVEL_ERROR, tag, _log_buf); \
    } while(0)

#define DLOG_W(tag, format, ...) \
    do { \
        char _log_buf[256]; \
        snprintf(_log_buf, sizeof(_log_buf), format, ##__VA_ARGS__); \
        log_w("[%s] %s", tag, _log_buf); \
        LoggerCallbacks::broadcast(LOG_LEVEL_WARN, tag, _log_buf); \
    } while(0)

#define DLOG_I(tag, format, ...) \
    do { \
        char _log_buf[256]; \
        snprintf(_log_buf, sizeof(_log_buf), format, ##__VA_ARGS__); \
        log_i("[%s] %s", tag, _log_buf); \
        LoggerCallbacks::broadcast(LOG_LEVEL_INFO, tag, _log_buf); \
    } while(0)

#define DLOG_D(tag, format, ...) \
    do { \
        char _log_buf[256]; \
        snprintf(_log_buf, sizeof(_log_buf), format, ##__VA_ARGS__); \
        log_d("[%s] %s", tag, _log_buf); \
        LoggerCallbacks::broadcast(LOG_LEVEL_DEBUG, tag, _log_buf); \
    } while(0)

#define DLOG_V(tag, format, ...) \
    do { \
        char _log_buf[256]; \
        snprintf(_log_buf, sizeof(_log_buf), format, ##__VA_ARGS__); \
        log_v("[%s] %s", tag, _log_buf); \
        LoggerCallbacks::broadcast(LOG_LEVEL_VERBOSE, tag, _log_buf); \
    } while(0)

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
#define LOG_STORAGE  "STORAGE"
#define LOG_NTP      "NTP"
#define LOG_CONSOLE  "CONSOLE"

// Log levels (for reference - controlled by CORE_DEBUG_LEVEL):
// 0 = None
// 1 = Error only
// 2 = Error + Warning
// 3 = Error + Warning + Info (default)
// 4 = Error + Warning + Info + Debug
// 5 = All (including Verbose)

#endif // DOMOTICS_CORE_LOGGER_H
