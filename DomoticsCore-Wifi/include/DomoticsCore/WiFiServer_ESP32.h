#pragma once

/**
 * @file WiFiServer_ESP32.h
 * @brief ESP32 WiFiServer implementation
 */

#include <WiFi.h>
#include <esp_netif.h>

namespace DomoticsCore {
namespace HAL {

// ESP32 WiFiServer is directly available from WiFi.h
using WiFiServer = ::WiFiServer;
using WiFiClient = ::WiFiClient;
using IPAddress = ::IPAddress;

/**
 * @brief Whether a listening socket can be opened yet.
 *
 * lwIP is started with the first network interface, whatever creates it — the
 * radio, or an Ethernet driver. Opening a server before that stops the firmware
 * inside tcpip_send_msg_wait_sem on an invalid mbox, so callers ask first.
 */
inline bool canOpenServer() { return esp_netif_get_nr_of_ifs() > 0; }

} // namespace HAL
} // namespace DomoticsCore
