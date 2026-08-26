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
 *
 * ## abort() only works before end()
 *
 * `end(true)` is the commit. On ESP32 it calls `esp_ota_set_boot_partition()`;
 * on ESP8266 it writes an eboot `ACTION_COPY_RAW` command staging a copy over
 * the running sketch. Neither Arduino core offers a way to undo that from the
 * application, so **`abort()` is only meaningful while the update is still in
 * flight**. Callers must reject an image before calling `end()`, never after.
 *
 * This is not a style preference. SEC-2 shipped in v2.0.1 as an `abort()` after
 * a successful `end(true)`, believed to roll back a firmware whose SHA-256 did
 * not match. It was inert on both platforms for two releases.
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
