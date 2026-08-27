#ifndef DOMOTICS_CORE_UPDATE_STUB_H
#define DOMOTICS_CORE_UPDATE_STUB_H

/**
 * @file Update_Stub.h
 * @brief Stub OTA update implementation for unsupported platforms.
 */

#include <DomoticsCore/Platform_HAL.h>
#include <cstdint>
#include <cstddef>

#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif

namespace DomoticsCore {
namespace HAL {
namespace OTAUpdate {

inline size_t s_stubBytesWritten = 0;

// Commit and discard are indistinguishable on a host — nothing is written to any
// flash. The counters give the native suite the one thing it otherwise cannot
// observe: whether an image was committed, and whether it was discarded first.
// See the abort()-before-end() contract in Update_HAL.h.
inline size_t s_stubEndCalls = 0;
inline size_t s_stubAbortCalls = 0;

// SEC-9: the argument end() was last called with. Both Arduino cores define a
// finished image as an exact equality — ESP32 `_progress == _size`
// (Update.h:116), ESP8266 `_currentAddress == (_startAddress + _size)`
// (Updater.h:165) — and refuse end() on anything short of it unless
// evenIfRemaining is set (ESP32 Updater.cpp:289, ESP8266 Updater.cpp:226). A
// streaming upload never knows its exact length before the last chunk, so every
// browser upload this library has ever committed did so through that `true`.
//
// What this observes is the argument OTA.cpp passes, and nothing more. end()
// here returns true whatever it is given, so the flag pins the call site and
// not the HAL: changing Update_ESP8266.h or Update_ESP32.h to pass false to the
// real Updater would leave every test in this repository green and break every
// browser upload on a board. That gap is TEST-8's.
inline bool s_stubEndEvenIfRemaining = false;

inline bool begin(size_t = UPDATE_SIZE_UNKNOWN) {
    s_stubBytesWritten = 0;
    s_stubEndCalls = 0;
    s_stubAbortCalls = 0;
    s_stubEndEvenIfRemaining = false;
    return true;
}

inline size_t write(uint8_t*, size_t len) {
    s_stubBytesWritten += len;
    return len;
}

inline bool end(bool evenIfRemaining = false) {
    ++s_stubEndCalls;
    s_stubEndEvenIfRemaining = evenIfRemaining;
    return true;
}

// Clears the flag as well as counting: the suite defines no setUp(), so this
// state outlives a test. A later assertion that read it without opening an
// update first would be reading the previous test's commit.
inline void abort() {
    ++s_stubAbortCalls;
    s_stubEndEvenIfRemaining = false;
}
inline String errorString() { return "Update not supported on this platform"; }
inline bool hasError() { return false; }

inline bool requiresBuffering() { return false; }
inline bool hasPendingData() { return false; }
inline bool hasBufferOverflow() { return false; }
inline size_t getBytesWritten() { return s_stubBytesWritten; }
inline int processBuffer(String& error) { (void)error; return 0; }

} // namespace OTAUpdate
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_UPDATE_STUB_H
