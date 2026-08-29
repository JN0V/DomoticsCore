/**
 * @file test_wifi_scan_esp8266.cpp
 * @brief ESP8266 hardware measurement of the WiFi scan-summary loops (MEM-2).
 *
 * Runs on a `nodemcuv2` seeing real networks. Nothing here is reachable from a
 * host build: the native `String` is `std::string` (`Platform_Stub.h:27`), so a
 * native run proves behaviour and never cost.
 *
 * WHAT THIS FILE MEASURES, AND WHAT PINS THE FORMAT
 *
 * The *text* both loops produce is pinned in `test_wifi_component.cpp`, which
 * runs in CI against a scriptable stub — exact entry text, the ", " join, the
 * ten-entry cap, and the zero-network and scan-failure branches. That is not
 * this file's job and this file does not duplicate it.
 *
 * This file measures cost, which needs a radio. Two instruments:
 *
 *   1. `ESP.getCycleCount()` across the loop — the reallocation and copy work
 *      the fix removes. Sampled from a LoggerCallbacks hook rather than around
 *      `scanNetworks()`, because that call blocks for ~2 s inside the SDK scan
 *      and 160 M cycles of radio would bury the ~10^4 the loop costs. The hook
 *      fires on the "Found N Wifi networks" line, the statement immediately
 *      before the loop, and on each "  <entry>" line, the last statement of
 *      each iteration.
 *
 *   2. A free-heap sample taken INSIDE the last iteration, while the entry
 *      String is still live. This is the discriminating one, and it is
 *      calibration-free:
 *
 *        before the fix, at the last iteration:  i+1 entries in the vector
 *                                                (push_back copied each one)
 *                                                PLUS the live `network`
 *        after the fix,  at the last iteration:  i entries in the vector
 *                                                PLUS the live `network`,
 *                                                which the move then hands
 *                                                straight to the vector
 *
 *      So the copy shows up as one extra live buffer at the sample point, and
 *      it is gone by the time `scanNetworks()` returns — in BOTH versions. That
 *      is why this suite compares the in-frame sample against the post-return
 *      heap and never asserts on the post-return heap itself: net free heap is
 *      identical with the fix and without it, so any such assertion would pass
 *      against unfixed code. (2026-08-26's lesson: ask what would still pass if
 *      the change under test were removed.)
 *
 * WHAT THE ASYNC TEST DOES NOT DO, STATED RATHER THAN IMPLIED
 *
 * The async summary loop has no per-iteration log line, so there is no in-frame
 * sample to take there and **no self-contained discriminating assertion exists
 * for it in this file**. A free-heap sample after that loop is worse than
 * useless: the fixed version reserves the worst case up front, so it would read
 * *higher* than the unfixed one and a naive threshold would reward the wrong
 * code. What stands in for it is twofold — the two loops build their entries
 * with the same construction, and `test_wifi_component.cpp` asserts that the
 * async summary is exactly the ", " join of what the synchronous path produces,
 * so they cannot drift apart unnoticed; and the async cycle figure is reported
 * for the two-run removal check below. Do not read the async test as evidence
 * on its own.
 *
 * HOW TO RUN THE REMOVAL CHECK
 *
 *   # The nodemcuv2 is the FTDI adapter A5069RR4. There are three boards here
 *   # and /dev/ttyUSB0 is whichever was plugged in first, so resolve the
 *   # adapter rather than trusting a device node.
 *   PORT=/dev/serial/by-id/$(ls /dev/serial/by-id | grep A5069RR4)
 *   cd DomoticsCore-Wifi && rm -rf .pio
 *   pio test -e esp8266dev --upload-port "$PORT" --test-port "$PORT"
 *
 * `rm -rf .pio` is not optional and it is needed in BOTH directions. Wifi.h
 * arrives here through a `file://` dependency, which `pio` copies into
 * `.pio/libdeps` once and never refreshes; a reverted header that is not
 * recopied reports the fixed figure twice and the check concludes the opposite
 * of the truth.
 *
 * PlatformIO filters the serial stream down to Unity lines, so the printf
 * figures below do not reach the test report. Every figure is therefore also
 * carried in an assertion message. Read the port directly for the rest.
 */

#include <unity.h>
#include <Arduino.h>
#include <string.h>
#include <vector>

#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Wifi.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// ============================================================================
// The instrument
// ============================================================================

// File-scope, and registered once for the whole run. A Unity assertion failure
// longjmps out of the test without unwinding the stack, so a probe that owned
// its registration in a destructor would leave the callback vector holding a
// pointer into a dead frame — and the next log line anywhere would follow it.
// That is the cascade shape this repository has already been bitten by.
//
// Not volatile: LoggerCallbacks::broadcast() is a plain synchronous call on the
// same thread as the code under test. Nothing here runs in an ISR, and marking
// it volatile would advertise a concurrency model this file does not have.
struct ScanProbe {
    bool armed = false;
    bool sawHeader = false;
    int entries = 0;
    uint32_t cyclesAtHeader = 0;
    uint32_t cyclesAtLastEntry = 0;
    uint32_t heapAtLastEntry = 0;

    void reset() {
        armed = false;
        sawHeader = false;
        entries = 0;
        cyclesAtHeader = 0;
        cyclesAtLastEntry = 0;
        heapAtLastEntry = 0;
    }

    uint32_t loopCycles() const {
        // Unsigned arithmetic, so the 32-bit counter's ~53 s wrap at 80 MHz is
        // harmless over a span this short.
        return cyclesAtLastEntry - cyclesAtHeader;
    }
};

static ScanProbe probe;

// Set by the radio gate, read by everything that depends on it. See the note on
// test_wifi_scan_sees_networks.
static bool gRadioSeesNetworks = false;
static int gRadioNetworkCount = -1;

// The hook costs the same in both versions, so it inflates the absolute cycle
// figure and cancels out of the comparison the removal check makes.
static void onLog(LogLevel level, const char* tag, const char* message) {
    if (!probe.armed || strcmp(tag, LOG_WIFI) != 0) return;

    if (!probe.sawHeader) {
        if (level == LOG_LEVEL_INFO && strncmp(message, "Found ", 6) == 0) {
            probe.sawHeader = true;
            probe.cyclesAtHeader = ESP.getCycleCount();
        }
        return;
    }

    // Every scan entry is logged as "  <ssid> (<rssi> dBm)". No other WIFI line
    // starts with two spaces, and the header gate above means only lines from
    // inside this one loop are counted.
    if (level == LOG_LEVEL_DEBUG && message[0] == ' ' && message[1] == ' ') {
        probe.entries++;
        probe.cyclesAtLastEntry = ESP.getCycleCount();
        probe.heapAtLastEntry = ESP.getFreeHeap();
    }
}

// ============================================================================
// The fixture
// ============================================================================

// A WifiComponent whose loop() reaches the async scan poll and does nothing
// else on the way.
//
// **The SSID must be non-empty.** WifiComponent::loop() returns at
// `Wifi.h:251-254` whenever `ssid.isEmpty()`, forty lines before the
// `if (scanInProgress)` poll — so a fixture built on the default WifiConfig
// never reaches the loop it was written to measure, and the async test below
// would spin for its full timeout and die with "never completed" having
// executed none of the code under test. That early return is also a live
// product defect in its own right, recorded in
// `_bmad-output/implementation-artifacts/deferred-work.md`: the WebUI scan
// button is pressed during AP provisioning, which is exactly when no SSID is
// configured.
//
// `autoConnect = false` leaves `shouldConnect` false, so no connection is
// attempted and the reconnect branch never fires. It also leaves `wifiEnabled`
// false, and since `setConfig()` only sets `pendingModeUpdate_` when
// `wifiEnabled || apEnabled` (`Wifi.h:670-672`), no deferred `updateWifiMode()`
// runs either — the radio is left exactly as the HAL calls below put it.
//
// begin() is deliberately not called: with no credentials it starts an access
// point, and with credentials it starts a connection attempt.
struct IdleWifi {
    WifiComponent wifi;

    IdleWifi() {
        WifiConfig cfg;
        cfg.ssid = "scan-fixture";  // non-empty, and never connected to
        cfg.autoConnect = false;
        cfg.enableAP = false;
        wifi.setConfig(cfg);

        HAL::WiFiHAL::init();
        HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);
        HAL::WiFiHAL::disconnect();
        HAL::WiFiHAL::scanDelete();  // drop anything a previous test left cached

        // Warm-up, outside every measurement: the first scan of a boot pulls in
        // SDK buffers that are a one-off cost and not the loop's.
        HAL::WiFiHAL::scanNetworks(false);
        HAL::WiFiHAL::scanDelete();
        yield();
    }
};

void setUp() { probe.reset(); }
void tearDown() { probe.armed = false; }

// Every measurement below is meaningless without networks in range. Rather than
// let each test rediscover that and report it as its own failure — three reds
// for one cause, on a board where this repository has already mistaken a
// cascade for independent failures — the gate runs first and the others stand
// down when it did not pass.
static bool requireRadio() {
    if (gRadioSeesNetworks) return true;
    char msg[192];
    snprintf(msg, sizeof(msg),
             "Skipped: the radio gate found %d networks, so there is nothing to measure. "
             "One cause, one failure -- see test_wifi_scan_sees_networks",
             gRadioNetworkCount);
    TEST_IGNORE_MESSAGE(msg);
    return false;  // not reached; TEST_IGNORE_MESSAGE longjmps
}

// ============================================================================
// The radio itself — a scan that finds nothing measures nothing
// ============================================================================

void test_wifi_scan_sees_networks() {
    IdleWifi f;

    const int16_t n = HAL::WiFiHAL::scanNetworks(false);
    HAL::WiFiHAL::scanDelete();

    gRadioNetworkCount = (int)n;
    gRadioSeesNetworks = (n > 0);

    Serial.printf("\n[WIFI SCAN VISIBILITY]\n  Networks seen: %d\n", (int)n);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Scan saw %d networks -- every other test in this file measures the loop that "
             "formats them, so with none in range the whole suite measures nothing. The other "
             "tests will report as IGNORED, not as further failures",
             (int)n);

    TEST_ASSERT_TRUE_MESSAGE(n > 0, msg);
}

// ============================================================================
// scanNetworks() — the site the copy lives on
// ============================================================================

void test_wifi_scan_networks_entry_is_not_copied() {
    requireRadio();

    IdleWifi f;

    std::vector<String> networks;

    probe.armed = true;
    const bool ok = f.wifi.scanNetworks(networks);
    const uint32_t heapAfter = ESP.getFreeHeap();
    probe.armed = false;

    const uint32_t heapAtLast = probe.heapAtLastEntry;
    const int entries = probe.entries;
    const uint32_t cycles = probe.loopCycles();
    const int32_t held = (int32_t)heapAfter - (int32_t)heapAtLast;

    Serial.printf("\n[WIFI SCAN LOOP -- scanNetworks()]\n");
    Serial.printf("  Networks:                    %u\n", (unsigned)networks.size());
    Serial.printf("  Entries seen by the probe:   %d\n", entries);
    Serial.printf("  Cycles across the loop:      %u\n", (unsigned)cycles);
    Serial.printf("  Free heap in last iteration: %u\n", (unsigned)heapAtLast);
    Serial.printf("  Free heap after the return:  %u\n", (unsigned)heapAfter);
    Serial.printf("  Held at the last iteration:  %d\n", (int)held);

    TEST_ASSERT_TRUE_MESSAGE(ok, "scanNetworks() returned false -- the scan failed");

    // Non-vacuity, before anything is measured. Each of these is a way this
    // test could pass while measuring nothing at all.
    char emptyMsg[160];
    snprintf(emptyMsg, sizeof(emptyMsg),
             "Scan returned %u networks -- the loop under test was never entered, so this "
             "test measured nothing", (unsigned)networks.size());
    TEST_ASSERT_TRUE_MESSAGE(networks.size() > 0, emptyMsg);

    TEST_ASSERT_TRUE_MESSAGE(probe.sawHeader,
                             "The probe never saw the \"Found N Wifi networks\" line -- the "
                             "instrument is detached from the code, not the code from the fix");

    char countMsg[160];
    snprintf(countMsg, sizeof(countMsg),
             "Probe saw %d entry lines for %u networks -- the sample below is not from the "
             "last iteration", entries, (unsigned)networks.size());
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)networks.size(), entries, countMsg);

    // The premise of the measurement, asserted before the measurement.
    //
    // The copy this test looks for only exists if the entry is on the heap, and
    // on an ESP8266 a String of 10 characters or fewer never leaves the
    // small-string buffer (WString.h:309-316). A hidden access point reports an
    // empty SSID, so its entry is " (-70 dBm)" — **exactly 10**. If such a
    // network happens to be scanned last, the copy costs nothing, `held` is ~0,
    // and the assertion at the bottom passes against unfixed code. That is the
    // neighbourhood deciding the result, which is the failure this gate refuses.
    const String& lastEntry = networks[networks.size() - 1];
    char ssoMsg[256];
    snprintf(ssoMsg, sizeof(ssoMsg),
             "The last entry is \"%s\", %u characters -- at or below the ESP8266's 10-character "
             "small-string buffer, so no copy is made either way and the measurement below "
             "would pass with the fix removed",
             lastEntry.c_str(), (unsigned)lastEntry.length());
    TEST_ASSERT_TRUE_MESSAGE(lastEntry.length() > 10, ssoMsg);

    // Shape, kept loose on purpose: a hidden AP legitimately produces an empty
    // SSID and an entry that begins with " (". The exact text is pinned in the
    // native suite, which can script the scan results; asserting it here would
    // make a correct build red in the wrong neighbourhood.
    for (size_t i = 0; i < networks.size(); ++i) {
        char shapeMsg[224];
        snprintf(shapeMsg, sizeof(shapeMsg),
                 "Entry %u is \"%s\" -- the formatted entry no longer matches "
                 "\"<ssid> (<rssi> dBm)\"", (unsigned)i, networks[i].c_str());
        TEST_ASSERT_TRUE_MESSAGE(networks[i].endsWith(" dBm)"), shapeMsg);
        TEST_ASSERT_TRUE_MESSAGE(networks[i].indexOf(" (") >= 0, shapeMsg);
    }

    // The measurement. `network` is built, logged, then moved into the vector,
    // so at the log line the vector holds i entries and `network` is the
    // (i+1)th — the same count that is live once the function returns. Before
    // the fix the entry was pushed as an lvalue BEFORE the log line, so the
    // copy and the original were both live here and one of them was freed on
    // the way out: heapAfter comes back higher than heapAtLast by one entry
    // buffer.
    //
    // Nothing yields between the last log line and the return, so no SDK task
    // can allocate inside the window.
    //
    // String rounds capacity to a 16-byte multiple (WString.cpp:229) and
    // umm_malloc adds its own header, so the copy cannot cost less than about
    // 24 bytes when it is made at all.
    const int32_t COPY_THRESHOLD = 8;

    char msg[384];
    snprintf(msg, sizeof(msg),
             "scanNetworks() still copies each entry: %ld B were live at the last iteration "
             "and freed by the return, over %u networks, threshold=%ld B, loop cost %lu cycles. "
             "NOTE the threshold is derived (16-byte rounding + umm header), never yet confirmed "
             "on a board -- if %ld B is small, suspect the threshold; if it is ~24 B or more, "
             "suspect the fix",
             (long)held, (unsigned)networks.size(), (long)COPY_THRESHOLD,
             (unsigned long)cycles, (long)held);

    TEST_ASSERT_TRUE_MESSAGE(held <= COPY_THRESHOLD, msg);
}

// ============================================================================
// The async summary — the loop nothing had ever executed
// ============================================================================

// Until this file existed, nothing in the repository ran this loop:
// test_wifi_component.cpp:228 called startScanAsync() and never called loop(),
// and the WiFi stub reported zero networks so the body would not have been
// entered anyway. It now also runs natively against the scriptable stub, which
// is what pins its text.
//
// Read the note at the top of this file before treating the figure below as
// evidence: this test has no self-contained discriminating assertion, and the
// cycle count is reported for the two-run removal check rather than compared
// against a threshold.
void test_wifi_async_scan_summary() {
    requireRadio();

    IdleWifi f;

    f.wifi.startScanAsync();

    uint32_t completingPassCycles = 0;
    String summary;
    const unsigned long deadline = millis() + 20000UL;

    while (millis() < deadline) {
        const uint32_t before = ESP.getCycleCount();
        f.wifi.loop();
        const uint32_t after = ESP.getCycleCount();

        summary = f.wifi.getLastScanSummary();
        if (summary != "Scanning...") {
            completingPassCycles = after - before;
            break;
        }
        delay(50);
    }

    // Count the entries the summary claims, by the suffix every one of them ends
    // with. Zero means the scan came back empty and the loop never ran.
    int entries = 0;
    for (int at = summary.indexOf(" dBm)"); at >= 0;
         at = summary.indexOf(" dBm)", at + 5)) {
        entries++;
    }

    Serial.printf("\n[WIFI SCAN LOOP -- async summary]\n");
    Serial.printf("  Summary: %s\n", summary.c_str());
    Serial.printf("  Entries: %d\n", entries);
    Serial.printf("  Cycles in the completing loop() pass: %u\n",
                  (unsigned)completingPassCycles);

    char stuckMsg[256];
    snprintf(stuckMsg, sizeof(stuckMsg),
             "The async scan never completed within 20 s -- summary is still \"%s\". If it "
             "reads \"Scanning...\", loop() is not reaching the poll branch at all",
             summary.c_str());
    TEST_ASSERT_TRUE_MESSAGE(summary != "Scanning...", stuckMsg);

    char failMsg[288];
    snprintf(failMsg, sizeof(failMsg),
             "Async scan summary is \"%s\" over %d entries -- the summary loop was never "
             "entered, so this test measured nothing", summary.c_str(), entries);
    TEST_ASSERT_TRUE_MESSAGE(summary != "Scan failed", failMsg);
    TEST_ASSERT_TRUE_MESSAGE(entries > 0, failMsg);

    // The cap is behaviour, not an optimisation: the summary shows at most ten.
    // Pinned exactly in the native suite; here it is a sanity bound on a
    // neighbourhood that may hold any number of networks.
    char capMsg[192];
    snprintf(capMsg, sizeof(capMsg),
             "Summary carries %d entries for a radio that saw %d networks -- the ten-entry cap "
             "is gone", entries, gRadioNetworkCount);
    TEST_ASSERT_TRUE_MESSAGE(entries <= 10, capMsg);

    char shapeMsg[288];
    snprintf(shapeMsg, sizeof(shapeMsg),
             "Async summary is \"%s\" (%lu cycles in the completing pass) -- the formatted text "
             "changed shape", summary.c_str(), (unsigned long)completingPassCycles);
    TEST_ASSERT_TRUE_MESSAGE(summary.endsWith(" dBm)"), shapeMsg);
    TEST_ASSERT_TRUE_MESSAGE(!summary.startsWith(", "), shapeMsg);
    TEST_ASSERT_TRUE_MESSAGE(summary.indexOf(", , ") < 0, shapeMsg);

    // Reported, not judged. The figure exists to be compared against the same
    // run with the loops reverted and `.pio` cleared; there is no threshold
    // here because none has been calibrated, and inventing one would be the
    // third instrument this lot had to withdraw.
    char cyclesMsg[256];
    snprintf(cyclesMsg, sizeof(cyclesMsg),
             "MEASUREMENT (not a verdict): the completing loop() pass cost %lu cycles for %d "
             "entries. Compare against the reverted loops from a cleared .pio",
             (unsigned long)completingPassCycles, entries);
    TEST_ASSERT_TRUE_MESSAGE(completingPassCycles > 0, cyclesMsg);
}

// ============================================================================
// Test Runner
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n========================================");
    Serial.println("WiFi ESP8266 Scan Loop Measurement (MEM-2)");
    Serial.println("========================================\n");

    // Registered once, never removed: see the note on ScanProbe.
    LoggerCallbacks::addCallback(onLog);

    UNITY_BEGIN();

    RUN_TEST(test_wifi_scan_sees_networks);
    RUN_TEST(test_wifi_scan_networks_entry_is_not_copied);
    RUN_TEST(test_wifi_async_scan_summary);

    UNITY_END();
}

void loop() {}
