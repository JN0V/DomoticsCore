// OBS-3: the flight recorder against the stub's RTC array. Every "reboot"
// below is resetForTest() without clearRtcForTest(): RAM goes, RTC stays —
// which is what a reset does. clearRtcForTest() is the power cycle.
#include <unity.h>
#include <DomoticsCore/FlightRecorder.h>
#include <DomoticsCore/Platform_HAL.h>

using namespace DomoticsCore;
namespace P = DomoticsCore::HAL::Platform;

static FlightRecorder& rec() { return FlightRecorder::instance(); }

void setUp(void) {
    rec().resetForTest();
    P::clearRtcForTest();
    P::resetDiagnosticsForTest();            // reset reason → Unknown
    P::setResetReasonForTest(P::ResetReason::PowerOn);
    P::setMillisForTest(1000);
    P::resetFreeHeapForTest();
    P::resetLargestFreeBlockForTest();
    P::restartHookInstallFailsForTest = false;
    P::resetFailedAllocForTest();
}
void tearDown(void) {
    P::resetMillisForTest();
}

// A reset: RAM state gone, RTC kept, a reason for the next boot to read.
static void reboot(P::ResetReason reason) {
    rec().resetForTest();
    P::setResetReasonForTest(reason);
    P::setMillisForTest(1000);
}

static FlightRecord readRtc() {
    FlightRecord r;
    P::rtcRead(0, r.w, FlightRecord::WORDS);
    return r;
}

static CrashInfo abortInfo(uint32_t failCaller = 0x40201287u, uint32_t failSize = 1024) {
    CrashInfo c;
    c.reason = 254; c.failCaller = failCaller; c.failSize = failSize;
    static const uint32_t stack[4] = { 0x11, 0x22, 0x33, 0x44 };
    c.stack = stack; c.stackWords = 4;
    return c;
}

// ---- first boot, clean boots ------------------------------------------------

void test_first_boot_starts_a_fresh_record_in_rtc() {
    rec().begin();
    TEST_ASSERT_FALSE(rec().hasPromotedRecord());
    FlightRecord r = readRtc();
    TEST_ASSERT_EQUAL_HEX32(FlightRecord::MAGIC, r.w[FlightRecord::W_MAGIC]);
    TEST_ASSERT_EQUAL_UINT32(FlightRecord::LAYOUT, r.layout());
    TEST_ASSERT_EQUAL_UINT32(0, r.bootSequence());
    TEST_ASSERT_EQUAL_HEX32(r.bodyCrc(), r.w[FlightRecord::W_CRC]);
    TEST_ASSERT_TRUE(r.phaseValid());
}

void test_owned_software_reset_is_not_promoted_and_bumps_the_sequence() {
    rec().begin();
    rec().markOurs();
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_FALSE(rec().hasPromotedRecord());
    TEST_ASSERT_EQUAL_UINT32(1, readRtc().bootSequence());
}

void test_garbage_in_rtc_reads_as_power_on() {
    P::stubRtcWordsForTest[0] = 0xDEADBEEF;
    P::stubRtcWordsForTest[4] = 77;
    rec().begin();
    TEST_ASSERT_FALSE(rec().hasPromotedRecord());
    TEST_ASSERT_EQUAL_UINT32(0, readRtc().bootSequence());
}

void test_a_restart_before_begin_still_marks_the_record_as_ours() {
    rec().begin();                                  // a previous run wrote a record
    rec().resetForTest();                           // this run restarts before Core::begin()
    P::restart();                                   // the hook is not installed yet: markOurs() on a raw record
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_FALSE(rec().hasPromotedRecord());
}

void test_dedup_key_uses_the_phase_when_there_is_no_site() {
    FlightRecord a{}; FlightRecord b{};
    a.w[FlightRecord::W_PHASE] = FlightRecord::encodePhase(3);
    b.w[FlightRecord::W_PHASE] = FlightRecord::encodePhase(5);
    TEST_ASSERT_NOT_EQUAL(a.dedupKey(4), b.dedupKey(4));   // two ESP32 panics from different phases
    a.w[FlightRecord::W_EXC + 2] = b.w[FlightRecord::W_EXC + 2] = 0x40201000u;
    TEST_ASSERT_EQUAL_HEX32(a.dedupKey(4), b.dedupKey(4));  // the same site in two phases is one death
}

void test_format_with_a_tiny_buffer_never_reports_more_than_it_wrote() {
    rec().begin();
    char buf[8];
    size_t n = rec().format(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n < sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[n]);
}

void test_tick_and_phase_before_begin_do_nothing() {
    rec().tick();
    rec().setPhase(3);
    TEST_ASSERT_EQUAL_HEX32(0, P::stubRtcWordsForTest[FlightRecord::W_MAGIC]);
}

// ---- the three legs of the promotion rule ----------------------------------

void test_crash_callback_promotes_a_software_reset() {
    rec().begin();
    rec().setPhase(5);
    rec().recordCrash(abortInfo());
    reboot(P::ResetReason::Software);           // what abort()/OOM read as on ESP8266
    rec().begin();
    TEST_ASSERT_TRUE(rec().hasPromotedRecord());
    TEST_ASSERT_EQUAL(FlightRecorder::Promotion::CrashCallback, rec().promotion());
    TEST_ASSERT_EQUAL_UINT32(254, rec().promoted().cbReason());
    TEST_ASSERT_EQUAL_UINT32(1024, rec().promoted().failSize());
    TEST_ASSERT_EQUAL_HEX32(0x40201287u, rec().promoted().failCaller());
    TEST_ASSERT_EQUAL_UINT16(5, rec().promoted().phase());
    TEST_ASSERT_EQUAL_HEX32(0x22, rec().promoted().w[FlightRecord::W_STACK + 1]);
    TEST_ASSERT_FALSE(rec().promotedIsTorn());
}

void test_unowned_software_reset_is_promoted() {
    rec().begin();                               // no markOurs()
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_EQUAL(FlightRecorder::Promotion::UnownedSoftwareReset, rec().promotion());
}

void test_unexpected_reset_without_callback_is_promoted_with_its_phase() {
    rec().begin();
    rec().setPhase(3);
    reboot(P::ResetReason::Watchdog);            // hardware WDT: no code ran
    rec().begin();
    TEST_ASSERT_EQUAL(FlightRecorder::Promotion::UnexpectedReset, rec().promotion());
    TEST_ASSERT_EQUAL_UINT16(3, rec().promoted().phase());
}

void test_power_on_after_a_valid_record_is_not_promoted() {
    rec().begin();
    reboot(P::ResetReason::PowerOn);             // RTC kept by the stub; a real power cycle clears it
    rec().begin();
    TEST_ASSERT_FALSE(rec().hasPromotedRecord());
}

// ---- torn records -----------------------------------------------------------

void test_torn_record_still_reports_its_phase() {
    rec().begin();
    rec().setPhase(7);
    P::stubRtcWordsForTest[FlightRecord::W_FAST + 3] ^= 0x1;   // a body word, after the crc was written
    reboot(P::ResetReason::Panic);
    rec().begin();
    TEST_ASSERT_TRUE(rec().hasPromotedRecord());
    TEST_ASSERT_TRUE(rec().promotedIsTorn());
    TEST_ASSERT_EQUAL_UINT16(7, rec().promoted().phase());
}

// The marker moves between flushes, straight to RTC; it must not tear the
// record — on the WROOM-32D every promoted record read "torn" before this.
void test_phase_moving_after_a_flush_does_not_tear_the_record() {
    rec().begin();
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();                                  // flush: crc written
    rec().setPhase(3);                             // the loop goes on
    rec().setPhase(7);
    reboot(P::ResetReason::Panic);
    rec().begin();
    TEST_ASSERT_TRUE(rec().hasPromotedRecord());
    TEST_ASSERT_FALSE(rec().promotedIsTorn());
    TEST_ASSERT_EQUAL_UINT16(7, rec().promoted().phase());
}

// ---- the record stays until acknowledged -----------------------------------

void test_promoted_record_stays_in_rtc_until_acknowledged() {
    rec().begin();
    rec().recordCrash(abortInfo());
    reboot(P::ResetReason::Software);
    rec().begin(true);
    FlightRecord held = readRtc();
    TEST_ASSERT_TRUE(held.flags() & FlightRecord::CALLBACK_RAN);
    TEST_ASSERT_TRUE(held.flags() & FlightRecord::UNPERSISTED);
    // sampling and the phase marker go to RAM only while held
    rec().setPhase(9);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS + 1);
    rec().tick();
    TEST_ASSERT_EQUAL_UINT32(254, readRtc().cbReason());
    TEST_ASSERT_EQUAL_UINT32(0, readRtc().bootSequence());
    rec().acknowledge();
    FlightRecord fresh = readRtc();
    TEST_ASSERT_EQUAL_UINT32(1, fresh.bootSequence());
    TEST_ASSERT_EQUAL_UINT32(0, fresh.flags() & (FlightRecord::CALLBACK_RAN | FlightRecord::UNPERSISTED));
    TEST_ASSERT_EQUAL_HEX32(fresh.bodyCrc(), fresh.w[FlightRecord::W_CRC]);
}

void test_dying_again_before_acknowledgement_keeps_the_first_death() {
    rec().begin();
    rec().recordCrash(abortInfo(0xAAAA, 512));   // death A
    reboot(P::ResetReason::Software);
    rec().begin(true);                           // boot 2: nobody acknowledges (Storage never came up)
    rec().recordCrash(abortInfo(0xBBBB, 64));    // death B, before persistence
    reboot(P::ResetReason::Software);
    rec().begin(true);                           // boot 3
    TEST_ASSERT_TRUE(rec().hasPromotedRecord());
    TEST_ASSERT_EQUAL(FlightRecorder::Promotion::CrashCallback, rec().promotion());
    TEST_ASSERT_EQUAL_HEX32(0xAAAA, rec().promoted().failCaller());
    TEST_ASSERT_EQUAL_UINT32(512, rec().promoted().failSize());
}

// The boot loop this exists for: a death inside begin(), before anything
// has acknowledged, on a boot that promoted nothing. The fresh record must
// take the callback's data — the hold guards a promoted record, not RTC.
void test_a_crash_during_bring_up_with_nothing_promoted_is_recorded() {
    rec().begin(true);                            // System's shape: hold until acknowledged
    rec().setPhase(FlightRecorder::PHASE_INIT | 3);
    rec().recordCrash(abortInfo(0xCAFE, 256));
    reboot(P::ResetReason::Software);
    rec().begin(true);
    TEST_ASSERT_EQUAL(FlightRecorder::Promotion::CrashCallback, rec().promotion());
    TEST_ASSERT_EQUAL_HEX32(0xCAFE, rec().promoted().failCaller());
    TEST_ASSERT_EQUAL_UINT16(FlightRecorder::PHASE_INIT | 3, rec().promoted().phase());
}

void test_the_first_begin_decides_the_hold() {
    rec().begin(true);
    rec().begin(false);                           // Core::begin() after System::begin()
    TEST_ASSERT_TRUE(rec().acknowledgementDeferred());
}

void test_a_record_with_another_layout_reads_as_power_on() {
    rec().begin();
    P::stubRtcWordsForTest[FlightRecord::W_META] = (3u << 24);   // layout 3: not this build's, not the migrated one
    reboot(P::ResetReason::Panic);
    rec().begin();
    TEST_ASSERT_FALSE(rec().hasPromotedRecord());
    TEST_ASSERT_EQUAL_UINT32(0, readRtc().bootSequence());
}

void test_a_failed_hook_registration_is_reported_not_claimed() {
    P::restartHookInstallFailsForTest = true;
    rec().begin();
    TEST_ASSERT_FALSE(rec().restartHookInstalled());
    P::restart();                                 // nobody marks ours
    reboot(P::ResetReason::Software);
    P::restartHookInstallFailsForTest = false;
    rec().begin();
    TEST_ASSERT_EQUAL(FlightRecorder::Promotion::UnownedSoftwareReset, rec().promotion());
}

void test_event_drops_saturate_at_sixteen_bits() {
    rec().begin();
    rec().noteEventDrops(0x12345678u);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFu, rec().current().eventDrops());
    TEST_ASSERT_EQUAL_UINT32(0, rec().current().bootSequence());
}

void test_format_prints_a_wrapped_ring_oldest_first() {
    rec().begin();
    P::setFreeHeapForTest(40960); P::setLargestFreeBlockForTest(20480);
    for (int i = 0; i < 17; ++i) {                // 17 samples: slot 0 holds the newest
        P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS); rec().tick();
    }
    reboot(P::ResetReason::Watchdog);
    rec().begin();
    char buf[600];
    rec().format(buf, sizeof(buf));
    const char* ring = strstr(buf, "ring: ");
    TEST_ASSERT_NOT_NULL(ring);
    TEST_ASSERT_EQUAL_STRING_LEN("ring: 21s:", ring, 10);     // the 2nd sample is the oldest kept
    TEST_ASSERT_NOT_NULL(strstr(buf, " 171s:"));              // the newest is there too
}

void test_acknowledge_without_promotion_is_harmless() {
    rec().begin();
    uint32_t before = readRtc().w[FlightRecord::W_CRC];
    rec().acknowledge();
    rec().acknowledge();
    TEST_ASSERT_EQUAL_HEX32(before, readRtc().w[FlightRecord::W_CRC]);
}

// ---- sampling ---------------------------------------------------------------

void test_running_minimum_survives_between_ticks() {
    rec().begin();
    P::setFreeHeapForTest(60000); rec().tick();
    P::advanceMillisForTest(1);
    P::setFreeHeapForTest(20000); rec().tick();       // a cliff, seen by no tick
    P::advanceMillisForTest(1);
    P::setFreeHeapForTest(60000); rec().tick();
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    TEST_ASSERT_EQUAL_UINT32(20000 - 20000 % 16, readRtc().minFreeBytes());
}

void test_cliff_earns_exactly_one_extra_walk_per_interval() {
    rec().begin();
    P::setFreeHeapForTest(60000);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();                                     // first tick: one walk, sets lastTickFree
    unsigned after_first = P::largestFreeBlockReadsForTest;
    P::setFreeHeapForTest(60000 - P::heapCliffThresholdBytes() - 16);
    for (int i = 0; i < 3; ++i) { P::advanceMillisForTest(1); rec().tick(); }   // the cliff, three loops
    TEST_ASSERT_EQUAL_UINT(after_first + 1, P::largestFreeBlockReadsForTest);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();                                     // tick: one more walk
    TEST_ASSERT_EQUAL_UINT(after_first + 2, P::largestFreeBlockReadsForTest);
}

void test_free_heap_is_read_at_most_once_per_millisecond() {
    rec().begin();
    P::setFreeHeapForTest(60000);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS); rec().tick();   // first tick reads
    P::advanceMillisForTest(1);
    P::setFreeHeapForTest(50000); rec().tick();       // read: new millisecond
    P::setFreeHeapForTest(100);   rec().tick();       // same millisecond: not read
    P::setFreeHeapForTest(50000);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS); rec().tick();
    TEST_ASSERT_EQUAL_UINT32(50000 - 50000 % 16, readRtc().minFreeBytes());
}

void test_no_walk_between_ticks_without_a_cliff() {
    rec().begin();
    P::setFreeHeapForTest(60000);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    unsigned after_first = P::largestFreeBlockReadsForTest;
    P::setFreeHeapForTest(60000 - 512);
    for (int i = 0; i < 100; ++i) { P::advanceMillisForTest(1); rec().tick(); }
    TEST_ASSERT_EQUAL_UINT(after_first, P::largestFreeBlockReadsForTest);
}

void test_fast_ring_wraps_after_sixteen_samples() {
    rec().begin();
    for (int i = 0; i < 17; ++i) {
        P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
        rec().tick();
    }
    FlightRecord r = readRtc();
    // 17th sample landed on slot 0: uptime 1 s + 17 × 10 s
    TEST_ASSERT_EQUAL_UINT32((1000 + 17 * FlightRecorder::FAST_INTERVAL_MS) / 1000, r.w[FlightRecord::W_FAST]);
    TEST_ASSERT_EQUAL_UINT32((1000 + 2 * FlightRecorder::FAST_INTERVAL_MS) / 1000, r.w[FlightRecord::W_FAST + 2]);
}

void test_slow_ring_samples_every_ten_minutes() {
    rec().begin();
    for (int i = 0; i < 61; ++i) {                    // 610 s
        P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
        rec().tick();
    }
    FlightRecord r = readRtc();
    TEST_ASSERT_NOT_EQUAL(0, r.w[FlightRecord::W_SLOW]);
    TEST_ASSERT_EQUAL_UINT32(0, r.w[FlightRecord::W_SLOW + 2]);
}

void test_tick_writes_body_then_crc_and_the_record_verifies() {
    rec().begin();
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    FlightRecord r = readRtc();
    TEST_ASSERT_EQUAL_HEX32(r.bodyCrc(), r.w[FlightRecord::W_CRC]);
    TEST_ASSERT_EQUAL_UINT32(1000 + FlightRecorder::FAST_INTERVAL_MS, r.lastUptimeMs());
}

void test_phase_marker_is_one_store_with_its_complement() {
    rec().begin();
    rec().setPhase(0x1234);
    TEST_ASSERT_EQUAL_HEX32(0x1234EDCBu, P::stubRtcWordsForTest[FlightRecord::W_PHASE]);
}

void test_event_drops_ride_the_sequence_word() {
    rec().begin();
    rec().noteEventDrops(3);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    reboot(P::ResetReason::Watchdog);
    rec().begin();
    TEST_ASSERT_EQUAL_UINT32(3, rec().promoted().eventDrops());
    TEST_ASSERT_EQUAL_UINT32(0, rec().promoted().bootSequence());
}

// ---- the crash callback path ----------------------------------------------

void test_crash_folds_the_running_minimum_into_the_record() {
    rec().begin();
    P::setFreeHeapForTest(60000);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();                                     // plateau at 60000
    P::advanceMillisForTest(1);
    P::setFreeHeapForTest(3000); rec().tick();        // burst inside the interval
    P::advanceMillisForTest(1);
    P::setFreeHeapForTest(58000); rec().tick();       // recovered: the crash-time figure is high
    rec().recordCrash(abortInfo());
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_EQUAL_UINT32(3000 - 3000 % 16, rec().promoted().minFreeBytes());   // the burst, not the crash-time heap
}

void test_platform_restart_marks_the_record_as_ours() {
    rec().begin();
    TEST_ASSERT_TRUE(rec().restartHookInstalled());
    P::restart();                                  // what OTA, WiFi and the console call
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_FALSE(rec().hasPromotedRecord());
}

static const CrashInfo* g_forwarded = nullptr;
static void userHook(const CrashInfo& c) { g_forwarded = &c; }

void test_user_crash_hook_runs_after_the_record() {
    rec().begin();
    rec().onCrash(&userHook);
    CrashInfo c = abortInfo();
    rec().recordCrash(c);
    TEST_ASSERT_EQUAL_PTR(&c, g_forwarded);
    TEST_ASSERT_TRUE(readRtc().flags() & FlightRecord::CALLBACK_RAN);   // written before the hook ran
}

void test_dedup_key_uses_the_fail_caller_when_there_is_no_epc1() {
    FlightRecord a{}; FlightRecord b{}; FlightRecord c{};
    a.w[FlightRecord::W_EXC] = b.w[FlightRecord::W_EXC] = c.w[FlightRecord::W_EXC] = 254;
    a.w[FlightRecord::W_EXC + 4] = 0x1000; b.w[FlightRecord::W_EXC + 4] = 0x2000; c.w[FlightRecord::W_EXC + 4] = 0x1000;
    TEST_ASSERT_NOT_EQUAL(a.dedupKey(), b.dedupKey());
    TEST_ASSERT_EQUAL_HEX32(a.dedupKey(), c.dedupKey());
    c.w[FlightRecord::W_EXC + 2] = 0x40202020;         // an epc1 wins over the caller
    TEST_ASSERT_NOT_EQUAL(a.dedupKey(), c.dedupKey());
    // ESP32: no callback, so a panic and a task watchdog differ only by the reset reason
    TEST_ASSERT_NOT_EQUAL(a.dedupKey(4), a.dedupKey(6));
}

void test_format_names_the_death() {
    rec().begin();
    rec().setPhase(2);
    rec().recordCrash(abortInfo());
    reboot(P::ResetReason::Software);
    rec().begin();
    char buf[320];
    size_t n = rec().format(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0 && n < sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "crash callback"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "reset Software reset"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "phase 2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "reason 254"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "1024 B from 0x40201287"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "stack 00000011 00000022"));
}

void test_format_prints_the_ring_oldest_first() {
    rec().begin();
    P::setFreeHeapForTest(40960); P::setLargestFreeBlockForTest(20480);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS); rec().tick();   // 11 s
    P::setFreeHeapForTest(8192);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS); rec().tick();   // 21 s
    reboot(P::ResetReason::Watchdog);
    rec().begin();
    char buf[400];
    rec().format(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ring: 11s:40.0k/20.0k 21s:8.0k/20.0k"));
}

void test_format_says_the_ring_is_empty_when_nothing_ticked() {
    rec().begin();
    reboot(P::ResetReason::Watchdog);
    rec().begin();
    char buf[400];
    rec().format(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ring: (empty)"));
}

void test_format_without_a_death_says_so_and_fits_a_small_buffer() {
    rec().begin();
    char buf[16];
    rec().format(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[15]);
    TEST_ASSERT_EQUAL_STRING_LEN("Last death: no", buf, 14);
}

void test_crc32_matches_zlib_on_a_known_vector() {
    // "123456789\0\0\0" as three little-endian words; reference from zlib.crc32
    const uint32_t v[3] = { 0x34333231u, 0x38373635u, 0x00000039u };
    TEST_ASSERT_EQUAL_HEX32(0x77D55834u, FlightRecord::crc32(v, 3));
}

// ---- the failed-allocation group (OBS-4) -----------------------------------

void test_count_word_encodes_with_its_complement_and_saturates() {
    TEST_ASSERT_EQUAL_HEX32(0x0003FFFCu, FlightRecord::encodeCount(3));
    TEST_ASSERT_EQUAL_HEX32(0xFFFF0000u, FlightRecord::encodeCount(0x12345));
    FlightRecord r{};
    r.w[FlightRecord::W_FAIL] = 0x00030000u;              // halves are not complements: no group
    TEST_ASSERT_FALSE(r.failGroupValid());
    TEST_ASSERT_EQUAL_UINT32(0, r.failAllocCount());
    r.w[FlightRecord::W_FAIL] = FlightRecord::encodeCount(3);
    TEST_ASSERT_EQUAL_UINT32(3, r.failAllocCount());
}

void test_a_failure_with_no_tick_then_a_boot_finds_the_group_in_rtc() {
    rec().begin();
    P::setFreeHeapForTest(5000); P::setLargestFreeBlockForTest(2000);
    P::setMillisForTest(401200);
    rec().noteFailedAlloc(4096, 0x1800);              // the terminal failure, a panic microseconds later
    reboot(P::ResetReason::Panic);
    rec().begin();
    TEST_ASSERT_TRUE(rec().hasPromotedRecord());
    const FlightRecord& p = rec().promoted();
    TEST_ASSERT_EQUAL_UINT32(1, p.failAllocCount());
    TEST_ASSERT_EQUAL_UINT32(4096, p.failAllocSize());
    TEST_ASSERT_EQUAL_HEX32(0x1800, p.failAllocSite());
    TEST_ASSERT_EQUAL_UINT32(401200, p.failAllocUptimeMs());
    TEST_ASSERT_EQUAL_UINT32(5000 - 5000 % 16, p.failAllocFreeBytes());
    TEST_ASSERT_EQUAL_UINT32(2000 - 2000 % 16, p.failAllocLargestBytes());
}

void test_the_flush_never_writes_the_group() {
    // A heap-hook store that RAM does not have yet (on ESP32 the hook lands
    // between the tick's read of the group and its write): the flush must
    // leave RTC's group alone rather than copy its own over it.
    rec().begin();
    P::rtcStoreWord(FlightRecord::W_FAIL + 1, 512);
    P::rtcStoreWord(FlightRecord::W_FAIL, FlightRecord::encodeCount(1));
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    FlightRecord r = readRtc();
    TEST_ASSERT_EQUAL_UINT32(1, r.failAllocCount());  // red on a flush that copies w56-60 from RAM
    TEST_ASSERT_EQUAL_UINT32(512, r.failAllocSize());
    TEST_ASSERT_EQUAL_UINT32(0, rec().failedAllocCount());
    TEST_ASSERT_EQUAL_HEX32(r.bodyCrc(), r.w[FlightRecord::W_CRC]);
}

void test_a_failure_after_a_flush_does_not_tear_the_record() {
    rec().begin();
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();                                     // crc written
    rec().noteFailedAlloc(64, 1);                     // the group changes in RTC after the crc
    FlightRecord r = readRtc();
    TEST_ASSERT_EQUAL_UINT32(1, r.failAllocCount());
    TEST_ASSERT_EQUAL_HEX32(r.bodyCrc(), r.w[FlightRecord::W_CRC]);   // red with w56-60 inside the crc
    reboot(P::ResetReason::Watchdog);
    rec().begin();
    TEST_ASSERT_FALSE(rec().promotedIsTorn());
}

void test_failures_count_and_keep_the_last() {
    rec().begin();
    for (uint32_t i = 1; i <= 5; ++i) rec().noteFailedAlloc(100 * i, i);
    TEST_ASSERT_EQUAL_UINT32(5, rec().failedAllocCount());
    TEST_ASSERT_EQUAL_UINT32(500, rec().lastFailedAllocSize());
    TEST_ASSERT_EQUAL_UINT32(5, readRtc().failAllocCount());
    TEST_ASSERT_EQUAL_HEX32(5, readRtc().failAllocSite());
}

void test_burst_and_cliff_in_one_interval_walk_once() {
    rec().begin();
    P::setFreeHeapForTest(60000);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    unsigned after_tick = P::largestFreeBlockReadsForTest;
    P::setFreeHeapForTest(60000 - P::heapCliffThresholdBytes() - 16);
    P::advanceMillisForTest(1); rec().tick();         // the cliff: one walk
    for (int i = 0; i < 5; ++i) rec().noteFailedAlloc(256, 1);   // a burst in the same interval: none
    TEST_ASSERT_EQUAL_UINT(after_tick + 1, P::largestFreeBlockReadsForTest);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();                                     // next interval: the tick's own walk
    after_tick = P::largestFreeBlockReadsForTest;
    for (int i = 0; i < 5; ++i) rec().noteFailedAlloc(256, 1);   // failures first: one walk
    P::setFreeHeapForTest(100);
    P::advanceMillisForTest(1); rec().tick();         // then the cliff: shares it
    TEST_ASSERT_EQUAL_UINT(after_tick + 1, P::largestFreeBlockReadsForTest);
}

void test_group_stays_in_ram_while_a_death_is_held_and_lands_at_acknowledge() {
    rec().begin();
    rec().recordCrash(abortInfo());
    reboot(P::ResetReason::Software);
    rec().begin(true);                                // System's hold
    rec().noteFailedAlloc(100, 7);                    // a failure during bring-up
    TEST_ASSERT_EQUAL_UINT32(0, readRtc().failAllocCount());   // the held death keeps its own (empty) group
    TEST_ASSERT_EQUAL_UINT32(0, rec().promoted().failAllocCount());
    TEST_ASSERT_EQUAL_UINT32(1, rec().failedAllocCount());
    rec().acknowledge();
    TEST_ASSERT_EQUAL_UINT32(1, readRtc().failAllocCount());
    TEST_ASSERT_EQUAL_UINT32(1, readRtc().bootSequence());
}

void test_a_layout_one_record_is_promoted_and_rewritten_as_layout_two() {
    rec().begin();
    rec().setPhase(4);
    rec().recordCrash(abortInfo());
    FlightRecord r = readRtc();                       // what a Lot B build would have left
    r.w[FlightRecord::W_META] = (r.w[FlightRecord::W_META] & 0x00FFFFFFu) | (FlightRecord::LAYOUT_V1 << 24);
    r.w[FlightRecord::W_CRC] = r.bodyCrcForLayout(FlightRecord::LAYOUT_V1);
    P::rtcWrite(0, r.w, FlightRecord::WORDS);
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_TRUE(rec().hasPromotedRecord());
    TEST_ASSERT_FALSE(rec().promotedIsTorn());
    TEST_ASSERT_EQUAL_UINT32(4, rec().promoted().phase());
    TEST_ASSERT_EQUAL_HEX32(0x40201287u, rec().promoted().failCaller());
    rec().acknowledge();
    TEST_ASSERT_EQUAL_UINT32(FlightRecord::LAYOUT, readRtc().layout());
    TEST_ASSERT_EQUAL_UINT32(1, readRtc().bootSequence());
}

void test_format_prints_the_failed_alloc_line_before_the_ring() {
    rec().begin();
    P::setFreeHeapForTest(5312); P::setLargestFreeBlockForTest(2048);
    P::setMillisForTest(401200);
    rec().noteFailedAlloc(4096, 0x1800);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    reboot(P::ResetReason::Watchdog);
    rec().begin();
    char buf[1024];
    rec().format(buf, sizeof(buf));
    const char* line = strstr(buf, "  failed allocs: 1 | last 4096 B site 0x00001800 at 401.200 s | free 5312 B, largest 2048 B\n");
    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT_TRUE(line < strstr(buf, "ring:"));
}

void test_format_saturated_stays_under_128_per_line_and_1024_in_all() {
    rec().begin();
    FlightRecord r = readRtc();
    for (size_t i = FlightRecord::W_SEQ; i < FlightRecord::WORDS; ++i) r.w[i] = 0xFFFFFFFFu;
    r.w[FlightRecord::W_PHASE] = FlightRecord::encodePhase(0xFFFF);
    r.w[FlightRecord::W_FAIL] = FlightRecord::encodeCount(0xFFFF);
    r.w[FlightRecord::W_META] |= FlightRecord::CALLBACK_RAN;
    r.w[FlightRecord::W_CRC] = r.bodyCrc();
    P::rtcWrite(0, r.w, FlightRecord::WORDS);
    reboot(P::ResetReason::Software);
    rec().begin();
    char buf[2048];
    size_t n = rec().format(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n < 1024);                       // Core::begin()'s buffer
    size_t lineLen = 0;
    for (size_t i = 0; i < n; ++i) {
        if (buf[i] == '\n') { TEST_ASSERT_TRUE(lineLen < 128); lineLen = 0; } else ++lineLen;
    }
    TEST_ASSERT_NOT_NULL(strstr(buf, "failed allocs: 65535"));
}

static uint32_t g_hookSize = 0, g_hookCountSeen = 0;
static void userFailedAllocHook(uint32_t size, uint32_t) { g_hookSize = size; g_hookCountSeen = rec().failedAllocCount(); }

void test_user_failed_alloc_hook_runs_after_the_record() {
    rec().begin();
    rec().onFailedAlloc(&userFailedAllocHook);
    rec().noteFailedAlloc(2048, 3);
    TEST_ASSERT_EQUAL_UINT32(2048, g_hookSize);
    TEST_ASSERT_EQUAL_UINT32(1, g_hookCountSeen);
}

void test_failed_alloc_before_begin_does_nothing() {
    rec().noteFailedAlloc(1, 1);
    TEST_ASSERT_EQUAL_HEX32(0, P::stubRtcWordsForTest[FlightRecord::W_FAIL]);
}

// ---- the ESP8266 latch-and-clear (OBS-4, S3) --------------------------------

void test_one_survived_failure_counts_once_across_many_samples() {
    rec().begin();
    P::setLastFailedAllocForTest(0x4020b41cu, 1048576);
    for (int i = 0; i < 20; ++i) { P::advanceMillisForTest(1); rec().tick(); }
    TEST_ASSERT_EQUAL_UINT32(1, rec().failedAllocCount());     // 20 without the clear
    TEST_ASSERT_EQUAL_HEX32(0x4020b41cu, readRtc().failAllocSite());
    TEST_ASSERT_EQUAL_UINT32(1048576, readRtc().failAllocSize());
}

void test_two_failures_with_a_sample_between_count_two() {
    rec().begin();
    P::setLastFailedAllocForTest(0x4020b41cu, 512);
    P::advanceMillisForTest(1); rec().tick();
    P::setLastFailedAllocForTest(0x4020b41cu, 512);            // the same failure again: a leak's shape
    P::advanceMillisForTest(1); rec().tick();
    TEST_ASSERT_EQUAL_UINT32(2, rec().failedAllocCount());
}

void test_a_latched_failure_is_cleared_for_the_crash_callback() {
    rec().begin();
    P::setLastFailedAllocForTest(0x4020b41cu, 512);
    P::advanceMillisForTest(1); rec().tick();                  // latched and cleared
    uint32_t a = 1, s = 1;
    TEST_ASSERT_FALSE(P::takeLastFailedAlloc(a, s));            // what the callback would now read: nothing
    rec().recordCrash(abortInfo(0, 0));                         // an unrelated death later
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_EQUAL_UINT32(0, rec().promoted().failCaller());       // not the stale survived failure
    TEST_ASSERT_EQUAL_UINT32(1, rec().promoted().failAllocCount());   // which is in the group instead
    TEST_ASSERT_EQUAL_HEX32(0x4020b41cu, rec().promoted().failAllocSite());
}

void test_a_latch_with_no_tick_then_a_boot_finds_the_group() {
    rec().begin();
    P::setLastFailedAllocForTest(0x4020b41cu, 256);
    P::advanceMillisForTest(1); rec().tick();                  // a sample, no 10 s flush
    reboot(P::ResetReason::Watchdog);                          // hardware watchdog: no code ran
    rec().begin();
    TEST_ASSERT_EQUAL_UINT32(1, rec().promoted().failAllocCount());
    TEST_ASSERT_EQUAL_UINT32(256, rec().promoted().failAllocSize());
}

void test_a_fatal_new_reaches_the_callback_fields_not_the_group() {
    rec().begin();
    rec().recordCrash(abortInfo(0x40201287u, 1024));           // globals set, no sample in between
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_EQUAL_HEX32(0x40201287u, rec().promoted().failCaller());
    TEST_ASSERT_EQUAL_UINT32(0, rec().promoted().failAllocCount());
}

// ---- the code review's four gaps (OBS-4) -----------------------------------

void test_the_count_saturates_and_the_group_stays_valid() {
    rec().begin();
    for (uint32_t i = 0; i < 0x10001u; ++i) rec().noteFailedAlloc(16, 1);
    TEST_ASSERT_EQUAL_UINT32(0xFFFF, rec().failedAllocCount());
    TEST_ASSERT_TRUE(readRtc().failGroupValid());
    TEST_ASSERT_EQUAL_HEX32(0xFFFF0000u, readRtc().w[FlightRecord::W_FAIL]);
}

void test_the_count_word_is_stored_last() {
    rec().begin();
    rec().noteFailedAlloc(64, 1);
    TEST_ASSERT_EQUAL_UINT32(FlightRecord::W_FAIL, P::lastRtcStoreOffsetForTest);   // a death mid-group leaves the old count, never a validated count over new fields
}

static void formatWithPlatform(uint8_t platform, uint32_t site, char* buf, size_t len) {
    rec().begin();
    P::setMillisForTest(12000);
    rec().noteFailedAlloc(4096, site);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    FlightRecord r = readRtc();
    r.w[FlightRecord::W_META] = (r.w[FlightRecord::W_META] & ~0x00FF0000u) | (static_cast<uint32_t>(platform) << 16);   // outside the crc
    P::rtcWrite(0, r.w, FlightRecord::WORDS);
    reboot(P::ResetReason::Watchdog);
    rec().begin();
    rec().format(buf, len);
}

void test_format_labels_the_site_as_a_caller_on_esp8266_and_as_caps_on_esp32() {
    char buf[1024];
    formatWithPlatform(1, 0x4020b41cu, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "last 4096 B from 0x4020b41c at 12.000 s"));
    rec().resetForTest(); P::clearRtcForTest(); P::setResetReasonForTest(P::ResetReason::PowerOn); P::setMillisForTest(1000);
    formatWithPlatform(2, 0x1800u, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "last 4096 B caps 0x1800 at 12.000 s"));
}

void test_a_survived_failure_that_reaches_the_callback_with_an_exception_goes_to_the_group() {
    rec().begin();
    CrashInfo c; c.reason = 2; c.exccause = 29; c.epc1 = 0x40201000u;
    c.failCaller = 0x4020b864u; c.failSize = 1048576;                 // survived, not yet latched when the exception hit
    rec().recordCrash(c);
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_EQUAL_UINT32(0, rec().promoted().failCaller());       // not the death's cause
    TEST_ASSERT_EQUAL_UINT32(0, rec().promoted().failSize());
    TEST_ASSERT_EQUAL_UINT32(1, rec().promoted().failAllocCount());
    TEST_ASSERT_EQUAL_HEX32(0x4020b864u, rec().promoted().failAllocSite());
    TEST_ASSERT_EQUAL_HEX32(0x40201000u, rec().promoted().epc1());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_boot_starts_a_fresh_record_in_rtc);
    RUN_TEST(test_owned_software_reset_is_not_promoted_and_bumps_the_sequence);
    RUN_TEST(test_garbage_in_rtc_reads_as_power_on);
    RUN_TEST(test_a_restart_before_begin_still_marks_the_record_as_ours);
    RUN_TEST(test_dedup_key_uses_the_phase_when_there_is_no_site);
    RUN_TEST(test_format_with_a_tiny_buffer_never_reports_more_than_it_wrote);
    RUN_TEST(test_tick_and_phase_before_begin_do_nothing);
    RUN_TEST(test_crash_callback_promotes_a_software_reset);
    RUN_TEST(test_unowned_software_reset_is_promoted);
    RUN_TEST(test_unexpected_reset_without_callback_is_promoted_with_its_phase);
    RUN_TEST(test_power_on_after_a_valid_record_is_not_promoted);
    RUN_TEST(test_torn_record_still_reports_its_phase);
    RUN_TEST(test_phase_moving_after_a_flush_does_not_tear_the_record);
    RUN_TEST(test_promoted_record_stays_in_rtc_until_acknowledged);
    RUN_TEST(test_dying_again_before_acknowledgement_keeps_the_first_death);
    RUN_TEST(test_a_crash_during_bring_up_with_nothing_promoted_is_recorded);
    RUN_TEST(test_the_first_begin_decides_the_hold);
    RUN_TEST(test_a_record_with_another_layout_reads_as_power_on);
    RUN_TEST(test_a_failed_hook_registration_is_reported_not_claimed);
    RUN_TEST(test_event_drops_saturate_at_sixteen_bits);
    RUN_TEST(test_format_prints_a_wrapped_ring_oldest_first);
    RUN_TEST(test_acknowledge_without_promotion_is_harmless);
    RUN_TEST(test_running_minimum_survives_between_ticks);
    RUN_TEST(test_cliff_earns_exactly_one_extra_walk_per_interval);
    RUN_TEST(test_free_heap_is_read_at_most_once_per_millisecond);
    RUN_TEST(test_no_walk_between_ticks_without_a_cliff);
    RUN_TEST(test_fast_ring_wraps_after_sixteen_samples);
    RUN_TEST(test_slow_ring_samples_every_ten_minutes);
    RUN_TEST(test_tick_writes_body_then_crc_and_the_record_verifies);
    RUN_TEST(test_phase_marker_is_one_store_with_its_complement);
    RUN_TEST(test_event_drops_ride_the_sequence_word);
    RUN_TEST(test_crash_folds_the_running_minimum_into_the_record);
    RUN_TEST(test_platform_restart_marks_the_record_as_ours);
    RUN_TEST(test_user_crash_hook_runs_after_the_record);
    RUN_TEST(test_dedup_key_uses_the_fail_caller_when_there_is_no_epc1);
    RUN_TEST(test_format_names_the_death);
    RUN_TEST(test_format_prints_the_ring_oldest_first);
    RUN_TEST(test_format_says_the_ring_is_empty_when_nothing_ticked);
    RUN_TEST(test_format_without_a_death_says_so_and_fits_a_small_buffer);
    RUN_TEST(test_crc32_matches_zlib_on_a_known_vector);
    RUN_TEST(test_count_word_encodes_with_its_complement_and_saturates);
    RUN_TEST(test_a_failure_with_no_tick_then_a_boot_finds_the_group_in_rtc);
    RUN_TEST(test_the_flush_never_writes_the_group);
    RUN_TEST(test_a_failure_after_a_flush_does_not_tear_the_record);
    RUN_TEST(test_failures_count_and_keep_the_last);
    RUN_TEST(test_burst_and_cliff_in_one_interval_walk_once);
    RUN_TEST(test_group_stays_in_ram_while_a_death_is_held_and_lands_at_acknowledge);
    RUN_TEST(test_a_layout_one_record_is_promoted_and_rewritten_as_layout_two);
    RUN_TEST(test_format_prints_the_failed_alloc_line_before_the_ring);
    RUN_TEST(test_format_saturated_stays_under_128_per_line_and_1024_in_all);
    RUN_TEST(test_user_failed_alloc_hook_runs_after_the_record);
    RUN_TEST(test_failed_alloc_before_begin_does_nothing);
    RUN_TEST(test_one_survived_failure_counts_once_across_many_samples);
    RUN_TEST(test_two_failures_with_a_sample_between_count_two);
    RUN_TEST(test_a_latched_failure_is_cleared_for_the_crash_callback);
    RUN_TEST(test_a_latch_with_no_tick_then_a_boot_finds_the_group);
    RUN_TEST(test_a_fatal_new_reaches_the_callback_fields_not_the_group);
    RUN_TEST(test_the_count_saturates_and_the_group_stays_valid);
    RUN_TEST(test_the_count_word_is_stored_last);
    RUN_TEST(test_format_labels_the_site_as_a_caller_on_esp8266_and_as_caps_on_esp32);
    RUN_TEST(test_a_survived_failure_that_reaches_the_callback_with_an_exception_goes_to_the_group);
    return UNITY_END();
}
