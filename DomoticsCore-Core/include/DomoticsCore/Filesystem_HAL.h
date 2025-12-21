#ifndef DOMOTICS_CORE_FILESYSTEM_HAL_H
#define DOMOTICS_CORE_FILESYSTEM_HAL_H

/**
 * @file Filesystem_HAL.h
 * @brief Hardware Abstraction Layer for filesystem operations.
 * 
 * This routing header includes the appropriate filesystem implementation
 * based on the target platform:
 * - ESP32: SPIFFS and LittleFS
 * - ESP8266: LittleFS only (SPIFFS deprecated)
 * - Other: Stub implementation
 * 
 * Constitution IX: All #ifdef platform directives are in HAL files only.
 */

#include "DomoticsCore/Platform_HAL.h"

#if DOMOTICS_PLATFORM_ESP32
    #include "Filesystem_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "Filesystem_ESP8266.h"
#else
    #include "Filesystem_Stub.h"
#endif

namespace DomoticsCore {
namespace HAL {
namespace Filesystem {

/**
 * @brief Initialize the filesystem
 * @return true if successful
 */
inline bool begin() {
    return FilesystemImpl::begin();
}

/**
 * @brief Check if a file exists
 * @param path File path
 * @return true if file exists
 */
inline bool exists(const String& path) {
    return FilesystemImpl::exists(path);
}

/**
 * @brief Get the underlying FS object for use with AsyncWebServer
 * @return Reference to the filesystem
 */
inline fs::FS& getFS() {
    return FilesystemImpl::getFS();
}

/**
 * @brief Format the filesystem
 * @return true if successful
 */
inline bool format() {
    return FilesystemImpl::format();
}

/**
 * @brief Get total filesystem size in bytes
 */
inline size_t totalBytes() {
    return FilesystemImpl::totalBytes();
}

/**
 * @brief Get used filesystem size in bytes
 */
inline size_t usedBytes() {
    return FilesystemImpl::usedBytes();
}

} // namespace Filesystem
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_FILESYSTEM_HAL_H
