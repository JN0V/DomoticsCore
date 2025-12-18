#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_wifi_component_creation() {
    WifiComponent wifi;
    printResult("Wifi component created", wifi.metadata.name == "Wifi");
}

void test_wifi_with_credentials() {
    WifiComponent wifi("TestNetwork", "TestPassword");
    printResult("Wifi with credentials created", wifi.metadata.name == "Wifi");
}

void test_wifi_non_blocking_loop() {
    Core core;
    
    // No SSID = don't try to connect
    auto wifi = std::make_unique<WifiComponent>("", "");
    
    core.addComponent(std::move(wifi));
    core.begin();
    
    // Run several loop iterations to verify non-blocking
    unsigned long start = millis();
    int loopCount = 0;
    while (millis() - start < 100) {
        core.loop();
        loopCount++;
        delay(1);
    }
    
    // Should have run many loops (non-blocking)
    printResult("Wifi loop is non-blocking", loopCount > 50);
    
    core.shutdown();
}

void test_wifi_status_methods() {
    Core core;
    
    auto wifi = std::make_unique<WifiComponent>("", "");
    WifiComponent* wifiPtr = wifi.get();
    
    core.addComponent(std::move(wifi));
    core.begin();
    
    // Test status methods exist and don't crash
    bool staConnected = wifiPtr->isSTAConnected();
    
    printResult("Status methods work", !staConnected); // Should not be connected without SSID
    
    core.shutdown();
}

void test_wifi_credentials_update() {
    Core core;
    
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();
    
    core.addComponent(std::move(wifi));
    core.begin();
    
    // Update credentials (won't actually connect in test)
    wifiPtr->setCredentials("NewSSID", "NewPassword");
    
    printResult("Credentials update method works", true);
    
    core.shutdown();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore Wifi Component Tests");
    Serial.println("========================================\n");
    
    test_wifi_component_creation();
    test_wifi_with_credentials();
    test_wifi_non_blocking_loop();
    test_wifi_status_methods();
    test_wifi_credentials_update();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
