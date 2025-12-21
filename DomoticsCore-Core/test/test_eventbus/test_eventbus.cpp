#include <unity.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Platform_Stub.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Utils;

EventBus* testBus = nullptr;

void setUp(void) {
    testBus = new EventBus();
}

void tearDown(void) {
    delete testBus;
    testBus = nullptr;
}

void test_subscribe_and_publish(void) {
    bool received = false;
    int receivedValue = 0;
    
    String topic = String("test/topic");
    testBus->subscribe(topic, [&](const void* payload) {
        if (payload) {
            receivedValue = *static_cast<const int*>(payload);
            received = true;
        }
    }, nullptr);
    
    int msg = 42;
    testBus->publish(topic, msg);
    testBus->poll();
    
    TEST_ASSERT_TRUE(received);
    TEST_ASSERT_EQUAL(42, receivedValue);
}

void test_multiple_subscribers(void) {
    int count = 0;
    
    String topic = String("multi/topic");
    testBus->subscribe(topic, [&](const void*) { count++; }, nullptr);
    testBus->subscribe(topic, [&](const void*) { count++; }, nullptr);
    testBus->subscribe(topic, [&](const void*) { count++; }, nullptr);
    
    int payload = 42;
    testBus->publish(topic, payload);
    testBus->poll();
    
    TEST_ASSERT_EQUAL(3, count);
}

void test_different_topics_isolated(void) {
    bool topic1Received = false;
    bool topic2Received = false;
    
    String topic1 = String("topic/one");
    String topic2 = String("topic/two");
    testBus->subscribe(topic1, [&](const void*) { topic1Received = true; }, nullptr);
    testBus->subscribe(topic2, [&](const void*) { topic2Received = true; }, nullptr);
    
    int payload = 1;
    testBus->publish(topic1, payload);
    testBus->poll();
    
    TEST_ASSERT_TRUE(topic1Received);
    TEST_ASSERT_FALSE(topic2Received);
}

void test_unsubscribe(void) {
    int count = 0;
    
    String topic = String("unsub/topic");
    uint32_t subId = testBus->subscribe(topic, [&](const void*) { count++; }, nullptr);
    
    int payload = 1;
    testBus->publish(topic, payload);
    testBus->poll();
    TEST_ASSERT_EQUAL(1, count);
    
    testBus->unsubscribe(subId);
    testBus->publish(topic, payload);
    testBus->poll();
    TEST_ASSERT_EQUAL(1, count);
}

void test_sticky_event(void) {
    int receivedValue = 0;
    
    int msg = 123;
    String topic = String("sticky/topic");
    testBus->publishSticky(topic, msg);
    
    testBus->subscribe(topic, [&](const void* payload) {
        if (payload) receivedValue = *static_cast<const int*>(payload);
    }, nullptr, true);
    
    testBus->poll();
    TEST_ASSERT_EQUAL(123, receivedValue);
}

void test_wildcard_subscription(void) {
    int sensorCount = 0;
    int actuatorCount = 0;
    
    String sensorTopic = String("sensor.temperature");
    String actuatorTopic = String("actuator.led");
    String wildcardTopic = String("sensor.*");
    
    testBus->subscribe(wildcardTopic, [&](const void*) { sensorCount++; }, nullptr);
    testBus->subscribe(String("actuator.*"), [&](const void*) { actuatorCount++; }, nullptr);
    
    int payload = 25;
    testBus->publish(sensorTopic, payload);
    testBus->publish(actuatorTopic, payload);
    testBus->poll();
    
    TEST_ASSERT_EQUAL(1, sensorCount);
    TEST_ASSERT_EQUAL(1, actuatorCount);
}

void test_message_order(void) {
    std::vector<int> received;
    testBus->subscribe(String("test.order"), [&](const void* payload) {
        auto* value = static_cast<const int*>(payload);
        if (value) received.push_back(*value);
    }, nullptr);
    
    // Publish 5 messages in order
    for (int i = 1; i <= 5; i++) {
        testBus->publish(String("test.order"), i);
    }
    
    // Process all messages
    for (int i = 0; i < 2; i++) {
        testBus->poll();
    }
    
    // Verify order is preserved
    TEST_ASSERT_EQUAL(5, received.size());
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(i + 1, received[i]);
    }
}

void test_unsubscribe_owner(void) {
    int count = 0;
    void* owner = (void*)0x1234; // Fake owner pointer
    
    testBus->subscribe(String("test.unsub"), [&](const void*) { count++; }, owner);
    testBus->publish(String("test.unsub"), 1);
    testBus->poll();
    TEST_ASSERT_EQUAL(1, count);
    
    // Unsubscribe all subscriptions for this owner
    testBus->unsubscribeOwner(owner);
    testBus->publish(String("test.unsub"), 2);
    testBus->poll();
    TEST_ASSERT_EQUAL(1, count); // Should still be 1, not 2
}

void test_backpressure(void) {
    std::vector<int> received;
    testBus->subscribe(String("test.pressure"), [&](const void* payload) {
        auto* value = static_cast<const int*>(payload);
        if (value) received.push_back(*value);
    }, nullptr);
    
    // Publish more than queue capacity (32) to test backpressure
    for (int i = 0; i < 100; i++) {
        testBus->publish(String("test.pressure"), i);
    }
    
    // Drain the queue
    for (int i = 0; i < 10; i++) {
        testBus->poll();
    }
    
    // Should only have last 32 messages (68-99) due to drop-oldest policy
    TEST_ASSERT_EQUAL(32, received.size());
    for (int i = 0; i < 32; i++) {
        TEST_ASSERT_EQUAL(68 + i, received[i]);
    }
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_subscribe_and_publish);
    RUN_TEST(test_multiple_subscribers);
    RUN_TEST(test_different_topics_isolated);
    RUN_TEST(test_unsubscribe);
    RUN_TEST(test_sticky_event);
    RUN_TEST(test_wildcard_subscription);
    RUN_TEST(test_message_order);
    RUN_TEST(test_unsubscribe_owner);
    RUN_TEST(test_backpressure);
    
    return UNITY_END();
}
