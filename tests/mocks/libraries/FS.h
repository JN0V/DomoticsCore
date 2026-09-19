#pragma once

/**
 * @file FS.h
 * @brief Mock Arduino filesystem header, under its real name.
 *
 * The stub filesystem derives from fs::FS and hands a reference to callers that
 * only pass it on. Nothing here touches a disk: the mock exists so headers
 * that name the type compile on the host.
 */

#include <DomoticsCore/Platform_HAL.h>

#include <cstddef>
#include <cstdint>

namespace fs {

class FileImpl;

/// Always false in boolean context: no file is ever open on the host.
class File {
public:
    explicit operator bool() const { return false; }
    size_t size() const { return 0; }
    void close() {}
};

class FS {
public:
    explicit FS(void* impl = nullptr) : impl_(impl) {}
    virtual ~FS() = default;

    bool exists(const char*) const { return false; }
    File open(const char*, const char* = "r") const { return File(); }

private:
    void* impl_;
};

}  // namespace fs

using File = fs::File;
