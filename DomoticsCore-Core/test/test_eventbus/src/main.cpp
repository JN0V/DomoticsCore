#include <Arduino.h>
#include <DomoticsCore/EventBus.h>

using namespace DomoticsCore::Utils;

int testsPassed = 0;
int testsFailed = 0;

void printResult(const char* testName, bool passed) {
    Serial.printf("%s %s\n", passed ? "✓" : "✗", testName);
    passed ? testsPassed++ : testsFailed++;
}

void test_subscribe_and_publish() {
    EventBus bus;
    bool received = false;
    String receivedValue = "";
    
    bus.subscribe("test/topic", [&](const void* payload) {
        if (payload) {
            receivedValue = *static_cast<const String*>(payload);
            received = true;
        }
    }, nullptr);
    
    String msg = "hello";
    bus.publish("test/topic", msg);
    bus.poll();
    
    printResult("Subscribe and publish works", received && receivedValue == "hello");
}

void test_multiple_subscribers() {
    EventBus bus;
    int count = 0;
    
    bus.subscribe("multi/topic", [&](const void*) { count++; }, nullptr);
    bus.subscribe("multi/topic", [&](const void*) { count++; }, nullptr);
    bus.subscribe("multi/topic", [&](const void*) { count++; }, nullptr);
    
    int payload = 42;
    bus.publish("multi/topic", payload);
    bus.poll();
    
    printResult("Multiple subscribers all receive", count == 3);
}

void test_different_topics_isolated() {
    EventBus bus;
    bool topic1Received = false;
    bool topic2Received = false;
    
    bus.subscribe("topic/one", [&](const void*) { topic1Received = true; }, nullptr);
    bus.subscribe("topic/two", [&](const void*) { topic2Received = true; }, nullptr);
    
    int payload = 1;
    bus.publish("topic/one", payload);
    bus.poll();
    
    printResult("Only correct topic receives", topic1Received && !topic2Received);
}

void test_unsubscribe() {
    EventBus bus;
    int count = 0;
    
    uint32_t subId = bus.subscribe("unsub/topic", [&](const void*) { count++; }, nullptr);
    
    int payload = 1;
    bus.publish("unsub/topic", payload);
    bus.poll();
    printResult("Receives before unsubscribe", count == 1);
    
    bus.unsubscribe(subId);
    bus.publish("unsub/topic", payload);
    bus.poll();
    printResult("Does not receive after unsubscribe", count == 1);
}

void test_sticky_event() {
    EventBus bus;
    String receivedValue = "";
    
    String msg = "sticky_value";
    bus.publishSticky("sticky/topic", msg);
    
    bus.subscribe("sticky/topic", [&](const void* payload) {
        if (payload) receivedValue = *static_cast<const String*>(payload);
    }, nullptr, true);
    
    bus.poll();
    printResult("Sticky event replayed to new subscriber", receivedValue == "sticky_value");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n========================================");
    Serial.println("DomoticsCore EventBus Tests");
    Serial.println("========================================\n");
    
    test_subscribe_and_publish();
    test_multiple_subscribers();
    test_different_topics_isolated();
    test_unsubscribe();
    test_sticky_event();
    
    Serial.printf("\nResults: %d passed, %d failed\n", testsPassed, testsFailed);
    Serial.println(testsFailed == 0 ? "🎉 ALL TESTS PASSED!" : "❌ SOME TESTS FAILED");
}

void loop() { delay(1000); }
