#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_namespace_isolation() {
    // Create two storage components with different namespaces
    Core core1;
    StorageConfig config1;
    config1.namespace_name = "ns_alpha";
    auto storage1 = std::make_unique<StorageComponent>(config1);
    StorageComponent* s1 = storage1.get();
    core1.addComponent(std::move(storage1));
    core1.begin();
    
    Core core2;
    StorageConfig config2;
    config2.namespace_name = "ns_beta";
    auto storage2 = std::make_unique<StorageComponent>(config2);
    StorageComponent* s2 = storage2.get();
    core2.addComponent(std::move(storage2));
    core2.begin();
    
    // Write same key to both namespaces with different values
    s1->putString("shared_key", "value_from_alpha");
    s2->putString("shared_key", "value_from_beta");
    
    // Read back - should get namespace-specific values
    String v1 = s1->getString("shared_key", "");
    String v2 = s2->getString("shared_key", "");
    
    printResult("Namespace alpha has correct value", v1 == "value_from_alpha");
    printResult("Namespace beta has correct value", v2 == "value_from_beta");
    printResult("Namespaces are isolated", v1 != v2);
    
    core1.shutdown();
    core2.shutdown();
}

void test_same_namespace_shared() {
    // Two components using same namespace should see each other's data
    Core core1;
    StorageConfig config1;
    config1.namespace_name = "shared_ns";
    auto storage1 = std::make_unique<StorageComponent>(config1);
    StorageComponent* s1 = storage1.get();
    core1.addComponent(std::move(storage1));
    core1.begin();
    
    s1->putString("persistence_test", "written_by_first");
    core1.shutdown();
    
    // Open same namespace again
    Core core2;
    StorageConfig config2;
    config2.namespace_name = "shared_ns";
    auto storage2 = std::make_unique<StorageComponent>(config2);
    StorageComponent* s2 = storage2.get();
    core2.addComponent(std::move(storage2));
    core2.begin();
    
    String retrieved = s2->getString("persistence_test", "not_found");
    printResult("Same namespace sees persisted data", retrieved == "written_by_first");
    
    core2.shutdown();
}

void test_default_namespace() {
    Core core;
    StorageConfig config; // Uses default namespace "domotics"
    auto storage = std::make_unique<StorageComponent>(config);
    StorageComponent* s = storage.get();
    core.addComponent(std::move(storage));
    core.begin();
    
    s->putString("default_ns_key", "default_value");
    String retrieved = s->getString("default_ns_key", "");
    
    printResult("Default namespace works", retrieved == "default_value");
    
    core.shutdown();
}

void test_namespace_clear() {
    Core core;
    StorageConfig config;
    config.namespace_name = "to_clear";
    auto storage = std::make_unique<StorageComponent>(config);
    StorageComponent* s = storage.get();
    core.addComponent(std::move(storage));
    core.begin();
    
    s->putString("key1", "value1");
    s->putString("key2", "value2");
    s->putInt("key3", 42);
    
    printResult("Keys exist before clear", 
        s->getString("key1", "") == "value1" &&
        s->getString("key2", "") == "value2" &&
        s->getInt("key3", 0) == 42);
    
    s->clear();
    
    printResult("Keys cleared after clear()", 
        s->getString("key1", "none") == "none" &&
        s->getString("key2", "none") == "none" &&
        s->getInt("key3", -1) == -1);
    
    core.shutdown();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore Storage Namespace Tests");
    Serial.println("========================================\n");
    
    test_namespace_isolation();
    test_same_namespace_shared();
    test_default_namespace();
    test_namespace_clear();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
