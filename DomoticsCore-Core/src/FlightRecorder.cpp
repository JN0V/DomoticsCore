#include "DomoticsCore/FlightRecorder.h"
#include <string.h>
#include <stdio.h>

// This translation unit holds the one definition of the RTC storage (ESP32)
// and, from S3, of the platform crash hooks. It is also where begin() lives,
// which Core::begin() always references: under lib_archive = true a strong
// symbol in an archive member only overrides the core's weak one when that
// member is linked, and this reference is what links it.

#if DOMOTICS_PLATFORM_ESP32
#include <esp_attr.h>
namespace DomoticsCore { namespace HAL { namespace Platform {
static RTC_NOINIT_ATTR uint32_t s_rtcWords[96];
bool rtcRead(uint32_t wordOffset, uint32_t* dst, size_t words) {
    if (wordOffset + words > 96) return false;
    memcpy(dst, &s_rtcWords[wordOffset], words * 4);
    return true;
}
bool rtcWrite(uint32_t wordOffset, const uint32_t* src, size_t words) {
    if (wordOffset + words > 96) return false;
    memcpy(&s_rtcWords[wordOffset], src, words * 4);
    return true;
}
void rtcStoreWord(uint32_t wordOffset, uint32_t value) {
    if (wordOffset < 96) s_rtcWords[wordOffset] = value;
}
}}} // namespace
#endif

// ---- the restart hook: one definition for every platform ------------------
namespace DomoticsCore { namespace HAL { namespace Platform {
static RestartHook s_restartHook = nullptr;
RestartHook restartHook() { return s_restartHook; }
#if DOMOTICS_PLATFORM_ESP32 && DOMOTICS_CRASH_HOOKS
static void shutdownTrampoline() { if (s_restartHook) s_restartHook(); }
bool installRestartHook(RestartHook hook) {
    s_restartHook = hook;
    // Covers esp_restart() callers outside this HAL too (the Arduino core's
    // ESP.restart()). IDF keeps a handful of handler slots; a full table is
    // reported, not fatal: HAL restarts are still marked through restart().
    return esp_register_shutdown_handler(&shutdownTrampoline) == ESP_OK;
}
#else
bool installRestartHook(RestartHook hook) { s_restartHook = hook; return true; }
#endif
}}} // namespace

namespace DomoticsCore {

namespace {
void markOursTrampoline() { FlightRecorder::instance().markOurs(); }

uint32_t buildIdCrc() {
#ifdef DOMOTICS_BUILD_ID
    static const char id[] = DOMOTICS_BUILD_ID;
    uint32_t words[16] = {};
    size_t n = strlen(id);
    if (n > sizeof(words)) n = sizeof(words);
    memcpy(words, id, n);
    return FlightRecord::crc32(words, (n + 3) / 4);
#else
    return 0;
#endif
}
} // namespace

FlightRecorder& FlightRecorder::instance() {
    static FlightRecorder r;
    return r;
}

void FlightRecorder::resetForTest() {
    memset(&current_, 0, sizeof(current_));
    memset(&previous_, 0, sizeof(previous_));
    promotion_ = Promotion::None;
    begun_ = hold_ = acknowledged_ = torn_ = false;
    runMin_ = 0xFFFFFFFFu;
    lastTickFree_ = largestAtMin_ = 0;
    lastTickMs_ = lastSlowMs_ = 0;
    fastIdx_ = slowIdx_ = 0;
    extraWalkDone_ = false;
    restartHookInstalled_ = false;
    userCrashHook_ = nullptr;
}

void FlightRecorder::startFresh(uint32_t seq) {
    memset(&current_, 0, sizeof(current_));
    current_.w[FlightRecord::W_MAGIC] = FlightRecord::MAGIC;
    current_.w[FlightRecord::W_META] = (FlightRecord::LAYOUT << 24) | (static_cast<uint32_t>(HAL::Platform::platformId()) << 16);
    current_.w[FlightRecord::W_BUILD] = buildIdCrc();
    current_.w[FlightRecord::W_SEQ] = seq & 0xFFFFu;
    current_.w[FlightRecord::W_PHASE] = FlightRecord::encodePhase(0);
}

void FlightRecorder::writeAll(const FlightRecord& r) {
    FlightRecord copy = r;
    copy.w[FlightRecord::W_CRC] = copy.bodyCrc();
    HAL::Platform::rtcWrite(0, copy.w, FlightRecord::WORDS);
}

void FlightRecorder::flush() {
    if (rtcHeld()) return;
    current_.w[FlightRecord::W_CRC] = current_.bodyCrc();
    // Body first, crc last: a death between the two leaves a record the next
    // boot reports as torn (phase still readable) rather than as absent.
    HAL::Platform::rtcWrite(FlightRecord::W_BUILD, &current_.w[FlightRecord::W_BUILD], FlightRecord::WORDS - FlightRecord::W_BUILD);
    HAL::Platform::rtcStoreWord(FlightRecord::W_CRC, current_.w[FlightRecord::W_CRC]);
}

void FlightRecorder::begin(bool holdUntilAcknowledged) {
    if (begun_) return;
    begun_ = true;
    hold_ = holdUntilAcknowledged;
    lastTickMs_ = lastSlowMs_ = HAL::Platform::getMillis();
    restartHookInstalled_ = HAL::Platform::installRestartHook(&markOursTrampoline);

    HAL::Platform::rtcRead(0, previous_.w, FlightRecord::WORDS);
    const bool magicOk = previous_.w[FlightRecord::W_MAGIC] == FlightRecord::MAGIC
                      && previous_.layout() == FlightRecord::LAYOUT;
    if (!magicOk) {
        startFresh(0);
        writeAll(current_);
        return;
    }
    const uint32_t flags = previous_.flags();
    torn_ = previous_.w[FlightRecord::W_CRC] != previous_.bodyCrc();

    if (flags & FlightRecord::UNPERSISTED) {
        // Died again before the last promotion was acknowledged: the record
        // in RTC is still the first death, promoted for the reason stored then.
        promotion_ = static_cast<Promotion>((flags & FlightRecord::PROMO_MASK) >> FlightRecord::PROMO_SHIFT);
        if (promotion_ == Promotion::None) promotion_ = Promotion::CrashCallback;
    } else {
        const HAL::Platform::ResetReason reason = HAL::Platform::getResetReason();
        if (flags & FlightRecord::CALLBACK_RAN) {
            promotion_ = Promotion::CrashCallback;
        } else if (HAL::Platform::wasUnexpectedReset(reason)) {
            promotion_ = Promotion::UnexpectedReset;
        } else if (reason == HAL::Platform::ResetReason::Software && !(flags & FlightRecord::OURS)) {
            promotion_ = Promotion::UnownedSoftwareReset;
        }
    }

    startFresh(previous_.bootSequence() + 1);
    if (hasPromotedRecord()) {
        uint32_t meta = previous_.w[FlightRecord::W_META] | FlightRecord::UNPERSISTED
                      | (static_cast<uint32_t>(promotion_) << FlightRecord::PROMO_SHIFT)
                      | (torn_ ? FlightRecord::TORN : 0u);
        previous_.w[FlightRecord::W_META] = meta;
        HAL::Platform::rtcStoreWord(FlightRecord::W_META, meta);   // outside the crc: the body stays verifiable
        return;   // RTC keeps the death until acknowledge()
    }
    writeAll(current_);
}

void FlightRecorder::acknowledge() {
    if (!begun_ || acknowledged_) return;
    acknowledged_ = true;
    hold_ = false;
    if (hasPromotedRecord()) writeAll(current_);
}

void FlightRecorder::setPhase(uint16_t v) {
    if (!begun_) return;
    current_.w[FlightRecord::W_PHASE] = FlightRecord::encodePhase(v);
    if (!rtcHeld()) HAL::Platform::rtcStoreWord(FlightRecord::W_PHASE, current_.w[FlightRecord::W_PHASE]);
}

void FlightRecorder::noteEventDrops(uint32_t totalDrops) {
    if (totalDrops > 0xFFFFu) totalDrops = 0xFFFFu;
    current_.w[FlightRecord::W_SEQ] = (current_.w[FlightRecord::W_SEQ] & 0xFFFFu) | (totalDrops << 16);
}

void FlightRecorder::markOurs() {
    if (!begun_) return;
    current_.w[FlightRecord::W_META] |= FlightRecord::OURS;
    if (!rtcHeld()) HAL::Platform::rtcStoreWord(FlightRecord::W_META, current_.w[FlightRecord::W_META]);
}

void FlightRecorder::tick() {
    if (!begun_) return;
    const uint32_t freeNow = HAL::Platform::getAllocatableFreeHeap();
    if (freeNow < runMin_) runMin_ = freeNow;
    // One extra largest-block walk per interval, only on a cliff (D3): the
    // walk runs with interrupts off on ESP8266.
    if (!extraWalkDone_ && lastTickFree_ > runMin_
        && lastTickFree_ - runMin_ >= HAL::Platform::heapCliffThresholdBytes()) {
        largestAtMin_ = HAL::Platform::getLargestFreeBlock();
        extraWalkDone_ = true;
    }
    const uint32_t now = HAL::Platform::getMillis();
    if (now - lastTickMs_ < FAST_INTERVAL_MS) return;

    const uint32_t largest = HAL::Platform::getLargestFreeBlock();
    const uint32_t largestMin = extraWalkDone_ && largestAtMin_ < largest ? largestAtMin_ : largest;
    const uint32_t sample = FlightRecord::packHeap(freeNow, largest);
    const uint32_t uptimeS = now / 1000u;

    current_.w[FlightRecord::W_FAST + 2 * fastIdx_] = uptimeS;
    current_.w[FlightRecord::W_FAST + 2 * fastIdx_ + 1] = sample;
    fastIdx_ = (fastIdx_ + 1) % FlightRecord::FAST_SAMPLES;
    if (now - lastSlowMs_ >= SLOW_INTERVAL_MS) {
        current_.w[FlightRecord::W_SLOW + 2 * slowIdx_] = uptimeS;
        current_.w[FlightRecord::W_SLOW + 2 * slowIdx_ + 1] = sample;
        slowIdx_ = (slowIdx_ + 1) % FlightRecord::SLOW_SAMPLES;
        lastSlowMs_ = now;
    }
    current_.w[FlightRecord::W_LAST_UPTIME] = now;
    current_.w[FlightRecord::W_LAST_HEAP] = FlightRecord::packHeap(runMin_, largestMin);

    lastTickMs_ = now;
    lastTickFree_ = freeNow;
    runMin_ = freeNow;
    largestAtMin_ = 0;
    extraWalkDone_ = false;
    flush();
}

void FlightRecorder::recordCrash(const CrashInfo& info) {
    if (!begun_) return;
    FlightRecord& r = current_;
    r.w[FlightRecord::W_EXC + 0] = info.reason;
    r.w[FlightRecord::W_EXC + 1] = info.exccause;
    r.w[FlightRecord::W_EXC + 2] = info.epc1;
    r.w[FlightRecord::W_EXC + 3] = info.excvaddr;
    r.w[FlightRecord::W_EXC + 4] = info.failCaller;
    r.w[FlightRecord::W_EXC + 5] = info.failSize;
    size_t n = info.stackWords < FlightRecord::STACK_WORDS ? info.stackWords : FlightRecord::STACK_WORDS;
    for (size_t i = 0; i < FlightRecord::STACK_WORDS; ++i) {
        r.w[FlightRecord::W_STACK + i] = (info.stack && i < n) ? info.stack[i] : 0;
    }
    // The minima since the last tick: a burst inside one interval is on the
    // record even though no tick saw it. No heap walk here — crash context.
    const uint32_t freeNow = HAL::Platform::getAllocatableFreeHeap();
    const uint32_t minFree = freeNow < runMin_ ? freeNow : runMin_;
    r.w[FlightRecord::W_LAST_UPTIME] = HAL::Platform::getMillis();
    r.w[FlightRecord::W_LAST_HEAP] = FlightRecord::packHeap(minFree, extraWalkDone_ ? largestAtMin_ : (r.w[FlightRecord::W_LAST_HEAP] & 0xFFFFu) * 16u);
    r.w[FlightRecord::W_META] |= FlightRecord::CALLBACK_RAN;
    if (!rtcHeld()) writeAll(r);   // else the first death stays until acknowledged
    if (userCrashHook_) userCrashHook_(info);
}

// ---- the ESP8266 crash callback -------------------------------------------
// The core declares custom_crash_callback weak; this strong definition takes
// over for exceptions, the soft WDT and the abort/OOM class (reasons 253/254),
// with the stack the core hands over. It runs before the restart, on the
// crashed stack: no allocation, no String, no log.
#if DOMOTICS_PLATFORM_ESP8266 && DOMOTICS_CRASH_HOOKS
extern "C" {
#include <user_interface.h>
extern void* umm_last_fail_alloc_addr;   // operator new records these on every build (abi.cpp)
extern int umm_last_fail_alloc_size;
void custom_crash_callback(struct rst_info* rst, uint32_t stack, uint32_t stack_end) {
    CrashInfo c;
    if (rst) {
        c.reason = rst->reason; c.exccause = rst->exccause;
        c.epc1 = rst->epc1; c.excvaddr = rst->excvaddr;
    }
    c.failCaller = reinterpret_cast<uint32_t>(umm_last_fail_alloc_addr);
    c.failSize = static_cast<uint32_t>(umm_last_fail_alloc_size);
    c.stack = reinterpret_cast<const uint32_t*>(stack);
    c.stackWords = stack_end > stack ? (stack_end - stack) / 4 : 0;
    FlightRecorder::instance().recordCrash(c);
}
}
#endif

const char* FlightRecorder::promotionName(Promotion p) {
    switch (p) {
        case Promotion::CrashCallback:        return "crash callback";
        case Promotion::UnexpectedReset:      return "unexpected reset";
        case Promotion::UnownedSoftwareReset: return "software reset not requested by the firmware";
        default:                              return "none";
    }
}

size_t FlightRecorder::format(char* buf, size_t len) const {
    if (!buf || len == 0) return 0;
    if (!hasPromotedRecord()) {
        return static_cast<size_t>(snprintf(buf, len, "Last death: none recorded\n"));
    }
    const FlightRecord& p = previous_;
    int n = snprintf(buf, len,
        "Last death: %s%s | boot #%lu | phase %u%s | uptime %lu.%03lu s | min free %lu B, largest %lu B | drops %lu | build %08lx\n",
        promotionName(promotion_), torn_ ? " (torn record)" : "",
        static_cast<unsigned long>(p.bootSequence()),
        static_cast<unsigned>(p.phase()), p.phaseValid() ? "" : "?",
        static_cast<unsigned long>(p.lastUptimeMs() / 1000u), static_cast<unsigned long>(p.lastUptimeMs() % 1000u),
        static_cast<unsigned long>(p.minFreeBytes()), static_cast<unsigned long>(p.largestBytes()),
        static_cast<unsigned long>(p.eventDrops()), static_cast<unsigned long>(p.w[FlightRecord::W_BUILD]));
    if (n < 0) return 0;
    size_t used = static_cast<size_t>(n) < len ? static_cast<size_t>(n) : len - 1;
    if (p.flags() & FlightRecord::CALLBACK_RAN) {
        int m = snprintf(buf + used, len - used,
            "  callback: reason %lu exccause %lu epc1 0x%08lx excvaddr 0x%08lx | last failed alloc %lu B from 0x%08lx\n",
            static_cast<unsigned long>(p.cbReason()), static_cast<unsigned long>(p.exccause()),
            static_cast<unsigned long>(p.epc1()), static_cast<unsigned long>(p.excvaddr()),
            static_cast<unsigned long>(p.failSize()), static_cast<unsigned long>(p.failCaller()));
        if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
    }
    return used;
}

} // namespace DomoticsCore
