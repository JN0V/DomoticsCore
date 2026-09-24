/**
 * @file test_console_capture.cpp
 * @brief The platform half of the core-log capture, on a board that has one.
 *
 * The host suite feeds the stub the lines a board would have produced; the sink
 * they arrive through exists only here. Every line in this file is provoked with
 * ets_printf, which is what the Arduino core's log macros and the SDK's own
 * narration ultimately write with, so what runs is the mechanism the required
 * cross-compilation checks can do no more than build.
 */

#include <Arduino.h>
#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/Wifi_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

extern "C" {
    int ets_printf(const char* format, ...);
    void ets_install_putc1(void (*p)(char c));
}

static Core* testCore = nullptr;
static RemoteConsoleComponent* console = nullptr;

void setUp(void) {
    // The console opens its telnet server in begin(). On ESP32 that reaches lwIP,
    // which does not exist until the radio has been brought up at least once, and
    // the firmware aborts on an invalid mbox before a line can be captured. No
    // network is joined: station mode is what starts the stack.
    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);

    testCore = new Core();
    RemoteConsoleConfig config;
    config.enabled = true;
    config.requireAuth = false;
    auto owned = std::make_unique<RemoteConsoleComponent>(config);
    console = owned.get();
    testCore->addComponent(std::move(owned));
    testCore->begin();
    console->clearBuffer();
}

void tearDown(void) {
    if (testCore) {
        testCore->shutdown();
        delete testCore;
        testCore = nullptr;
    }
    console = nullptr;
}

/** @brief Drain the intake the way loop() does, with room for a burst. */
static void drain(int loops = 8) {
    for (int i = 0; i < loops; ++i) testCore->loop();
}

static int platformEntries() {
    int n = 0;
    for (const auto& entry : console->getRecentLogs(100)) {
        if (entry.tag == LOG_PLATFORM) n++;
    }
    return n;
}

static bool platformLineSaying(const char* needle, LogLevel* levelOut = nullptr,
                               size_t* lengthOut = nullptr) {
    for (const auto& entry : console->getRecentLogs(100)) {
        if (entry.tag == LOG_PLATFORM && entry.message.indexOf(needle) >= 0) {
            if (levelOut) *levelOut = entry.level;
            if (lengthOut) *lengthOut = entry.message.length();
            return true;
        }
    }
    return false;
}

// What the whole mechanism exists for: a line this framework never wrote,
// printed the way the platform prints its own, reaching a client's buffer.
void test_a_line_the_platform_printed_reaches_the_console() {
    TEST_ASSERT_TRUE_MESSAGE(HAL::CoreLog::captureInstalled(),
        "begin() did not take the sink, so nothing below measures anything");

    ets_printf("probe: a line the platform wrote\n");
    drain();

    LogLevel level = LOG_LEVEL_NONE;
    TEST_ASSERT_TRUE_MESSAGE(platformLineSaying("a line the platform wrote", &level),
        "the sink is held but the line never arrived");
    TEST_ASSERT_EQUAL_MESSAGE(LOG_LEVEL_INFO, level, "a line with no level is information");
}

// The two shapes a board actually prints, read on the board rather than fed to
// the stub: ESP-IDF's "E (…)" and the Arduino core's "[  …][E]".
void test_the_level_is_read_from_the_shape_on_the_board() {
    ets_printf("E (1591) gpio: probe idf error\n");
    ets_printf("[  1638][W][Preferences.cpp:50] begin(): probe arduhal warning\n");
    drain();

    LogLevel idf = LOG_LEVEL_NONE;
    LogLevel arduhal = LOG_LEVEL_NONE;
    TEST_ASSERT_TRUE(platformLineSaying("probe idf error", &idf));
    TEST_ASSERT_TRUE(platformLineSaying("probe arduhal warning", &arduhal));
    TEST_ASSERT_EQUAL_MESSAGE(LOG_LEVEL_ERROR, idf, "an ESP-IDF error stopped being greppable as one");
    TEST_ASSERT_EQUAL_MESSAGE(LOG_LEVEL_WARN, arduhal, "an Arduino core warning stopped being greppable as one");
}

// A slot is fixed and a core line is not: the head carries the level and the
// subject, so it is the tail that goes.
void test_a_line_over_a_slot_keeps_its_head() {
    char long_line[HAL::CoreLog::MAX_LINE * 2];
    memset(long_line, 'x', sizeof(long_line));
    memcpy(long_line, "probe overlong ", 15);
    long_line[sizeof(long_line) - 2] = 'Z';   // the tail that must not survive
    long_line[sizeof(long_line) - 1] = '\0';

    ets_printf("%s\n", long_line);
    drain();

    size_t length = 0;
    TEST_ASSERT_TRUE(platformLineSaying("probe overlong", nullptr, &length));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(HAL::CoreLog::MAX_LINE - 1, (uint32_t)length,
        "the intake kept more or less than one slot");
    TEST_ASSERT_FALSE_MESSAGE(platformLineSaying("Z"), "the tail of an overlong line was published");
}

// Installing is idempotent — a second install that chained the hook to itself
// would deliver the same line twice — and removing gives the sink back.
void test_the_sink_is_taken_once_and_given_back() {
    HAL::CoreLog::installCapture();          // the second one: begin() took it already
    ets_printf("probe: taken once\n");
    drain();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, platformEntries(),
        "the line arrived twice, so the sink was chained to itself");

    console->clearBuffer();
    HAL::CoreLog::removeCapture();
    TEST_ASSERT_FALSE(HAL::CoreLog::captureInstalled());
    ets_printf("probe: after the sink was given back\n");
    drain();
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, platformEntries(),
        "a line still reached the console after the capture was removed");

    HAL::CoreLog::installCapture();
    ets_printf("probe: taken again\n");
    drain();
    TEST_ASSERT_TRUE_MESSAGE(platformLineSaying("taken again"),
        "the capture could not be reinstalled after being removed");
}

// The reader is in loop() and the writer is in whatever context printed: the
// intake refuses the newest line rather than growing, and counts it.
void test_a_full_intake_refuses_and_counts() {
    HAL::CoreLog::reset();
    for (size_t i = 0; i < HAL::CoreLog::SLOTS + 3; ++i) {
        ets_printf("probe: filling %u\n", (unsigned)i);   // no drain in between
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3, HAL::CoreLog::droppedLines(),
        "the intake grew, or dropped the wrong number of lines");

    drain();
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)HAL::CoreLog::SLOTS, platformEntries(),
        "the lines the intake did hold did not all reach the console");
}

// One platform narrates only while it is switched on, and the switch reinstalls
// the sink because an application's setDebugOutput() may have taken it.
void test_the_sdk_switch_takes_the_sink_back() {
    if (!HAL::CoreLog::supportsSdkOutputSwitch()) {
        TEST_ASSERT_FALSE_MESSAGE(HAL::CoreLog::setSdkOutput(true),
            "a platform with no switch answered that it switched something");
        return;
    }

    ets_install_putc1([](char) {});   // what an application taking the sink looks like
    ets_printf("probe: while the sink was elsewhere\n");
    drain();
    TEST_ASSERT_FALSE_MESSAGE(platformLineSaying("while the sink was elsewhere"),
        "the foreign sink never took, so the assertion below proves nothing");

    TEST_ASSERT_TRUE(HAL::CoreLog::setSdkOutput(true));
    ets_printf("probe: after the switch took it back\n");
    drain();
    TEST_ASSERT_TRUE_MESSAGE(platformLineSaying("after the switch took it back"),
        "switching the narration on did not reinstall the sink");

    HAL::CoreLog::setSdkOutput(false);
}

// What the intake costs where it is smallest: a fixed structure in static RAM,
// sized by the platform, plus whatever the console's own buffer grew to.
void test_the_intake_costs_what_the_platform_sized_it_at() {
    const size_t slots = HAL::CoreLog::SLOTS;
    const size_t line = HAL::CoreLog::MAX_LINE;
    const size_t ceiling = (slots * line) + line + slots + 32;   // slots, assembly, flags

    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(ceiling, (uint32_t)sizeof(HAL::CoreLog::Ring),
        "the intake grew beyond the slots the platform sized it at");

    const uint32_t free_heap = HAL::Platform::getFreeHeap();
    char note[96];
    snprintf(note, sizeof(note), "%u bytes of intake, %u bytes of heap free",
             (unsigned)sizeof(HAL::CoreLog::Ring), (unsigned)free_heap);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(8192, free_heap, note);
}

int runAllTests() {
    UNITY_BEGIN();
    RUN_TEST(test_a_line_the_platform_printed_reaches_the_console);
    RUN_TEST(test_the_level_is_read_from_the_shape_on_the_board);
    RUN_TEST(test_a_line_over_a_slot_keeps_its_head);
    RUN_TEST(test_the_sink_is_taken_once_and_given_back);
    RUN_TEST(test_a_full_intake_refuses_and_counts);
    RUN_TEST(test_the_sdk_switch_takes_the_sink_back);
    RUN_TEST(test_the_intake_costs_what_the_platform_sized_it_at);
    return UNITY_END();
}

void setup() {
    delay(2000);   // the runner opens the port after the board has booted
    runAllTests();
}

void loop() {}
