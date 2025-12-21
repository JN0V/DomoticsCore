#ifndef DOMOTICS_CORE_FILESYSTEM_ESP8266_H
#define DOMOTICS_CORE_FILESYSTEM_ESP8266_H

/**
 * @file Filesystem_ESP8266.h
 * @brief ESP8266-specific filesystem implementation using LittleFS.
 * 
 * ESP8266 uses LittleFS (SPIFFS is deprecated on ESP8266).
 */

#if DOMOTICS_PLATFORM_ESP8266

#include <FS.h>
#include <LittleFS.h>

namespace DomoticsCore {
namespace HAL {
namespace FilesystemImpl {

inline bool begin() {
    return LittleFS.begin();
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
    FSInfo info;
    LittleFS.info(info);
    return info.totalBytes;
}

inline size_t usedBytes() {
    FSInfo info;
    LittleFS.info(info);
    return info.usedBytes;
}

} // namespace FilesystemImpl
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP8266

#endif // DOMOTICS_CORE_FILESYSTEM_ESP8266_H
