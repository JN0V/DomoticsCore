#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

uint32_t getHeapUsed() {
    return ESP.getHeapSize() - ESP.getFreeHeap();
}

void test_webui_component_creation() {
    uint32_t heapBefore = ESP.getFreeHeap();
    
    WebUIConfig config;
    config.enableWebSocket = false;  // Disable for memory test
    WebUIComponent webui(config);
    
    uint32_t heapAfter = ESP.getFreeHeap();
    uint32_t heapUsed = heapBefore - heapAfter;
    
    Serial.printf("  Heap used by WebUI creation: %u bytes\n", heapUsed);
    
    // WebUI should use less than 10KB on creation
    printResult("WebUI creation uses reasonable memory", heapUsed < 10240);
}

void test_webui_no_leak_on_loop() {
    Core core;
    
    WebUIConfig config;
    config.enableWebSocket = false;
    
    auto webui = std::make_unique<WebUIComponent>(config);
    
    core.addComponent(std::move(webui));
    core.begin();
    
    // Warm-up loops
    for (int i = 0; i < 10; i++) {
        core.loop();
        delay(10);
    }
    
    uint32_t heapBefore = ESP.getFreeHeap();
    
    // Run 100 loop iterations
    for (int i = 0; i < 100; i++) {
        core.loop();
        delay(1);
    }
    
    uint32_t heapAfter = ESP.getFreeHeap();
    int32_t heapDiff = (int32_t)heapBefore - (int32_t)heapAfter;
    
    Serial.printf("  Heap change after 100 loops: %d bytes\n", heapDiff);
    
    // Allow small variance (< 1KB) for timing variations
    printResult("No significant heap leak in loop", abs(heapDiff) < 1024);
    
    core.shutdown();
}

void test_webui_provider_registration() {
    Core core;
    
    WebUIConfig config;
    config.enableWebSocket = false;
    
    auto webui = std::make_unique<WebUIComponent>(config);
    WebUIComponent* webuiPtr = webui.get();
    
    core.addComponent(std::move(webui));
    core.begin();
    
    // WebUI should auto-register itself as a provider
    int clients = webuiPtr->getWebSocketClients();
    
    printResult("WebUI getWebSocketClients works", clients >= 0);
    
    core.shutdown();
}

void test_webui_config_update() {
    WebUIConfig config;
    config.theme = "dark";
    config.deviceName = "TestDevice";
    
    WebUIComponent webui(config);
    
    // Update config
    WebUIConfig newConfig;
    newConfig.theme = "light";
    newConfig.deviceName = "NewDevice";
    webui.setConfig(newConfig);
    
    const WebUIConfig& currentConfig = webui.getConfig();
    
    printResult("Config update works", 
                currentConfig.theme == "light" && 
                currentConfig.deviceName == "NewDevice");
}

void test_webui_multiple_create_destroy() {
    uint32_t heapStart = ESP.getFreeHeap();
    
    // Create and destroy WebUI 5 times
    for (int i = 0; i < 5; i++) {
        WebUIConfig config;
        config.enableWebSocket = false;
        WebUIComponent* webui = new WebUIComponent(config);
        delete webui;
    }
    
    uint32_t heapEnd = ESP.getFreeHeap();
    int32_t heapDiff = (int32_t)heapStart - (int32_t)heapEnd;
    
    Serial.printf("  Heap change after 5 create/destroy: %d bytes\n", heapDiff);
    
    // Should have no permanent leak
    printResult("No leak after multiple create/destroy", abs(heapDiff) < 512);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore WebUI Memory Tests");
    Serial.println("========================================\n");
    
    Serial.printf("Initial free heap: %u bytes\n\n", ESP.getFreeHeap());
    
    test_webui_component_creation();
    test_webui_no_leak_on_loop();
    test_webui_provider_registration();
    test_webui_config_update();
    test_webui_multiple_create_destroy();
    
    Serial.printf("\nFinal free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
