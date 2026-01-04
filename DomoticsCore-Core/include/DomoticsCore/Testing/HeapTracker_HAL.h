#pragma once

/**
 * @file HeapTracker_HAL.h
 * @brief HAL routing header for HeapTracker platform implementations
 */

#include <DomoticsCore/Platform_HAL.h>

#if DOMOTICS_PLATFORM_ESP32
    #include <DomoticsCore/Testing/HeapTracker_ESP32.h>
#elif DOMOTICS_PLATFORM_ESP8266
    #include <DomoticsCore/Testing/HeapTracker_ESP8266.h>
#else
    #include <DomoticsCore/Testing/HeapTracker_Native.h>
#endif
