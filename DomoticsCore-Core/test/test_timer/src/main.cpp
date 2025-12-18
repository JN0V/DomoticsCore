#include <Arduino.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore::Utils;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_initial_state() {
    NonBlockingDelay timer(1000);
    printResult("Timer not ready immediately", !timer.isReady());
}

void test_ready_after_interval() {
    NonBlockingDelay timer(100);
    delay(150);
    printResult("Timer ready after interval elapsed", timer.isReady());
}

void test_reset_on_check() {
    NonBlockingDelay timer(100);
    delay(150);
    bool firstCheck = timer.isReady();
    bool secondCheck = timer.isReady();
    printResult("First check returns true", firstCheck);
    printResult("Second check returns false (reset)", !secondCheck);
}

void test_not_ready_before_interval() {
    NonBlockingDelay timer(500);
    delay(100);
    printResult("Timer not ready before interval", !timer.isReady());
}

void test_set_interval() {
    NonBlockingDelay timer(1000);
    timer.setInterval(50);
    delay(100);
    printResult("setInterval changes interval", timer.isReady());
}

void test_get_interval() {
    NonBlockingDelay timer(500);
    printResult("getInterval returns correct value", timer.getInterval() == 500);
    timer.setInterval(1000);
    printResult("getInterval returns updated value", timer.getInterval() == 1000);
}

void test_reset() {
    NonBlockingDelay timer(100);
    delay(80);
    timer.reset();
    printResult("Timer not ready after reset", !timer.isReady());
    delay(120);
    printResult("Timer ready after full interval from reset", timer.isReady());
}

void test_elapsed() {
    NonBlockingDelay timer(1000);
    delay(100);
    unsigned long elapsed = timer.elapsed();
    printResult("elapsed() returns reasonable value", elapsed >= 90 && elapsed <= 150);
}

void test_remaining() {
    NonBlockingDelay timer(1000);
    delay(100);
    unsigned long remaining = timer.remaining();
    printResult("remaining() returns reasonable value", remaining >= 850 && remaining <= 950);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore NonBlockingDelay Tests");
    Serial.println("========================================\n");
    
    test_initial_state();
    test_ready_after_interval();
    test_reset_on_check();
    test_not_ready_before_interval();
    test_set_interval();
    test_get_interval();
    test_reset();
    test_elapsed();
    test_remaining();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
