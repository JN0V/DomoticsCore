// TEST-7: MemoryManager against the stub's scriptable heap. The singleton has
// no reset and this file is one process, so the first RUN_TEST is the only
// place the lazy first getProfile() can be observed; every later case
// detects on its own.
#include <unity.h>
#include <DomoticsCore/MemoryManager.h>
#include <DomoticsCore/Platform_HAL.h>

using namespace DomoticsCore;
namespace P = DomoticsCore::HAL::Platform;

static MemoryManager& mm() { return MemoryManager::instance(); }

static MemoryProfile profileAt(uint32_t bytes) {
    P::setFreeHeapForTest(bytes);
    return mm().detectProfile();
}

void setUp(void) {
    mm().setThresholds(MemoryThresholds{});
    P::resetFreeHeapForTest();
}
void tearDown(void) {}

// Must run first: nothing has called detectProfile() yet in this process.
void test_the_first_get_profile_classifies_the_live_heap_on_its_own(void) {
    P::setFreeHeapForTest(20000);
    TEST_ASSERT_EQUAL(MemoryProfile::STANDARD, mm().getProfile());
    TEST_ASSERT_EQUAL_UINT32(20000, mm().getHeapAtBoot());
}

void test_full_starts_at_30720_bytes(void) {
    TEST_ASSERT_EQUAL(MemoryProfile::FULL, profileAt(30720));
    TEST_ASSERT_EQUAL(MemoryProfile::STANDARD, profileAt(30719));
}

void test_standard_starts_at_15360_bytes(void) {
    TEST_ASSERT_EQUAL(MemoryProfile::STANDARD, profileAt(15360));
    TEST_ASSERT_EQUAL(MemoryProfile::MINIMAL, profileAt(15359));
}

void test_minimal_starts_at_8192_bytes_and_critical_is_below(void) {
    TEST_ASSERT_EQUAL(MemoryProfile::MINIMAL, profileAt(8192));
    TEST_ASSERT_EQUAL(MemoryProfile::CRITICAL, profileAt(8191));
    TEST_ASSERT_EQUAL(MemoryProfile::CRITICAL, profileAt(0));
}

void test_get_profile_answers_from_the_cache_until_detect_profile(void) {
    TEST_ASSERT_EQUAL(MemoryProfile::FULL, profileAt(40000));
    P::setFreeHeapForTest(5000);
    TEST_ASSERT_EQUAL(MemoryProfile::FULL, mm().getProfile());
    TEST_ASSERT_EQUAL(MemoryProfile::CRITICAL, mm().detectProfile());
    TEST_ASSERT_EQUAL(MemoryProfile::CRITICAL, mm().getProfile());
}

void test_heap_at_boot_is_the_classified_sample_not_the_live_heap(void) {
    profileAt(40000);
    P::setFreeHeapForTest(5000);
    TEST_ASSERT_EQUAL_UINT32(40000, mm().getHeapAtBoot());
    TEST_ASSERT_EQUAL_UINT32(5000, mm().getCurrentFreeHeap());
}

void test_the_four_profile_names(void) {
    profileAt(40000); TEST_ASSERT_EQUAL_STRING("FULL", mm().getProfileName());
    profileAt(20000); TEST_ASSERT_EQUAL_STRING("STANDARD", mm().getProfileName());
    profileAt(10000); TEST_ASSERT_EQUAL_STRING("MINIMAL", mm().getProfileName());
    profileAt(5000);  TEST_ASSERT_EQUAL_STRING("CRITICAL", mm().getProfileName());
}

// The three boots read from the bench logs: a WROOM-32D running FullStack,
// the nodemcuv2 running the observability probe, the nodemcuv2 running
// FullStack (the CI-14 boot, where the WebUI cut its client limit).
void test_the_logged_boots_classify_as_the_boards_reported(void) {
    TEST_ASSERT_EQUAL(MemoryProfile::FULL, profileAt(276220));
    TEST_ASSERT_EQUAL(MemoryProfile::STANDARD, profileAt(26064));
    TEST_ASSERT_EQUAL(MemoryProfile::MINIMAL, profileAt(15088));
}

void test_max_ws_clients_per_profile(void) {
    profileAt(40000); TEST_ASSERT_EQUAL_UINT8(8, mm().getMaxWsClients());
    profileAt(20000); TEST_ASSERT_EQUAL_UINT8(4, mm().getMaxWsClients());
    profileAt(10000); TEST_ASSERT_EQUAL_UINT8(2, mm().getMaxWsClients());
    profileAt(5000);  TEST_ASSERT_EQUAL_UINT8(1, mm().getMaxWsClients());
}

void test_chart_history_points_per_profile(void) {
    profileAt(40000); TEST_ASSERT_EQUAL_UINT8(60, mm().getChartHistoryPoints());
    profileAt(20000); TEST_ASSERT_EQUAL_UINT8(30, mm().getChartHistoryPoints());
    profileAt(10000); TEST_ASSERT_EQUAL_UINT8(10, mm().getChartHistoryPoints());
    profileAt(5000);  TEST_ASSERT_EQUAL_UINT8(0, mm().getChartHistoryPoints());
}

void test_ws_update_interval_per_profile_and_zero_means_disabled(void) {
    profileAt(40000); TEST_ASSERT_EQUAL_UINT32(2000, mm().getWsUpdateInterval());
    profileAt(20000); TEST_ASSERT_EQUAL_UINT32(5000, mm().getWsUpdateInterval());
    profileAt(10000); TEST_ASSERT_EQUAL_UINT32(10000, mm().getWsUpdateInterval());
    profileAt(5000);  TEST_ASSERT_EQUAL_UINT32(0, mm().getWsUpdateInterval());
}

static void assertBuffers(size_t ws, size_t http, size_t json, size_t log) {
    TEST_ASSERT_EQUAL_size_t(ws,   mm().getBufferSize(BufferType::WebSocket));
    TEST_ASSERT_EQUAL_size_t(http, mm().getBufferSize(BufferType::HttpResponse));
    TEST_ASSERT_EQUAL_size_t(json, mm().getBufferSize(BufferType::JsonDocument));
    TEST_ASSERT_EQUAL_size_t(log,  mm().getBufferSize(BufferType::LogBuffer));
}

void test_buffer_sizes_per_profile(void) {
    profileAt(40000); assertBuffers(8192, 4096, 8192, 200);
    profileAt(20000); assertBuffers(4096, 2048, 4096, 100);
    profileAt(10000); assertBuffers(2048, 1024, 2048, 50);
    profileAt(5000);  assertBuffers(1024, 512, 1024, 20);
}

void test_should_enable_per_profile(void) {
    profileAt(40000);
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::WebSocketUpdates));
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::ChartHistory));
    TEST_ASSERT_FALSE(mm().shouldEnable(Feature::SettingsLazyLoad));
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::SchemaCompression));
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::FullDashboard));
    profileAt(20000);
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::WebSocketUpdates));
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::ChartHistory));
    TEST_ASSERT_FALSE(mm().shouldEnable(Feature::SettingsLazyLoad));
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::FullDashboard));
    profileAt(10000);
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::WebSocketUpdates));
    TEST_ASSERT_FALSE(mm().shouldEnable(Feature::ChartHistory));
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::SettingsLazyLoad));
    TEST_ASSERT_FALSE(mm().shouldEnable(Feature::FullDashboard));
    profileAt(5000);
    TEST_ASSERT_FALSE(mm().shouldEnable(Feature::WebSocketUpdates));
    TEST_ASSERT_FALSE(mm().shouldEnable(Feature::ChartHistory));
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::SettingsLazyLoad));
    TEST_ASSERT_TRUE(mm().shouldEnable(Feature::SchemaCompression));
    TEST_ASSERT_FALSE(mm().shouldEnable(Feature::FullDashboard));
}

void test_is_low_memory_reads_the_live_heap_and_leaves_the_profile_alone(void) {
    profileAt(40000);
    P::setFreeHeapForTest(8191);
    TEST_ASSERT_TRUE(mm().isLowMemory());
    TEST_ASSERT_EQUAL(MemoryProfile::FULL, mm().getProfile());
    P::setFreeHeapForTest(8192);
    TEST_ASSERT_FALSE(mm().isLowMemory());
}

void test_is_critical_memory_is_half_the_minimal_threshold(void) {
    profileAt(40000);
    P::setFreeHeapForTest(4095);
    TEST_ASSERT_TRUE(mm().isCriticalMemory());
    P::setFreeHeapForTest(4096);
    TEST_ASSERT_FALSE(mm().isCriticalMemory());
}

void test_default_thresholds(void) {
    const MemoryThresholds& t = mm().getThresholds();
    TEST_ASSERT_EQUAL_UINT32(30720, t.fullMin);
    TEST_ASSERT_EQUAL_UINT32(15360, t.standardMin);
    TEST_ASSERT_EQUAL_UINT32(8192, t.minimalMin);
}

void test_set_thresholds_reclassifies_at_the_next_detect_profile_only(void) {
    TEST_ASSERT_EQUAL(MemoryProfile::STANDARD, profileAt(20000));
    MemoryThresholds t;
    t.fullMin = 10000; t.standardMin = 6000; t.minimalMin = 3000;
    mm().setThresholds(t);
    TEST_ASSERT_EQUAL(MemoryProfile::STANDARD, mm().getProfile());
    TEST_ASSERT_EQUAL(MemoryProfile::FULL, mm().detectProfile());
    TEST_ASSERT_EQUAL_UINT32(10000, mm().getThresholds().fullMin);
}

void test_set_thresholds_moves_is_low_memory_at_once(void) {
    profileAt(20000);
    TEST_ASSERT_FALSE(mm().isLowMemory());
    MemoryThresholds t;
    t.minimalMin = 25000;
    mm().setThresholds(t);
    TEST_ASSERT_TRUE(mm().isLowMemory());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_first_get_profile_classifies_the_live_heap_on_its_own);
    RUN_TEST(test_full_starts_at_30720_bytes);
    RUN_TEST(test_standard_starts_at_15360_bytes);
    RUN_TEST(test_minimal_starts_at_8192_bytes_and_critical_is_below);
    RUN_TEST(test_get_profile_answers_from_the_cache_until_detect_profile);
    RUN_TEST(test_heap_at_boot_is_the_classified_sample_not_the_live_heap);
    RUN_TEST(test_the_four_profile_names);
    RUN_TEST(test_the_logged_boots_classify_as_the_boards_reported);
    RUN_TEST(test_max_ws_clients_per_profile);
    RUN_TEST(test_chart_history_points_per_profile);
    RUN_TEST(test_ws_update_interval_per_profile_and_zero_means_disabled);
    RUN_TEST(test_buffer_sizes_per_profile);
    RUN_TEST(test_should_enable_per_profile);
    RUN_TEST(test_is_low_memory_reads_the_live_heap_and_leaves_the_profile_alone);
    RUN_TEST(test_is_critical_memory_is_half_the_minimal_threshold);
    RUN_TEST(test_default_thresholds);
    RUN_TEST(test_set_thresholds_reclassifies_at_the_next_detect_profile_only);
    RUN_TEST(test_set_thresholds_moves_is_low_memory_at_once);
    return UNITY_END();
}
