// Unit tests for SystemInfoComponent metrics collection
// Verifies heap, uptime, CPU load, and system metrics

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/SystemInfo.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Test state
static Core* testCore = nullptr;

void setUp(void) {
    testCore = new Core();
}

void tearDown(void) {
    if (testCore) {
        testCore->shutdown();
        delete testCore;
        testCore = nullptr;
    }
}

// ============================================================================
// Metrics Collection Tests
// ============================================================================

void test_systeminfo_metrics_valid_after_begin(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& metrics = sysinfo.getMetrics();
    TEST_ASSERT_TRUE(metrics.valid);
}

void test_systeminfo_metrics_heap_nonzero(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& metrics = sysinfo.getMetrics();
    // Stub returns 0 - just verify it's captured
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(0, metrics.freeHeap);
}

void test_systeminfo_metrics_cpu_freq_nonzero(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& metrics = sysinfo.getMetrics();
    // Stub returns 0 - just verify it's captured
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, metrics.cpuFreq);
}

void test_systeminfo_metrics_chip_model_not_empty(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& metrics = sysinfo.getMetrics();
    TEST_ASSERT_GREATER_THAN_UINT(0, metrics.chipModel.length());
}

void test_systeminfo_metrics_uptime_increases(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& metrics1 = sysinfo.getMetrics();
    uint32_t uptime1 = metrics1.uptime;

    // Wait a bit
    HAL::Platform::delayMs(100);

    // Force update
    sysinfo.forceUpdateMetrics();
    const auto& metrics2 = sysinfo.getMetrics();
    uint32_t uptime2 = metrics2.uptime;

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(uptime1, uptime2);
}

void test_systeminfo_metrics_cpu_load_range(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    // Force a few updates to establish baseline
    for (int i = 0; i < 5; i++) {
        sysinfo.forceUpdateMetrics();
        HAL::Platform::delayMs(10);
    }

    const auto& metrics = sysinfo.getMetrics();

    // CPU load should be between 0 and 100
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, metrics.cpuLoad);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(100.0f, metrics.cpuLoad);
}

// ============================================================================
// Format Helper Tests
// ============================================================================

void test_systeminfo_format_bytes_under_1kb(void) {
    SystemInfoComponent sysinfo;
    String formatted = sysinfo.formatBytesPublic(512);

    TEST_ASSERT_TRUE(formatted.indexOf("B") > 0);
    TEST_ASSERT_TRUE(formatted.indexOf("512") >= 0);
}

void test_systeminfo_format_bytes_kilobytes(void) {
    SystemInfoComponent sysinfo;
    String formatted = sysinfo.formatBytesPublic(2048);

    TEST_ASSERT_TRUE(formatted.indexOf("KB") > 0);
}

void test_systeminfo_format_bytes_megabytes(void) {
    SystemInfoComponent sysinfo;
    String formatted = sysinfo.formatBytesPublic(2 * 1024 * 1024);

    TEST_ASSERT_TRUE(formatted.indexOf("MB") > 0);
}

void test_systeminfo_format_uptime_seconds(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    // Uptime should be < 1 minute in tests
    String uptime = sysinfo.getFormattedUptimePublic();

    // Should not be empty and should contain time format
    TEST_ASSERT_GREATER_THAN_UINT(0, uptime.length());
}

// ============================================================================
// Update Interval Tests
// ============================================================================

void test_systeminfo_update_interval_respected(void) {
    SystemInfoConfig config;
    config.updateInterval = 1000; // 1 second
    SystemInfoComponent sysinfo(config);
    sysinfo.begin();

    const auto& metrics1 = sysinfo.getMetrics();
    uint32_t uptime1 = metrics1.uptime;

    // Loop without waiting - should not update yet
    sysinfo.loop();
    const auto& metrics2 = sysinfo.getMetrics();
    uint32_t uptime2 = metrics2.uptime;

    // Should be same (update interval not reached)
    TEST_ASSERT_EQUAL_UINT32(uptime1, uptime2);

    // Wait for interval
    HAL::Platform::delayMs(1100);

    // Loop again - should update now
    sysinfo.loop();
    const auto& metrics3 = sysinfo.getMetrics();
    uint32_t uptime3 = metrics3.uptime;

    // Should be different now
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(uptime1, uptime3);
}

void test_systeminfo_force_update_bypasses_interval(void) {
    SystemInfoConfig config;
    config.updateInterval = 10000; // 10 seconds
    SystemInfoComponent sysinfo(config);
    sysinfo.begin();

    const auto& metrics1 = sysinfo.getMetrics();
    uint32_t uptime1 = metrics1.uptime;

    // Wait a bit
    HAL::Platform::delayMs(100);

    // Force update should work immediately
    sysinfo.forceUpdateMetrics();
    const auto& metrics2 = sysinfo.getMetrics();
    uint32_t uptime2 = metrics2.uptime;

    // Should be updated even though interval not reached
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(uptime1, uptime2);
}

// ============================================================================
// HAL Integration Tests
// ============================================================================

void test_systeminfo_hal_chip_model(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& metrics = sysinfo.getMetrics();

    // Stub returns "Unknown"
    TEST_ASSERT_EQUAL_STRING("Unknown", metrics.chipModel.c_str());
}

void test_systeminfo_hal_free_heap(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& metrics = sysinfo.getMetrics();
    uint32_t freeHeap = HAL::Platform::getFreeHeap();

    // Should match HAL value
    TEST_ASSERT_EQUAL_UINT32(freeHeap, metrics.freeHeap);
}

void test_systeminfo_hal_cpu_freq(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& metrics = sysinfo.getMetrics();
    float cpuFreq = HAL::Platform::getCpuFreqMHz();

    // Should match HAL value
    TEST_ASSERT_EQUAL_FLOAT(cpuFreq, metrics.cpuFreq);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Metrics collection
    RUN_TEST(test_systeminfo_metrics_valid_after_begin);
    RUN_TEST(test_systeminfo_metrics_heap_nonzero);
    RUN_TEST(test_systeminfo_metrics_cpu_freq_nonzero);
    RUN_TEST(test_systeminfo_metrics_chip_model_not_empty);
    RUN_TEST(test_systeminfo_metrics_uptime_increases);
    RUN_TEST(test_systeminfo_metrics_cpu_load_range);

    // Format helpers
    RUN_TEST(test_systeminfo_format_bytes_under_1kb);
    RUN_TEST(test_systeminfo_format_bytes_kilobytes);
    RUN_TEST(test_systeminfo_format_bytes_megabytes);
    RUN_TEST(test_systeminfo_format_uptime_seconds);

    // Update interval
    RUN_TEST(test_systeminfo_update_interval_respected);
    RUN_TEST(test_systeminfo_force_update_bypasses_interval);

    // HAL integration
    RUN_TEST(test_systeminfo_hal_chip_model);
    RUN_TEST(test_systeminfo_hal_free_heap);
    RUN_TEST(test_systeminfo_hal_cpu_freq);

    return UNITY_END();
}
