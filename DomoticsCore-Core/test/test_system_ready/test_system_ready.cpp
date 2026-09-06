#include <unity.h>
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

void test_loop_marks_the_phase_and_returns_to_idle() {
    testCore->addComponent(std::unique_ptr<SimpleComponent>(new SimpleComponent("A")));
    TEST_ASSERT_TRUE(testCore->begin());
    testCore->loop();
    // after a full loop the marker is back at idle, encoded with its complement
    TEST_ASSERT_EQUAL_HEX32(FlightRecord::encodePhase(FlightRecorder::PHASE_IDLE),
                            HAL::Platform::stubRtcWordsForTest[FlightRecord::W_PHASE]);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_flight_recorder_promotes_before_any_component_begins);
    RUN_TEST(test_loop_marks_the_phase_and_returns_to_idle);
    RUN_TEST(test_component_count_after_init);
    RUN_TEST(test_get_component_after_init);
    RUN_TEST(test_remove_component);
    RUN_TEST(test_begin_fails_on_component_failure);
    RUN_TEST(test_remove_nonexistent_component);
    RUN_TEST(test_device_id_configuration);
    RUN_TEST(test_logging_initialization);
    
    return UNITY_END();
}
