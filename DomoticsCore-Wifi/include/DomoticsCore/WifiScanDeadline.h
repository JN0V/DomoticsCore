#pragma once

#include <stdint.h>

namespace DomoticsCore {
namespace HAL {
namespace WiFiHAL {

// Measured on Arduino core 2.0.17: its scanComplete() reports -2 once 6 s have
// passed (300 ms a channel x 20), yet with an AP running the SDK takes 6-8 s and
// still delivers. A -2 inside our own deadline of an async start is still -1.
constexpr uint32_t ASYNC_SCAN_DEADLINE_MS = 15000;

/** @param startedAt millis() of the async start, 0 when none is outstanding. */
inline int16_t maskEarlyScanFailure(int16_t raw, uint32_t startedAt, uint32_t now) {
    if (raw == -2 && startedAt && now - startedAt < ASYNC_SCAN_DEADLINE_MS) return -1;
    return raw;
}

} // namespace WiFiHAL
} // namespace HAL
} // namespace DomoticsCore
