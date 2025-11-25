#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core core;

void setup() {
    Serial.begin(115200);
    delay(1000);

    DLOG_I("TEST", "===========================================");
    DLOG_I("TEST", "Testing Storage Namespace Isolation");
    DLOG_I("TEST", "===========================================\n");

    // 1. Initialize Storage A (Namespace "ns_a")
    StorageConfig configA;
    configA.namespace_name = "ns_a";
    auto storageA = std::make_unique<StorageComponent>(configA);
    // We manually begin them to test without Core loop for now, 
    // or we could add them to core.
    // But StorageComponent::begin() opens preferences, so it's enough.
    storageA->begin();
    storageA->clear(); // Clean start

    // 2. Initialize Storage B (Namespace "ns_b")
    StorageConfig configB;
    configB.namespace_name = "ns_b";
    auto storageB = std::make_unique<StorageComponent>(configB);
    storageB->begin();
    storageB->clear(); // Clean start

    DLOG_I("TEST", ">>> Writing 'test_key' to both namespaces with DIFFERENT values");
    
    // Write "value_A" to ns_a
    storageA->putString("test_key", "value_A");
    DLOG_I("TEST", "[NS_A] Wrote test_key = value_A");

    // Write "value_B" to ns_b
    storageB->putString("test_key", "value_B");
    DLOG_I("TEST", "[NS_B] Wrote test_key = value_B");

    DLOG_I("TEST", "\n>>> Verifying isolation...");

    // Read back
    String valA = storageA->getString("test_key");
    String valB = storageB->getString("test_key");

    DLOG_I("TEST", "[NS_A] Read test_key = %s", valA.c_str());
    DLOG_I("TEST", "[NS_B] Read test_key = %s", valB.c_str());

    bool passed = true;
    if (valA != "value_A") {
        DLOG_E("TEST", "❌ NS_A value incorrect! Expected 'value_A', got '%s'", valA.c_str());
        passed = false;
    }
    if (valB != "value_B") {
        DLOG_E("TEST", "❌ NS_B value incorrect! Expected 'value_B', got '%s'", valB.c_str());
        passed = false;
    }

    if (passed) {
        DLOG_I("TEST", "\n🎉 TEST PASSED! Namespaces are isolated.");
    } else {
        DLOG_E("TEST", "\n❌ TEST FAILED! Namespace collision detected.");
    }
    DLOG_I("TEST", "===========================================\n");
}

void loop() {
    delay(1000);
}
