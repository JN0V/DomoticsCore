#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Components/WiFi.h>
#include <DomoticsCore/Components/Storage.h>
#include <DomoticsCore/Components/LED.h>
// Note: WebUI.h is NOT included - testing without WebUI

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

Core* core = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    DLOG_I(LOG_CORE, "\n=== DomoticsCore ComponentWithoutWebUI Demo ===");
    DLOG_I(LOG_CORE, "Testing components WITHOUT WebUI dependency");
    
    // Create core configuration
    CoreConfig config;
    config.deviceName = "TestDevice";
    config.logLevel = 3; // INFO level
    core = new Core();
    
    // Create WiFi component without WebUI
    auto wifiComponent = std::unique_ptr<WiFiComponent>(new WiFiComponent("YourWifiSSID", "YourWifiPassword"));
    core->addComponent(std::move(wifiComponent));
    
    // Create Storage component without WebUI
    StorageConfig storageConfig;
    storageConfig.namespace_name = "test_app";
    storageConfig.readOnly = false;
    storageConfig.maxEntries = 50;
    auto storageComponent = std::unique_ptr<StorageComponent>(new StorageComponent(storageConfig));
    core->addComponent(std::move(storageComponent));
    
    // Create LED component without WebUI
    auto ledComponent = std::unique_ptr<LEDComponent>(new LEDComponent());
    core->addComponent(std::move(ledComponent));
    
    // Initialize core
    if (!core->begin(config)) {
        DLOG_E(LOG_CORE, "Failed to initialize core!");
        return;
    }
    
    DLOG_I(LOG_CORE, "Core initialized successfully!");
    DLOG_I(LOG_CORE, "Components registered: %d", core->getComponentCount());
    
    // Test storage functionality
    auto* storage = core->getComponent<StorageComponent>("Storage");
    if (storage) {
        DLOG_I(LOG_CORE, "\n=== Testing Storage Component ===");
        storage->putString("test_key", "Hello World!");
        storage->putInt("counter", 42);
        storage->putBool("enabled", true);
        
        DLOG_I(LOG_CORE, "Stored string: %s", storage->getString("test_key", "default").c_str());
        DLOG_I(LOG_CORE, "Stored int: %d", storage->getInt("counter", 0));
        DLOG_I(LOG_CORE, "Stored bool: %s", storage->getBool("enabled", false) ? "true" : "false");
        DLOG_I(LOG_CORE, "Storage entries: %d", storage->getEntryCount());
    }
    
    // Test LED functionality
    auto* led = core->getComponent<LEDComponent>("LEDComponent");
    if (led) {
        DLOG_I(LOG_CORE, "\n=== Testing LED Component ===");
        // Add a simple LED for testing
        LEDConfig ledConfig;
        ledConfig.pin = 2; // Use GPIO 2 instead of LED_BUILTIN
        ledConfig.name = "TestLED";
        ledConfig.invertLogic = true;
        led->addLED(ledConfig);
        
        // Set LED color and effect
        led->setLED(0, LEDColor::Red(), 128);
        led->setLEDEffect(0, LEDEffect::Blink);
        DLOG_I(LOG_CORE, "LED configured for red blinking");
    }
    
    DLOG_I(LOG_CORE, "\n=== Setup Complete ===");
    DLOG_I(LOG_CORE, "Components are running WITHOUT WebUI dependency!");
}

void loop() {
    if (core) {
        core->loop();
    }
    
    // Simple test output every 10 seconds
    static unsigned long lastOutput = 0;
    if (millis() - lastOutput > 10000) {
        lastOutput = millis();
        DLOG_I(LOG_CORE, "Components running without WebUI - SUCCESS!");
    }
}
