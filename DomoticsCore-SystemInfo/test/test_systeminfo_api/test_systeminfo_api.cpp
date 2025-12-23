// Unit tests for SystemInfoComponent API
// Verifies component creation, configuration, and lifecycle

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
// Component Creation Tests
// ============================================================================

void test_systeminfo_creation_default(void) {
    SystemInfoComponent sysinfo;

    TEST_ASSERT_EQUAL_STRING("System Info", sysinfo.metadata.name.c_str());
    TEST_ASSERT_EQUAL_STRING("1.4.0", sysinfo.metadata.version.c_str());
    TEST_ASSERT_EQUAL_STRING("system_info", sysinfo.getTypeKey());
}

void test_systeminfo_creation_with_config(void) {
    SystemInfoConfig config;
    config.deviceName = "TestDevice";
    config.manufacturer = "TestMfg";
    config.firmwareVersion = "2.0.0";
    config.updateInterval = 10000;

    SystemInfoComponent sysinfo(config);

    const auto& cfg = sysinfo.getConfig();
    TEST_ASSERT_EQUAL_STRING("TestDevice", cfg.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("TestMfg", cfg.manufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("2.0.0", cfg.firmwareVersion.c_str());
    TEST_ASSERT_EQUAL_INT(10000, cfg.updateInterval);
}

// ============================================================================
// Configuration Tests
// ============================================================================

void test_systeminfo_config_defaults(void) {
    SystemInfoConfig config;

    TEST_ASSERT_EQUAL_STRING("DomoticsCore Device", config.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.manufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("1.0.0", config.firmwareVersion.c_str());
    TEST_ASSERT_TRUE(config.enableDetailedInfo);
    TEST_ASSERT_TRUE(config.enableMemoryInfo);
    TEST_ASSERT_EQUAL_INT(5000, config.updateInterval);
    TEST_ASSERT_TRUE(config.enableBootDiagnostics);
}

void test_systeminfo_setconfig(void) {
    SystemInfoComponent sysinfo;

    SystemInfoConfig newConfig;
    newConfig.deviceName = "UpdatedDevice";
    newConfig.manufacturer = "UpdatedMfg";
    newConfig.firmwareVersion = "3.0.0";
    newConfig.updateInterval = 15000;
    newConfig.enableDetailedInfo = false;
    newConfig.enableMemoryInfo = false;

    sysinfo.setConfig(newConfig);

    const auto& cfg = sysinfo.getConfig();
    TEST_ASSERT_EQUAL_STRING("UpdatedDevice", cfg.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("UpdatedMfg", cfg.manufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("3.0.0", cfg.firmwareVersion.c_str());
    TEST_ASSERT_EQUAL_INT(15000, cfg.updateInterval);
    TEST_ASSERT_FALSE(cfg.enableDetailedInfo);
    TEST_ASSERT_FALSE(cfg.enableMemoryInfo);
}

void test_systeminfo_config_accessors(void) {
    SystemInfoConfig config;
    config.updateInterval = 8000;
    config.enableDetailedInfo = true;
    config.enableMemoryInfo = false;

    SystemInfoComponent sysinfo(config);

    TEST_ASSERT_EQUAL_INT(8000, sysinfo.getUpdateInterval());
    TEST_ASSERT_TRUE(sysinfo.isDetailedInfoEnabled());
    TEST_ASSERT_FALSE(sysinfo.isMemoryInfoEnabled());
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

void test_systeminfo_lifecycle_begin(void) {
    auto sysinfo = std::make_unique<SystemInfoComponent>();
    testCore->addComponent(std::move(sysinfo));

    bool status = testCore->begin();

    TEST_ASSERT_TRUE(status);
}

void test_systeminfo_lifecycle_loop(void) {
    auto sysinfo = std::make_unique<SystemInfoComponent>();
    testCore->addComponent(std::move(sysinfo));
    testCore->begin();

    // Loop should not crash
    testCore->loop();
    testCore->loop();
    testCore->loop();

    TEST_ASSERT_TRUE(true); // If we get here, loop worked
}

void test_systeminfo_lifecycle_shutdown(void) {
    auto sysinfo = std::make_unique<SystemInfoComponent>();
    testCore->addComponent(std::move(sysinfo));
    testCore->begin();

    testCore->shutdown();

    TEST_ASSERT_TRUE(true); // If we get here, shutdown succeeded
}

void test_systeminfo_lifecycle_complete(void) {
    auto sysinfo = std::make_unique<SystemInfoComponent>();
    testCore->addComponent(std::move(sysinfo));

    // Full lifecycle: begin -> loop -> shutdown
    TEST_ASSERT_TRUE(testCore->begin());

    for (int i = 0; i < 5; i++) {
        testCore->loop();
    }

    testCore->shutdown();
    TEST_ASSERT_TRUE(true); // If we get here, lifecycle completed
}

// ============================================================================
// Boot Count Tests
// ============================================================================

void test_systeminfo_boot_count_default(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& bootDiag = sysinfo.getBootDiagnostics();
    TEST_ASSERT_EQUAL_UINT32(0, bootDiag.bootCount);
}

void test_systeminfo_set_boot_count(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    sysinfo.setBootCount(42);

    const auto& bootDiag = sysinfo.getBootDiagnostics();
    TEST_ASSERT_EQUAL_UINT32(42, bootDiag.bootCount);
}

void test_systeminfo_boot_diagnostics_valid(void) {
    SystemInfoComponent sysinfo;
    sysinfo.begin();

    const auto& bootDiag = sysinfo.getBootDiagnostics();
    TEST_ASSERT_TRUE(bootDiag.valid);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Component creation
    RUN_TEST(test_systeminfo_creation_default);
    RUN_TEST(test_systeminfo_creation_with_config);

    // Configuration
    RUN_TEST(test_systeminfo_config_defaults);
    RUN_TEST(test_systeminfo_setconfig);
    RUN_TEST(test_systeminfo_config_accessors);

    // Lifecycle
    RUN_TEST(test_systeminfo_lifecycle_begin);
    RUN_TEST(test_systeminfo_lifecycle_loop);
    RUN_TEST(test_systeminfo_lifecycle_shutdown);
    RUN_TEST(test_systeminfo_lifecycle_complete);

    // Boot diagnostics
    RUN_TEST(test_systeminfo_boot_count_default);
    RUN_TEST(test_systeminfo_set_boot_count);
    RUN_TEST(test_systeminfo_boot_diagnostics_valid);

    return UNITY_END();
}
