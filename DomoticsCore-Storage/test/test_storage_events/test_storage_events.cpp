// Unit tests for StorageComponent event emissions
// Verifies EVENT_READY is published when storage initializes

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/StorageEvents.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Test state
static Core* testCore = nullptr;
static bool storageReadyReceived = false;
static String storageReadyNamespace;

void setUp(void) {
    testCore = new Core();
    storageReadyReceived = false;
    storageReadyNamespace = "";
}

void tearDown(void) {
    if (testCore) {
        testCore->shutdown();
        delete testCore;
        testCore = nullptr;
    }
}

// ============================================================================
// Storage Event Tests
// ============================================================================

void test_storage_ready_event_published(void) {
    // Subscribe to storage ready event BEFORE adding component
    testCore->getEventBus().subscribe(StorageEvents::EVENT_READY, [](const void* payload) {
        storageReadyReceived = true;
        if (payload) {
            storageReadyNamespace = *static_cast<const String*>(payload);
        }
    });

    // Create storage component with custom namespace
    StorageConfig config;
    config.namespace_name = "test_events";
    auto storage = std::make_unique<StorageComponent>(config);

    testCore->addComponent(std::move(storage));

    // Initialize - should emit EVENT_READY
    testCore->begin();

    // Process events
    testCore->loop();

    // Verify
    TEST_ASSERT_TRUE_MESSAGE(storageReadyReceived, "EVENT_READY should be published");
    TEST_ASSERT_EQUAL_STRING("test_events", storageReadyNamespace.c_str());
}

void test_storage_ready_event_contains_namespace(void) {
    // Subscribe to storage ready event
    testCore->getEventBus().subscribe(StorageEvents::EVENT_READY, [](const void* payload) {
        storageReadyReceived = true;
        if (payload) {
            storageReadyNamespace = *static_cast<const String*>(payload);
        }
    });

    // Create with different namespace
    StorageConfig config;
    config.namespace_name = "custom_ns";
    auto storage = std::make_unique<StorageComponent>(config);

    testCore->addComponent(std::move(storage));
    testCore->begin();
    testCore->loop();

    // Namespace should be in payload
    TEST_ASSERT_EQUAL_STRING("custom_ns", storageReadyNamespace.c_str());
}

void test_storage_ready_not_emitted_on_failure(void) {
    // Subscribe to storage ready event
    testCore->getEventBus().subscribe(StorageEvents::EVENT_READY, [](const void*) {
        storageReadyReceived = true;
    });

    // Create with invalid namespace (too long - max 15 chars)
    StorageConfig config;
    config.namespace_name = "this_namespace_is_way_too_long";
    auto storage = std::make_unique<StorageComponent>(config);

    testCore->addComponent(std::move(storage));
    testCore->begin();
    testCore->loop();

    // Should fail and NOT emit event
    TEST_ASSERT_FALSE_MESSAGE(storageReadyReceived, "EVENT_READY should NOT be published on failure");
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_storage_ready_event_published);
    RUN_TEST(test_storage_ready_event_contains_namespace);
    RUN_TEST(test_storage_ready_not_emitted_on_failure);

    return UNITY_END();
}
