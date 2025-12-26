#ifndef DOMOTICS_CORE_UPDATE_ESP32_H
#define DOMOTICS_CORE_UPDATE_ESP32_H

/**
 * @file Update_ESP32.h
 * @brief ESP32-specific OTA firmware update implementation.
 *
 * ESP32 can write directly to flash from async context (no yield panic issue).
 *
 * Constitution IX: Platform-specific code in dedicated HAL files.
 */

#if DOMOTICS_PLATFORM_ESP32

#include <Update.h>
#include <Arduino.h>

#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif

namespace DomoticsCore {
namespace HAL {
namespace OTAUpdate {

// Track bytes written for progress reporting
static size_t s_bytesWritten = 0;

inline bool begin(size_t size = UPDATE_SIZE_UNKNOWN) {
    s_bytesWritten = 0;
    return Update.begin(size);
}

inline size_t write(uint8_t* data, size_t len) {
    size_t written = Update.write(data, len);
    s_bytesWritten += written;
    return written;
}

inline bool end(bool evenIfRemaining = false) {
    return Update.end(evenIfRemaining);
}

inline void abort() {
    Update.abort();
}

inline String errorString() {
    return Update.errorString();
}

inline bool hasError() {
    return Update.hasError();
}

/**
 * @brief Check if buffering is required for this platform
 */
inline bool requiresBuffering() {
    return false;
}

/**
 * @brief Check if buffer has pending data to process (always false on ESP32)
 */
inline bool hasPendingData() {
    return false;
}

/**
 * @brief Check if buffer overflow occurred (always false on ESP32)
 */
inline bool hasBufferOverflow() {
    return false;
}

/**
 * @brief Get bytes written to flash
 */
inline size_t getBytesWritten() {
    return s_bytesWritten;
}

/**
 * @brief Process buffered data - no-op on ESP32 (direct write)
 * @param error Output string set if error occurs
 * @return 0 = no-op, never returns 1 or -1 on ESP32
 */
inline int processBuffer(String& error) {
    (void)error;
    return 0;  // No buffering needed
}

} // namespace OTAUpdate
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP32

#endif // DOMOTICS_CORE_UPDATE_ESP32_H
