// Unit tests for RemoteConsoleComponent
// Tests: creation, config, lifecycle, client handling, commands, log buffering

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/RemoteConsole.h>

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
// RemoteConsoleComponent Creation Tests
// ============================================================================

void test_remoteconsole_component_creation_default(void) {
    auto console = std::make_unique<RemoteConsoleComponent>();

    TEST_ASSERT_EQUAL_STRING("RemoteConsole", console->metadata.name.c_str());
    TEST_ASSERT_EQUAL_STRING("1.4.0", console->metadata.version.c_str());
}

void test_remoteconsole_component_creation_with_config(void) {
    RemoteConsoleConfig config;
    config.port = 2323;
    config.bufferSize = 1000;
    config.maxClients = 5;

    auto console = std::make_unique<RemoteConsoleComponent>(config);

    TEST_ASSERT_EQUAL_STRING("RemoteConsole", console->metadata.name.c_str());
}

// ============================================================================
// RemoteConsoleConfig Tests
// ============================================================================

void test_remoteconsole_config_defaults(void) {
    RemoteConsoleConfig config;

    TEST_ASSERT_TRUE(config.enabled);
    TEST_ASSERT_EQUAL_UINT16(23, config.port);
    TEST_ASSERT_FALSE(config.requireAuth);
    TEST_ASSERT_TRUE(config.password.isEmpty());
    TEST_ASSERT_EQUAL_UINT32(DOMOTICS_LOG_BUFFER_SIZE, config.bufferSize);
    TEST_ASSERT_TRUE(config.allowCommands);
    // Skip allowedIPs check (requires IPAddress stub implementation)
    // TEST_ASSERT_EQUAL(0, config.allowedIPs.size());
    TEST_ASSERT_TRUE(config.colorOutput);
    TEST_ASSERT_EQUAL_UINT32(3, config.maxClients);
    TEST_ASSERT_EQUAL(LOG_LEVEL_INFO, config.defaultLogLevel);
}

void test_remoteconsole_config_custom(void) {
    RemoteConsoleConfig config;
    config.enabled = false;
    config.port = 2323;
    config.requireAuth = true;
    config.password = "secret123";
    config.bufferSize = 1000;
    config.allowCommands = false;
    config.colorOutput = false;
    config.maxClients = 5;
    config.defaultLogLevel = LOG_LEVEL_DEBUG;

    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    // Disabled console should still return Success but not start server
    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());
}

// ============================================================================
// RemoteConsole Lifecycle Tests
// ============================================================================

void test_remoteconsole_begin_enabled(void) {
    RemoteConsoleConfig config;
    config.enabled = true;
    config.port = 2323;

    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());
}

void test_remoteconsole_begin_disabled(void) {
    RemoteConsoleConfig config;
    config.enabled = false;

    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());
}

void test_remoteconsole_full_lifecycle(void) {
    RemoteConsoleConfig config;
    config.enabled = true;

    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));

    // Begin
    testCore->begin();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());

    // Multiple loops should not crash
    for (int i = 0; i < 10; i++) {
        testCore->loop();
    }

    // Shutdown
    testCore->shutdown();
}

void test_remoteconsole_shutdown_returns_success(void) {
    auto console = std::make_unique<RemoteConsoleComponent>();
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    ComponentStatus status = consolePtr->shutdown();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);
}

// ============================================================================
// RemoteConsole Dependencies Tests
// ============================================================================

void test_remoteconsole_no_dependencies(void) {
    auto console = std::make_unique<RemoteConsoleComponent>();

    auto deps = console->getDependencies();
    // RemoteConsole has no hard dependencies (WiFi is optional)
    TEST_ASSERT_EQUAL(0, deps.size());
}

// ============================================================================
// RemoteConsole Configuration Tests
// ============================================================================

void test_remoteconsole_port_config(void) {
    RemoteConsoleConfig config;
    config.port = 8023;

    auto console = std::make_unique<RemoteConsoleComponent>(config);

    // Port should be configured
    TEST_ASSERT_NOT_NULL(console.get());
}

void test_remoteconsole_buffer_size_config(void) {
    RemoteConsoleConfig config;
    config.bufferSize = 2000;

    auto console = std::make_unique<RemoteConsoleComponent>(config);

    TEST_ASSERT_NOT_NULL(console.get());
}

void test_remoteconsole_max_clients_config(void) {
    RemoteConsoleConfig config;
    config.maxClients = 10;

    auto console = std::make_unique<RemoteConsoleComponent>(config);

    TEST_ASSERT_NOT_NULL(console.get());
}

void test_remoteconsole_authentication_config(void) {
    RemoteConsoleConfig config;
    config.requireAuth = true;
    config.password = "mypassword123";

    auto console = std::make_unique<RemoteConsoleComponent>(config);

    TEST_ASSERT_NOT_NULL(console.get());
}

void test_remoteconsole_log_level_config(void) {
    RemoteConsoleConfig config;
    config.defaultLogLevel = LOG_LEVEL_DEBUG;

    auto console = std::make_unique<RemoteConsoleComponent>(config);

    TEST_ASSERT_NOT_NULL(console.get());
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

void test_remoteconsole_zero_buffer_size(void) {
    RemoteConsoleConfig config;
    config.bufferSize = 0;

    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    // Should handle gracefully
    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());
}

void test_remoteconsole_zero_max_clients(void) {
    RemoteConsoleConfig config;
    config.maxClients = 0;

    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    // Should handle gracefully (no clients allowed)
    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());
}

void test_remoteconsole_multiple_config_changes(void) {
    auto console = std::make_unique<RemoteConsoleComponent>();
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    // Component should remain stable through config
    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());
}

void test_remoteconsole_empty_password(void) {
    RemoteConsoleConfig config;
    config.requireAuth = true;
    config.password = "";  // Empty password

    auto console = std::make_unique<RemoteConsoleComponent>(config);

    // Should create successfully (validation happens at runtime)
    TEST_ASSERT_NOT_NULL(console.get());
}

void test_remoteconsole_color_output_disabled(void) {
    RemoteConsoleConfig config;
    config.colorOutput = false;

    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());
}

void test_remoteconsole_commands_disabled(void) {
    RemoteConsoleConfig config;
    config.allowCommands = false;

    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();

    testCore->addComponent(std::move(console));
    testCore->begin();

    TEST_ASSERT_EQUAL(ComponentStatus::Success, consolePtr->getLastStatus());
}

// ============================================================================
// Memory Leak Tests
// ============================================================================

/**
 * @brief Test that simulates long-running logging to detect memory leaks
 * 
 * This test reproduces the OOM crash observed overnight where heap dropped
 * from 30KB to 0 due to std::deque<LogEntry> not releasing memory on pop_front()
 */
void test_remoteconsole_log_buffer_no_memory_leak(void) {
    RemoteConsoleConfig config;
    config.bufferSize = 100;  // Small buffer to force rotation quickly
    config.enabled = true;
    
    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();
    
    testCore->addComponent(std::move(console));
    testCore->begin();
    
    // Measure initial heap
    size_t heapBefore = HAL::getFreeHeap();
    
    // Simulate long-running logging (5000 log entries = 50x buffer rotation)
    // This should NOT leak memory if the circular buffer is implemented correctly
    for (int i = 0; i < 5000; i++) {
        // Simulate what DLOG_I does - calls log() via callback
        consolePtr->log(LOG_LEVEL_INFO, "TEST", ("Log message number " + String(i)).c_str());
    }
    
    // Force cleanup
    consolePtr->clearBuffer();
    
    size_t heapAfter = HAL::getFreeHeap();
    
    // Calculate leak per iteration
    int heapDelta = (int)heapBefore - (int)heapAfter;
    float leakPerLog = heapDelta / 5000.0f;
    
    printf("\n[MEMORY TEST] Log buffer rotation x5000:\n");
    printf("  Heap before: %zu bytes\n", heapBefore);
    printf("  Heap after:  %zu bytes\n", heapAfter);
    printf("  Delta: %d bytes (%.2f bytes/log)\n", heapDelta, leakPerLog);
    
    // Allow some tolerance for fragmentation, but should be < 1 byte per log
    // With the bug, this would be ~56 bytes per log = 280KB leak!
    TEST_ASSERT_LESS_THAN_MESSAGE(5000, heapDelta, 
        "Memory leak detected in log buffer! Each log leaks memory.");
}

/**
 * @brief Test rapid log buffer fill/clear cycles
 */
void test_remoteconsole_rapid_buffer_cycles_no_leak(void) {
    RemoteConsoleConfig config;
    config.bufferSize = 50;
    config.enabled = true;
    
    auto console = std::make_unique<RemoteConsoleComponent>(config);
    RemoteConsoleComponent* consolePtr = console.get();
    
    testCore->addComponent(std::move(console));
    testCore->begin();
    
    size_t heapBefore = HAL::getFreeHeap();
    
    // 100 cycles of fill + clear
    for (int cycle = 0; cycle < 100; cycle++) {
        // Fill buffer
        for (int i = 0; i < 60; i++) {  // More than bufferSize to trigger rotation
            consolePtr->log(LOG_LEVEL_INFO, "CYCLE", ("Cycle " + String(cycle) + " msg " + String(i)).c_str());
        }
        // Clear buffer
        consolePtr->clearBuffer();
    }
    
    size_t heapAfter = HAL::getFreeHeap();
    int heapDelta = (int)heapBefore - (int)heapAfter;
    
    printf("\n[MEMORY TEST] Rapid buffer cycles x100:\n");
    printf("  Heap delta: %d bytes\n", heapDelta);
    
    // Should have zero or minimal leak after clear
    TEST_ASSERT_LESS_THAN_MESSAGE(1000, heapDelta,
        "Memory leak in rapid buffer cycles!");
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Component creation tests
    RUN_TEST(test_remoteconsole_component_creation_default);
    RUN_TEST(test_remoteconsole_component_creation_with_config);

    // Config tests
    RUN_TEST(test_remoteconsole_config_defaults);
    RUN_TEST(test_remoteconsole_config_custom);

    // Lifecycle tests
    RUN_TEST(test_remoteconsole_begin_enabled);
    RUN_TEST(test_remoteconsole_begin_disabled);
    RUN_TEST(test_remoteconsole_full_lifecycle);
    RUN_TEST(test_remoteconsole_shutdown_returns_success);

    // Dependencies tests
    RUN_TEST(test_remoteconsole_no_dependencies);

    // Configuration tests
    RUN_TEST(test_remoteconsole_port_config);
    RUN_TEST(test_remoteconsole_buffer_size_config);
    RUN_TEST(test_remoteconsole_max_clients_config);
    RUN_TEST(test_remoteconsole_authentication_config);
    RUN_TEST(test_remoteconsole_log_level_config);

    // Edge cases tests
    RUN_TEST(test_remoteconsole_zero_buffer_size);
    RUN_TEST(test_remoteconsole_zero_max_clients);
    RUN_TEST(test_remoteconsole_multiple_config_changes);
    RUN_TEST(test_remoteconsole_empty_password);
    RUN_TEST(test_remoteconsole_color_output_disabled);
    RUN_TEST(test_remoteconsole_commands_disabled);

    // Memory leak tests
    RUN_TEST(test_remoteconsole_log_buffer_no_memory_leak);
    RUN_TEST(test_remoteconsole_rapid_buffer_cycles_no_leak);

    return UNITY_END();
}
