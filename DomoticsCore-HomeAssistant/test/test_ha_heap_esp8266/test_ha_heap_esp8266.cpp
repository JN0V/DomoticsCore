/**
 * @file test_ha_heap_esp8266.cpp
 * @brief ESP8266 hardware measurement for MEM-2: HomeAssistant parses an inbound
 *        MQTT message without asking the allocator for the topic or the id.
 *
 * The host suites cannot reach this claim at all. The native String is
 * std::string (Platform_Stub.h:27), so nothing on a PC measures what a board's
 * umm_malloc does — and both Arduino cores carry a 14-character small-string
 * buffer, so the claim only exists for strings longer than that. Every string
 * here is deliberately on the allocating side of that threshold.
 *
 * **A net-heap assertion would be vacuous.** The Strings this lot removed were
 * function-local temporaries, released before handleCommand returned, so free
 * heap is back at its baseline by the time any test could read it — with the fix
 * and without it. A sample taken before the emit is no better: EventBus copies
 * the 828-byte MQTTMessageEvent into a std::vector (EventBus.h:28-34), and that
 * allocation would be charged to the parse.
 *
 * So both samples are taken **inside a single dispatch of one event**, with the
 * queue's own copy live for both, where it cancels:
 *
 *   1. an mqtt/message subscriber registered *before* the component — dispatch
 *      follows registration order (EventBus.h:58) — samples free heap first;
 *   2. the component parses the topic and finds no such entity;
 *   3. the LoggerCallbacks hook on the unknown-entity warning samples again,
 *      with the topic and the extracted id both still alive.
 *
 * The difference is exactly what HomeAssistant's parse is holding: zero after
 * the conversion, one topic allocation plus one id allocation before it.
 *
 * This measures the **discard path**, which is where the parse is pure waste and
 * where the logger gives a hook inside the frame. The accepted path runs the
 * same parse; its equivalent hook is a DLOG_D behind a registered entity, and
 * that inference is stated rather than hidden.
 *
 * The warning's text is load bearing: the probe below matches on it, so changing
 * that message disarms the measurement silently. The test asserts the text it
 * matched, and asserts that the baseline sample really was taken before it.
 */

#include <unity.h>
#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/HAEvents.h>
#include <DomoticsCore/MQTTEvents.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

// An id of 26 characters, and it has to be: at 14 or fewer both cores keep it in
// the small-string buffer, the substring this lot removed would never have
// reached the allocator, and the whole measurement would read zero against
// unfixed code. That is the vacuous pass this repository has already shipped
// once (SEC-8's removal check), and the length is the guard against it.
static const char* UNKNOWN_ID = "unregistered_ceiling_light";
static const char* NODE_ID    = "heapnode";

// ---------------------------------------------------------------------------
// Probes. Static storage, never a reference to a test's stack: a failed Unity
// assertion longjmps out of the test, and a logger callback holding a dangling
// reference would then be written through by every later DLOG in the suite.
// ---------------------------------------------------------------------------

static volatile uint32_t g_heapAtDispatch = 0;
static volatile uint32_t g_heapAtWarning = 0;
static volatile uint32_t g_dispatches = 0;
static volatile uint32_t g_dispatchesAtWarning = 0;
static volatile uint32_t g_warnings = 0;
static char g_warningText[128] = {};

static bool g_probeActive = false;
static LoggerCallbacks::CallbackId g_probeId = 0;

static void resetProbeState() {
    g_heapAtDispatch = 0;
    g_heapAtWarning = 0;
    g_dispatches = 0;
    g_dispatchesAtWarning = 0;
    g_warnings = 0;
    g_warningText[0] = '\0';
}

// Sample 1 — the baseline. The queue's copy of the event exists, HomeAssistant
// has not run yet.
static void baselineProbe(const void*) {
    g_heapAtDispatch = ESP.getFreeHeap();
    g_dispatches++;
}

// Sample 2 — inside handleCommand, at the unknown-entity warning. Read the heap
// before doing anything else: strncpy below writes into a fixed buffer and
// allocates nothing, but it runs after the sample regardless.
static void stopWarningProbe();

static void startWarningProbe() {
    // Never orphan a live one. CallbackId is a uint8_t handed out by a counter,
    // and a probe left installed by a test that longjmped out would keep firing
    // and overwriting the samples of every test after it.
    stopWarningProbe();
    g_probeId = LoggerCallbacks::addCallback(
        [](LogLevel level, const char* tag, const char* message) {
            if (level != LOG_LEVEL_WARN) return;
            if (!strstr(message, "unknown entity")) return;
            uint32_t heap = ESP.getFreeHeap();
            g_heapAtWarning = heap;
            g_dispatchesAtWarning = g_dispatches;
            strncpy(g_warningText, message, sizeof(g_warningText) - 1);
            g_warningText[sizeof(g_warningText) - 1] = '\0';
            g_warnings++;
        });
    g_probeActive = true;
}

static void stopWarningProbe() {
    if (!g_probeActive) return;
    LoggerCallbacks::removeCallback(g_probeId);
    g_probeActive = false;
}

void setUp() {
    resetProbeState();
}

// Every device test releases its state here. The logger callback list is a
// process-wide singleton, so a test that longjmps out of an assertion would
// otherwise leave its probe installed and let it fire inside the next one.
void tearDown() {
    stopWarningProbe();
}

// ---------------------------------------------------------------------------
// Fixture: a Core with one HomeAssistant component, and the baseline probe
// subscribed ahead of it.
// ---------------------------------------------------------------------------

struct HAUnderTest {
    Core core;

    // Shut the component down on the way out, so the fixture's lifecycle is
    // stated here rather than inferred: HomeAssistant publishes an offline
    // availability and removes its discovery in shutdown(). ~Core() calls
    // shutdown() too when it is still initialized, and shutdown() returns early
    // if it is not, so this is explicit rather than load bearing — and neither
    // runs at all if a Unity assertion longjmps out of a test, which is why the
    // logger probe is cleaned up in tearDown() instead of here.
    ~HAUnderTest() { core.shutdown(); }

    HAUnderTest() {
        HAConfig cfg;
        HA::setField(cfg.nodeId, NODE_ID, sizeof(cfg.nodeId));

        auto ha = std::make_unique<HomeAssistantComponent>(cfg);
        ha->addSwitch("sw1", "Switch 1");

        // Before addComponent, and that is the whole trick: the component
        // subscribes to mqtt/message inside begin(), and dispatch walks the
        // topic's handler vector in registration order.
        core.getEventBus().subscribe(String(DomoticsCore::MQTTEvents::EVENT_MESSAGE),
                                     baselineProbe, nullptr);

        core.addComponent(std::move(ha));
        core.begin();
    }
};

static void sendCommand(Core& core, const char* entityId, const char* payload) {
    MQTTMessageEvent msg{};
    snprintf(msg.topic, sizeof(msg.topic), "homeassistant/switch/%s/%s/set", NODE_ID, entityId);
    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
    msg.payload[sizeof(msg.payload) - 1] = '\0';
    core.emit<MQTTMessageEvent>(DomoticsCore::MQTTEvents::EVENT_MESSAGE, msg);
    // Drain, as firmware does every pass. Without it the queue holds the event
    // and its occupancy is charged to whatever is measured next — the error that
    // produced STOR-ESP-1.
    for (int i = 0; i < 5; i++) {
        core.loop();
        yield();
    }
}

// ============================================================================
// Baseline
// ============================================================================

void test_ha_heap_baseline() {
    uint32_t freeHeap = ESP.getFreeHeap();

    Serial.printf("\n[HA HEAP BASELINE]\n");
    Serial.printf("  Free heap: %u bytes\n", freeHeap);

    TEST_ASSERT_TRUE(freeHeap > 0);
    TEST_ASSERT_TRUE(freeHeap < 82000);
}

// ============================================================================
// MEM-2 — the measurement
// ============================================================================

void test_ha_discarded_command_holds_no_heap() {
    HAUnderTest ha;
    startWarningProbe();

    // Warm-up outside the measurement, as the Storage suite does. The first
    // message through a fresh bus pays one-off costs — the topic's node in the
    // subscription map, the first split of a fresh umm region — that are not the
    // per-message cost this measures.
    sendCommand(ha.core, UNKNOWN_ID, "ON");

    resetProbeState();
    sendCommand(ha.core, UNKNOWN_ID, "ON");

    // Off before the first assertion: from here on a failure may longjmp.
    stopWarningProbe();

    const int32_t held = (int32_t)g_heapAtDispatch - (int32_t)g_heapAtWarning;

    Serial.printf("\n[HA COMMAND PARSE — IN-DISPATCH SAMPLES]\n");
    Serial.printf("  At mqtt/message dispatch: %u bytes free\n", (unsigned)g_heapAtDispatch);
    Serial.printf("  At unknown-entity warning: %u bytes free\n", (unsigned)g_heapAtWarning);
    Serial.printf("  Held by the parse: %d bytes\n", (int)held);

    // Non-vacuity, four ways, all of them before the figure is looked at.
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, g_dispatches,
        "the mqtt/message event never dispatched — no sample was taken and the "
        "delta below is two zeroes subtracted from each other");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, g_warnings,
        "HomeAssistant never reached the unknown-entity warning: either the "
        "message was not parsed, or the warning's text changed and this probe no "
        "longer matches it");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, g_dispatchesAtWarning,
        "the warning fired before the baseline sample — the probe is subscribed "
        "after the component, so the 'baseline' is really a reading taken once "
        "the parse had already finished, and would read zero either way");
    TEST_ASSERT_TRUE_MESSAGE(strstr(g_warningText, UNKNOWN_ID) != NULL,
        "the warning does not name the id that was extracted");

    // Exactly zero, not a tolerance. Between the two samples the component logs
    // (stack buffers), scans the topic for two slashes, copies the id into a
    // stack array and compares it against one registered entity. Nothing on that
    // path reaches the allocator, so any non-zero figure here is a regression
    // and not noise. Against the String parse this replaced, two allocations
    // land in this window: the topic, which is 60 characters on this topic and
    // 38 at its shortest in the field, and the 26-character id.
    char msg[224];
    snprintf(msg, sizeof(msg),
             "the parse held %ld B between the two in-dispatch samples (%lu free at "
             "dispatch, %lu at the warning) -- it is allocating again: id=%u chars, "
             "topic=%u chars, both over the cores' 14-character small-string buffer",
             (long)held, (unsigned long)g_heapAtDispatch, (unsigned long)g_heapAtWarning,
             (unsigned)strlen(UNKNOWN_ID),
             (unsigned)(strlen("homeassistant/switch/") + strlen(NODE_ID) + 1 +
                        strlen(UNKNOWN_ID) + strlen("/set")));

    TEST_ASSERT_EQUAL_INT32_MESSAGE(0, held, msg);
}

// ============================================================================
// The queue never approached its cap — asserted once, not as a property of N
// ============================================================================

void test_ha_repeated_commands_are_never_dropped() {
    // A plateau must not be readable as success. This suite drains after every
    // message, so EventBus's 32-entry cap (EventBus.h:253) is never approached
    // and nothing is dropped; if it ever were, dispatches would fall short of
    // the messages sent and the measurement above would be sampling a different
    // event from the one it thinks it is.
    HAUnderTest ha;
    startWarningProbe();

    // Warm-up outside the window, for the same one-off costs as the test above.
    sendCommand(ha.core, UNKNOWN_ID, "ON");

    const uint32_t MESSAGES = 20;
    resetProbeState();
    const uint32_t heapBeforeLoop = ESP.getFreeHeap();
    for (uint32_t i = 0; i < MESSAGES; i++) {
        sendCommand(ha.core, UNKNOWN_ID, "ON");
    }
    // sendCommand drains as it goes, so the queue is empty here and this reads
    // what the run left behind rather than what it is still holding.
    const uint32_t heapAfterLoop = ESP.getFreeHeap();

    stopWarningProbe();

    const int32_t heldOnLast = (int32_t)g_heapAtDispatch - (int32_t)g_heapAtWarning;
    const int32_t leakedAcrossLoop = (int32_t)heapBeforeLoop - (int32_t)heapAfterLoop;

    Serial.printf("\n[HA REPEATED COMMANDS]\n");
    Serial.printf("  Sent: %u  dispatched: %u  warned: %u\n",
                  (unsigned)MESSAGES, (unsigned)g_dispatches, (unsigned)g_warnings);
    Serial.printf("  Held by the last parse: %d bytes\n", (int)heldOnLast);
    Serial.printf("  Left behind by the whole loop: %d bytes\n", (int)leakedAcrossLoop);

    char dropMsg[192];
    snprintf(dropMsg, sizeof(dropMsg),
             "%lu of %lu messages dispatched: the queue reached its 32-entry cap and "
             "dropped events, so nothing here measured a full parse",
             (unsigned long)g_dispatches, (unsigned long)MESSAGES);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(MESSAGES, g_dispatches, dropMsg);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(MESSAGES, g_warnings,
        "a message was dispatched without reaching the unknown-entity warning");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(MESSAGES, g_dispatchesAtWarning,
        "the last warning fired before its own baseline sample — the probe is "
        "ordered after the component, and a delta measured that way reads zero "
        "whether the parse allocates or not");

    char msg[192];
    snprintf(msg, sizeof(msg),
             "the parse held %ld B inside the dispatch of the last of %lu messages "
             "-- the per-message cost is back",
             (long)heldOnLast, (unsigned long)MESSAGES);
    TEST_ASSERT_EQUAL_INT32_MESSAGE(0, heldOnLast, msg);

    // A different question from the one above, and worth being explicit about
    // which: the in-dispatch delta is the evidence for MEM-2, and this is not.
    // Free heap returns to its baseline after every message with the conversion
    // *and* without it — the Strings it replaced were function-local temporaries
    // — so this assertion passes against unfixed code and proves nothing about
    // the allocation. What it does catch is the fault that in-dispatch sampling
    // cannot see at all: something on this path retaining a little on every
    // message, which twenty messages would show and one would not.
    char leakMsg[224];
    snprintf(leakMsg, sizeof(leakMsg),
             "%lu commands left %ld B unaccounted for (%lu free before, %lu after, "
             "queue drained both times) -- something on the command path keeps a "
             "little of every message",
             (unsigned long)MESSAGES, (long)leakedAcrossLoop,
             (unsigned long)heapBeforeLoop, (unsigned long)heapAfterLoop);
    TEST_ASSERT_EQUAL_INT32_MESSAGE(0, leakedAcrossLoop, leakMsg);
}

// ============================================================================
// The accepted path still works — behaviour on silicon, not cost
// ============================================================================

static volatile bool g_commandEventFired = false;
static char g_commandEntityId[64] = {};
static char g_commandPayload[128] = {};

void test_ha_known_entity_command_still_routes() {
    // The conversion runs the same parse for a message that is ours. Its cost is
    // not measurable from here — the accepted path's hook is a DLOG_D behind a
    // registered entity — but its behaviour is, and this is the one board test
    // that a broken extraction would fail outright.
    g_commandEventFired = false;
    g_commandEntityId[0] = '\0';
    g_commandPayload[0] = '\0';

    HAUnderTest ha;
    ha.core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [](const void* data) {
        auto& ev = *reinterpret_cast<const HAEvents::HACommandEvent*>(data);
        g_commandEventFired = true;
        strncpy(g_commandEntityId, ev.entityId, sizeof(g_commandEntityId) - 1);
        g_commandEntityId[sizeof(g_commandEntityId) - 1] = '\0';
        strncpy(g_commandPayload, ev.command, sizeof(g_commandPayload) - 1);
        g_commandPayload[sizeof(g_commandPayload) - 1] = '\0';
    }, nullptr);

    sendCommand(ha.core, "sw1", "ON");

    TEST_ASSERT_TRUE_MESSAGE(g_commandEventFired,
        "a command for a registered entity produced no ha/command event");
    TEST_ASSERT_EQUAL_STRING("sw1", g_commandEntityId);
    TEST_ASSERT_EQUAL_STRING("ON", g_commandPayload);
}

// ============================================================================
// Test Runner
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n========================================");
    Serial.println("HomeAssistant ESP8266 MEM-2 Heap Tests");
    Serial.println("========================================\n");

    UNITY_BEGIN();

    RUN_TEST(test_ha_heap_baseline);
    RUN_TEST(test_ha_discarded_command_holds_no_heap);
    RUN_TEST(test_ha_repeated_commands_are_never_dropped);
    RUN_TEST(test_ha_known_entity_command_still_routes);

    UNITY_END();
}

void loop() {}
