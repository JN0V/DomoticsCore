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

void test_storage_component_creation() {
    StorageConfig config;
    config.namespace_name = "test_ns";
    StorageComponent storage(config);
    
    printResult("Storage component created", storage.metadata.name == "Storage");
}

void test_storage_put_get_string() {
    Core core;
    StorageConfig config;
    config.namespace_name = "test_str";
    
    auto storage = std::make_unique<StorageComponent>(config);
    StorageComponent* storagePtr = storage.get();
    
    core.addComponent(std::move(storage));
    core.begin();
    
    storagePtr->putString("test_key", "test_value");
    String retrieved = storagePtr->getString("test_key", "default");
    
    printResult("Put/Get string works", retrieved == "test_value");
    
    core.shutdown();
}

void test_storage_put_get_int() {
    Core core;
    StorageConfig config;
    config.namespace_name = "test_int";
    
    auto storage = std::make_unique<StorageComponent>(config);
    StorageComponent* storagePtr = storage.get();
    
    core.addComponent(std::move(storage));
    core.begin();
    
    storagePtr->putInt("int_key", 42);
    int32_t retrieved = storagePtr->getInt("int_key", 0);
    
    printResult("Put/Get int works", retrieved == 42);
    
    core.shutdown();
}

void test_storage_put_get_bool() {
    Core core;
    StorageConfig config;
    config.namespace_name = "test_bool";
    
    auto storage = std::make_unique<StorageComponent>(config);
    StorageComponent* storagePtr = storage.get();
    
    core.addComponent(std::move(storage));
    core.begin();
    
    storagePtr->putBool("bool_key", true);
    bool retrieved = storagePtr->getBool("bool_key", false);
    
    printResult("Put/Get bool works", retrieved == true);
    
    core.shutdown();
}

void test_storage_put_get_float() {
    Core core;
    StorageConfig config;
    config.namespace_name = "test_float";
    
    auto storage = std::make_unique<StorageComponent>(config);
    StorageComponent* storagePtr = storage.get();
    
    core.addComponent(std::move(storage));
    core.begin();
    
    storagePtr->putFloat("float_key", 3.14f);
    float retrieved = storagePtr->getFloat("float_key", 0.0f);
    
    printResult("Put/Get float works", abs(retrieved - 3.14f) < 0.01f);
    
    core.shutdown();
}

void test_storage_default_value() {
    Core core;
    StorageConfig config;
    config.namespace_name = "test_default";
    
    auto storage = std::make_unique<StorageComponent>(config);
    StorageComponent* storagePtr = storage.get();
    
    core.addComponent(std::move(storage));
    core.begin();
    
    String retrieved = storagePtr->getString("nonexistent_key", "default_value");
    
    printResult("Default value returned for missing key", retrieved == "default_value");
    
    core.shutdown();
}

void test_storage_remove_key() {
    Core core;
    StorageConfig config;
    config.namespace_name = "test_remove";
    
    auto storage = std::make_unique<StorageComponent>(config);
    StorageComponent* storagePtr = storage.get();
    
    core.addComponent(std::move(storage));
    core.begin();
    
    storagePtr->putString("to_remove", "value");
    String before = storagePtr->getString("to_remove", "none");
    printResult("Key exists before remove", before == "value");
    
    storagePtr->remove("to_remove");
    String after = storagePtr->getString("to_remove", "none");
    printResult("Key removed successfully", after == "none");
    
    core.shutdown();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore Storage Basic Tests");
    Serial.println("========================================\n");
    
    test_storage_component_creation();
    test_storage_put_get_string();
    test_storage_put_get_int();
    test_storage_put_get_bool();
    test_storage_put_get_float();
    test_storage_default_value();
    test_storage_remove_key();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
