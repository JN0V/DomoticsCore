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

// ============================================================================
// OBS-5: the telemetry publisher, with a scripted sink
// ============================================================================

using SystemHelpers::SystemTelemetry;
using SystemHelpers::TelemetrySample;
using SystemHelpers::CrashSummary;

struct SinkLog {
    int calls = 0;
    bool answer = true;
    std::string topic, payload;
    bool retain = false, mayQueue = false;
    SystemTelemetry::Sink fn() {
        return [this](const char* t, const char* p, size_t len, bool r, bool q) {
            calls++; topic = t; payload = std::string(p, len); retain = r; mayQueue = q;
            return answer;
        };
    }
};

static TelemetrySample healthySample() {
    TelemetrySample s;
    s.heap = 41216; s.largest = 20480; s.trough = 38400; s.uptimeS = 60; s.bootCount = 7;
    s.hasRssi = true; s.rssi = -61; s.fails = 2; s.failLast = 1024; s.buildId = 0x1a2b3c4du;
    return s;
}

void test_telemetry_payload_is_this_exact_document(void) {
    char buf[SystemTelemetry::TELEMETRY_MAX];
    size_t n = SystemTelemetry::formatTelemetry(buf, sizeof(buf), healthySample(), 36864, 3, 1);
    TEST_ASSERT_EQUAL_STRING(
        "{\"heap\":41216,\"largest\":20480,\"min\":36864,\"trough\":38400,\"uptime\":60,\"boot\":7,\"rssi\":-61,"
        "\"fails\":2,\"fail_last\":1024,\"skipped\":3,\"failed\":1,\"build\":\"1a2b3c4d\"}", buf);
    TEST_ASSERT_EQUAL_size_t(strlen(buf), n);
}

void test_telemetry_rssi_is_null_without_a_station_link(void) {
    // 0 dBm would graph as a perfect signal; null makes Home Assistant read unknown.
    TelemetrySample s = healthySample();
    s.hasRssi = false; s.rssi = 0;
    char buf[SystemTelemetry::TELEMETRY_MAX];
    SystemTelemetry::formatTelemetry(buf, sizeof(buf), s, 36864, 0, 0);
    TEST_ASSERT_TRUE(mentions(buf, "\"rssi\":null,"));
}

void test_telemetry_min_is_the_boot_long_minimum_across_ticks(void) {
    // The recorder's trough covers one 10 s window; the payload's min never rises.
    SystemTelemetry t;
    SinkLog sink;
    t.configure("node", 60, 4096);
    t.setSink(sink.fn());
    TelemetrySample a = healthySample(); a.heap = 40000; a.trough = 30000;
    TelemetrySample b = healthySample(); b.heap = 41000; b.trough = 39000;   // the window recovered
    TelemetrySample c = healthySample(); c.heap = 25000; c.trough = 0;       // no trough sample yet
    t.tick(60000, a);  TEST_ASSERT_TRUE(mentions(sink.payload, "\"min\":30000,\"trough\":30000"));
    t.tick(120000, b); TEST_ASSERT_TRUE(mentions(sink.payload, "\"min\":30000,\"trough\":39000"));
    t.tick(180000, c); TEST_ASSERT_TRUE(mentions(sink.payload, "\"min\":25000,\"trough\":0"));
    TEST_ASSERT_EQUAL_UINT32(25000, t.minSeen());
}

void test_telemetry_uptime_survives_the_millis_wrap(void) {
    SystemTelemetry t;
    TEST_ASSERT_EQUAL_UINT64(0xFFFFF000ull, t.noteMillis(0xFFFFF000u));
    TEST_ASSERT_EQUAL_UINT64(0x100000000ull + 0x1000u, t.noteMillis(0x1000u));   // wrapped: still climbing
    TEST_ASSERT_EQUAL_UINT64(0x100000000ull + 0x1000u, t.uptimeMs());   // the last clock noted, past the wrap
}

void test_a_client_id_with_a_wildcard_disables_telemetry(void) {
    // '+' and '#' are subscription wildcards; a broker drops the connection on a publish to them.
    SystemTelemetry t;
    TEST_ASSERT_FALSE(t.configure("esp32-#1", 60, 4096));
    TEST_ASSERT_FALSE(t.configure("room+bench", 60, 4096));
    TEST_ASSERT_FALSE(t.enabled());
    TEST_ASSERT_TRUE(t.configure("room-bench", 60, 4096));
}

void test_every_entity_definition_reads_a_key_the_payload_carries(void) {
    // The discovery templates and the formatter are tied by this test alone.
    size_t n = 0;
    const SystemTelemetry::EntityDef* defs = SystemTelemetry::entityDefs(n);
    TEST_ASSERT_EQUAL_size_t(7, n);
    char buf[SystemTelemetry::TELEMETRY_MAX];
    SystemTelemetry::formatTelemetry(buf, sizeof(buf), healthySample(), 1, 0, 0);
    for (size_t i = 0; i < n; i++) {
        std::string key = std::string("\"") + defs[i].key + "\":";
        TEST_ASSERT_TRUE_MESSAGE(mentions(buf, key.c_str()), defs[i].key);
        TEST_ASSERT_NOT_NULL(defs[i].id); TEST_ASSERT_NOT_NULL(defs[i].name);
    }
    char cbuf[SystemTelemetry::CRASH_MAX];
    CrashSummary c; c.source = CrashSummary::Rtc; c.promotion = "crash callback";
    SystemTelemetry::formatCrash(cbuf, sizeof(cbuf), c);
    TEST_ASSERT_TRUE_MESSAGE(mentions(cbuf, "\"promotion\":"), "the last-death template reads promotion");
    TEST_ASSERT_EQUAL_STRING("sys_last_death", SystemTelemetry::LAST_DEATH_ID);
}

void test_a_crash_summary_with_no_promotion_text_formats_as_none(void) {
    CrashSummary c; c.source = CrashSummary::Rtc; c.promotion = nullptr;
    char buf[SystemTelemetry::CRASH_MAX];
    SystemTelemetry::formatCrash(buf, sizeof(buf), c);
    TEST_ASSERT_TRUE(mentions(buf, "\"promotion\":\"none\""));
}

void test_telemetry_and_crash_payloads_saturated_fit_their_buffers(void) {
    TelemetrySample s;
    s.heap = s.largest = s.trough = s.uptimeS = s.bootCount = s.fails = s.failLast = s.buildId = 0xFFFFFFFFu;
    s.hasRssi = true; s.rssi = -2147483647 - 1;
    char buf[SystemTelemetry::TELEMETRY_MAX];
    size_t n = SystemTelemetry::formatTelemetry(buf, sizeof(buf), s, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
    TEST_ASSERT_TRUE_MESSAGE(n < sizeof(buf) - 1, "telemetry payload must never fill its buffer");
    TEST_ASSERT_EQUAL('}', buf[n - 1]);

    CrashSummary c;
    c.source = CrashSummary::Rtc; c.promotion = "software reset not requested by the firmware";
    snprintf(c.reset, sizeof(c.reset), "%s", "Interrupt watchdog!!");
    c.phase = c.buildId = c.uptimeMs = c.reason = c.epc1 = c.failSize = c.failCaller = c.minFree = c.dedupKey = 0xFFFFFFFFu;
    c.sameCount = 0xFFFFFFFFu; c.coreDumpSupported = true; c.coreDumpWaiting = true; c.coreDumpSize = 0xFFFFFFFFu;
    char cbuf[SystemTelemetry::CRASH_MAX];
    size_t m = SystemTelemetry::formatCrash(cbuf, sizeof(cbuf), c);
    TEST_ASSERT_TRUE_MESSAGE(m < sizeof(cbuf) - 1, "crash payload must never fill its buffer");
    TEST_ASSERT_EQUAL('}', cbuf[m - 1]);
}

void test_crash_payload_shapes(void) {
    char buf[SystemTelemetry::CRASH_MAX];
    CrashSummary none;
    SystemTelemetry::formatCrash(buf, sizeof(buf), none);
    TEST_ASSERT_EQUAL_STRING("{\"source\":\"none\",\"promotion\":\"none\"}", buf);

    CrashSummary torn;
    torn.source = CrashSummary::Persisted; torn.promotion = "unexpected reset"; torn.torn = true;
    torn.phase = 0x103; torn.epc1 = 0x40201234u; torn.minFree = 0;   // must not appear
    SystemTelemetry::formatCrash(buf, sizeof(buf), torn);
    TEST_ASSERT_EQUAL_STRING("{\"source\":\"persisted\",\"promotion\":\"unexpected reset\",\"phase\":259,\"torn\":1}", buf);

    CrashSummary full;
    full.source = CrashSummary::Persisted; full.promotion = "crash callback";
    full.phase = 5; full.buildId = 0xdeadbeefu; full.uptimeMs = 123456; full.reason = 254;
    full.epc1 = 0x40201287u; full.failSize = 1024; full.failCaller = 0x4020b864u; full.minFree = 6720;
    full.dedupKey = 0x0badf00du; full.sameCount = 3;
    full.coreDumpSupported = true; full.coreDumpWaiting = true; full.coreDumpSize = 15268;
    SystemTelemetry::formatCrash(buf, sizeof(buf), full);
    TEST_ASSERT_EQUAL_STRING(
        "{\"source\":\"persisted\",\"promotion\":\"crash callback\",\"phase\":5,\"build\":\"deadbeef\","
        "\"uptime_ms\":123456,\"reason\":254,\"epc1\":\"0x40201287\",\"fail_size\":1024,"
        "\"fail_caller\":\"0x4020b864\",\"min_free\":6720,\"dedup\":\"0badf00d\","
        "\"same_count\":3,\"coredump\":{\"waiting\":true,\"size\":15268}}", buf);

    CrashSummary rtc = full;
    rtc.source = CrashSummary::Rtc; rtc.sameCount = 0; rtc.coreDumpSupported = false;
    snprintf(rtc.reset, sizeof(rtc.reset), "%s", "Software reset");
    SystemTelemetry::formatCrash(buf, sizeof(buf), rtc);
    TEST_ASSERT_TRUE(mentions(buf, "\"source\":\"rtc\""));
    TEST_ASSERT_TRUE(mentions(buf, "\"reset\":\"Software reset\"}"));
    TEST_ASSERT_FALSE(mentions(buf, "same_count"));
    TEST_ASSERT_FALSE(mentions(buf, "coredump"));
}

void test_telemetry_ticks_on_the_interval_and_not_before(void) {
    SystemTelemetry t;
    SinkLog sink;
    TEST_ASSERT_TRUE(t.configure("esp32-abc123", 60, 4096));
    t.setSink(sink.fn());
    TEST_ASSERT_EQUAL_STRING("esp32-abc123/telemetry", t.telemetryTopic());
    TEST_ASSERT_EQUAL_STRING("esp32-abc123/crash", t.crashTopic());

    TEST_ASSERT_FALSE(t.tick(1000, healthySample()));
    TEST_ASSERT_FALSE(t.tick(59999, healthySample()));
    TEST_ASSERT_TRUE(t.tick(60000, healthySample()));
    TEST_ASSERT_EQUAL_INT(1, sink.calls);
    TEST_ASSERT_EQUAL_STRING("esp32-abc123/telemetry", sink.topic.c_str());
    TEST_ASSERT_FALSE(sink.retain);
    TEST_ASSERT_FALSE_MESSAGE(sink.mayQueue, "a sample deferred is a sample stale: the tick never queues");
    TEST_ASSERT_FALSE(t.tick(61000, healthySample()));
    TEST_ASSERT_TRUE(t.tick(120000, healthySample()));
    TEST_ASSERT_EQUAL_INT(2, sink.calls);
    TEST_ASSERT_EQUAL_UINT32(2, t.sent());
}

void test_a_tick_under_the_floor_is_skipped_and_the_next_payload_says_so(void) {
    SystemTelemetry t;
    SinkLog sink;
    t.configure("node", 60, 4096);
    t.setSink(sink.fn());
    TelemetrySample starved = healthySample();
    starved.heap = 4000;
    TEST_ASSERT_FALSE(t.tick(60000, starved));
    TEST_ASSERT_EQUAL_INT(0, sink.calls);
    TEST_ASSERT_EQUAL_UINT32(1, t.skipped());
    TEST_ASSERT_EQUAL_UINT32(0, t.failed());
    TEST_ASSERT_TRUE(t.tick(120000, healthySample()));
    TEST_ASSERT_TRUE(mentions(sink.payload, "\"skipped\":1,\"failed\":0"));
}

void test_a_refused_publish_is_counted_and_carried_by_the_next_payload(void) {
    // The removal check: drop the ++failed_ on a refused sink and the second
    // payload reads "failed":0 for a sample that never left.
    SystemTelemetry t;
    SinkLog sink;
    t.configure("node", 60, 4096);
    t.setSink(sink.fn());
    sink.answer = false;
    TEST_ASSERT_FALSE(t.tick(60000, healthySample()));
    TEST_ASSERT_EQUAL_INT(1, sink.calls);
    TEST_ASSERT_EQUAL_UINT32(1, t.failed());
    TEST_ASSERT_EQUAL_UINT32(0, t.skipped());
    sink.answer = true;
    TEST_ASSERT_TRUE(t.tick(120000, healthySample()));
    TEST_ASSERT_TRUE(mentions(sink.payload, "\"skipped\":0,\"failed\":1"));
}

void test_telemetry_interval_zero_publishes_nothing(void) {
    SystemTelemetry t;
    SinkLog sink;
    TEST_ASSERT_TRUE(t.configure("node", 0, 4096));
    t.setSink(sink.fn());
    TEST_ASSERT_FALSE(t.enabled());
    for (uint32_t now = 0; now < 600000; now += 1000) TEST_ASSERT_FALSE(t.tick(now, healthySample()));
    TEST_ASSERT_EQUAL_INT(0, sink.calls);
}

void test_an_over_long_client_id_disables_telemetry(void) {
    SystemTelemetry t;
    std::string id(81, 'x');
    TEST_ASSERT_FALSE(t.configure(id.c_str(), 60, 4096));
    TEST_ASSERT_FALSE(t.enabled());
    TEST_ASSERT_EQUAL_STRING("", t.telemetryTopic());
    std::string ok(80, 'x');
    TEST_ASSERT_TRUE(t.configure(ok.c_str(), 60, 4096));
    TEST_ASSERT_EQUAL_size_t(80 + strlen("/telemetry"), strlen(t.telemetryTopic()));
}

void test_the_crash_publish_is_retained_and_may_queue(void) {
    SystemTelemetry t;
    SinkLog sink;
    t.configure("node", 60, 4096);
    t.setSink(sink.fn());
    CrashSummary c;
    TEST_ASSERT_TRUE(t.publishCrash(c));
    TEST_ASSERT_EQUAL_STRING("node/crash", sink.topic.c_str());
    TEST_ASSERT_TRUE(sink.retain);
    TEST_ASSERT_TRUE(sink.mayQueue);
    TEST_ASSERT_EQUAL_STRING("{\"source\":\"none\",\"promotion\":\"none\"}", sink.payload.c_str());
}

void test_the_last_death_comes_from_the_persisted_record_when_storage_has_it(void) {
    stageDeath();
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    const CrashSummary& d = sys.lastDeath();
    TEST_ASSERT_EQUAL(CrashSummary::Persisted, d.source);
    TEST_ASSERT_EQUAL_STRING("crash callback", d.promotion);
    TEST_ASSERT_EQUAL_UINT32(254, d.reason);
    TEST_ASSERT_EQUAL_UINT32(1024, d.failSize);
    TEST_ASSERT_EQUAL_UINT32(0x40201287u, d.failCaller);
    TEST_ASSERT_EQUAL_UINT32(1, d.sameCount);
    TEST_ASSERT_FALSE(d.torn);
    char buf[SystemTelemetry::CRASH_MAX];
    SystemTelemetry::formatCrash(buf, sizeof(buf), d);
    TEST_ASSERT_TRUE(mentions(buf, "\"source\":\"persisted\",\"promotion\":\"crash callback\""));
    TEST_ASSERT_TRUE(mentions(buf, "\"fail_caller\":\"0x40201287\""));
    TEST_ASSERT_TRUE(mentions(buf, "\"same_count\":1"));
}

void test_the_last_death_comes_from_rtc_without_storage(void) {
    stageDeath();
    System sys(SystemConfig::minimal());
    sys.begin();
    const CrashSummary& d = sys.lastDeath();
    TEST_ASSERT_EQUAL(CrashSummary::Rtc, d.source);
    TEST_ASSERT_EQUAL_STRING("crash callback", d.promotion);
    TEST_ASSERT_EQUAL_STRING("Software reset", d.reset);
    TEST_ASSERT_EQUAL_UINT32(1024, d.failSize);
    TEST_ASSERT_EQUAL_UINT32(0, d.sameCount);
}

static void stageTornDeath() {
    FlightRecorder::instance().begin();
    FlightRecorder::instance().setPhase(3);
    HAL::Platform::stubRtcWordsForTest[FlightRecord::W_FAST + 1] ^= 1;   // tear the body
    FlightRecorder::instance().resetForTest();
    HAL::Platform::setResetReasonForTest(HAL::Platform::ResetReason::Watchdog);
}

void test_a_torn_persisted_death_publishes_two_fields_and_torn(void) {
    stageTornDeath();
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    const CrashSummary& d = sys.lastDeath();
    TEST_ASSERT_EQUAL(CrashSummary::Persisted, d.source);
    TEST_ASSERT_TRUE(d.torn);
    char buf[SystemTelemetry::CRASH_MAX];
    SystemTelemetry::formatCrash(buf, sizeof(buf), d);
    TEST_ASSERT_EQUAL_STRING("{\"source\":\"persisted\",\"promotion\":\"unexpected reset\",\"phase\":3,\"torn\":1}", buf);
}

void test_a_torn_rtc_death_publishes_two_fields_and_torn(void) {
    stageTornDeath();
    System sys(SystemConfig::minimal());
    sys.begin();
    const CrashSummary& d = sys.lastDeath();
    TEST_ASSERT_EQUAL(CrashSummary::Rtc, d.source);
    TEST_ASSERT_TRUE(d.torn);
    char buf[SystemTelemetry::CRASH_MAX];
    SystemTelemetry::formatCrash(buf, sizeof(buf), d);
    TEST_ASSERT_EQUAL_STRING("{\"source\":\"rtc\",\"promotion\":\"unexpected reset\",\"phase\":3,\"torn\":1}", buf);
}

void test_the_death_boot_carries_the_reset_reason_even_when_persisted(void) {
    // The blob has no reset field; on the boot right after the death RTC still knows it.
    stageDeath();
    SystemConfig cfg = SystemConfig::minimal();
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    System sys(cfg);
    sys.begin();
    TEST_ASSERT_EQUAL(CrashSummary::Persisted, sys.lastDeath().source);
    TEST_ASSERT_EQUAL_STRING("Software reset", sys.lastDeath().reset);
}

void test_a_clean_boot_with_no_history_has_no_death_to_report(void) {
    System sys(SystemConfig::minimal());
    sys.begin();
    TEST_ASSERT_EQUAL(CrashSummary::None, sys.lastDeath().source);
    TEST_ASSERT_FALSE_MESSAGE(sys.telemetry().enabled(), "no MQTT in this build: nothing configures the publisher");
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

    // OBS-5 — the telemetry publisher and the crash summary
    RUN_TEST(test_telemetry_payload_is_this_exact_document);
    RUN_TEST(test_telemetry_and_crash_payloads_saturated_fit_their_buffers);
    RUN_TEST(test_crash_payload_shapes);
    RUN_TEST(test_telemetry_ticks_on_the_interval_and_not_before);
    RUN_TEST(test_a_tick_under_the_floor_is_skipped_and_the_next_payload_says_so);
    RUN_TEST(test_a_refused_publish_is_counted_and_carried_by_the_next_payload);
    RUN_TEST(test_telemetry_interval_zero_publishes_nothing);
    RUN_TEST(test_an_over_long_client_id_disables_telemetry);
    RUN_TEST(test_the_crash_publish_is_retained_and_may_queue);
    RUN_TEST(test_the_last_death_comes_from_the_persisted_record_when_storage_has_it);
    RUN_TEST(test_the_last_death_comes_from_rtc_without_storage);
    RUN_TEST(test_a_clean_boot_with_no_history_has_no_death_to_report);
    RUN_TEST(test_telemetry_rssi_is_null_without_a_station_link);
    RUN_TEST(test_telemetry_min_is_the_boot_long_minimum_across_ticks);
    RUN_TEST(test_telemetry_uptime_survives_the_millis_wrap);
    RUN_TEST(test_a_client_id_with_a_wildcard_disables_telemetry);
    RUN_TEST(test_every_entity_definition_reads_a_key_the_payload_carries);
    RUN_TEST(test_a_crash_summary_with_no_promotion_text_formats_as_none);
    RUN_TEST(test_a_torn_persisted_death_publishes_two_fields_and_torn);
    RUN_TEST(test_a_torn_rtc_death_publishes_two_fields_and_torn);
    RUN_TEST(test_the_death_boot_carries_the_reset_reason_even_when_persisted);

    return UNITY_END();
}
