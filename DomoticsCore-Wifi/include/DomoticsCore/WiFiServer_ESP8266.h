#pragma once

/**
 * @file WiFiServer_ESP8266.h
 * @brief ESP8266 WiFiServer implementation
 */

#include <ESP8266WiFi.h>

namespace DomoticsCore {
namespace HAL {

// ESP8266 WiFiServer is directly available from ESP8266WiFi.h
using WiFiServer = ::WiFiServer;
using WiFiClient = ::WiFiClient;
using IPAddress = ::IPAddress;

} // namespace HAL
} // namespace DomoticsCore
