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

using namespace DomoticsCore;

void setUp(void) {}
void tearDown(void) {}

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

    return UNITY_END();
}
