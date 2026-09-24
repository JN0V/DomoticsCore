/**
 * @file test_console_capture.cpp
 * @brief The platform half of the core-log capture, on a board that has one.
 *
 * The host suite feeds the stub the lines a board would have produced; the sink
 * they arrive through exists only here. Every line is provoked with ets_printf,
 * which is what the Arduino core's log macros and the SDK's own narration write
 * with, so what runs is the mechanism the required cross-compilation checks can
 * do no more than build.
 *
 * One half stays out of reach: the vprintf hook ESP32 installs beside this one
 * carries what the precompiled ESP-IDF libraries write, and nothing an Arduino
 * build can call reaches it. What is asserted here is the character sink.
 */

#include <Arduino.h>
#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/WiFiServer_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

extern "C" {
    int ets_printf(const char* format, ...);
    void ets_install_putc1(void (*p)(char c));
}

static Core* testCore = nullptr;
static RemoteConsoleComponent* console = nullptr;

void setUp(void) {
    // No radio is brought up here, deliberately: the console has to start with no
    // network stack at all, which is the shape that used to abort an ESP32.
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
    // A Unity longjmp out of a failed assertion can leave the capture removed, or
    // the sink pointed at a test's own swallowing stub: the next test would then
    // measure that rather than the code.
    HAL::CoreLog::removeCapture();
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

// Counts what this suite printed and nothing else: a line another task wrote
// through the same sink is a platform line too, and would move every figure.
static int platformEntriesSaying(const char* needle) {
    int n = 0;
    for (const auto& entry : console->getRecentLogs(100)) {
        if (entry.tag == LOG_PLATFORM && entry.message.indexOf(needle) >= 0) n++;
    }
    return n;
}

// A build whose console is USB CDC has no character sink for the capture to
// take — ESP32 says so through uartGetDebug() — and every assertion below would
// fail for the build's reason rather than the code's. Measured once, by the
// first test, and reported as ignored rather than red.
static bool characterSink = true;

static bool platformLineSaying(const char* needle, LogLevel* levelOut = nullptr,
                               String* messageOut = nullptr) {
    for (const auto& entry : console->getRecentLogs(100)) {
        if (entry.tag == LOG_PLATFORM && entry.message.indexOf(needle) >= 0) {
            if (levelOut) *levelOut = entry.level;
            if (messageOut) *messageOut = entry.message;
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
    if (!platformLineSaying("a line the platform wrote", &level)) {
        characterSink = false;
        TEST_IGNORE_MESSAGE("this build has no character sink to take: nothing below can run");
    }
    TEST_ASSERT_EQUAL_MESSAGE(LOG_LEVEL_INFO, level, "a line with no level is information");
}

// The two shapes a board actually prints, read on the board rather than fed to
// the stub: ESP-IDF's "E (…)" and the Arduino core's "[  …][E]".
void test_the_level_is_read_from_the_shape_on_the_board() {
    if (!characterSink) TEST_IGNORE_MESSAGE("no character sink on this build");
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
    if (!characterSink) TEST_IGNORE_MESSAGE("no character sink on this build");
    char long_line[HAL::CoreLog::MAX_LINE * 2];
    memset(long_line, 'x', sizeof(long_line));
    memcpy(long_line, "probe overlong ", 15);
    long_line[sizeof(long_line) - 2] = 'Z';   // the tail that must not survive
    long_line[sizeof(long_line) - 1] = '\0';

    ets_printf("%s\n", long_line);
    drain();

    String kept;
    TEST_ASSERT_TRUE(platformLineSaying("probe overlong", nullptr, &kept));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(HAL::CoreLog::MAX_LINE - 1, (uint32_t)kept.length(),
        "the intake kept more or less than one slot");
    TEST_ASSERT_TRUE_MESSAGE(kept.indexOf('Z') < 0,
        "the tail of the overlong line survived, so it was not cut at a slot");
}

// Installing is idempotent — a second install that chained the hook to itself
// would deliver the same line twice — and removing gives the sink back.
void test_the_sink_is_taken_once_and_given_back() {
    if (!characterSink) TEST_IGNORE_MESSAGE("no character sink on this build");
    HAL::CoreLog::installCapture();          // the second one: begin() took it already
    ets_printf("probe: taken once\n");
    drain();
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, platformEntriesSaying("taken once"),
        "the line arrived twice, so the sink was chained to itself");

    console->clearBuffer();
    HAL::CoreLog::removeCapture();
    TEST_ASSERT_FALSE(HAL::CoreLog::captureInstalled());
    ets_printf("probe: after the sink was given back\n");
    drain();
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, platformEntriesSaying("after the sink was given back"),
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
    if (!characterSink) TEST_IGNORE_MESSAGE("no character sink on this build");
    HAL::CoreLog::reset();
    for (size_t i = 0; i < HAL::CoreLog::SLOTS + 3; ++i) {
        ets_printf("probe: filling %u\n", (unsigned)i);   // no drain in between
    }
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32_MESSAGE(3, HAL::CoreLog::droppedLines(),
        "the intake grew instead of refusing what did not fit");

    drain();
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)HAL::CoreLog::SLOTS, platformEntriesSaying("probe: filling"),
        "the lines the intake did hold did not all reach the console");
}

// One platform narrates only while it is switched on, and the switch reinstalls
// the sink because an application's setDebugOutput() may have taken it.
void test_the_sdk_switch_takes_the_sink_back() {
    if (!characterSink) TEST_IGNORE_MESSAGE("no character sink on this build");
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

// The intake is static RAM a board pays for whether or not a line ever arrives,
// so its size is a figure and not an implementation detail. Anything but the
// slots, the line being assembled and the flags would show up here.
void test_the_intake_is_the_size_the_platform_declared() {
    const size_t slots = HAL::CoreLog::SLOTS;
    const size_t line = HAL::CoreLog::MAX_LINE;
    const size_t ceiling = (slots * line) + line + slots + 32;   // slots, assembly, flags

    char note[96];
    snprintf(note, sizeof(note), "%u slots of %u, %u bytes of intake, %u bytes of heap free",
             (unsigned)slots, (unsigned)line, (unsigned)sizeof(HAL::CoreLog::Ring),
             (unsigned)HAL::Platform::getFreeHeap());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(ceiling, (uint32_t)sizeof(HAL::CoreLog::Ring), note);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32_MESSAGE(slots * line, (uint32_t)sizeof(HAL::CoreLog::Ring), note);
}

// The console starts here with nothing under it: no radio, no interface, no
// stack. It must reach this line at all — the firmware used to stop inside lwIP
// — and it must open its server once a transport appears.
void test_a_console_started_before_the_stack_waits_for_one() {
    if (HAL::canOpenServer()) {
        TEST_IGNORE_MESSAGE("this platform has an IP stack from boot: nothing is deferred here");
    }
    TEST_ASSERT_NULL_MESSAGE(console->getServer(),
        "a server was opened on a platform that says it cannot open one");

    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);   // joins nothing; starts the stack
    for (int i = 0; i < 20 && !console->getServer(); ++i) {
        delay(50);
        testCore->loop();
    }

    TEST_ASSERT_NOT_NULL_MESSAGE(console->getServer(),
        "no server a second after a network interface existed");
}

int runAllTests() {
    UNITY_BEGIN();
    RUN_TEST(test_a_console_started_before_the_stack_waits_for_one);
    RUN_TEST(test_a_line_the_platform_printed_reaches_the_console);
    RUN_TEST(test_the_level_is_read_from_the_shape_on_the_board);
    RUN_TEST(test_a_line_over_a_slot_keeps_its_head);
    RUN_TEST(test_the_sink_is_taken_once_and_given_back);
    RUN_TEST(test_a_full_intake_refuses_and_counts);
    RUN_TEST(test_the_sdk_switch_takes_the_sink_back);
    RUN_TEST(test_the_intake_is_the_size_the_platform_declared);
    return UNITY_END();
}

void setup() {
    delay(2000);   // the runner opens the port after the board has booted
    runAllTests();
}

void loop() {}
