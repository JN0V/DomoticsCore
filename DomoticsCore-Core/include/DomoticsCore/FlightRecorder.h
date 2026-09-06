/**
 * @file FlightRecorder.h
 * @brief Flight recorder in RTC memory: what the last death looked like (OBS-3).
 *
 * One record of 83 words survives every reset but power loss (and, on ESP32,
 * an EN-pin reset). At boot it is read first, before any component, and
 * promoted when it describes a death: the crash callback ran, the reset was
 * unexpected, or a software reset the firmware did not ask for. The promoted
 * record stays in RTC until the owner acknowledges it, so a device that dies
 * again during bring-up keeps the first death.
 *
 * Layout, whole words (see spec-obs-crash-observability.md §L1):
 *   w0 magic   w1 layout|platform|flags   w2 crc32(w3..w82)   w3 build id
 *   w4 drops<<16 | boot sequence          w5 phase marker (v<<16 | ~v)
 *   w6 last uptime ms                      w7 min free16 | largest16
 *   w8-39  fast ring 16 × {uptime s, free16|largest16}   every 10 s
 *   w40-55 slow ring  8 × {uptime s, free16|largest16}   every 10 min
 *   w56-60 last failed alloc: seq, size, free, largest, uptime ms (Lot C)
 *   w61-66 exception: cb reason, exccause, epc1, excvaddr, fail caller, fail size
 *   w67-82 16 stack words from the crash callback
 * Heap fields are 16-bit in 16-byte units.
 */
#ifndef DOMOTICS_CORE_FLIGHT_RECORDER_H
#define DOMOTICS_CORE_FLIGHT_RECORDER_H

#include <stdint.h>
#include <stddef.h>
#include "DomoticsCore/Platform_HAL.h"

namespace DomoticsCore {

struct FlightRecord {
    enum : uint32_t { MAGIC = 0x444F4D46u, LAYOUT = 1u };
    enum : size_t {
        WORDS = 83, FAST_SAMPLES = 16, SLOW_SAMPLES = 8, STACK_WORDS = 16,
        W_MAGIC = 0, W_META = 1, W_CRC = 2, W_BUILD = 3, W_SEQ = 4, W_PHASE = 5,
        W_LAST_UPTIME = 6, W_LAST_HEAP = 7, W_FAST = 8, W_SLOW = 40, W_FAIL = 56,
        W_EXC = 61, W_STACK = 67
    };
    enum Flag : uint32_t {
        CALLBACK_RAN = 1u << 0,   // the crash callback filled w61-82
        OURS         = 1u << 1,   // the firmware itself asked for the restart
        UNPERSISTED  = 1u << 2,   // promoted at a boot that has not acknowledged it yet
        TORN         = 1u << 3,   // magic matched, crc did not
        PROMO_SHIFT  = 4, PROMO_MASK = 3u << 4
    };

    uint32_t w[WORDS];

    uint32_t flags() const { return w[W_META] & 0xFFFFu; }
    uint32_t layout() const { return w[W_META] >> 24; }
    uint32_t bootSequence() const { return w[W_SEQ] & 0xFFFFu; }
    uint32_t eventDrops() const { return w[W_SEQ] >> 16; }
    bool phaseValid() const { return (w[W_PHASE] >> 16) == (~w[W_PHASE] & 0xFFFFu); }
    uint16_t phase() const { return static_cast<uint16_t>(w[W_PHASE] >> 16); }
    uint32_t lastUptimeMs() const { return w[W_LAST_UPTIME]; }
    uint32_t minFreeBytes() const { return (w[W_LAST_HEAP] >> 16) * 16u; }
    uint32_t largestBytes() const { return (w[W_LAST_HEAP] & 0xFFFFu) * 16u; }
    uint32_t cbReason() const { return w[W_EXC + 0]; }
    uint32_t exccause() const { return w[W_EXC + 1]; }
    uint32_t epc1() const { return w[W_EXC + 2]; }
    uint32_t excvaddr() const { return w[W_EXC + 3]; }
    uint32_t failCaller() const { return w[W_EXC + 4]; }
    uint32_t failSize() const { return w[W_EXC + 5]; }

    static uint32_t packHeap(uint32_t freeBytes, uint32_t largestBytes) {
        uint32_t f = freeBytes / 16u, l = largestBytes / 16u;
        if (f > 0xFFFFu) f = 0xFFFFu;
        if (l > 0xFFFFu) l = 0xFFFFu;
        return (f << 16) | l;
    }
    static uint32_t encodePhase(uint16_t v) { return (static_cast<uint32_t>(v) << 16) | (~static_cast<uint32_t>(v) & 0xFFFFu); }
    static uint32_t crc32(const uint32_t* words, size_t count) {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < count; ++i) {
            uint32_t v = words[i];
            for (int b = 0; b < 4; ++b) {
                crc ^= (v >> (8 * b)) & 0xFFu;
                for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
            }
        }
        return ~crc;
    }
    uint32_t bodyCrc() const { return crc32(&w[W_BUILD], WORDS - W_BUILD); }
    /** Same death or not: build, reason, then epc1 — or the failed caller when there is no epc1 (abort/OOM). */
    uint32_t dedupKey() const {
        uint32_t site = epc1() ? epc1() : failCaller();
        uint32_t k[3] = { w[W_BUILD], cbReason(), site };
        return crc32(k, 3);
    }
};

/** What the crash callback hands the recorder (S3 wires the platform callbacks). */
struct CrashInfo {
    uint32_t reason = 0, exccause = 0, epc1 = 0, excvaddr = 0, failCaller = 0, failSize = 0;
    const uint32_t* stack = nullptr;
    size_t stackWords = 0;
};

class FlightRecorder {
public:
    enum class Promotion : uint8_t { None = 0, CrashCallback = 1, UnexpectedReset = 2, UnownedSoftwareReset = 3 };
    enum : uint32_t { FAST_INTERVAL_MS = 10000, SLOW_INTERVAL_MS = 600000 };
    // Phase marker values: 0 idle, 1..N the component's 1-based initialization
    // index (ComponentRegistry::loopAll), 0xFF event dispatch. Handlers that do
    // real work outside loop() may set their own in the high byte later.
    enum : uint16_t { PHASE_IDLE = 0, PHASE_EVENT_DISPATCH = 0xFF };

    static FlightRecorder& instance();

    /**
     * Read RTC, decide, and either start the fresh record or hold the promoted
     * one. Idempotent: the first caller's `holdUntilAcknowledged` wins. System
     * passes true and acknowledges after persistence; a bare Core passes
     * false and Core::begin() acknowledges after logging the decision.
     */
    void begin(bool holdUntilAcknowledged = false);
    bool begun() const { return begun_; }
    bool acknowledgementDeferred() const { return hold_; }

    bool hasPromotedRecord() const { return promotion_ != Promotion::None; }
    Promotion promotion() const { return promotion_; }
    bool promotedIsTorn() const { return torn_; }
    const FlightRecord& promoted() const { return previous_; }
    const FlightRecord& current() const { return current_; }

    /** The promoted record has been consumed: the fresh record takes RTC. */
    void acknowledge();

    /** Every Core::loop(): free heap running minimum; every 10 s a sample and a flush. */
    void tick();
    /** One store to RTC word 5: who was running when it died. */
    void setPhase(uint16_t v);
    /** EventBus::getDroppedCount(), read at tick time. */
    void noteEventDrops(uint32_t totalDrops);
    /** The firmware is restarting on purpose (ESP.restart, OTA reboot). */
    void markOurs();
    /** From the platform's crash callback: no allocation, no log, one RTC write. */
    void recordCrash(const CrashInfo& info);

    /** The promoted record as text, allocation-free. Returns characters written. */
    size_t format(char* buf, size_t len) const;
    static const char* promotionName(Promotion p);

    void resetForTest();

private:
    FlightRecorder() { resetForTest(); }
    void startFresh(uint32_t seq);
    void writeAll(const FlightRecord& r);
    void flush();
    bool rtcHeld() const { return hold_ || (hasPromotedRecord() && !acknowledged_); }

    FlightRecord current_;
    FlightRecord previous_;
    Promotion promotion_;
    bool begun_, hold_, acknowledged_, torn_;
    uint32_t runMin_, lastTickFree_, largestAtMin_, lastTickMs_, lastSlowMs_;
    uint32_t fastIdx_, slowIdx_;
    bool extraWalkDone_;
};

} // namespace DomoticsCore

#endif // DOMOTICS_CORE_FLIGHT_RECORDER_H
