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
 *   w56-60 last failed alloc (OBS-4): count<<16 | ~count, size, free16|largest16,
 *          site (ESP8266 caller / ESP32 caps), uptime ms — outside the crc, like w5
 *   w61-66 exception: cb reason, exccause, epc1, excvaddr, fail caller, fail size
 *   w67-82 16 stack words from the crash callback
 * Heap fields are 16-bit in 16-byte units. Layout 2 = layout 1 with w56-60
 * outside the crc; a layout-1 record is still read (OBS-4 migration).
 */
#ifndef DOMOTICS_CORE_FLIGHT_RECORDER_H
#define DOMOTICS_CORE_FLIGHT_RECORDER_H

#include <stdint.h>
#include <stddef.h>
#include "DomoticsCore/Platform_HAL.h"

// The platform crash hooks (ESP8266 custom_crash_callback, ESP32 shutdown
// handler) are on by default. -DDOMOTICS_CRASH_HOOKS=0 removes their
// definitions for a sketch or library that defines its own.
#ifndef DOMOTICS_CRASH_HOOKS
#define DOMOTICS_CRASH_HOOKS 1
#endif
// -DDOMOTICS_FLIGHT_RECORDER_TICK=0 compiles the sampler and the phase marker
// out (promotion and the crash record stay): the removal check for the rings,
// and the other side of the loop-cost measurement. The ESP8266 latch of OBS-4
// lives in the sampler: with it out, survived failures go unrecorded there.
#ifndef DOMOTICS_FLIGHT_RECORDER_TICK
#define DOMOTICS_FLIGHT_RECORDER_TICK 1
#endif
// -DDOMOTICS_FLIGHT_RECORDER_PHASE=0 compiles the phase marker out alone:
// its cost is the RTC stores, one per component per loop.
#ifndef DOMOTICS_FLIGHT_RECORDER_PHASE
#define DOMOTICS_FLIGHT_RECORDER_PHASE 1
#endif

namespace DomoticsCore {

struct FlightRecord {
    enum : uint32_t { MAGIC = 0x444F4D46u, LAYOUT = 2u, LAYOUT_V1 = 1u };
    enum : size_t {
        WORDS = 83, FAST_SAMPLES = 16, SLOW_SAMPLES = 8, STACK_WORDS = 16,
        W_MAGIC = 0, W_META = 1, W_CRC = 2, W_BUILD = 3, W_SEQ = 4, W_PHASE = 5,
        W_LAST_UPTIME = 6, W_LAST_HEAP = 7, W_FAST = 8, W_SLOW = 40, W_FAIL = 56,
        W_EXC = 61, W_STACK = 67, FAIL_WORDS = 5
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
    // The failed-allocation group (OBS-4). Its count word validates itself
    // like the phase marker: count 3 is 0x0003FFFC, saturated is 0xFFFF0000.
    static uint32_t encodeCount(uint32_t count) {
        if (count > 0xFFFFu) count = 0xFFFFu;
        return (count << 16) | (~count & 0xFFFFu);
    }
    bool failGroupValid() const { return (w[W_FAIL] >> 16) == (~w[W_FAIL] & 0xFFFFu); }
    uint32_t failAllocCount() const { return failGroupValid() ? (w[W_FAIL] >> 16) : 0u; }
    uint32_t failAllocSize() const { return w[W_FAIL + 1]; }
    uint32_t failAllocFreeBytes() const { return (w[W_FAIL + 2] >> 16) * 16u; }
    uint32_t failAllocLargestBytes() const { return (w[W_FAIL + 2] & 0xFFFFu) * 16u; }
    uint32_t failAllocSite() const { return w[W_FAIL + 3]; }
    uint32_t failAllocUptimeMs() const { return w[W_FAIL + 4]; }
    uint8_t platform() const { return static_cast<uint8_t>(w[W_META] >> 16); }

    static uint32_t packHeap(uint32_t freeBytes, uint32_t largestBytes) {
        uint32_t f = freeBytes / 16u, l = largestBytes / 16u;
        if (f > 0xFFFFu) f = 0xFFFFu;
        if (l > 0xFFFFu) l = 0xFFFFu;
        return (f << 16) | l;
    }
    static uint32_t encodePhase(uint16_t v) { return (static_cast<uint32_t>(v) << 16) | (~static_cast<uint32_t>(v) & 0xFFFFu); }
    static uint32_t crc32Feed(uint32_t crc, const uint32_t* words, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            uint32_t v = words[i];
            for (int b = 0; b < 4; ++b) {
                crc ^= (v >> (8 * b)) & 0xFFu;
                for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
            }
        }
        return crc;
    }
    static uint32_t crc32(const uint32_t* words, size_t count) { return ~crc32Feed(0xFFFFFFFFu, words, count); }
    /**
     * The crc covers w3..w82 except the words stored straight to RTC between
     * flushes, which validate themselves by a complement: the phase marker
     * (w5, every layout — covering it tore every record on the WROOM-32D) and,
     * from layout 2, the failed-allocation group (w56-60, OBS-4).
     */
    uint32_t bodyCrcForLayout(uint32_t layout) const {
        uint32_t crc = crc32Feed(0xFFFFFFFFu, &w[W_BUILD], W_PHASE - W_BUILD);
        if (layout == LAYOUT_V1) {
            crc = crc32Feed(crc, &w[W_PHASE + 1], WORDS - (W_PHASE + 1));
        } else {
            crc = crc32Feed(crc, &w[W_PHASE + 1], W_FAIL - (W_PHASE + 1));
            crc = crc32Feed(crc, &w[W_EXC], WORDS - W_EXC);
        }
        return ~crc;
    }
    uint32_t bodyCrc() const { return bodyCrcForLayout(LAYOUT); }
    /**
     * Same death or not: build, the callback's reason, epc1 — or the failed
     * caller when there is no epc1 (abort/OOM) — and the reset reason the next
     * boot read, which is what tells a panic from a task watchdog on ESP32,
     * where no callback fills the rest.
     */
    uint32_t dedupKey(uint32_t resetReason = 0) const {
        uint32_t site = epc1() ? epc1() : failCaller();
        // Without a site (abort, hardware WDT, every ESP32 death) the phase is
        // the only position left; with one, the phase would split a site.
        uint32_t k[5] = { w[W_BUILD], cbReason(), site, resetReason, site ? 0u : phase() };
        return crc32(k, 5);
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
    enum : uint16_t { PHASE_IDLE = 0, PHASE_EVENT_DISPATCH = 0xFF, PHASE_INIT = 0x100 };   // 0x100 | index: that component's begin()

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
    /** The reset reason this boot read, as the promotion rule saw it. */
    HAL::Platform::ResetReason resetReason() const { return resetReason_; }
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
    /** From the platform's crash callback: no allocation, no log, one RTC write; then the user hook. */
    void recordCrash(const CrashInfo& info);
    /** A survived allocation failure (OBS-4), from the ESP32 heap hook on any task or the
     *  ESP8266 latch in tick(): no allocation, no log; the group goes to RTC at once, count last. */
    void noteFailedAlloc(uint32_t size, uint32_t site) { noteFailedAllocImpl(size, site, true); }
    typedef void (*FailedAllocHook)(uint32_t size, uint32_t site);
    /** Runs in the failing task's context, inside the allocator: no allocation, no log, no blocking. */
    void onFailedAlloc(FailedAllocHook hook) { userFailedAllocHook_ = hook; }
    uint32_t failedAllocCount() const { return current_.failAllocCount(); }
    uint32_t lastFailedAllocSize() const { return current_.failAllocSize(); }
    /** Count and last size read together under the group's section (OBS-4). */
    void failedAllocSnapshot(uint32_t& count, uint32_t& lastSize) const;
    /** A user hook to run after the record is written, from the crash context. */
    typedef void (*CrashHook)(const CrashInfo&);
    void onCrash(CrashHook hook) { userCrashHook_ = hook; }
    /** False when the platform could not register the restart hook (ESP32 shutdown slots full). */
    bool restartHookInstalled() const { return restartHookInstalled_; }
    /** False where the platform has no heap hook (ESP8266), opted out, or refused (OBS-4). */
    bool failedAllocHookInstalled() const { return failedAllocHookInstalled_; }

    /** The promoted record as text, allocation-free. Returns characters written. */
    size_t format(char* buf, size_t len) const;
    static const char* promotionName(Promotion p);

    void resetForTest();

private:
    FlightRecorder() { resetForTest(); }
    void startFresh(uint32_t seq);
    void writeAll(const FlightRecord& r);
    void flush();
    void noteFailedAllocImpl(uint32_t size, uint32_t site, bool walk);
    // RTC is held only while a PROMOTED record waits for acknowledgement: a
    // fresh record must be writable during bring-up, or a death in begin()
    // — the boot loop this exists for — leaves nothing (found in review).
    bool rtcHeld() const { return hasPromotedRecord() && !acknowledged_; }

    FlightRecord current_;
    FlightRecord previous_;
    Promotion promotion_;
    HAL::Platform::ResetReason resetReason_;
    bool begun_, hold_, acknowledged_, torn_;
    uint32_t runMin_, lastTickFree_, largestAtMin_, lastTickMs_, lastSlowMs_, lastSampleMs_;
    uint32_t fastIdx_, slowIdx_;
    bool extraWalkDone_;
    bool restartHookInstalled_, failedAllocHookInstalled_, inUserFailedAllocHook_;
    CrashHook userCrashHook_;
    FailedAllocHook userFailedAllocHook_;
};

} // namespace DomoticsCore

#endif // DOMOTICS_CORE_FLIGHT_RECORDER_H
