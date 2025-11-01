#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Test result tracking
int testsPassed = 0;
int testsFailed = 0;

#define TEST_ASSERT(condition, message) \
    if (condition) { \
        Serial.printf("  ✅ PASS: %s\n", message); \
        testsPassed++; \
    } else { \
        Serial.printf("  ❌ FAIL: %s\n", message); \
        testsFailed++; \
    }

#define TEST_BEGIN(name) \
    Serial.printf("\n>>> Test: %s\n", name);

#define TEST_END() \
    Serial.println("\n==========================================="); \
    if (testsFailed > 0) { \
        Serial.printf("❌ TEST SUITE FAILED: %d passed, %d failed\n", testsPassed, testsFailed); \
        Serial.println("===========================================\n"); \
        while(1) { delay(1000); } \
    } else { \
        Serial.printf("✅ TEST SUITE PASSED: All %d tests passed!\n", testsPassed); \
        Serial.println("===========================================\n"); \
    }

Core core1, core2;
StorageComponent* storage1;
StorageComponent* storage2;

void testNamespaceIsolation() {
    TEST_BEGIN("Namespace Isolation - Same Keys, Different Values");
    
    // Write to namespace1
    storage1->putString("key1", "value1_ns1");
    storage1->putInt("key2", 100);
    storage1->putBool("key3", true);
    
    // Write to namespace2 (same keys, different values)
    storage2->putString("key1", "value1_ns2");
    storage2->putInt("key2", 200);
    storage2->putBool("key3", false);
    
    // Verify namespace1 values
    String val1_ns1 = storage1->getString("key1", "");
    int val2_ns1 = storage1->getInt("key2", 0);
    bool val3_ns1 = storage1->getBool("key3", false);
    
    TEST_ASSERT(val1_ns1 == "value1_ns1", "Namespace1 String value correct");
    TEST_ASSERT(val2_ns1 == 100, "Namespace1 Int value correct");
    TEST_ASSERT(val3_ns1 == true, "Namespace1 Bool value correct");
    
    // Verify namespace2 values
    String val1_ns2 = storage2->getString("key1", "");
    int val2_ns2 = storage2->getInt("key2", 0);
    bool val3_ns2 = storage2->getBool("key3", true);
    
    TEST_ASSERT(val1_ns2 == "value1_ns2", "Namespace2 String value correct");
    TEST_ASSERT(val2_ns2 == 200, "Namespace2 Int value correct");
    TEST_ASSERT(val3_ns2 == false, "Namespace2 Bool value correct");
    
    // Verify isolation (values are different)
    TEST_ASSERT(val1_ns1 != val1_ns2, "String values isolated between namespaces");
    TEST_ASSERT(val2_ns1 != val2_ns2, "Int values isolated between namespaces");
    TEST_ASSERT(val3_ns1 != val3_ns2, "Bool values isolated between namespaces");
}

void testNamespaceNames() {
    TEST_BEGIN("Namespace Name Configuration");
    
    String ns1 = storage1->getNamespace();
    String ns2 = storage2->getNamespace();
    
    TEST_ASSERT(ns1 == "test_ns1", "Namespace1 name is 'test_ns1'");
    TEST_ASSERT(ns2 == "test_ns2", "Namespace2 name is 'test_ns2'");
    TEST_ASSERT(ns1 != ns2, "Namespace names are different");
}

void testEntryCount() {
    TEST_BEGIN("Entry Count Per Namespace");
    
    // Each namespace should have its own entries
    size_t count1 = storage1->getEntryCount();
    size_t count2 = storage2->getEntryCount();
    
    TEST_ASSERT(count1 >= 3, "Namespace1 has at least 3 entries");
    TEST_ASSERT(count2 >= 3, "Namespace2 has at least 3 entries");
    
    Serial.printf("  ℹ️  Namespace1 entries: %d\n", count1);
    Serial.printf("  ℹ️  Namespace2 entries: %d\n", count2);
}

void testClear() {
    TEST_BEGIN("Clear Per Namespace");
    
    // Clear namespace1
    storage1->clear();
    
    // Verify namespace1 is empty
    String val1 = storage1->getString("key1", "DEFAULT");
    TEST_ASSERT(val1 == "DEFAULT", "Namespace1 cleared (key1 returns default)");
    
    // Verify namespace2 is NOT affected
    String val2 = storage2->getString("key1", "DEFAULT");
    TEST_ASSERT(val2 == "value1_ns2", "Namespace2 NOT affected by namespace1 clear");
}

void testMultipleTypes() {
    TEST_BEGIN("Multiple Data Types Isolation");
    
    // Write various types to namespace1
    storage1->putString("str", "test_string");
    storage1->putInt("int", 42);
    storage1->putBool("bool", true);
    storage1->putFloat("float", 3.14f);
    storage1->putULong64("ulong64", 1234567890UL);
    
    // Write different values to namespace2
    storage2->putString("str", "different_string");
    storage2->putInt("int", 99);
    storage2->putBool("bool", false);
    storage2->putFloat("float", 2.71f);
    storage2->putULong64("ulong64", 9876543210UL);
    
    // Verify isolation for all types
    TEST_ASSERT(storage1->getString("str", "") == "test_string", "String type isolated");
    TEST_ASSERT(storage2->getString("str", "") == "different_string", "String type isolated (ns2)");
    
    TEST_ASSERT(storage1->getInt("int", 0) == 42, "Int type isolated");
    TEST_ASSERT(storage2->getInt("int", 0) == 99, "Int type isolated (ns2)");
    
    TEST_ASSERT(storage1->getBool("bool", false) == true, "Bool type isolated");
    TEST_ASSERT(storage2->getBool("bool", true) == false, "Bool type isolated (ns2)");
    
    TEST_ASSERT(abs(storage1->getFloat("float", 0.0f) - 3.14f) < 0.01f, "Float type isolated");
    TEST_ASSERT(abs(storage2->getFloat("float", 0.0f) - 2.71f) < 0.01f, "Float type isolated (ns2)");
    
    TEST_ASSERT(storage1->getULong64("ulong64", 0UL) == 1234567890UL, "ULong64 type isolated");
    TEST_ASSERT(storage2->getULong64("ulong64", 0UL) == 9876543210UL, "ULong64 type isolated (ns2)");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n===========================================");
    Serial.println("Storage: Namespace Isolation Unit Test");
    Serial.println("===========================================");
    Serial.println("Testing: Multiple namespaces can coexist");
    Serial.println("         without key conflicts");
    
    // Create two storage components with different namespaces
    StorageConfig config1;
    config1.namespace_name = "test_ns1";
    auto s1 = std::make_unique<StorageComponent>(config1);
    storage1 = s1.get();
    core1.addComponent(std::move(s1));
    
    StorageConfig config2;
    config2.namespace_name = "test_ns2";
    auto s2 = std::make_unique<StorageComponent>(config2);
    storage2 = s2.get();
    core2.addComponent(std::move(s2));
    
    // Initialize both
    CoreConfig coreConfig;
    coreConfig.deviceName = "NamespaceTest";
    core1.begin(coreConfig);
    core2.begin(coreConfig);
    
    Serial.println("\nRunning tests...");
    
    // Run all tests
    testNamespaceNames();
    testNamespaceIsolation();
    testEntryCount();
    testMultipleTypes();
    testClear();
    
    // Final result
    TEST_END();
}

void loop() {
    // Test complete - do nothing
    delay(10000);
}
