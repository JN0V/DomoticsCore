#pragma once

/**
 * @file WifiEvents.h
 * @brief WiFi component events.
 *
 * Published by WifiComponent when network state changes.
 * Subscribe to these events to react to WiFi connectivity.
 */

namespace DomoticsCore {
namespace WifiEvents {

/// Published when STA mode connects to an access point
inline constexpr const char* EVENT_STA_CONNECTED = "wifi/sta/connected";

/// Published when AP mode is enabled
inline constexpr const char* EVENT_AP_ENABLED = "wifi/ap/enabled";

/// Published when network becomes available (STA or AP)
inline constexpr const char* EVENT_NETWORK_READY = "network/ready";

} // namespace WifiEvents
} // namespace DomoticsCore
