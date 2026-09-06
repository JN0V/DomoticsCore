#include <unity.h>
#include <string>
#include <vector>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/FlightRecorder.h>
#include <DomoticsCore/Platform_Stub.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

class SimpleComponent : public IComponent {
public:
    SimpleComponent(const char* name) {
        metadata.name = name;
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { return {}; }
};

class FailingComponent : public IComponent {
public:
    FailingComponent() {
        metadata.name = "FailingComp";
        metadata.version = "1.0.0";
    }
    ComponentStatus begin() override { return ComponentStatus::ConfigError; }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { return {}; }
};

// OBS-3: a component that looks at the flight recorder from inside begin().
// Asserting after Core::begin() returns would pass whatever the order was.
class SeesRecorderComponent : public IComponent {
public:
    bool recorderHadBegun = false;
    bool sawPromotedRecord = false;
    SeesRecorderComponent() { metadata.name = "SeesRecorder"; metadata.version = "1.0.0"; }
    ComponentStatus begin() override {
        recorderHadBegun = FlightRecorder::instance().begun();
        sawPromotedRecord = FlightRecorder::instance().hasPromotedRecord();
        return ComponentStatus::Success;
    }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { return {}; }
};

Core* testCore = nullptr;

void setUp(void) {
    FlightRecorder::instance().resetForTest();
    HAL::Platform::clearRtcForTest();
    HAL::Platform::resetDiagnosticsForTest();
    HAL::Platform::resetFailedAllocForTest();
    testCore = new Core();
}

void tearDown(void) {
    delete testCore;
    testCore = nullptr;
}

void test_component_count_after_init(void) {
    testCore->addComponent(std::make_unique<SimpleComponent>("A"));
    testCore->addComponent(std::make_unique<SimpleComponent>("B"));
    testCore->addComponent(std::make_unique<SimpleComponent>("C"));
    testCore->begin();
    TEST_ASSERT_EQUAL(3, testCore->getComponentCount());
}

void test_get_component_after_init(void) {
    testCore->addComponent(std::make_unique<SimpleComponent>("MyComponent"));
    testCore->begin();
    IComponent* comp = testCore->getComponent("MyComponent");
    TEST_ASSERT_NOT_NULL(comp);
    TEST_ASSERT_EQUAL_STRING("MyComponent", comp->metadata.name);
}

void test_remove_component(void) {
    testCore->addComponent(std::make_unique<SimpleComponent>("ToRemove"));
    testCore->addComponent(std::make_unique<SimpleComponent>("ToKeep"));
    testCore->begin();
    TEST_ASSERT_EQUAL(2, testCore->getComponentCount());
    
    bool removed = testCore->removeComponent("ToRemove");
    TEST_ASSERT_TRUE(removed);
    TEST_ASSERT_EQUAL(1, testCore->getComponentCount());
    TEST_ASSERT_NULL(testCore->getComponent("ToRemove"));
    TEST_ASSERT_NOT_NULL(testCore->getComponent("ToKeep"));
}

void test_begin_fails_on_component_failure(void) {
    testCore->addComponent(std::make_unique<FailingComponent>());
    bool result = testCore->begin();
    TEST_ASSERT_FALSE(result);
}

void test_remove_nonexistent_component(void) {
    testCore->addComponent(std::make_unique<SimpleComponent>("Exists"));
    testCore->begin();
    bool removed = testCore->removeComponent("DoesNotExist");
    TEST_ASSERT_FALSE(removed);
    TEST_ASSERT_NULL(testCore->getComponent("NonExistent"));
}

void test_device_id_configuration(void) {
    auto testCore = std::make_unique<Core>();
    String customDeviceId = "test-device-123";
    
    // Test custom device ID
    CoreConfig config;
    config.deviceId = customDeviceId;
    testCore->begin(config);
    
    // Verify device ID is set
    TEST_ASSERT_EQUAL_STRING(customDeviceId.c_str(), testCore->getDeviceId().c_str());
}

void test_logging_initialization(void) {
    // Test that logging can be initialized without crashing
    HAL::initializeLogging(115200);
    
    // Test basic logging functionality
    DLOG_I("TEST", "Test log message");
    
    // Should not crash
    TEST_PASS();
}

void test_flight_recorder_promotes_before_any_component_begins() {
    // A previous run died in the crash callback; RTC kept it (the stub's array
    // survives resetForTest, not clearRtcForTest).
    FlightRecorder::instance().begin();
    CrashInfo death; death.reason = 254; death.failSize = 1024;
    FlightRecorder::instance().recordCrash(death);
    FlightRecorder::instance().resetForTest();
    HAL::Platform::setResetReasonForTest(HAL::Platform::ResetReason::Software);

    auto comp = std::unique_ptr<SeesRecorderComponent>(new SeesRecorderComponent());
    SeesRecorderComponent* raw = comp.get();
    testCore->addComponent(std::move(comp));
    TEST_ASSERT_TRUE(testCore->begin());

    TEST_ASSERT_TRUE_MESSAGE(raw->recorderHadBegun, "recorder must begin before the first component");
    TEST_ASSERT_TRUE_MESSAGE(raw->sawPromotedRecord, "the death must already be promoted when components start");
    // A bare Core acknowledges after logging: RTC now holds the fresh record.
    FlightRecord fresh;
    HAL::Platform::rtcRead(0, fresh.w, FlightRecord::WORDS);
    TEST_ASSERT_EQUAL_UINT32(1, fresh.bootSequence());
    TEST_ASSERT_EQUAL_UINT32(0, fresh.flags() & FlightRecord::UNPERSISTED);
}

// The component sees its own phase from inside loop(): the index the registry stored.
class SeesPhaseComponent : public IComponent {
public:
    uint16_t phaseSeen = 0xFFFF;
    SeesPhaseComponent() { metadata.name = "SeesPhase"; metadata.version = "1.0.0"; }
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override { phaseSeen = FlightRecorder::instance().current().phase(); }
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    std::vector<Dependency> getDependencies() const override { return {}; }
};

void test_loop_hands_each_component_its_own_phase() {
    testCore->addComponent(std::unique_ptr<SimpleComponent>(new SimpleComponent("First")));
    auto comp = std::unique_ptr<SeesPhaseComponent>(new SeesPhaseComponent());
    SeesPhaseComponent* raw = comp.get();
    testCore->addComponent(std::move(comp));
    TEST_ASSERT_TRUE(testCore->begin());
    testCore->loop();
    // The registry decides the order; the phase must name this component whatever it is.
    TEST_ASSERT_NOT_EQUAL(0xFFFF, raw->phaseSeen);
    TEST_ASSERT_EQUAL_STRING("SeesPhase", testCore->componentNameAtInitIndex(raw->phaseSeen));
}

void test_loop_ticks_the_recorder_into_rtc() {
    testCore->addComponent(std::unique_ptr<SimpleComponent>(new SimpleComponent("A")));
    TEST_ASSERT_TRUE(testCore->begin());
    HAL::Platform::setMillisForTest(1000);
    testCore->loop();
    HAL::Platform::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    testCore->loop();
    FlightRecord r;
    HAL::Platform::rtcRead(0, r.w, FlightRecord::WORDS);
    TEST_ASSERT_EQUAL_UINT32(1000 + FlightRecorder::FAST_INTERVAL_MS, r.lastUptimeMs());
    HAL::Platform::resetMillisForTest();
}

void test_loop_carries_the_bus_drops_into_the_record() {
    testCore->addComponent(std::unique_ptr<SimpleComponent>(new SimpleComponent("A")));
    TEST_ASSERT_TRUE(testCore->begin());
    for (int i = 0; i < 40; ++i) testCore->emit(String("storm"), i);   // past the cap of 32, on top of the lifecycle events already queued
    testCore->loop();
    TEST_ASSERT_TRUE(FlightRecorder::instance().current().eventDrops() >= 8);
    TEST_ASSERT_EQUAL_UINT32(testCore->getEventBus().getDroppedCount(), FlightRecorder::instance().current().eventDrops());
}

static std::vector<std::string> g_lines;

void test_begin_logs_the_promoted_record_line_by_line() {
    FlightRecorder::instance().begin();
    HAL::Platform::setMillisForTest(1000);
    HAL::Platform::advanceMillisForTest(FlightRecorder::FAST_INTERVAL_MS);
    FlightRecorder::instance().tick();                                  // one ring sample
    CrashInfo death; death.reason = 254; death.failSize = 512;
    FlightRecorder::instance().recordCrash(death);
    FlightRecorder::instance().resetForTest();
    HAL::Platform::setResetReasonForTest(HAL::Platform::ResetReason::Software);
    g_lines.clear();
    auto id = LoggerCallbacks::addCallback([](LogLevel, const char*, const char* msg) { g_lines.emplace_back(msg ? msg : ""); });
    TEST_ASSERT_TRUE(testCore->begin());
    LoggerCallbacks::removeCallback(id);
    bool sawDeath = false, sawRing = false;
    for (const auto& l : g_lines) {
        if (l.find("Last death:") != std::string::npos) sawDeath = true;
        if (l.find("ring: 11s:") != std::string::npos) sawRing = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(sawDeath, "the death line must reach the log");
    TEST_ASSERT_TRUE_MESSAGE(sawRing, "the ring must reach the log as its own line");
    HAL::Platform::resetMillisForTest();
}

void test_loop_marks_the_phase_and_returns_to_idle() {
    testCore->addComponent(std::unique_ptr<SimpleComponent>(new SimpleComponent("A")));
    TEST_ASSERT_TRUE(testCore->begin());
    testCore->loop();
    // after a full loop the marker is back at idle, encoded with its complement
    TEST_ASSERT_EQUAL_HEX32(FlightRecord::encodePhase(FlightRecorder::PHASE_IDLE),
                            HAL::Platform::stubRtcWordsForTest[FlightRecord::W_PHASE]);
}

// ---- OBS-4: the failed-allocation hook through Core ------------------------

static FlightRecord readRtcRecord() {
    FlightRecord r;
    HAL::Platform::rtcRead(0, r.w, FlightRecord::WORDS);
    return r;
}

void test_begin_registers_the_heap_hook_after_the_record_is_written() {
    // A failure on another task at the moment of registration: registered
    // last, the group lands on a record begin() will not rewrite.
    HAL::Platform::failedAllocFireOnInstallForTest = true;
    TEST_ASSERT_TRUE(testCore->begin());
    TEST_ASSERT_TRUE(FlightRecorder::instance().failedAllocHookInstalled());
    TEST_ASSERT_EQUAL_UINT32(1, readRtcRecord().failAllocCount());     // red when registered before startFresh()/writeAll()
    TEST_ASSERT_EQUAL_UINT32(4096, readRtcRecord().failAllocSize());
}

void test_a_platform_failure_reaches_the_record_through_the_hook() {
    TEST_ASSERT_TRUE(testCore->begin());
    HAL::Platform::fireFailedAllocForTest(640, 0x1800);
    TEST_ASSERT_EQUAL_UINT32(1, FlightRecorder::instance().failedAllocCount());
    TEST_ASSERT_EQUAL_HEX32(0x1800, readRtcRecord().failAllocSite());
}

void test_a_failed_hook_registration_is_reported_not_claimed() {
    HAL::Platform::failedAllocHookInstallFailsForTest = true;
    g_lines.clear();
    auto id = LoggerCallbacks::addCallback([](LogLevel, const char*, const char* msg) { g_lines.emplace_back(msg ? msg : ""); });
    TEST_ASSERT_TRUE(testCore->begin());
    LoggerCallbacks::removeCallback(id);
    TEST_ASSERT_FALSE(FlightRecorder::instance().failedAllocHookInstalled());
    bool said = false;
    for (const auto& l : g_lines) if (l.find("Failed-allocation hook not registered") != std::string::npos) said = true;
    TEST_ASSERT_TRUE(said);
}

static unsigned countAllocWarnings() {
    unsigned n = 0;
    for (const auto& l : g_lines) if (l.find("Allocation failures since boot") != std::string::npos) ++n;
    return n;
}

void test_loop_warns_about_survived_failures_at_most_once_a_minute() {
    HAL::Platform::setMillisForTest(1000);
    TEST_ASSERT_TRUE(testCore->begin());
    g_lines.clear();
    auto id = LoggerCallbacks::addCallback([](LogLevel, const char*, const char* msg) { g_lines.emplace_back(msg ? msg : ""); });
    HAL::Platform::advanceMillisForTest(60000);            // clear of any earlier test's minute
    HAL::Platform::fireFailedAllocForTest(512, 0x1800);
    testCore->loop();
    TEST_ASSERT_EQUAL_UINT(1, countAllocWarnings());
    HAL::Platform::fireFailedAllocForTest(512, 0x1800);
    HAL::Platform::advanceMillisForTest(1000);
    testCore->loop();                                       // same minute: silent
    TEST_ASSERT_EQUAL_UINT(1, countAllocWarnings());
    HAL::Platform::advanceMillisForTest(60000);
    testCore->loop();                                       // next minute: the new count
    TEST_ASSERT_EQUAL_UINT(2, countAllocWarnings());
    bool sawTwo = false;
    for (const auto& l : g_lines) if (l.find("since boot: 2, last 512 B") != std::string::npos) sawTwo = true;
    TEST_ASSERT_TRUE(sawTwo);
    LoggerCallbacks::removeCallback(id);
    HAL::Platform::resetMillisForTest();
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_begin_registers_the_heap_hook_after_the_record_is_written);
    RUN_TEST(test_a_platform_failure_reaches_the_record_through_the_hook);
    RUN_TEST(test_a_failed_hook_registration_is_reported_not_claimed);
    RUN_TEST(test_loop_warns_about_survived_failures_at_most_once_a_minute);
    
    RUN_TEST(test_flight_recorder_promotes_before_any_component_begins);
    RUN_TEST(test_loop_marks_the_phase_and_returns_to_idle);
    RUN_TEST(test_loop_hands_each_component_its_own_phase);
    RUN_TEST(test_loop_ticks_the_recorder_into_rtc);
    RUN_TEST(test_loop_carries_the_bus_drops_into_the_record);
    RUN_TEST(test_begin_logs_the_promoted_record_line_by_line);
    RUN_TEST(test_component_count_after_init);
    RUN_TEST(test_get_component_after_init);
    RUN_TEST(test_remove_component);
    RUN_TEST(test_begin_fails_on_component_failure);
    RUN_TEST(test_remove_nonexistent_component);
    RUN_TEST(test_device_id_configuration);
    RUN_TEST(test_logging_initialization);
    
    return UNITY_END();
}
