#ifndef DOMOTICS_CORE_UPDATE_ESP8266_H
#define DOMOTICS_CORE_UPDATE_ESP8266_H

/**
 * @file Update_ESP8266.h
 * @brief ESP8266-specific OTA firmware update implementation.
 *
 * Uses Update.runAsync(true) to disable yield() calls within Update.write(),
 * allowing direct flash writes from AsyncWebServer callbacks without __yield panic.
 */

#if DOMOTICS_PLATFORM_ESP8266

#include <Updater.h>
#include <Arduino.h>
#include <eboot_command.h>

#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0
#endif

namespace DomoticsCore {
namespace HAL {
namespace OTAUpdate {

// Track state for progress reporting
inline size_t s_bytesWritten = 0;
inline bool s_updateActive = false;

inline bool begin(size_t size = UPDATE_SIZE_UNKNOWN) {
    s_bytesWritten = 0;
    s_updateActive = false;

    if (size == 0) {
        size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    }

    // Enable async mode to prevent yield() calls in Update.write()
    // This is safe because AsyncWebServer already handles yielding between requests
    Update.runAsync(true);

    if (!Update.begin(size)) {
        return false;
    }

    s_updateActive = true;
    return true;
}

inline size_t write(uint8_t* data, size_t len) {
    size_t written = Update.write(data, len);
    s_bytesWritten += written;
    return written;
}

inline bool end(bool evenIfRemaining = false) {
    s_updateActive = false;
    // Keep async mode enabled - end() also calls yield() internally
    // Async mode will be reset on next begin()
    return Update.end(evenIfRemaining);
}

/**
 * @brief Discard an update that is still in flight.
 *
 * Only meaningful before end() — see the contract in Update_HAL.h.
 *
 * The ESP8266 Updater exposes no abort(), so the only way to release its buffer
 * is to call end(). That is safe while the image is incomplete: end(false) hits
 * the `!isFinished() && !evenIfRemaining` guard and just resets. It is *not* safe
 * once every announced byte has been written — end() then runs to completion and
 * stages an eboot ACTION_COPY_RAW, committing the very image we are discarding.
 * That is exactly the state a failed SHA-256 check leaves behind, so clear the
 * staged command afterwards. eboot_command_clear() is harmless when nothing is
 * staged, which makes this correct either way.
 */
inline void abort() {
    s_updateActive = false;
    Update.runAsync(false);
    Update.end(false);
    eboot_command_clear();
    Update.clearError();
}

inline String errorString() {
    return Update.getErrorString();
}

inline bool hasError() {
    return Update.hasError();
}

/**
 * @brief Check if buffering is required for this platform
 * With runAsync(true), direct writes are safe - no buffering needed
 */
inline bool requiresBuffering() {
    return false;
}

/**
 * @brief Check if buffer has pending data to process (no buffering used)
 */
inline bool hasPendingData() {
    return false;
}

/**
 * @brief Check if buffer overflow occurred (no buffering used)
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
 * @brief Process buffered data - no-op since we use async mode for direct writes
 */
inline int processBuffer(String& error) {
    (void)error;
    return 0;
}

} // namespace OTAUpdate
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP8266

#endif // DOMOTICS_CORE_UPDATE_ESP8266_H
