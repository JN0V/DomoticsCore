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

static size_t s_stubBytesWritten = 0;

inline bool begin(size_t = UPDATE_SIZE_UNKNOWN) {
    s_stubBytesWritten = 0;
    return true;
}

inline size_t write(uint8_t*, size_t len) {
    s_stubBytesWritten += len;
    return len;
}

inline bool end(bool = false) { return true; }
inline void abort() {}
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
