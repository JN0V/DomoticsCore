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
    P::setFreeHeapForTest(20000); rec().tick();       // a cliff, seen by no tick
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
    rec().tick(); rec().tick(); rec().tick();         // the cliff, three loops
    TEST_ASSERT_EQUAL_UINT(after_first + 1, P::largestFreeBlockReadsForTest);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();                                     // tick: one more walk
    TEST_ASSERT_EQUAL_UINT(after_first + 2, P::largestFreeBlockReadsForTest);
}

void test_no_walk_between_ticks_without_a_cliff() {
    rec().begin();
    P::setFreeHeapForTest(60000);
    P::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    rec().tick();
    unsigned after_first = P::largestFreeBlockReadsForTest;
    P::setFreeHeapForTest(60000 - 512);
    for (int i = 0; i < 100; ++i) rec().tick();
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
    P::setFreeHeapForTest(3000); rec().tick();        // burst inside the interval
    P::setFreeHeapForTest(2000);
    rec().recordCrash(abortInfo());
    reboot(P::ResetReason::Software);
    rec().begin();
    TEST_ASSERT_EQUAL_UINT32(2000 - 2000 % 16, rec().promoted().minFreeBytes());
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
    TEST_ASSERT_NOT_NULL(strstr(buf, "phase 2"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "reason 254"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "1024 B from 0x40201287"));
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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_boot_starts_a_fresh_record_in_rtc);
    RUN_TEST(test_owned_software_reset_is_not_promoted_and_bumps_the_sequence);
    RUN_TEST(test_garbage_in_rtc_reads_as_power_on);
    RUN_TEST(test_tick_and_phase_before_begin_do_nothing);
    RUN_TEST(test_crash_callback_promotes_a_software_reset);
    RUN_TEST(test_unowned_software_reset_is_promoted);
    RUN_TEST(test_unexpected_reset_without_callback_is_promoted_with_its_phase);
    RUN_TEST(test_power_on_after_a_valid_record_is_not_promoted);
    RUN_TEST(test_torn_record_still_reports_its_phase);
    RUN_TEST(test_promoted_record_stays_in_rtc_until_acknowledged);
    RUN_TEST(test_dying_again_before_acknowledgement_keeps_the_first_death);
    RUN_TEST(test_acknowledge_without_promotion_is_harmless);
    RUN_TEST(test_running_minimum_survives_between_ticks);
    RUN_TEST(test_cliff_earns_exactly_one_extra_walk_per_interval);
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
    RUN_TEST(test_format_without_a_death_says_so_and_fits_a_small_buffer);
    RUN_TEST(test_crc32_matches_zlib_on_a_known_vector);
    return UNITY_END();
}
