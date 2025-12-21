#ifndef DOMOTICS_CORE_FILESYSTEM_ESP32_H
#define DOMOTICS_CORE_FILESYSTEM_ESP32_H

/**
 * @file Filesystem_ESP32.h
 * @brief ESP32-specific filesystem implementation using LittleFS.
 * 
 * ESP32 supports both SPIFFS and LittleFS. We use LittleFS as the primary
 * filesystem as it's more reliable and actively maintained.
 */

#if DOMOTICS_PLATFORM_ESP32

#include <FS.h>
#include <LittleFS.h>

namespace DomoticsCore {
namespace HAL {
namespace FilesystemImpl {

inline bool begin() {
    return LittleFS.begin(true); // Format on fail
}

inline bool exists(const String& path) {
    return LittleFS.exists(path);
}

inline fs::FS& getFS() {
    return LittleFS;
}

inline bool format() {
    return LittleFS.format();
}

inline size_t totalBytes() {
    return LittleFS.totalBytes();
}

inline size_t usedBytes() {
    return LittleFS.usedBytes();
}

} // namespace FilesystemImpl
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP32

#endif // DOMOTICS_CORE_FILESYSTEM_ESP32_H
