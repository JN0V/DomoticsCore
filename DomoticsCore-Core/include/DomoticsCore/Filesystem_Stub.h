#ifndef DOMOTICS_CORE_FILESYSTEM_STUB_H
#define DOMOTICS_CORE_FILESYSTEM_STUB_H

/**
 * @file Filesystem_Stub.h
 * @brief Stub filesystem implementation for unsupported platforms.
 */

#include <FS.h>

namespace DomoticsCore {
namespace HAL {
namespace FilesystemImpl {

// Stub FS class for platforms without filesystem support
class StubFS : public fs::FS {
public:
    StubFS() : fs::FS(nullptr) {}
};

static StubFS stubFS;

inline bool begin() {
    return false;
}

inline bool exists(const String&) {
    return false;
}

inline fs::FS& getFS() {
    return stubFS;
}

inline bool format() {
    return false;
}

inline size_t totalBytes() {
    return 0;
}

inline size_t usedBytes() {
    return 0;
}

} // namespace FilesystemImpl
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_FILESYSTEM_STUB_H
