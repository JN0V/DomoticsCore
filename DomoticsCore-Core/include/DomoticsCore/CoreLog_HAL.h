#ifndef DOMOTICS_CORE_HAL_CORELOG_H
#define DOMOTICS_CORE_HAL_CORELOG_H

/**
 * @file CoreLog_HAL.h
 * @brief Capture of the log lines the platform itself writes.
 *
 * The Arduino core, the vendor SDK and ESP-IDF write to the UART, which a device
 * reachable only over the network does not have. This is the platform-independent
 * half: a ring of fixed line slots filled from whatever context the platform's
 * logger ran in, and drained from loop(). Nothing here allocates, blocks or logs.
 * The push functions are forced inline so they land where the platform put its
 * sink — IRAM on ESP8266, where the SDK can print with flash unmapped, and flash
 * on ESP32, which is where that core keeps its own sink. A full ring refuses the
 * newest line and counts it.
 *
 * The mechanisms are in CoreLog_ESP32.h, CoreLog_ESP8266.h and CoreLog_Stub.h.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// First: the platform macros, the slot sizes and the critical section.
#include "Platform_HAL.h"

// Sized in the platform headers; these are the fallbacks a host build uses.
#ifndef DOMOTICS_CORE_LOG_SLOTS
    #define DOMOTICS_CORE_LOG_SLOTS 8
#endif
#ifndef DOMOTICS_CORE_LOG_LINE
    #define DOMOTICS_CORE_LOG_LINE 128
#endif

namespace DomoticsCore {
namespace HAL {
namespace CoreLog {

constexpr size_t MAX_LINE = DOMOTICS_CORE_LOG_LINE;
constexpr size_t SLOTS    = DOMOTICS_CORE_LOG_SLOTS;

struct Ring {
    char     slot[SLOTS][MAX_LINE];  // whole lines, waiting for the drain
    uint8_t  used[SLOTS];            // 0 while a slot is free, 1 once a line is in it
    char     building[MAX_LINE];     // a character sink assembles here, alone: a whole
    size_t   partial;                // line from the other sink must not overwrite it
    uint8_t  spoiled;                // and one interrupted mid-assembly is dropped
    size_t   head;                   // next slot to fill
    size_t   tail;                   // next slot to drain
    uint32_t dropped;                // lines refused: the ring was full, or one was cut
    uint32_t ignored;                // whole lines skipped because they were ours
    uint8_t  installed;              // a sink is held: install and remove are idempotent
};

// Header-only state as a class template's static member: one definition across
// every translation unit, zero-initialised before main(), and no C++17 inline
// variable, which the ESP32 build (gnu++14) does not have.
template <typename Tag = void>
struct Storage {
    static Ring ring;
    static bool suppressed;
};
template <typename Tag> Ring Storage<Tag>::ring = {};
template <typename Tag> bool Storage<Tag>::suppressed = false;

/** @brief Whether a platform sink is currently held. */
inline uint8_t& captureInstalled() { return Storage<>::ring.installed; }

/** @brief The one ring. */
inline Ring& ring() { return Storage<>::ring; }

/**
 * @brief Whether the capture is ignoring what it is handed, which Logger holds
 *        while it prints: on ESP32 its own lines reach the same sink.
 *
 * One flag for the process, so another context's line is lost with ours — whole
 * ones counted in ignoredLines(), a half-assembled one dropped with the rest.
 */
inline bool& suppressed() { return Storage<>::suppressed; }

/** @brief Holds the suppression for a scope: see Logger's DLOG macros. */
struct SuppressOwnOutput {
    SuppressOwnOutput() { suppressed() = true; }
    ~SuppressOwnOutput() { suppressed() = false; }
    SuppressOwnOutput(const SuppressOwnOutput&) = delete;
    SuppressOwnOutput& operator=(const SuppressOwnOutput&) = delete;
};

/**
 * @brief Hand a whole line to the ring. Safe from a task or an interrupt.
 *
 * A line longer than a slot is kept up to the slot and the rest discarded: the
 * head of a log line carries its level and its subject, which is what a reader
 * needs.
 */
inline __attribute__((always_inline)) void pushLine(const char* line, size_t len) {
    if (!line || len == 0) return;
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) --len;
    if (len == 0) return;

    const uint32_t state = Platform::enterCoreLogCritical();
    Ring& r = ring();
    if (suppressed()) {
        r.ignored++;
        Platform::leaveCoreLogCritical(state);
        return;
    }
    if (r.used[r.head]) {
        r.dropped++;                       // the reader is behind; the newest goes
        Platform::leaveCoreLogCritical(state);
        return;
    }
    const size_t copy = len < (MAX_LINE - 1) ? len : (MAX_LINE - 1);
    memcpy(r.slot[r.head], line, copy);
    r.slot[r.head][copy] = '\0';
    r.used[r.head] = 1;
    r.head = (r.head + 1) % SLOTS;
    Platform::leaveCoreLogCritical(state);
}

/**
 * @brief Assemble a line one character at a time, which is how a character sink
 *        delivers it. The line is published on the newline.
 */
inline __attribute__((always_inline)) void pushChar(char c) {
    if (c == '\r') return;

    const uint32_t state = Platform::enterCoreLogCritical();
    Ring& r = ring();
    if (suppressed()) {
        if (r.partial > 0) r.spoiled = 1;   // our characters would land inside theirs
        Platform::leaveCoreLogCritical(state);
        return;
    }
    if (c != '\n') {
        if (r.partial < MAX_LINE - 1) r.building[r.partial++] = c;   // a longer line keeps its head
        Platform::leaveCoreLogCritical(state);
        return;
    }
    const bool spoiled = r.spoiled != 0;
    const size_t len = r.partial;
    r.partial = 0;
    r.spoiled = 0;
    if (spoiled) {
        r.dropped++;                        // its middle is missing: not a line any more
        Platform::leaveCoreLogCritical(state);
        return;
    }
    if (len == 0) {                         // a bare newline is not a line
        Platform::leaveCoreLogCritical(state);
        return;
    }
    if (r.used[r.head]) {
        r.dropped++;                        // the reader is behind, and this is the path
        Platform::leaveCoreLogCritical(state);  // ESP8266's only sink takes
        return;
    }
    memcpy(r.slot[r.head], r.building, len);
    r.slot[r.head][len] = '\0';
    r.used[r.head] = 1;
    r.head = (r.head + 1) % SLOTS;
    Platform::leaveCoreLogCritical(state);
}

/**
 * @brief Take the oldest line, oldest first.
 * @return its length, or 0 when the ring is empty.
 */
inline size_t drainLine(char* buf, size_t len) {
    if (!buf || len < 2) return 0;   // too small to hold anything: leave the line where it is

    const uint32_t state = Platform::enterCoreLogCritical();
    Ring& r = ring();
    if (!r.used[r.tail]) {
        Platform::leaveCoreLogCritical(state);
        return 0;
    }
    const size_t n = strlen(r.slot[r.tail]);
    const size_t copy = n < (len - 1) ? n : (len - 1);
    memcpy(buf, r.slot[r.tail], copy);
    buf[copy] = '\0';
    r.used[r.tail] = 0;
    r.tail = (r.tail + 1) % SLOTS;
    Platform::leaveCoreLogCritical(state);
    return copy;
}

// Both counters are read without the lock: a 32-bit load is atomic on every
// target here, and a reader one line behind costs nothing.
/** @brief Lines the ring refused: it was full, or one was cut in the middle. */
inline uint32_t droppedLines() { return ring().dropped; }

/** @brief Whole lines skipped because this framework was printing its own. */
inline uint32_t ignoredLines() { return ring().ignored; }

/** @brief Forget every line and every drop: the state a fresh install starts from. */
inline void reset() {
    const uint32_t state = Platform::enterCoreLogCritical();
    Ring& r = ring();
    for (size_t i = 0; i < SLOTS; ++i) r.used[i] = 0;
    r.head = r.tail = r.partial = 0;
    r.spoiled = 0;
    r.dropped = 0;
    r.ignored = 0;
    Platform::leaveCoreLogCritical(state);
}

} // namespace CoreLog
} // namespace HAL
} // namespace DomoticsCore

// The mechanism itself, one file per platform: installCapture(), removeCapture()
// and setSdkOutput() live there and nowhere else.
#if DOMOTICS_PLATFORM_ESP32
    #include "CoreLog_ESP32.h"
#elif DOMOTICS_PLATFORM_ESP8266
    #include "CoreLog_ESP8266.h"
#else
    #include "CoreLog_Stub.h"
#endif

#endif // DOMOTICS_CORE_HAL_CORELOG_H
