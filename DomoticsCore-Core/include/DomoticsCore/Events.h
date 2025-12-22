#pragma once

/**
 * @file Events.h
 * @brief Core lifecycle events only.
 *
 * Component-specific events are defined in their respective components:
 * - DomoticsCore-Wifi: WifiEvents.h
 * - DomoticsCore-MQTT: MQTTEvents.h
 * - DomoticsCore-NTP: NTPEvents.h
 * - DomoticsCore-OTA: OTAEvents.h
 * - DomoticsCore-HomeAssistant: HAEvents.h
 * - DomoticsCore-Storage: StorageEvents.h
 */

namespace DomoticsCore {
namespace Events {

// Core Lifecycle Events
inline constexpr const char* EVENT_COMPONENT_READY = "component/ready";
inline constexpr const char* EVENT_COMPONENT_ERROR = "component/error";
inline constexpr const char* EVENT_SYSTEM_READY = "system/ready";
inline constexpr const char* EVENT_SYSTEM_REBOOT = "system/reboot";
inline constexpr const char* EVENT_SHUTDOWN_START = "shutdown/start";

} // namespace Events
} // namespace DomoticsCore
