#pragma once

namespace DomoticsCore {
namespace Events {

// MQTT Events
static const char* EVENT_MQTT_CONNECTED = "mqtt/connected";
static const char* EVENT_MQTT_DISCONNECTED = "mqtt/disconnected";
static const char* EVENT_MQTT_MESSAGE = "mqtt/message";
static const char* EVENT_MQTT_PUBLISH = "mqtt/publish";
static const char* EVENT_MQTT_SUBSCRIBE = "mqtt/subscribe";

// WiFi Events
static const char* EVENT_WIFI_STA_CONNECTED = "wifi/sta/connected";
static const char* EVENT_WIFI_AP_ENABLED = "wifi/ap/enabled";

// NTP Events
static const char* EVENT_NTP_SYNCED = "ntp/synced";
static const char* EVENT_NTP_SYNC_FAILED = "ntp/sync_failed";

// HomeAssistant Events
static const char* EVENT_HA_DISCOVERY_PUBLISHED = "ha/discovery_published";
static const char* EVENT_HA_ENTITY_ADDED = "ha/entity_added";

// OTA Events
static const char* EVENT_OTA_START = "ota/start";
static const char* EVENT_OTA_PROGRESS = "ota/progress";
static const char* EVENT_OTA_END = "ota/end";
static const char* EVENT_OTA_ERROR = "ota/error";
static const char* EVENT_OTA_INFO = "ota/info";
static const char* EVENT_OTA_COMPLETE = "ota/complete"; // Intermediate completion
static const char* EVENT_OTA_COMPLETED = "ota/completed"; // Final completion with reboot status

// System Events
static const char* EVENT_SYSTEM_READY = "system/ready";
static const char* EVENT_SYSTEM_REBOOT = "system/reboot";

} // namespace Events
} // namespace DomoticsCore
