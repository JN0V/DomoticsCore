#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/NTP.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_ntp_component_creation() {
    NTPComponent ntp;
    printResult("NTP component created", ntp.metadata.name == "NTP");
}

void test_ntp_with_config() {
    NTPConfig config;
    config.servers = {"pool.ntp.org", "time.google.com"};
    config.timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
    config.syncInterval = 3600;
    
    NTPComponent ntp(config);
    printResult("NTP with config created", ntp.metadata.name == "NTP");
}

void test_ntp_non_blocking_loop() {
    Core core;
    
    NTPConfig config;
    config.servers.clear();  // No server = don't try to sync
    
    auto ntp = std::make_unique<NTPComponent>(config);
    
    core.addComponent(std::move(ntp));
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
    printResult("NTP loop is non-blocking", loopCount > 50);
    
    core.shutdown();
}

void test_ntp_sync_status() {
    Core core;
    
    NTPConfig config;
    config.servers.clear();  // No server
    
    auto ntp = std::make_unique<NTPComponent>(config);
    NTPComponent* ntpPtr = ntp.get();
    
    core.addComponent(std::move(ntp));
    core.begin();
    
    // Without network, should not be synced
    bool synced = ntpPtr->isSynced();
    
    printResult("NTP sync status available", !synced);  // Should not be synced without network
    
    core.shutdown();
}

void test_ntp_timezone_config() {
    NTPConfig config;
    config.timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
    
    NTPComponent ntp(config);
    
    const NTPConfig& currentConfig = ntp.getConfig();
    
    printResult("Timezone config stored correctly", 
                currentConfig.timezone == "CET-1CEST,M3.5.0,M10.5.0/3");
}

void test_ntp_uses_nonblocking_delay() {
    // Test that NTP uses NonBlockingDelay for sync intervals
    Core core;
    
    NTPConfig config;
    config.syncInterval = 1000;  // 1 second interval
    
    auto ntp = std::make_unique<NTPComponent>(config);
    
    core.addComponent(std::move(ntp));
    core.begin();
    
    // Run multiple loops - should not block
    unsigned long start = millis();
    int iterations = 0;
    
    while (millis() - start < 50) {
        core.loop();
        iterations++;
    }
    
    // Should complete many iterations without blocking
    printResult("NTP uses non-blocking sync", iterations > 10);
    
    core.shutdown();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore NTP Component Tests");
    Serial.println("========================================\n");
    
    test_ntp_component_creation();
    test_ntp_with_config();
    test_ntp_non_blocking_loop();
    test_ntp_sync_status();
    test_ntp_timezone_config();
    test_ntp_uses_nonblocking_delay();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
