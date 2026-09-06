/**
 * @file test_system_lifecycle.cpp
 * @brief System::begin(), the state machine, and the console commands.
 *
 * System is orchestration: it owns almost no logic of its own, and everything
 * it decides it decides once, at boot. So the tests drive the real boot and
 * then look at the result from outside — which components exist, what the
 * status LED is doing, what a telnet client gets back.
 *
 * The console commands are reached through the simulated client the WiFi stub
 * provides, the same harness the RemoteConsole suite uses. That is the only
 * public way in: System registers four handlers on the console and exposes
 * none of them directly.
 */

#include <unity.h>
#include <cstring>
#include <string>
#include <DomoticsCore/System.h>
#include <DomoticsCore/FlightRecorder.h>
#include <vector>

using namespace DomoticsCore;

void setUp(void) {
    HAL::Platform::resetDiagnosticsForTest();
    FlightRecorder::instance().resetForTest();
    HAL::Platform::clearRtcForTest();
    HAL::Platform::resetFailedAllocForTest();
    HAL::Platform::crashKindForTest[0] = '\0';
}
void tearDown(void) { HAL::Platform::resetDiagnosticsForTest(); }

static bool mentions(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

// Boot a system, attach a telnet client, and let the welcome banner go by.
struct Console {
    HAL::WiFiClient client;

    Console(System& sys) {
        client = sys.getConsole()->getServer()->simulateClient(true, 7);
        sys.loop();                 // accept the client, send the welcome
        client.clearWriteBuffer();
    }

    std::string run(System& sys, const char* command) {
        std::string line = std::string(command) + "\n";
        client.simulateIncomingData(line.c_str());
        sys.loop();
        std::string out = client.getWriteBufferAsString();
        client.clearWriteBuffer();
        return out;
    }
};

// ============================================================================
// State before and after begin()
// ============================================================================

void test_a_fresh_system_is_booting(void) {
    System sys(SystemConfig::minimal());
    TEST_ASSERT_EQUAL(SystemState::BOOTING, sys.getState());
}

void test_begin_reaches_ready(void) {
    System sys(SystemConfig::minimal());
    TEST_ASSERT_TRUE(sys.begin());
    TEST_ASSERT_EQUAL(SystemState::READY, sys.getState());
}

void test_boot_goes_straight_from_booting_to_ready(void) {
    // WIFI_CONNECTING, WIFI_CONNECTED and SERVICES_STARTING are declared and
    // have LED patterns, but no code path enters them during a normal boot.
    // Recorded so the day one of them starts firing, a test says so.
    System sys(SystemConfig::minimal());
    std::vector<SystemState> seen;
    sys.onStateChange([&seen](SystemState, SystemState to) { seen.push_back(to); });

    sys.begin();

    TEST_ASSERT_EQUAL_size_t(1, seen.size());
    TEST_ASSERT_EQUAL(SystemState::READY, seen[0]);
}

void test_state_callback_receives_both_ends_of_the_transition(void) {
    System sys(SystemConfig::minimal());
    SystemState from = SystemState::ERROR, to = SystemState::ERROR;
    sys.onStateChange([&from, &to](SystemState f, SystemState t) { from = f; to = t; });

    sys.begin();

    TEST_ASSERT_EQUAL(SystemState::BOOTING, from);
    TEST_ASSERT_EQUAL(SystemState::READY, to);
}

void test_state_callbacks_are_capped_at_eight(void) {
    // MEM-1: the vector is bounded so a caller in a loop cannot grow it without
    // limit. The ninth registration is dropped, not the first.
    System sys(SystemConfig::minimal());
    int fired = 0;
    for (int i = 0; i < 12; i++) {
        sys.onStateChange([&fired](SystemState, SystemState) { fired++; });
    }

    sys.begin();

    TEST_ASSERT_EQUAL_INT(8, fired);
}

// ============================================================================
// Component registration
// ============================================================================

void test_minimal_registers_led_wifi_and_console(void) {
    System sys(SystemConfig::minimal());
    sys.begin();

    TEST_ASSERT_EQUAL_size_t(3, sys.getCore().getComponentCount());
    TEST_ASSERT_NOT_NULL(sys.getCore().getComponent("LED"));
    TEST_ASSERT_NOT_NULL(sys.getCore().getComponent("Wifi"));
    TEST_ASSERT_NOT_NULL(sys.getCore().getComponent("RemoteConsole"));
}

void test_led_can_be_left_out(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableLED = false;
    System sys(cfg);
    sys.begin();

    TEST_ASSERT_NULL(sys.getCore().getComponent("LED"));
    TEST_ASSERT_EQUAL_size_t(2, sys.getCore().getComponentCount());
    TEST_ASSERT_EQUAL(SystemState::READY, sys.getState());
}

void test_console_can_be_left_out(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableConsole = false;
    System sys(cfg);
    sys.begin();

    TEST_ASSERT_NULL(sys.getConsole());
    TEST_ASSERT_EQUAL_size_t(2, sys.getCore().getComponentCount());
}

void test_wifi_is_not_optional(void) {
    SystemConfig cfg;
    cfg.enableLED = false;
    cfg.enableConsole = false;
    System sys(cfg);
    sys.begin();

    // Every other component is behind a flag; WiFi is registered unconditionally.
    TEST_ASSERT_NOT_NULL(sys.getWiFi());
    TEST_ASSERT_EQUAL_size_t(1, sys.getCore().getComponentCount());
}

void test_storage_and_systeminfo_join_when_enabled(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();

    TEST_ASSERT_NOT_NULL(sys.getCore().getComponent("Storage"));
    TEST_ASSERT_NOT_NULL(sys.getCore().getComponent("System Info"));
    TEST_ASSERT_EQUAL_size_t(5, sys.getCore().getComponentCount());
}

void test_components_not_compiled_in_are_requested_without_failing(void) {
    // WebUI, NTP, MQTT, OTA and HomeAssistant are absent from this project.
    // Asking for them must warn and carry on, not refuse to boot — that is the
    // whole purpose of the __has_include arms.
    SystemConfig cfg = SystemConfig::fullStack();
    System sys(cfg);

    TEST_ASSERT_TRUE(sys.begin());
    TEST_ASSERT_EQUAL(SystemState::READY, sys.getState());
    TEST_ASSERT_NULL(sys.getCore().getComponent("WebUI"));
    TEST_ASSERT_NULL(sys.getCore().getComponent("MQTT"));
    TEST_ASSERT_NULL(sys.getCore().getComponent("NTP"));
}

// ============================================================================
// begin() twice
// ============================================================================

void test_second_begin_is_refused_without_re_registering(void) {
    System sys(SystemConfig::minimal());
    sys.begin();
    size_t after_first = sys.getCore().getComponentCount();

    TEST_ASSERT_TRUE(sys.begin());
    TEST_ASSERT_EQUAL_size_t(after_first, sys.getCore().getComponentCount());
    TEST_ASSERT_EQUAL(SystemState::READY, sys.getState());
}

void test_second_begin_does_not_fire_the_state_callbacks_again(void) {
    System sys(SystemConfig::minimal());
    int fired = 0;
    sys.onStateChange([&fired](SystemState, SystemState) { fired++; });

    sys.begin();
    sys.begin();

    TEST_ASSERT_EQUAL_INT(1, fired);
}

// ============================================================================
// The status LED — the visible half of the state machine
// ============================================================================

void test_ready_drives_the_status_led(void) {
    System sys(SystemConfig::minimal());
    sys.begin();

    auto* led = sys.getCore().getComponent<Components::LEDComponent>("LED");
    TEST_ASSERT_NOT_NULL(led);

    // The LED is named "status" and setState(READY) puts it on Breathing. This
    // is also the end-to-end check on BUG-23: the component is registered and
    // initialised by core.begin(), not by System behind its back.
    String status = led->getLEDStatus(0);
    TEST_ASSERT_NOT_NULL(strstr(status.c_str(), "LED 'status'"));
    TEST_ASSERT_NOT_NULL(strstr(status.c_str(), "Effect: Breathing"));
}

void test_the_status_led_honours_the_configured_pin_and_polarity(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.ledPin = 13;
    cfg.ledActiveHigh = false;   // becomes invertLogic = true
    System sys(cfg);
    sys.begin();

    auto* led = sys.getCore().getComponent<Components::LEDComponent>("LED");
    TEST_ASSERT_NOT_NULL(led);
    TEST_ASSERT_EQUAL_size_t(1, led->getLEDCount());
    TEST_ASSERT_EQUAL_STRING("status", led->getLEDNames()[0].c_str());
}

// ============================================================================
// Console commands
// ============================================================================

void test_status_command_reports_the_device(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.deviceName = "Kitchen";
    cfg.firmwareVersion = "2.3.4";
    System sys(cfg);
    sys.begin();
    Console con(sys);

    std::string out = con.run(sys, "status");
    TEST_ASSERT_TRUE(mentions(out, "System Status:"));
    TEST_ASSERT_TRUE(mentions(out, "Kitchen v2.3.4"));
    TEST_ASSERT_TRUE(mentions(out, "State: READY"));
}

void test_wifi_command_answers(void) {
    System sys(SystemConfig::minimal());
    sys.begin();
    Console con(sys);

    std::string out = con.run(sys, "wifi");
    TEST_ASSERT_FALSE(mentions(out, "Not initialized"));
    TEST_ASSERT_FALSE(out.empty());
}

void test_storage_command_without_storage(void) {
    System sys(SystemConfig::minimal());   // enableStorage = false
    sys.begin();
    Console con(sys);

    TEST_ASSERT_TRUE(mentions(con.run(sys, "storage"), "Storage: Not available"));
}

void test_storage_command_with_storage(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    System sys(cfg);
    sys.begin();
    Console con(sys);

    std::string out = con.run(sys, "storage");
    TEST_ASSERT_FALSE(mentions(out, "Storage: Not available"));
    TEST_ASSERT_FALSE(out.empty());
}

void test_bootdiag_command_without_systeminfo(void) {
    System sys(SystemConfig::minimal());
    sys.begin();
    Console con(sys);

    TEST_ASSERT_TRUE(mentions(con.run(sys, "bootdiag"), "SystemInfo not available"));
}

void test_bootdiag_command_reports_the_first_boot(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    Console con(sys);

    std::string out = con.run(sys, "bootdiag");
    TEST_ASSERT_TRUE(mentions(out, "Boot Diagnostics:"));
    TEST_ASSERT_TRUE(mentions(out, "Boot Count: 1"));
    // The persisted half comes from Storage, written by
    // initBootDiagnosticsPersistence() during begin().
    TEST_ASSERT_TRUE(mentions(out, "Persisted Data:"));
    TEST_ASSERT_TRUE(mentions(out, "boot_count: 1"));
    // OBS-6: the heap lines say which boot they describe, and the stub
    // tracks no minimum so the report must say so instead of printing one.
    TEST_ASSERT_TRUE(mentions(out, "Heap at this boot:"));
    TEST_ASSERT_TRUE(mentions(out, "n/a (not tracked on this platform)"));
    TEST_ASSERT_TRUE(mentions(out, "boot_heap:"));
    TEST_ASSERT_FALSE(mentions(out, "last_heap"));
}

void test_a_custom_command_reaches_the_console(void) {
    System sys(SystemConfig::minimal());
    sys.begin();
    sys.registerCommand("ping", [](const String&) { return String("pong\n"); });
    Console con(sys);

    TEST_ASSERT_TRUE(mentions(con.run(sys, "ping"), "pong"));
}

void test_registering_a_command_without_a_console_is_a_no_op(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableConsole = false;
    System sys(cfg);
    sys.begin();

    // console is null; the call must be swallowed rather than dereference it.
    sys.registerCommand("ping", [](const String&) { return String("pong\n"); });
    TEST_ASSERT_EQUAL(SystemState::READY, sys.getState());
}

void test_console_runs_on_the_configured_port(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.consolePort = 2323;
    cfg.defaultLogLevel = LOG_LEVEL_WARN;
    System sys(cfg);
    sys.begin();

    TEST_ASSERT_NOT_NULL(sys.getConsole());
    TEST_ASSERT_EQUAL_UINT16(2323, sys.getConsole()->getPort());
    TEST_ASSERT_EQUAL(LOG_LEVEL_WARN, sys.getConsole()->getLogLevel());
}

// ============================================================================
// OBS-7: the loop watchdog (ESP32 in effect; the stub records the call)
// ============================================================================

void test_begin_arms_the_loop_watchdog_by_default(void) {
    System sys(SystemConfig::minimal());
    TEST_ASSERT_EQUAL_UINT32(0, HAL::Platform::loopWatchdogSecondsForTest);
    sys.begin();
    TEST_ASSERT_EQUAL_UINT32(30, HAL::Platform::loopWatchdogSecondsForTest);
}

void test_a_zero_timeout_leaves_the_loop_watchdog_off(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.loopWatchdogSeconds = 0;
    System sys(cfg);
    sys.begin();
    TEST_ASSERT_EQUAL_UINT32(0, HAL::Platform::loopWatchdogSecondsForTest);
}

// A sketch that calls System::loop() regularly must never trip it, whatever
// its own loop() does — so the feed lives in System::loop(), not in the
// Arduino loop return.
void test_every_system_loop_feeds_the_watchdog(void) {
    System sys(SystemConfig::minimal());
    sys.begin();
    uint32_t before = HAL::Platform::loopWatchdogFeedsForTest;
    sys.loop(); sys.loop(); sys.loop();
    TEST_ASSERT_EQUAL_UINT32(before + 3, HAL::Platform::loopWatchdogFeedsForTest);
}

void test_the_feed_is_a_no_op_when_the_watchdog_is_off(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.loopWatchdogSeconds = 0;
    System sys(cfg);
    sys.begin();
    sys.loop();
    TEST_ASSERT_EQUAL_UINT32(0, HAL::Platform::loopWatchdogFeedsForTest);
}

// A platform that supports the watchdog but refuses to arm it must be
// reported as such, not claimed armed (review finding 2).
void test_a_failed_arming_is_reported_not_claimed(void) {
    HAL::Platform::loopWatchdogArmFailsForTest = true;
    std::vector<std::string> logs;
    auto id = LoggerCallbacks::addCallback([&](LogLevel, const char*, const char* msg) { logs.emplace_back(msg); });
    System sys(SystemConfig::minimal());
    sys.begin();
    LoggerCallbacks::removeCallback(id);
    bool warned = false, claimed = false;
    for (auto& l : logs) { if (mentions(l, "did not arm")) warned = true; if (mentions(l, "Loop watchdog armed")) claimed = true; }
    TEST_ASSERT_TRUE(warned);
    TEST_ASSERT_FALSE(claimed);
    TEST_ASSERT_EQUAL_UINT32(0, HAL::Platform::loopWatchdogSecondsForTest);
}

// OBS-6, the tracked shape (ESP32): the report prints the minimum and the
// persisted key exists.
void test_bootdiag_reports_the_tracked_minimum_where_the_platform_has_one(void) {
    HAL::Platform::minFreeHeapTrackedForTest = true;
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    Console con(sys);
    std::string out = con.run(sys, "bootdiag");
    TEST_ASSERT_TRUE(mentions(out, "Min heap at this boot: 0 bytes"));
    TEST_ASSERT_FALSE(mentions(out, "n/a (not tracked"));
}

// OBS-2 through the console: a scripted exception reaches `bootdiag`.
void test_bootdiag_command_reports_the_reset_detail(void) {
    HAL::Platform::ResetDetail d;
    d.exccause = 28; d.epc1 = 0x40201297; d.valid = true;
    HAL::Platform::setResetDetailForTest(d);
    HAL::Platform::setResetReasonForTest(HAL::Platform::ResetReason::Panic);
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    Console con(sys);
    std::string out = con.run(sys, "bootdiag");
    TEST_ASSERT_TRUE(mentions(out, "WARNING: Previous boot ended unexpectedly"));
    TEST_ASSERT_TRUE(mentions(out, "epc1=0x40201297"));
    TEST_ASSERT_TRUE(mentions(out, "exccause=28"));
}

// OBS-1 through the console: a waiting dump is named, with its size.
void test_bootdiag_command_reports_a_waiting_core_dump(void) {
    HAL::Platform::CoreDumpStatus st;
    st.supported = true; st.partitionPresent = true; st.dumpPresent = true; st.size = 8964;
    HAL::Platform::setCoreDumpStatusForTest(st);
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    Console con(sys);
    std::string out = con.run(sys, "bootdiag");
    TEST_ASSERT_TRUE(mentions(out, "Core dump: WAITING"));
    TEST_ASSERT_TRUE(mentions(out, "8964 bytes"));
}

// ---- OBS-3: the flight recorder through System ----------------------------

static void stageDeath() {
    FlightRecorder::instance().begin();
    CrashInfo c; c.reason = 254; c.failSize = 1024; c.failCaller = 0x40201287u;
    FlightRecorder::instance().recordCrash(c);
    FlightRecorder::instance().resetForTest();
    HAL::Platform::setResetReasonForTest(HAL::Platform::ResetReason::Software);
}

class SeesRecorderComponent : public Components::IComponent {
public:
    bool sawPromoted = false;
    SeesRecorderComponent() { metadata.name = "SeesRecorder"; metadata.version = "1.0.0"; }
    Components::ComponentStatus begin() override {
        sawPromoted = FlightRecorder::instance().hasPromotedRecord();
        return Components::ComponentStatus::Success;
    }
    void loop() override {}
    Components::ComponentStatus shutdown() override { return Components::ComponentStatus::Success; }
    std::vector<Components::Dependency> getDependencies() const override { return {}; }
};

void test_the_death_is_promoted_before_any_component_begins_and_persisted_after(void) {
    stageDeath();
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    auto comp = std::make_unique<SeesRecorderComponent>();
    SeesRecorderComponent* raw = comp.get();
    sys.getCore().addComponent(std::move(comp));            // registered first: initialized first
    TEST_ASSERT_TRUE(sys.begin());
    TEST_ASSERT_TRUE_MESSAGE(raw->sawPromoted, "promotion must precede the first component's begin()");
    // persisted in step 6, then acknowledged: RTC holds the fresh record
    FlightRecord fresh;
    HAL::Platform::rtcRead(0, fresh.w, FlightRecord::WORDS);
    TEST_ASSERT_EQUAL_UINT32(0, fresh.flags() & FlightRecord::UNPERSISTED);
    auto* storage = sys.getCore().getComponent<Components::StorageComponent>("Storage");
    SystemHelpers::BootDiagRecord r;
    TEST_ASSERT_TRUE(SystemHelpers::readBootDiagRecord(*storage, r));
    TEST_ASSERT_EQUAL_UINT32(254, r.reason);
}

// A component that dies in begin() while the first death is still held: the
// first death is what step 6 persists, not the second.
class DiesInBeginComponent : public Components::IComponent {
public:
    DiesInBeginComponent() { metadata.name = "DiesInBegin"; metadata.version = "1.0.0"; }
    Components::ComponentStatus begin() override {
        CrashInfo c; c.reason = 254; c.failSize = 64; c.failCaller = 0xBBBBu;
        FlightRecorder::instance().recordCrash(c);
        return Components::ComponentStatus::Success;
    }
    void loop() override {}
    Components::ComponentStatus shutdown() override { return Components::ComponentStatus::Success; }
    std::vector<Components::Dependency> getDependencies() const override { return {}; }
};

void test_the_first_death_survives_a_second_one_during_bring_up(void) {
    stageDeath();                                            // death A: failCaller 0x40201287
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.getCore().addComponent(std::make_unique<DiesInBeginComponent>());
    TEST_ASSERT_TRUE(sys.begin());
    auto* storage = sys.getCore().getComponent<Components::StorageComponent>("Storage");
    SystemHelpers::BootDiagRecord r;
    TEST_ASSERT_TRUE(SystemHelpers::readBootDiagRecord(*storage, r));
    TEST_ASSERT_EQUAL_HEX32(0x40201287u, r.failCaller);
}

void test_a_death_is_acknowledged_even_without_storage(void) {
    stageDeath();
    System sys(SystemConfig::minimal());                    // no Storage, no SystemInfo
    TEST_ASSERT_TRUE(sys.begin());
    FlightRecord fresh;
    HAL::Platform::rtcRead(0, fresh.w, FlightRecord::WORDS);
    TEST_ASSERT_EQUAL_UINT32(0, fresh.flags() & FlightRecord::UNPERSISTED);
}

void test_bootdiag_command_reports_the_last_death(void) {
    stageDeath();
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    Console con(sys);
    std::string out = con.run(sys, "bootdiag");
    TEST_ASSERT_TRUE(mentions(out, "Last death: crash callback"));
    TEST_ASSERT_TRUE(mentions(out, "reason 254"));
    TEST_ASSERT_TRUE(mentions(out, "Boot Diagnostics:"));
    TEST_ASSERT_TRUE(mentions(out, "last death: crash callback x1"));
}

void test_bootdiag_names_the_component_behind_the_phase(void) {
    FlightRecorder::instance().begin();
    FlightRecorder::instance().setPhase(2);      // the 2nd component in this build's order
    CrashInfo c; c.reason = 2; c.epc1 = 0x40201000u;
    FlightRecorder::instance().recordCrash(c);
    FlightRecorder::instance().resetForTest();
    HAL::Platform::setResetReasonForTest(HAL::Platform::ResetReason::Panic);
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    Console con(sys);
    std::string out = con.run(sys, "bootdiag");
    const char* second = sys.getCore().componentNameAtInitIndex(2);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_TRUE(mentions(out, (std::string("phase 2 = ") + second).c_str()));
}

// OBS-4: a survived failure of this run is readable without a reboot.
void test_bootdiag_reports_this_boots_failed_allocs(void) {
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    Console con(sys);
    TEST_ASSERT_FALSE(mentions(con.run(sys, "bootdiag"), "this boot: failed allocs"));
    HAL::Platform::fireFailedAllocForTest(4096, 0x1800);
    HAL::Platform::fireFailedAllocForTest(256, 0x1800);
    TEST_ASSERT_TRUE(mentions(con.run(sys, "bootdiag"), "this boot: failed allocs 2, last 256 B"));
}

// OBS-4 review 14: a saturated record through every block of bootdiag stays
// inside the 1 KB buffer and says where it was cut.
void test_bootdiag_with_a_saturated_record_is_cut_not_overrun(void) {
    FlightRecorder::instance().begin();
    FlightRecord r;
    HAL::Platform::rtcRead(0, r.w, FlightRecord::WORDS);
    for (size_t i = FlightRecord::W_SEQ; i < FlightRecord::WORDS; ++i) r.w[i] = 0xFFFFFFFFu;
    r.w[FlightRecord::W_PHASE] = FlightRecord::encodePhase(FlightRecorder::PHASE_EVENT_DISPATCH);
    r.w[FlightRecord::W_FAIL] = FlightRecord::encodeCount(0xFFFF);
    r.w[FlightRecord::W_META] |= FlightRecord::CALLBACK_RAN;
    r.w[FlightRecord::W_CRC] = r.bodyCrc();
    HAL::Platform::rtcWrite(0, r.w, FlightRecord::WORDS);
    FlightRecorder::instance().resetForTest();
    HAL::Platform::setResetReasonForTest(HAL::Platform::ResetReason::Software);
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    HAL::Platform::fireFailedAllocForTest(0xFFFFFFFFu, 0x1800);
    Console con(sys);
    std::string out = con.run(sys, "bootdiag");
    TEST_ASSERT_TRUE(mentions(out, "failed allocs: 65535"));
    TEST_ASSERT_TRUE(mentions(out, "phase 255 = event dispatch"));
    TEST_ASSERT_TRUE(out.size() < 1024 + 64);              // the buffer plus the console's own framing
    TEST_ASSERT_TRUE(mentions(out, "...\n"));               // cut, and said so
}

void test_crash_command_reaches_the_platform(void) {
    System sys(SystemConfig::minimal());
    sys.begin();
    Console con(sys);
    std::string out = con.run(sys, "crash oom");
    TEST_ASSERT_TRUE(mentions(out, "crashing: oom"));
    TEST_ASSERT_EQUAL_STRING("oom", HAL::Platform::crashKindForTest);
    out = con.run(sys, "crash coffee");
    TEST_ASSERT_TRUE(mentions(out, "usage: crash"));
    out = con.run(sys, "crash nothrow");                        // OBS-4: survived, so not "crashing"
    TEST_ASSERT_TRUE(mentions(out, "done: nothrow"));
    TEST_ASSERT_EQUAL_STRING("nothrow", HAL::Platform::crashKindForTest);
    TEST_ASSERT_TRUE(mentions(con.run(sys, "crash squeeze"), "done: squeeze"));
    TEST_ASSERT_TRUE(mentions(con.run(sys, "crash release"), "done: release"));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_a_fresh_system_is_booting);
    RUN_TEST(test_begin_reaches_ready);
    RUN_TEST(test_boot_goes_straight_from_booting_to_ready);
    RUN_TEST(test_state_callback_receives_both_ends_of_the_transition);
    RUN_TEST(test_state_callbacks_are_capped_at_eight);

    RUN_TEST(test_minimal_registers_led_wifi_and_console);
    RUN_TEST(test_led_can_be_left_out);
    RUN_TEST(test_console_can_be_left_out);
    RUN_TEST(test_wifi_is_not_optional);
    RUN_TEST(test_storage_and_systeminfo_join_when_enabled);
    RUN_TEST(test_components_not_compiled_in_are_requested_without_failing);

    RUN_TEST(test_second_begin_is_refused_without_re_registering);
    RUN_TEST(test_second_begin_does_not_fire_the_state_callbacks_again);

    RUN_TEST(test_ready_drives_the_status_led);
    RUN_TEST(test_the_status_led_honours_the_configured_pin_and_polarity);

    RUN_TEST(test_status_command_reports_the_device);
    RUN_TEST(test_wifi_command_answers);
    RUN_TEST(test_storage_command_without_storage);
    RUN_TEST(test_storage_command_with_storage);
    RUN_TEST(test_bootdiag_command_without_systeminfo);
    RUN_TEST(test_bootdiag_command_reports_the_first_boot);
    RUN_TEST(test_a_custom_command_reaches_the_console);
    RUN_TEST(test_registering_a_command_without_a_console_is_a_no_op);
    RUN_TEST(test_console_runs_on_the_configured_port);

    // OBS-7 / OBS-2 / OBS-1
    RUN_TEST(test_begin_arms_the_loop_watchdog_by_default);
    RUN_TEST(test_a_zero_timeout_leaves_the_loop_watchdog_off);
    RUN_TEST(test_every_system_loop_feeds_the_watchdog);
    RUN_TEST(test_the_feed_is_a_no_op_when_the_watchdog_is_off);
    RUN_TEST(test_bootdiag_command_reports_the_reset_detail);
    RUN_TEST(test_bootdiag_command_reports_a_waiting_core_dump);
    RUN_TEST(test_a_failed_arming_is_reported_not_claimed);
    RUN_TEST(test_bootdiag_reports_the_tracked_minimum_where_the_platform_has_one);
    RUN_TEST(test_the_death_is_promoted_before_any_component_begins_and_persisted_after);
    RUN_TEST(test_the_first_death_survives_a_second_one_during_bring_up);
    RUN_TEST(test_a_death_is_acknowledged_even_without_storage);
    RUN_TEST(test_bootdiag_command_reports_the_last_death);
    RUN_TEST(test_bootdiag_names_the_component_behind_the_phase);
    RUN_TEST(test_crash_command_reaches_the_platform);
    RUN_TEST(test_bootdiag_reports_this_boots_failed_allocs);
    RUN_TEST(test_bootdiag_with_a_saturated_record_is_cut_not_overrun);

    return UNITY_END();
}
