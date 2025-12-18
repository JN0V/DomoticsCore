#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/LED.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_led_component_creation() {
    LEDComponent led;
    led.addSingleLED(2, "TestLED");
    
    printResult("LED component created", led.metadata.name == "LEDComponent");
}

void test_led_set_by_name() {
    Core core;
    
    auto led = std::make_unique<LEDComponent>();
    led->addSingleLED(2, "BrightnessTest");
    LEDComponent* ledPtr = led.get();
    
    core.addComponent(std::move(led));
    core.begin();
    
    bool result = ledPtr->setLED("BrightnessTest", LEDColor::White(), 128);
    
    printResult("Set LED by name works", result);
    
    core.shutdown();
}

void test_led_set_rgb_color() {
    Core core;
    
    auto led = std::make_unique<LEDComponent>();
    led->addRGBLED(25, 26, 27, "RGBTest");
    LEDComponent* ledPtr = led.get();
    
    core.addComponent(std::move(led));
    core.begin();
    
    bool result = ledPtr->setLED("RGBTest", LEDColor::Red(), 255);
    
    printResult("Set RGB color works", result);
    
    core.shutdown();
}

void test_led_effect_blink() {
    Core core;
    
    auto led = std::make_unique<LEDComponent>();
    led->addSingleLED(2, "BlinkTest");
    LEDComponent* ledPtr = led.get();
    
    core.addComponent(std::move(led));
    core.begin();
    
    bool result = ledPtr->setLEDEffect("BlinkTest", LEDEffect::Blink, 500);
    
    // Run several loop iterations to verify non-blocking
    unsigned long start = millis();
    int loopCount = 0;
    while (millis() - start < 100) {
        core.loop();
        loopCount++;
        delay(1);
    }
    
    printResult("Blink effect set", result);
    printResult("Blink effect is non-blocking", loopCount > 50);
    
    core.shutdown();
}

void test_led_effect_fade() {
    Core core;
    
    auto led = std::make_unique<LEDComponent>();
    led->addSingleLED(2, "FadeTest");
    LEDComponent* ledPtr = led.get();
    
    core.addComponent(std::move(led));
    core.begin();
    
    bool result = ledPtr->setLEDEffect("FadeTest", LEDEffect::Fade, 1000);
    
    unsigned long start = millis();
    int loopCount = 0;
    while (millis() - start < 100) {
        core.loop();
        loopCount++;
        delay(1);
    }
    
    printResult("Fade effect set", result);
    printResult("Fade effect is non-blocking", loopCount > 50);
    
    core.shutdown();
}

void test_led_multiple_leds() {
    Core core;
    
    auto led = std::make_unique<LEDComponent>();
    led->addSingleLED(2, "LED1");
    led->addSingleLED(4, "LED2");
    LEDComponent* ledPtr = led.get();
    
    core.addComponent(std::move(led));
    core.begin();
    
    bool r1 = ledPtr->setLED("LED1", LEDColor::White(), 255);
    bool r2 = ledPtr->setLED("LED2", LEDColor::White(), 128);
    
    printResult("Multiple LEDs supported", r1 && r2);
    
    core.shutdown();
}

void test_led_by_index() {
    Core core;
    
    auto led = std::make_unique<LEDComponent>();
    led->addSingleLED(2, "IndexTest");
    LEDComponent* ledPtr = led.get();
    
    core.addComponent(std::move(led));
    core.begin();
    
    bool r1 = ledPtr->setLED(0, LEDColor::White(), 255);
    bool r2 = ledPtr->setLED(99, LEDColor::White(), 255); // Invalid index
    
    printResult("Set LED by index works", r1);
    printResult("Invalid index returns false", !r2);
    
    core.shutdown();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore LED Patterns Tests");
    Serial.println("========================================\n");
    
    test_led_component_creation();
    test_led_set_by_name();
    test_led_set_rgb_color();
    test_led_effect_blink();
    test_led_effect_fade();
    test_led_multiple_leds();
    test_led_by_index();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
