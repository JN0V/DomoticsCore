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
#include <freertos/FreeRTOS.h>
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

// OBS-4: the failed-allocation group's section and the single heap_caps slot.
static portMUX_TYPE s_failGroupMux = portMUX_INITIALIZER_UNLOCKED;
void failGroupEnter() { portENTER_CRITICAL_SAFE(&s_failGroupMux); }
void failGroupLeave() { portEXIT_CRITICAL_SAFE(&s_failGroupMux); }
#if DOMOTICS_CRASH_HOOKS
static FailedAllocHook s_failedAllocHook = nullptr;
// IDF's signature, on the failing task after the heap lock is released. Not
// IRAM: a failed allocation never runs cache-off (heap_caps_get_free_size is
// in .text for the same reason). OBS-4.
static void failedAllocTrampoline(size_t size, uint32_t caps, const char*) {
    if (s_failedAllocHook) s_failedAllocHook(static_cast<uint32_t>(size), caps);
}
bool installFailedAllocHook(FailedAllocHook hook) {
    s_failedAllocHook = hook;
    return heap_caps_register_failed_alloc_callback(&failedAllocTrampoline) == ESP_OK;
}
#else
bool installFailedAllocHook(FailedAllocHook) { return false; }
#endif
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
#elif !DOMOTICS_PLATFORM_ESP32 && !DOMOTICS_PLATFORM_ESP8266
bool installRestartHook(RestartHook hook) {
    if (restartHookInstallFailsForTest) { s_restartHook = nullptr; return false; }
    s_restartHook = hook; return true;
}
#else
bool installRestartHook(RestartHook hook) { s_restartHook = hook; return true; }
#endif
}}} // namespace

namespace DomoticsCore {

namespace {
void markOursTrampoline() { FlightRecorder::instance().markOurs(); }
void noteFailedAllocTrampoline(uint32_t size, uint32_t site) { FlightRecorder::instance().noteFailedAlloc(size, site); }

uint32_t buildIdCrc() {
    // Without DOMOTICS_BUILD_ID the compile time stands in: not reproducible,
    // but two firmwares get two ids, so a death at a reused address after an
    // OTA is not counted as the old build's repeat.
#ifdef DOMOTICS_BUILD_ID
    static const char id[] = DOMOTICS_BUILD_ID;
#else
    static const char id[] = __DATE__ " " __TIME__;
#endif
    uint32_t words[16] = {};
    size_t n = strlen(id);
    if (n > sizeof(words)) n = sizeof(words);
    memcpy(words, id, n);
    return FlightRecord::crc32(words, (n + 3) / 4);
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
    resetReason_ = HAL::Platform::ResetReason::Unknown;
    begun_ = hold_ = acknowledged_ = torn_ = false;
    runMin_ = 0xFFFFFFFFu;
    lastTickFree_ = largestAtMin_ = 0;
    lastTickMs_ = lastSlowMs_ = lastSampleMs_ = 0;
    fastIdx_ = slowIdx_ = 0;
    extraWalkDone_ = false;
    restartHookInstalled_ = failedAllocHookInstalled_ = inUserFailedAllocHook_ = false;
    userCrashHook_ = nullptr;
    userFailedAllocHook_ = nullptr;
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
    // The failed-alloc group is refreshed and written inside its section: a
    // heap hook on the other core between the copy and the write would
    // otherwise be overwritten by the stale copy (OBS-4).
    HAL::Platform::failGroupEnter();
    memcpy(&copy.w[FlightRecord::W_FAIL], &r.w[FlightRecord::W_FAIL], FlightRecord::FAIL_WORDS * 4);
    HAL::Platform::rtcWrite(0, copy.w, FlightRecord::WORDS);
    HAL::Platform::failGroupLeave();
}

void FlightRecorder::flush() {
    if (rtcHeld()) return;
    current_.w[FlightRecord::W_CRC] = current_.bodyCrc();
    // Body first, crc last: a death between the two leaves a record the next
    // boot reports as torn (phase still readable) rather than as absent.
    // The failed-alloc group (w56-60) is never written from here: the heap
    // hook stores it straight to RTC and a copy would clobber it (OBS-4).
    HAL::Platform::rtcWrite(FlightRecord::W_BUILD, &current_.w[FlightRecord::W_BUILD], FlightRecord::W_FAIL - FlightRecord::W_BUILD);
    HAL::Platform::rtcWrite(FlightRecord::W_EXC, &current_.w[FlightRecord::W_EXC], FlightRecord::WORDS - FlightRecord::W_EXC);
    HAL::Platform::rtcStoreWord(FlightRecord::W_CRC, current_.w[FlightRecord::W_CRC]);
}

void FlightRecorder::begin(bool holdUntilAcknowledged) {
    if (begun_) return;
    begun_ = true;
    hold_ = holdUntilAcknowledged;
    lastTickMs_ = lastSlowMs_ = HAL::Platform::getMillis();
    restartHookInstalled_ = HAL::Platform::installRestartHook(&markOursTrampoline);
    resetReason_ = HAL::Platform::getResetReason();
    // A cliff or a crash inside the first interval measures against this.
    lastTickFree_ = runMin_ = HAL::Platform::getAllocatableFreeHeap();

    HAL::Platform::rtcRead(0, previous_.w, FlightRecord::WORDS);
    // Layout 1 (before OBS-4) is read with its own crc rule and rewritten as
    // layout 2: the first boot after that upgrade keeps its death.
    const uint32_t layout = previous_.layout();
    const bool magicOk = previous_.w[FlightRecord::W_MAGIC] == FlightRecord::MAGIC
                      && (layout == FlightRecord::LAYOUT || layout == FlightRecord::LAYOUT_V1);
    if (!magicOk) {
        startFresh(0);
        writeAll(current_);
    } else {
        const uint32_t flags = previous_.flags();
        torn_ = previous_.w[FlightRecord::W_CRC] != previous_.bodyCrcForLayout(layout);

        if (flags & FlightRecord::UNPERSISTED) {
            // Died again before the last promotion was acknowledged: the record
            // in RTC is still the first death, promoted for the reason stored then.
            promotion_ = static_cast<Promotion>((flags & FlightRecord::PROMO_MASK) >> FlightRecord::PROMO_SHIFT);
            if (promotion_ == Promotion::None) promotion_ = Promotion::CrashCallback;
        } else {
            const HAL::Platform::ResetReason reason = resetReason_;
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
            // RTC keeps the death until acknowledge()
        } else {
            writeAll(current_);
        }
    }
    // Registered last: a failure on another task during the lines above would
    // write a group that startFresh()/writeAll() then erase (OBS-4).
    failedAllocHookInstalled_ = HAL::Platform::installFailedAllocHook(&noteFailedAllocTrampoline);
}

void FlightRecorder::acknowledge() {
    if (!begun_ || acknowledged_) return;
    acknowledged_ = true;
    hold_ = false;
    if (hasPromotedRecord()) writeAll(current_);
}

void FlightRecorder::setPhase(uint16_t v) {
    if (!DOMOTICS_FLIGHT_RECORDER_TICK || !DOMOTICS_FLIGHT_RECORDER_PHASE || !begun_) return;
    current_.w[FlightRecord::W_PHASE] = FlightRecord::encodePhase(v);
    if (!rtcHeld()) HAL::Platform::rtcStoreWord(FlightRecord::W_PHASE, current_.w[FlightRecord::W_PHASE]);
}

void FlightRecorder::noteEventDrops(uint32_t totalDrops) {
    if (totalDrops > 0xFFFFu) totalDrops = 0xFFFFu;
    current_.w[FlightRecord::W_SEQ] = (current_.w[FlightRecord::W_SEQ] & 0xFFFFu) | (totalDrops << 16);
}

void FlightRecorder::markOurs() {
    if (!begun_) {
        // A restart before Core::begin(): mark whatever record RTC holds, so
        // the next boot does not promote a deliberate restart.
        uint32_t meta = 0;
        if (HAL::Platform::rtcRead(FlightRecord::W_META, &meta, 1)) {
            HAL::Platform::rtcStoreWord(FlightRecord::W_META, meta | FlightRecord::OURS);
        }
        return;
    }
    current_.w[FlightRecord::W_META] |= FlightRecord::OURS;
    if (!rtcHeld()) HAL::Platform::rtcStoreWord(FlightRecord::W_META, current_.w[FlightRecord::W_META]);
}

void FlightRecorder::tick() {
    if (!DOMOTICS_FLIGHT_RECORDER_TICK || !begun_) return;
    const uint32_t now = HAL::Platform::getMillis();
    // The free-heap read is at most once per millisecond: a loop runs every
    // 40-90 us and the read cost 7-19 us of it when taken every time
    // (measured, both boards); a cliff shorter than 1 ms is not a trend.
    if (now == lastSampleMs_ && now != lastTickMs_) return;
    lastSampleMs_ = now;
    // OBS-4, ESP8266: the core's last failed allocation, latched into the
    // group and cleared, so the crash callback reads only a fatal one.
    uint32_t failAddr = 0, failSize = 0;
    if (HAL::Platform::takeLastFailedAlloc(failAddr, failSize)) noteFailedAlloc(failSize, failAddr);
    const uint32_t freeNow = HAL::Platform::getAllocatableFreeHeap();
    if (freeNow < runMin_) runMin_ = freeNow;
    // One extra largest-block walk per interval, only on a cliff (D3): the
    // walk runs with interrupts off on ESP8266.
    if (!extraWalkDone_ && lastTickFree_ > runMin_
        && lastTickFree_ - runMin_ >= HAL::Platform::heapCliffThresholdBytes()) {
        largestAtMin_ = HAL::Platform::getLargestFreeBlock();
        extraWalkDone_ = true;
    }
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

void FlightRecorder::noteFailedAllocImpl(uint32_t size, uint32_t site, bool walk) {
    if (!begun_) return;
    const uint32_t freeNow = HAL::Platform::getAllocatableFreeHeap();
    if (freeNow < runMin_) runMin_ = freeNow;   // a benign race with tick() on the other core: a minimum
    // One largest-block walk per tick interval, shared with the cliff walk
    // (interrupts off on ESP8266); none from the crash callback. Checked
    // outside the section: two writers may walk twice, never more.
    uint32_t largest = largestAtMin_;
    if (walk && (!extraWalkDone_ || largest == 0)) {   // 0: tick() reset the gate between the two reads
        largest = largestAtMin_ = HAL::Platform::getLargestFreeBlock();
        extraWalkDone_ = true;
    }
    const uint32_t now = HAL::Platform::getMillisAnyContext();
    HAL::Platform::failGroupEnter();
    uint32_t* g = &current_.w[FlightRecord::W_FAIL];
    const uint32_t count = current_.failAllocCount() + 1;
    g[1] = size;
    g[2] = FlightRecord::packHeap(freeNow, largest);
    g[3] = site;
    g[4] = now;
    g[0] = FlightRecord::encodeCount(count);
    if (!rtcHeld()) {   // a held death keeps its own group; acknowledge() writes this one
        for (size_t i = 1; i < FlightRecord::FAIL_WORDS; ++i) HAL::Platform::rtcStoreWord(FlightRecord::W_FAIL + i, g[i]);
        HAL::Platform::rtcStoreWord(FlightRecord::W_FAIL, g[0]);   // count last: the word that validates the group
    }
    HAL::Platform::failGroupLeave();
    // A user hook that allocates and fails would re-enter here: once, not forever.
    if (userFailedAllocHook_ && !inUserFailedAllocHook_) {
        inUserFailedAllocHook_ = true;
        userFailedAllocHook_(size, site);
        inUserFailedAllocHook_ = false;
    }
}

void FlightRecorder::failedAllocSnapshot(uint32_t& count, uint32_t& lastSize) const {
    HAL::Platform::failGroupEnter();
    count = current_.failAllocCount();
    lastSize = current_.failAllocSize();
    HAL::Platform::failGroupLeave();
}

void FlightRecorder::recordCrash(const CrashInfo& info) {
    if (!begun_) return;
    FlightRecord& r = current_;
    // The fail fields belong to the abort/OOM class (253/254): a fatal new set
    // them and aborted. With any other reason they are a survived failure the
    // sampler had not latched yet — the group's, not the death's (OBS-4).
    uint32_t failCaller = info.failCaller, failSize = info.failSize;
    if (info.reason != 253 && info.reason != 254 && (failCaller || failSize)) {
        noteFailedAllocImpl(failSize, failCaller, false);
        failCaller = failSize = 0;
    }
    r.w[FlightRecord::W_EXC + 0] = info.reason;
    r.w[FlightRecord::W_EXC + 1] = info.exccause;
    r.w[FlightRecord::W_EXC + 2] = info.epc1;
    r.w[FlightRecord::W_EXC + 3] = info.excvaddr;
    r.w[FlightRecord::W_EXC + 4] = failCaller;
    r.w[FlightRecord::W_EXC + 5] = failSize;
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
// umm_last_fail_alloc_addr/size: declared with the HAL (Platform_ESP8266.h).
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
        int n = snprintf(buf, len, "Last death: none recorded\n");
        return n < 0 ? 0 : (static_cast<size_t>(n) < len ? static_cast<size_t>(n) : len - 1);
    }
    // Every line stays under 128 characters: the ESP8266's log buffer.
    const FlightRecord& p = previous_;
    int n = snprintf(buf, len,
        "Last death: %s%s | reset %s | boot #%lu | phase %u%s | uptime %lu.%03lu s\n",
        promotionName(promotion_), torn_ ? " (torn record)" : "",
        HAL::Platform::getResetReasonString(resetReason_).c_str(),
        static_cast<unsigned long>(p.bootSequence()),
        static_cast<unsigned>(p.phase()), p.phaseValid() ? "" : "?",
        static_cast<unsigned long>(p.lastUptimeMs() / 1000u), static_cast<unsigned long>(p.lastUptimeMs() % 1000u));
    if (n < 0) return 0;
    size_t used = static_cast<size_t>(n) < len ? static_cast<size_t>(n) : len - 1;
    int m = snprintf(buf + used, len - used,
        "  min free %lu B, largest %lu B | drops %lu | build %08lx\n",
        static_cast<unsigned long>(p.minFreeBytes()), static_cast<unsigned long>(p.largestBytes()),
        static_cast<unsigned long>(p.eventDrops()), static_cast<unsigned long>(p.w[FlightRecord::W_BUILD]));
    if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
    if (p.flags() & FlightRecord::CALLBACK_RAN) {
        m = snprintf(buf + used, len - used,
            "  callback: reason %lu exccause %lu epc1 0x%08lx excvaddr 0x%08lx\n",
            static_cast<unsigned long>(p.cbReason()), static_cast<unsigned long>(p.exccause()),
            static_cast<unsigned long>(p.epc1()), static_cast<unsigned long>(p.excvaddr()));
        if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
        m = snprintf(buf + used, len - used,
            "  last failed alloc %lu B from 0x%08lx | stack %08lx %08lx %08lx %08lx\n",
            static_cast<unsigned long>(p.failSize()), static_cast<unsigned long>(p.failCaller()),
            static_cast<unsigned long>(p.w[FlightRecord::W_STACK]), static_cast<unsigned long>(p.w[FlightRecord::W_STACK + 1]),
            static_cast<unsigned long>(p.w[FlightRecord::W_STACK + 2]), static_cast<unsigned long>(p.w[FlightRecord::W_STACK + 3]));
        if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
    }
    // Before the ring, so truncation eats samples rather than the failure.
    if (p.failAllocCount() > 0) {
        const uint8_t plat = p.platform();   // 1 ESP8266: a caller; 2 ESP32: the caps word
        const uint32_t up = p.failAllocUptimeMs();
        m = snprintf(buf + used, len - used,
            "  failed allocs: %lu | last %lu B %s 0x%0*lx at %lu.%03lu s | free %lu B, largest %lu B\n",
            static_cast<unsigned long>(p.failAllocCount()), static_cast<unsigned long>(p.failAllocSize()),
            plat == 1 ? "from" : plat == 2 ? "caps" : "site", plat == 2 ? 4 : 8,
            static_cast<unsigned long>(p.failAllocSite()),
            static_cast<unsigned long>(up / 1000u), static_cast<unsigned long>(up % 1000u),
            static_cast<unsigned long>(p.failAllocFreeBytes()), static_cast<unsigned long>(p.failAllocLargestBytes()));
        if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
    }
    // The fast ring, oldest first — the record keeps no index, so the oldest
    // sample is the one with the smallest uptime — "uptime_s:free_kB/largest_kB",
    // four per line so a line stays under the ESP8266's 128-byte log buffer.
    if (used < len - 1) {
        size_t start = 0;
        uint32_t oldest = 0xFFFFFFFFu;
        for (size_t i = 0; i < FlightRecord::FAST_SAMPLES; ++i) {
            const uint32_t t = p.w[FlightRecord::W_FAST + 2 * i];
            const uint32_t h = p.w[FlightRecord::W_FAST + 2 * i + 1];
            if ((t != 0 || h != 0) && t < oldest) { oldest = t; start = i; }
        }
        int m = snprintf(buf + used, len - used, "  ring:");
        if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
        bool any = false;
        size_t printed = 0;
        for (size_t k = 0; k < FlightRecord::FAST_SAMPLES && used < len - 1; ++k) {
            const size_t i = (start + k) % FlightRecord::FAST_SAMPLES;
            const uint32_t t = p.w[FlightRecord::W_FAST + 2 * i];
            const uint32_t h = p.w[FlightRecord::W_FAST + 2 * i + 1];
            if (t == 0 && h == 0) continue;
            if (any && printed % 4 == 0) {
                m = snprintf(buf + used, len - used, "\n  ring:");
                if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
            }
            any = true;
            ++printed;
            m = snprintf(buf + used, len - used, " %lus:%lu.%luk/%lu.%luk",
                         static_cast<unsigned long>(t),
                         static_cast<unsigned long>((h >> 16) * 16u / 1024u), static_cast<unsigned long>(((h >> 16) * 16u % 1024u) * 10u / 1024u),
                         static_cast<unsigned long>((h & 0xFFFFu) * 16u / 1024u), static_cast<unsigned long>(((h & 0xFFFFu) * 16u % 1024u) * 10u / 1024u));
            if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
        }
        m = snprintf(buf + used, len - used, any ? "\n" : " (empty)\n");
        if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
    }
    return used;
}

} // namespace DomoticsCore
