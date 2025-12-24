#pragma once

/**
 * @file WiFiServer_ESP32.h
 * @brief ESP32 WiFiServer implementation
 */

#include <WiFi.h>

namespace DomoticsCore {
namespace HAL {

// ESP32 WiFiServer is directly available from WiFi.h
using WiFiServer = ::WiFiServer;
using WiFiClient = ::WiFiClient;
using IPAddress = ::IPAddress;

} // namespace HAL
} // namespace DomoticsCore
