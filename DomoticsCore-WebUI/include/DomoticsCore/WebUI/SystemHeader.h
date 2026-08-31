#pragma once

/**
 * @file SystemHeader.h
 * @brief The opening system-header step of a WebUI update, extracted from
 *        `WebUIComponent::buildUpdateJson` so a native test can reach it (BUG-32,
 *        and the first slice of SIZE-1).
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "DomoticsCore/WebUI/JsonEscape.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief Write the opening `{"system":{...},"contexts":{` of an update into `buf`.
 *
 * `deviceName` is JSON-escaped before interpolation. BUG-32: it was interpolated
 * raw with `%s`, so a `"` or `\\` in the name — which reaches here from the WebUI
 * settings handler, from `SystemConfig`, and from persistence — corrupted every
 * WebSocket and polling update for every client, persistently after reboot.
 *
 * @return bytes written excluding the NUL, or a negative value if `buf` is too
 *         small to hold the whole header (the caller treats that as a failure,
 *         exactly as the previous `DSNPRINTF_P` truncation check did).
 */
inline int buildSystemHeader(char* buf, size_t cap, uint32_t uptime, uint32_t heap,
                             int clients, const char* deviceName) {
    // 31 * 6 (\u00xx) + 1: the worst case for the char[32] WebUIConfig.deviceName,
    // which setDeviceName truncates at 31. Sized here so a longer name cannot make
    // the escaper stop mid-name and drop the tail.
    char escaped[188];
    jsonEscape(escaped, sizeof(escaped), deviceName);

    const int n = snprintf(buf, cap,
        "{\"system\":{\"uptime\":%u,\"heap\":%u,\"clients\":%d,\"device_name\":\"%s\"},\"contexts\":{",
        static_cast<unsigned>(uptime), static_cast<unsigned>(heap), clients, escaped);

    if (n < 0 || static_cast<size_t>(n) >= cap) return -1;
    return n;
}

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
