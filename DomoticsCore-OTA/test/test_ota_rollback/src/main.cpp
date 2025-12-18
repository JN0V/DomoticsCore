#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/OTA.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_ota_component_creation() {
    OTAComponent ota;
    printResult("OTA component created", ota.metadata.name == "OTA");
}

void test_ota_with_config() {
    OTAConfig config;
    config.updateUrl = "https://example.com/firmware.bin";
    config.checkIntervalMs = 3600000;
    config.autoReboot = false;
    config.requireTLS = true;
    
    OTAComponent ota(config);
    printResult("OTA with config created", ota.metadata.name == "OTA");
}

void test_ota_initial_state() {
    OTAComponent ota;
    
    OTAComponent::State state = ota.getState();
    bool isIdle = ota.isIdle();
    
    printResult("OTA initial state is Idle", state == OTAComponent::State::Idle && isIdle);
}

void test_ota_progress_tracking() {
    OTAComponent ota;
    
    float progress = ota.getProgress();
    size_t downloaded = ota.getDownloadedBytes();
    size_t total = ota.getTotalBytes();
    
    // Initial progress should be 0
    printResult("OTA progress tracking works", 
                progress == 0.0f && downloaded == 0 && total == 0);
}

void test_ota_config_update() {
    OTAComponent ota;
    
    OTAConfig newConfig;
    newConfig.updateUrl = "https://new.example.com/firmware.bin";
    newConfig.autoReboot = false;
    
    ota.setConfig(newConfig);
    
    const OTAConfig& current = ota.getConfig();
    
    printResult("OTA config update works", 
                current.updateUrl == "https://new.example.com/firmware.bin" &&
                current.autoReboot == false);
}

void test_ota_non_blocking_loop() {
    Core core;
    
    OTAConfig config;
    config.updateUrl = "";  // No URL = won't try to update
    config.checkIntervalMs = 0;  // Disable auto-check
    
    auto ota = std::make_unique<OTAComponent>(config);
    
    core.addComponent(std::move(ota));
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
    printResult("OTA loop is non-blocking", loopCount > 50);
    
    core.shutdown();
}

void test_ota_upload_api() {
    OTAComponent ota;
    
    // Begin upload without actually sending data
    bool beginResult = ota.beginUpload(1024);
    
    // Should start in a valid state
    bool isBusy = ota.isBusy();
    
    // Abort the upload
    ota.abortUpload("Test abort");
    
    bool isIdleAfter = ota.isIdle();
    
    printResult("OTA upload API works", beginResult && isBusy && isIdleAfter);
}

void test_ota_security_config() {
    OTAConfig config;
    config.requireTLS = true;
    config.allowDowngrades = false;
    config.maxDownloadSize = 2 * 1024 * 1024;  // 2MB limit
    
    OTAComponent ota(config);
    
    const OTAConfig& current = ota.getConfig();
    
    printResult("OTA security config stored", 
                current.requireTLS == true &&
                current.allowDowngrades == false &&
                current.maxDownloadSize == 2 * 1024 * 1024);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore OTA Component Tests");
    Serial.println("========================================\n");
    
    test_ota_component_creation();
    test_ota_with_config();
    test_ota_initial_state();
    test_ota_progress_tracking();
    test_ota_config_update();
    test_ota_non_blocking_loop();
    test_ota_upload_api();
    test_ota_security_config();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
