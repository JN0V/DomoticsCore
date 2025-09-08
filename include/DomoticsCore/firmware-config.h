#ifndef FIRMWARE_CONFIG_H
#define FIRMWARE_CONFIG_H

// General system configuration
#define DEVICE_NAME "JNOV-ESP32-Domotics"
#define MANUFACTURER "JNOV"

// SemVer2 versioning (MAJOR.MINOR.PATCH)
#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 2

// Build metadata (automatically generated) - numeric universal format
// Extract components from __DATE__ ("Mmm dd yyyy") and __TIME__ ("hh:mm:ss")
// Compute a 64-bit numeric build number: YYYYMMDDHHMM
// Month mapping from textual month abbreviation
#define BUILD_YEAR  (((__DATE__[7] - '0') * 1000) + ((__DATE__[8] - '0') * 100) + ((__DATE__[9] - '0') * 10) + (__DATE__[10] - '0'))
#define BUILD_MONTH ( \
  (__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? 1 : ((__DATE__[2] == 'n') ? 6 : 7)) : \
  (__DATE__[0] == 'F') ? 2 : \
  (__DATE__[0] == 'M') ? ((__DATE__[2] == 'r') ? 3 : 5) : \
  (__DATE__[0] == 'A') ? ((__DATE__[1] == 'p') ? 4 : 8) : \
  (__DATE__[0] == 'S') ? 9 : \
  (__DATE__[0] == 'O') ? 10 : \
  (__DATE__[0] == 'N') ? 11 : 12 )
#define BUILD_DAY   (((__DATE__[4] == ' ') ? 0 : (__DATE__[4] - '0')) * 10 + (__DATE__[5] - '0'))
#define BUILD_HOUR  (((__TIME__[0] - '0') * 10) + (__TIME__[1] - '0'))
#define BUILD_MIN   (((__TIME__[3] - '0') * 10) + (__TIME__[4] - '0'))

// 64-bit numeric build number: YYYYMMDDHHMM
#define BUILD_NUMBER_NUM ( \
  ( (unsigned long long)BUILD_YEAR  * 100000000ULL ) + \
  ( (unsigned long long)BUILD_MONTH *   1000000ULL ) + \
  ( (unsigned long long)BUILD_DAY   *     10000ULL ) + \
  ( (unsigned long long)BUILD_HOUR  *       100ULL ) + \
  ( (unsigned long long)BUILD_MIN ) )

// Helper macros for version string construction
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Complete version string (SemVer2 format: MAJOR.MINOR.PATCH)
// Build metadata is exposed separately via BUILD_NUMBER_NUM
#define FIRMWARE_VERSION TOSTRING(VERSION_MAJOR) "." TOSTRING(VERSION_MINOR) "." TOSTRING(VERSION_PATCH)

// Hardware configuration
#define LED_PIN 2
#define RELAY_PIN 4
#define SENSOR_PIN 5

// WiFi configuration
#define WIFI_CONFIG_PORTAL_TIMEOUT 300  // 5 minutes
#define WIFI_CONNECT_TIMEOUT 20         // 20 seconds
#define WIFI_RECONNECT_TIMEOUT 30000    // 30 seconds
#define WIFI_RECONNECT_INTERVAL 5000    // 5 seconds
#define WIFI_MAX_RECONNECT_ATTEMPTS 5   // Max attempts before restart

// Web server configuration
#define WEB_SERVER_PORT 80

// System monitoring
#define SYSTEM_LOG_INTERVAL 10000       // 10 seconds
#define LOOP_DELAY 100                  // 100ms

// NTP configuration
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600             // GMT+1 (adjust for your timezone)
#define DAYLIGHT_OFFSET_SEC 3600        // DST offset

// MQTT default configuration
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_MQTT_CLIENT_ID "jnov-esp32-domotics"

// Logging configuration
#define DEBUG_LEVEL 3  // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug

#endif
