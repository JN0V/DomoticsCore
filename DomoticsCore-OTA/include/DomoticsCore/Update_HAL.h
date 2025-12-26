#ifndef DOMOTICS_CORE_UPDATE_HAL_H
#define DOMOTICS_CORE_UPDATE_HAL_H

/**
 * @file Update_HAL.h
 * @brief Hardware Abstraction Layer routing header for OTA firmware updates.
 * 
 * This routing header includes the appropriate Update implementation
 * based on the target platform:
 * - ESP32: Uses ESP32 Update library
 * - ESP8266: Uses ESP8266 Updater library  
 * - Other: Stub implementation
 */

#include "DomoticsCore/Platform_HAL.h"

#if DOMOTICS_PLATFORM_ESP32
    #include "Update_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "Update_ESP8266.h"
#else
    #include "Update_Stub.h"
#endif

#endif // DOMOTICS_CORE_UPDATE_HAL_H
