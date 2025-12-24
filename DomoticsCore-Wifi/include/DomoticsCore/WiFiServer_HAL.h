#pragma once

/**
 * @file WiFiServer_HAL.h
 * @brief HAL routing header for WiFiServer abstraction
 *
 * Provides platform-independent WiFiServer interface for TCP server operations.
 */

#include "DomoticsCore/Platform_HAL.h"

#if DOMOTICS_PLATFORM_ESP32
    #include "WiFiServer_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "WiFiServer_ESP8266.h"
#else
    #include "WiFiServer_Stub.h"
#endif
