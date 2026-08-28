// Unit tests for StorageComponent event emissions
// Verifies EVENT_READY and EVENT_CHANGED are published correctly

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>
#include <DomoticsCore/StorageEvents.h>
#include <DomoticsCore/Testing/HeapTracker.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Test state
static Core* testCore = nullptr;
static bool storageReadyReceived = false;
static String storageReadyNamespace;

// M15 test state
static int changedCount = 0;
static String lastChangedKey;

void setUp(void) {
    testCore = new Core();
    storageReadyReceived = false;
    storageReadyNamespace = "";
    changedCount = 0;
    lastChangedKey = "";
}

void tearDown(void) {
    if (testCore) {
        testCore->shutdown();
        delete testCore;
        testCore = nullptr;
    }
}

// Helper: create storage component, subscribe to EVENT_CHANGED, begin + loop
static StorageComponent* setupStorageWithChangedSubscription(const String& ns = "test_m15") {
    testCore->getEventBus().subscribe(StorageEvents::EVENT_CHANGED, [](const void* payload) {
        if (payload) {
            auto* ev = static_cast<const StorageEvents::StorageChangedEvent*>(payload);
            lastChangedKey = ev->key;
        }
        changedCount++;
    });

    StorageConfig config;
    config.namespace_name = ns;
    auto storagePtr = std::make_unique<StorageComponent>(config);
    StorageComponent* storage = storagePtr.get();
    testCore->addComponent(std::move(storagePtr));
    testCore->begin();
    testCore->loop();
    return storage;
}

// ============================================================================
// Existing EVENT_READY Tests
// ============================================================================

void test_storage_ready_event_published(void) {
    // Subscribe to storage ready event BEFORE adding component
    testCore->getEventBus().subscribe(StorageEvents::EVENT_READY, [](const void* payload) {
        storageReadyReceived = true;
        if (payload) {
            // BUG-30: the payload is bytes now, not a String object. It used to
            // be `*static_cast<const String*>(payload)` — reading a String whose
            // pointer, length and capacity had been byte-copied out of the
            // component's config member. That worked only because the member
            // outlived dispatch; it was the same defect as Wifi.h's seven, on a
            // slower fuse. Storage publishes `namespace_name.c_str()` with its
            // NUL, so a subscriber reads a plain C string.
            storageReadyNamespace = static_cast<const char*>(payload);
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
            storageReadyNamespace = static_cast<const char*>(payload);  // BUG-30, see above
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
// M15 — Storage Changed Event Tests
// ============================================================================

void test_storage_changed_on_putString(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putString("mykey", "hello");
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    TEST_ASSERT_EQUAL_STRING("mykey", lastChangedKey.c_str());
}

void test_storage_changed_dirty_check_string(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putString("dk", "same");
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);

    // Same value again — should NOT emit
    storage->putString("dk", "same");
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);
}

void test_storage_changed_on_putInt(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putInt("temp", 42);
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    TEST_ASSERT_EQUAL_STRING("temp", lastChangedKey.c_str());
}

void test_storage_changed_dirty_check_int(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putInt("x", 42);
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);

    storage->putInt("x", 42);
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);
}

void test_storage_changed_on_putFloat(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putFloat("pi", 3.14f);
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    TEST_ASSERT_EQUAL_STRING("pi", lastChangedKey.c_str());
}

void test_storage_changed_dirty_check_float(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putFloat("f", 1.5f);
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);

    storage->putFloat("f", 1.5f);
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);
}

void test_storage_changed_on_putBool(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putBool("flag", true);
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    TEST_ASSERT_EQUAL_STRING("flag", lastChangedKey.c_str());
}

void test_storage_changed_dirty_check_bool(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putBool("b", false);
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);

    storage->putBool("b", false);
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);
}

void test_storage_changed_on_putULong64(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putULong64("counter", 100);
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    TEST_ASSERT_EQUAL_STRING("counter", lastChangedKey.c_str());

    // Verify round-trip (AC-7a)
    TEST_ASSERT_EQUAL_UINT64(100, storage->getULong64("counter"));
}

void test_storage_changed_dirty_check_ulong64(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putULong64("counter", 100);
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);

    // Same value — should NOT emit (AC-7b)
    storage->putULong64("counter", 100);
    testCore->loop();
    TEST_ASSERT_EQUAL_INT(1, changedCount);
}

void test_storage_changed_on_putBlob(void) {
    auto* storage = setupStorageWithChangedSubscription();

    uint8_t data[] = {1, 2, 3, 4};
    storage->putBlob("blob1", data, sizeof(data));
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    TEST_ASSERT_EQUAL_STRING("blob1", lastChangedKey.c_str());
}

void test_storage_changed_on_remove(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putString("delme", "value");
    testCore->loop();
    changedCount = 0;

    storage->remove("delme");
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    TEST_ASSERT_EQUAL_STRING("delme", lastChangedKey.c_str());
}

void test_storage_changed_on_clear(void) {
    auto* storage = setupStorageWithChangedSubscription();

    storage->putString("a", "1");
    storage->putString("b", "2");
    testCore->loop();
    changedCount = 0;

    storage->clear();
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    TEST_ASSERT_EQUAL_STRING("*", lastChangedKey.c_str());
}

void test_storage_changed_key_truncation(void) {
    auto* storage = setupStorageWithChangedSubscription();

    // Key longer than 63 chars
    String longKey = "abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789_extra";
    storage->putString(longKey, "val");
    testCore->loop();

    TEST_ASSERT_EQUAL_INT(1, changedCount);
    // Key should be truncated to 63 chars (null terminator at 64)
    TEST_ASSERT_TRUE(strlen(lastChangedKey.c_str()) <= 63);
}

void test_storage_changed_no_emit_on_failure(void) {
    // Subscribe BEFORE adding component — but do NOT call begin()
    testCore->getEventBus().subscribe(StorageEvents::EVENT_CHANGED, [](const void* payload) {
        if (payload) {
            auto* ev = static_cast<const StorageEvents::StorageChangedEvent*>(payload);
            lastChangedKey = ev->key;
        }
        changedCount++;
    });

    StorageConfig config;
    config.namespace_name = "test_noemit";
    auto storagePtr = std::make_unique<StorageComponent>(config);
    StorageComponent* storage = storagePtr.get();
    testCore->addComponent(std::move(storagePtr));
    // Deliberately NOT calling testCore->begin() — HAL not initialized

    bool result = storage->putString("key", "val");
    testCore->loop();

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(0, changedCount);
}

// ============================================================================
// Memory Stability Test for M15 emit calls
// ============================================================================

void test_storage_changed_memory_stability(void) {
    auto* storage = setupStorageWithChangedSubscription();

    // Pre-fill cache so growth doesn't affect measurement
    for (int i = 0; i < 10; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i);
        storage->putInt(key, -1);
        testCore->loop();
    }

    Testing::HeapTracker tracker;
    tracker.checkpoint("before");

    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i % 10);
        storage->putInt(key, i);
        testCore->loop();
    }

    tracker.checkpoint("after");
    Testing::MemoryTestResult result = tracker.assertStable("before", "after", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Existing EVENT_READY tests
    RUN_TEST(test_storage_ready_event_published);
    RUN_TEST(test_storage_ready_event_contains_namespace);
    RUN_TEST(test_storage_ready_not_emitted_on_failure);

    // M15 EVENT_CHANGED tests
    RUN_TEST(test_storage_changed_on_putString);
    RUN_TEST(test_storage_changed_dirty_check_string);
    RUN_TEST(test_storage_changed_on_putInt);
    RUN_TEST(test_storage_changed_dirty_check_int);
    RUN_TEST(test_storage_changed_on_putFloat);
    RUN_TEST(test_storage_changed_dirty_check_float);
    RUN_TEST(test_storage_changed_on_putBool);
    RUN_TEST(test_storage_changed_dirty_check_bool);
    RUN_TEST(test_storage_changed_on_putULong64);
    RUN_TEST(test_storage_changed_dirty_check_ulong64);
    RUN_TEST(test_storage_changed_on_putBlob);
    RUN_TEST(test_storage_changed_on_remove);
    RUN_TEST(test_storage_changed_on_clear);
    RUN_TEST(test_storage_changed_key_truncation);
    RUN_TEST(test_storage_changed_no_emit_on_failure);

    // Memory stability
    RUN_TEST(test_storage_changed_memory_stability);

    return UNITY_END();
}
