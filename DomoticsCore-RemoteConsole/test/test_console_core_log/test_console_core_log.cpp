/**
 * @file test_console_core_log.cpp
 * @brief The lines the platform writes itself, carried to the console.
 *
 * The host has no core to capture, so the platform stub is the seam: a test
 * feeds the lines a board's logger would have produced, whole or one character
 * at a time, and asserts what the component does with them — the level it reads
 * from their shape, the client that receives them, the drops it counts, and the
 * command that reports all three.
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/Testing/HeapTracker.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Testing;

static Core* testCore = nullptr;

void setUp(void) {
    HAL::Platform::setMillisForTest(1000);
    testCore = new Core();
}

void tearDown(void) {
    HAL::Platform::resetMillisForTest();
    if (testCore) {
        testCore->shutdown();
        delete testCore;
        testCore = nullptr;
    }
}

static RemoteConsoleComponent* startConsole() {
    RemoteConsoleConfig config;
    config.enabled = true;
    config.requireAuth = false;
    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* ptr = console.get();
    testCore->addComponent(std::move(console));
    testCore->begin();
    return ptr;
}

static int countPlatformEntries(RemoteConsoleComponent* console) {
    int n = 0;
    for (const auto& entry : console->getRecentLogs(100)) {
        if (entry.tag == "PLATFORM") n++;
    }
    return n;
}

static bool hasPlatformMessage(RemoteConsoleComponent* console, const char* needle, LogLevel expected) {
    for (const auto& entry : console->getRecentLogs(100)) {
        if (entry.tag == "PLATFORM" && entry.message.indexOf(needle) >= 0) {
            return entry.level == expected;
        }
    }
    return false;
}

// ============================================================================
// The intake is drained by loop(), never by the sink
// ============================================================================

void test_a_platform_line_reaches_the_buffer_only_after_a_loop(void) {
    RemoteConsoleComponent* console = startConsole();

    HAL::CoreLog::feedLineForTest("E (1591) gpio: io_num=99 can only be input");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countPlatformEntries(console),
        "the sink wrote into the console's buffer from the logger's own context");

    testCore->loop();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countPlatformEntries(console),
        "the line the platform wrote never reached the console");
}

void test_a_character_sink_publishes_a_line_on_its_newline(void) {
    RemoteConsoleComponent* console = startConsole();

    HAL::CoreLog::feedCharsForTest("scandone");
    testCore->loop();
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countPlatformEntries(console),
        "half a line was published as a line");

    HAL::CoreLog::feedCharsForTest("\n");
    testCore->loop();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countPlatformEntries(console),
        "the newline did not publish the line it ends");
    TEST_ASSERT_TRUE(hasPlatformMessage(console, "scandone", LOG_LEVEL_INFO));
}

// ============================================================================
// The level is read from the line, so an error stays an error
// ============================================================================

void test_the_level_is_read_from_the_shape_of_the_line(void) {
    RemoteConsoleComponent* console = startConsole();
    console->setLogLevel(LOG_LEVEL_VERBOSE);   // so a debug line is not filtered out

    HAL::CoreLog::feedLineForTest("E (1591) gpio: io_num=99 can only be input");
    HAL::CoreLog::feedLineForTest("W (1620) nvs: partition not found");
    testCore->loop();
    HAL::CoreLog::feedLineForTest("[  1638][E][Preferences.cpp:50] begin(): nvs_open failed");
    HAL::CoreLog::feedLineForTest("[  1700][D][esp32-hal-cpu.c:244] setCpuFrequencyMhz(): PLL");
    testCore->loop();
    HAL::CoreLog::feedLineForTest("scandone");
    testCore->loop();

    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "io_num=99", LOG_LEVEL_ERROR),
        "an ESP-IDF error is no longer greppable as one");
    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "partition not found", LOG_LEVEL_WARN),
        "an ESP-IDF warning was read as something else");
    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "nvs_open failed", LOG_LEVEL_ERROR),
        "an Arduino core error is no longer greppable as one");
    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "setCpuFrequencyMhz", LOG_LEVEL_DEBUG),
        "an Arduino core debug line was read as something else");
    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "scandone", LOG_LEVEL_INFO),
        "a line with no level of its own must be information, not silence");
}

// ============================================================================
// A reader that falls behind loses lines, and is told how many
// ============================================================================

void test_an_intake_nobody_drained_counts_what_it_refused(void) {
    RemoteConsoleComponent* console = startConsole();

    static String warnings;
    warnings = "";
    auto cb = LoggerCallbacks::addCallback([](LogLevel level, const char* tag, const char* message) {
        if (level == LOG_LEVEL_WARN && strcmp(tag, LOG_CONSOLE) == 0) { warnings += message; warnings += "\n"; }
    });

    char line[64];
    for (size_t i = 0; i < HAL::CoreLog::SLOTS + 2; ++i) {
        snprintf(line, sizeof(line), "E (%u) probe: line %u", (unsigned)i, (unsigned)i);
        HAL::CoreLog::feedLineForTest(line);
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2, HAL::CoreLog::droppedLines(),
        "an intake that cannot grow must count what it turns away");

    // The drain takes four per loop, so the ring empties over several passes.
    for (int i = 0; i < 8; ++i) testCore->loop();
    LoggerCallbacks::removeCallback(cb);

    TEST_ASSERT_EQUAL_INT_MESSAGE((int)HAL::CoreLog::SLOTS, countPlatformEntries(console),
        "the lines the intake did hold were not all delivered");
    TEST_ASSERT_TRUE_MESSAGE(warnings.indexOf("dropped 2 lines") >= 0,
        "the drop was silent, which is what the event queue's own counter exists not to be");
    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "line 0", LOG_LEVEL_ERROR),
        "the oldest line was dropped: a full intake must refuse the newest");
}

// ============================================================================
// The client, the command, and the release
// ============================================================================

void test_a_captured_line_reaches_a_connected_client(void) {
    RemoteConsoleComponent* console = startConsole();
    HAL::WiFiClient client = console->getServer()->simulateClient(true, 42);
    testCore->loop();
    client.clearWriteBuffer();

    HAL::CoreLog::feedLineForTest("E (1591) gpio: io_num=99 can only be input");
    testCore->loop();

    const std::string written = client.getWriteBufferAsString();
    TEST_ASSERT_TRUE_MESSAGE(written.find("PLATFORM") != std::string::npos,
        "the captured line did not reach the client");
    TEST_ASSERT_TRUE_MESSAGE(written.find("io_num=99") != std::string::npos,
        "the client received the tag without the line");
}

void test_the_core_command_reports_what_the_platform_can_and_cannot_do(void) {
    RemoteConsoleComponent* console = startConsole();
    HAL::WiFiClient client = console->getServer()->simulateClient(true, 42);
    testCore->loop();

    client.clearWriteBuffer();
    client.simulateIncomingData("core\n");
    testCore->loop();
    std::string out = client.getWriteBufferAsString();
    TEST_ASSERT_TRUE_MESSAGE(out.find("Platform lines: captured") != std::string::npos,
        "the command says nothing about the capture it reports on");
    TEST_ASSERT_TRUE_MESSAGE(out.find("not a switch on this platform") != std::string::npos,
        "the host has no SDK to narrate, and the command must say so rather than pretend");

    client.clearWriteBuffer();
    client.simulateIncomingData("core on\n");
    testCore->loop();
    out = client.getWriteBufferAsString();
    TEST_ASSERT_TRUE_MESSAGE(out.find("nothing to switch") != std::string::npos,
        "a platform with no switch answered as though it had one");

    client.clearWriteBuffer();
    client.simulateIncomingData("core zzz\n");
    testCore->loop();
    out = client.getWriteBufferAsString();
    TEST_ASSERT_TRUE_MESSAGE(out.find("Usage: core") != std::string::npos,
        "an argument that is neither on nor off was accepted");
}

void test_shutdown_releases_the_capture(void) {
    RemoteConsoleComponent* console = startConsole();
    HAL::CoreLog::feedLineForTest("E (1) probe: before");
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, countPlatformEntries(console));

    console->shutdown();
    HAL::CoreLog::feedLineForTest("E (2) probe: after");

    char line[HAL::CoreLog::MAX_LINE];
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, (uint32_t)HAL::CoreLog::drainLine(line, sizeof(line)),
        "a line arrived after the capture was released");
}

// This framework's own lines reach the Arduino core's logger, so on ESP32 the
// character sink sees them too: without the guard every DomoticsCore line would
// arrive at the client twice, once under its component's tag and once as a
// platform line. Found on a WROOM-32D, where six of them did.
void test_the_capture_ignores_what_this_framework_prints(void) {
    RemoteConsoleComponent* console = startConsole();

    {
        HAL::CoreLog::SuppressOwnOutput guard;
        HAL::CoreLog::feedLineForTest("[  939][I][System.h:538] printReadyBanner(): [SYSTEM] ====");
        HAL::CoreLog::feedCharsForTest("[  940][I][Core.cpp:1] begin(): [CORE] up\n");
    }
    testCore->loop();
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countPlatformEntries(console),
        "a line this framework printed itself was captured and would be shown twice");

    HAL::CoreLog::feedLineForTest("E (1591) gpio: io_num=99 can only be input");
    testCore->loop();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countPlatformEntries(console),
        "the guard did not release, so the platform's own lines are lost too");
}

void test_the_intake_allocates_nothing(void) {
    RemoteConsoleComponent* console = startConsole();
    (void)console;

    HeapTracker tracker;
    HEAP_CHECKPOINT(tracker, "before");
    for (int i = 0; i < 50; ++i) {
        HAL::CoreLog::feedLineForTest("E (1591) gpio: io_num=99 can only be input");
        HAL::CoreLog::feedCharsForTest("scandone\n");
    }
    HEAP_CHECKPOINT(tracker, "after");
    // Tolerance 200: HeapTracker's own checkpoint bookkeeping uses ~144 bytes on
    // native. The sink itself must not allocate — it can run in an interrupt.
    HEAP_ASSERT_STABLE(tracker, "before", "after", 200);
}

// ============================================================================
// What the accounting has to see, and what the two sinks do to each other
// ============================================================================

// The character sink is the only one ESP8266 has, and every Arduino core line on
// ESP32 arrives through it: a loss there that the counter cannot see is the very
// silence this capture exists to end.
void test_the_character_path_counts_what_it_could_not_hold(void) {
    RemoteConsoleComponent* console = startConsole();
    (void)console;

    char line[64];
    for (size_t i = 0; i < HAL::CoreLog::SLOTS; ++i) {
        snprintf(line, sizeof(line), "E (%u) probe: filler %u", (unsigned)i, (unsigned)i);
        HAL::CoreLog::feedLineForTest(line);
    }
    TEST_ASSERT_EQUAL_UINT32(0, HAL::CoreLog::droppedLines());

    HAL::CoreLog::feedCharsForTest("wifi evt: 2\n");
    HAL::CoreLog::feedCharsForTest("scandone\n");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2, HAL::CoreLog::droppedLines(),
        "a line the character sink could not store was lost without a count");
}

// A line another context writes inside this framework's own output window is
// lost with ours. Whole ones are counted apart, since they are not the reader
// falling behind; a half-assembled one is spoiled and dropped.
void test_what_the_suppression_costs_is_counted(void) {
    RemoteConsoleComponent* console = startConsole();

    {
        HAL::CoreLog::SuppressOwnOutput guard;
        HAL::CoreLog::feedLineForTest("E (2) gpio: a whole line from another task");
        HAL::CoreLog::feedLineForTest("E (3) gpio: and another");
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2, HAL::CoreLog::ignoredLines(),
        "a whole line skipped while we printed is invisible in every counter");
    TEST_ASSERT_EQUAL_UINT32(0, HAL::CoreLog::droppedLines());

    HAL::CoreLog::feedCharsForTest("wifi ev");
    {
        HAL::CoreLog::SuppressOwnOutput guard;
        HAL::CoreLog::feedCharsForTest("[I][CORE] ours\n");
    }
    HAL::CoreLog::feedCharsForTest("t: 2\n");
    testCore->loop();
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, countPlatformEntries(console),
        "a line whose middle is missing was shown as though it were whole");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, HAL::CoreLog::droppedLines(),
        "the spoiled line was neither shown nor counted");
}

// ESP32 installs both sinks, so a whole line can land between two characters of
// another. They must not share a slot.
void test_a_whole_line_does_not_overwrite_one_being_assembled(void) {
    RemoteConsoleComponent* console = startConsole();

    HAL::CoreLog::feedCharsForTest("wifi ev");
    HAL::CoreLog::feedLineForTest("E (3) gpio: io_num=99 can only be input");
    HAL::CoreLog::feedCharsForTest("t: 2\n");
    for (int i = 0; i < 3; ++i) testCore->loop();

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, countPlatformEntries(console),
        "one of the two lines was lost to the other");
    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "io_num=99", LOG_LEVEL_ERROR),
        "the whole line did not arrive");
    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "wifi evt: 2", LOG_LEVEL_INFO),
        "the assembled line arrived truncated: the two sinks shared a slot");
}

void test_a_line_longer_than_a_slot_keeps_its_head(void) {
    RemoteConsoleComponent* console = startConsole();

    String tooLong = "E (9) probe: ";
    while (tooLong.length() < HAL::CoreLog::MAX_LINE + 40) tooLong += 'x';
    HAL::CoreLog::feedLineForTest(tooLong.c_str());
    HAL::CoreLog::feedCharsForTest((tooLong + "\n").c_str());
    for (int i = 0; i < 3; ++i) testCore->loop();

    int seen = 0;
    for (const auto& entry : console->getRecentLogs(100)) {
        if (entry.tag == "PLATFORM") {
            seen++;
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(HAL::CoreLog::MAX_LINE - 1, entry.message.length(),
                "an over-long line was cut somewhere other than the slot's end");
            TEST_ASSERT_EQUAL_MESSAGE(LOG_LEVEL_ERROR, entry.level,
                "the head carries the level, and the head is what a cut must keep");
        }
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, seen, "both paths must survive an over-long line");
}

void test_a_buffer_too_small_leaves_the_line_where_it_is(void) {
    startConsole();
    HAL::CoreLog::feedLineForTest("E (1) probe: keep me");

    char tiny[1];
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)HAL::CoreLog::drainLine(tiny, sizeof(tiny)));

    char line[HAL::CoreLog::MAX_LINE];
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, (uint32_t)HAL::CoreLog::drainLine(line, sizeof(line)),
        "the line was consumed by a call that could not carry it");
    TEST_ASSERT_TRUE(strstr(line, "keep me") != nullptr);
}

void test_installing_twice_changes_nothing(void) {
    RemoteConsoleComponent* console = startConsole();
    TEST_ASSERT_TRUE(HAL::CoreLog::captureInstalled());

    HAL::CoreLog::feedLineForTest("E (1) probe: before the second install");
    TEST_ASSERT_TRUE(HAL::CoreLog::installCapture());   // on ESP32 a second install
    testCore->loop();                                   // would chain the hook to itself

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, countPlatformEntries(console),
        "the second install emptied a ring that had a line in it");

    console->shutdown();
    TEST_ASSERT_FALSE_MESSAGE(HAL::CoreLog::captureInstalled(),
        "the capture outlived the component that took it");
    HAL::CoreLog::removeCapture();                      // twice, as a Core shutdown does
}

void test_the_level_survives_the_core_s_log_colours(void) {
    RemoteConsoleComponent* console = startConsole();

    // With CONFIG_ARDUHAL_LOG_COLORS on, every core line opens with an escape
    // sequence — and the level sits behind it.
    HAL::CoreLog::feedLineForTest("\033[0;31m[  1638][E][Preferences.cpp:50] begin(): nvs_open failed\033[0m");
    testCore->loop();

    TEST_ASSERT_TRUE_MESSAGE(hasPlatformMessage(console, "nvs_open failed", LOG_LEVEL_ERROR),
        "a coloured core line lost its level, so every error reads as information");
}

void test_the_drop_report_is_paced(void) {
    RemoteConsoleComponent* console = startConsole();
    (void)console;

    static String warnings;
    warnings = "";
    auto cb = LoggerCallbacks::addCallback([](LogLevel level, const char* tag, const char* message) {
        if (level == LOG_LEVEL_WARN && strcmp(tag, LOG_CONSOLE) == 0) { warnings += message; warnings += "\n"; }
    });

    auto overflow = [](int lines) {
        char line[64];
        for (int i = 0; i < lines; ++i) {
            snprintf(line, sizeof(line), "E (%d) probe: line %d", i, i);
            HAL::CoreLog::feedLineForTest(line);
        }
    };

    overflow((int)HAL::CoreLog::SLOTS + 2);
    for (int i = 0; i < 8; ++i) testCore->loop();
    const int first = warnings.length();
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, first, "the first drop was not reported at all");

    HAL::Platform::advanceMillisForTest(30000);
    overflow((int)HAL::CoreLog::SLOTS + 2);
    for (int i = 0; i < 8; ++i) testCore->loop();
    TEST_ASSERT_EQUAL_INT_MESSAGE(first, warnings.length(),
        "a second report inside the minute: the pacing is gone and the warnings evict the lines");

    HAL::Platform::advanceMillisForTest(31000);
    overflow((int)HAL::CoreLog::SLOTS + 2);
    for (int i = 0; i < 8; ++i) testCore->loop();
    const int second = warnings.length();
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(first, second,
        "past the minute the report must come back");

    // The first millisecond of an uptime is a timestamp, not a sentinel: a
    // "never reported yet" test written as `at == 0` reports on every loop here.
    HAL::Platform::setMillisForTest(0);
    warnings = "";
    overflow((int)HAL::CoreLog::SLOTS + 2);
    for (int i = 0; i < 8; ++i) testCore->loop();
    const int atZero = warnings.length();
    overflow((int)HAL::CoreLog::SLOTS + 2);
    for (int i = 0; i < 8; ++i) testCore->loop();
    LoggerCallbacks::removeCallback(cb);
    TEST_ASSERT_EQUAL_INT_MESSAGE(atZero, warnings.length(),
        "at millis() == 0 the report fires every loop: the sentinel is a legal time");
}

// The branch only ESP8266 takes, reached through the stub's seam: without it the
// suite tests the "nothing to switch" answer and nothing else.
void test_the_core_command_switches_the_sdk_where_one_exists(void) {
    RemoteConsoleComponent* console = startConsole();
    HAL::CoreLog::sdkSwitchSupportedForTest() = true;
    HAL::WiFiClient client = console->getServer()->simulateClient(true, 42);
    testCore->loop();

    client.clearWriteBuffer();
    client.simulateIncomingData("core on\n");
    testCore->loop();
    std::string out = client.getWriteBufferAsString();
    TEST_ASSERT_TRUE_MESSAGE(out.find("SDK narration on") != std::string::npos,
        "the switch answered nothing");
    TEST_ASSERT_TRUE_MESSAGE(HAL::CoreLog::sdkOutputForTest(),
        "the command answered success without switching anything");

    client.clearWriteBuffer();
    client.simulateIncomingData("core\n");
    testCore->loop();
    out = client.getWriteBufferAsString();
    TEST_ASSERT_TRUE_MESSAGE(out.find("SDK narration: on") != std::string::npos,
        "the report does not follow what the command did");

    client.clearWriteBuffer();
    client.simulateIncomingData("core off\n");
    testCore->loop();
    TEST_ASSERT_FALSE_MESSAGE(HAL::CoreLog::sdkOutputForTest(),
        "off left the narration on");
    HAL::CoreLog::sdkSwitchSupportedForTest() = false;
}

int runAllTests() {
    UNITY_BEGIN();
    RUN_TEST(test_a_platform_line_reaches_the_buffer_only_after_a_loop);
    RUN_TEST(test_a_character_sink_publishes_a_line_on_its_newline);
    RUN_TEST(test_the_level_is_read_from_the_shape_of_the_line);
    RUN_TEST(test_an_intake_nobody_drained_counts_what_it_refused);
    RUN_TEST(test_a_captured_line_reaches_a_connected_client);
    RUN_TEST(test_the_core_command_reports_what_the_platform_can_and_cannot_do);
    RUN_TEST(test_shutdown_releases_the_capture);
    RUN_TEST(test_the_capture_ignores_what_this_framework_prints);
    RUN_TEST(test_the_character_path_counts_what_it_could_not_hold);
    RUN_TEST(test_what_the_suppression_costs_is_counted);
    RUN_TEST(test_a_whole_line_does_not_overwrite_one_being_assembled);
    RUN_TEST(test_a_line_longer_than_a_slot_keeps_its_head);
    RUN_TEST(test_a_buffer_too_small_leaves_the_line_where_it_is);
    RUN_TEST(test_installing_twice_changes_nothing);
    RUN_TEST(test_the_level_survives_the_core_s_log_colours);
    RUN_TEST(test_the_drop_report_is_paced);
    RUN_TEST(test_the_core_command_switches_the_sdk_where_one_exists);
    RUN_TEST(test_the_intake_allocates_nothing);
    return UNITY_END();
}

int main(int, char**) {
    return runAllTests();
}
