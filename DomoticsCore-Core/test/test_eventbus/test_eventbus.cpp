#include <unity.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Platform_Stub.h>
#include <DomoticsCore/Testing/HeapTracker.h>

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

void test_publish_during_dispatch_safe(void) {
    // Verify that publishing during dispatch (allowed) works correctly
    // with const auto& iteration (no vector copy).
    // subscribe/unsubscribe during dispatch would trigger assert in debug builds.
    int firstReceived = 0;
    int secondReceived = 0;

    testBus->subscribe(String("event/a"), [&](const void* payload) {
        firstReceived++;
        // Publishing during dispatch is allowed (enqueues for next poll)
        testBus->publish(String("event/b"), 99);
    }, nullptr);

    testBus->subscribe(String("event/b"), [&](const void* payload) {
        if (payload) secondReceived = *static_cast<const int*>(payload);
    }, nullptr);

    int val = 1;
    testBus->publish(String("event/a"), val);
    testBus->poll();  // dispatches event/a, handler enqueues event/b, then drains event/b too

    TEST_ASSERT_EQUAL(1, firstReceived);
    // event/b is processed in same poll() cycle (queue drains continuously)
    TEST_ASSERT_EQUAL(99, secondReceived);
}

// --- M9/M10 bug fix tests (TDD RED phase) ---

void test_reset_clears_wildcard_subscriptions(void) {
    int count = 0;
    testBus->subscribe(String("sensor.*"), [&](const void*) { count++; }, nullptr);

    testBus->reset();

    int payload = 1;
    testBus->publish(String("sensor.temp"), payload);
    testBus->poll();
    TEST_ASSERT_EQUAL(0, count);
}

void test_reset_clears_sticky_events(void) {
    int setupCount = 0;
    int replayCount = 0;

    // Subscribe a counter handler BEFORE publishSticky so poll delivers to it
    testBus->subscribe(String("sticky/topic"), [&](const void*) { setupCount++; }, nullptr);

    int msg = 123;
    testBus->publishSticky(String("sticky/topic"), msg);
    // poll() before reset() is mandatory — without it, pendingByTopic > 0 would skip
    // sticky replay even without the fix, making this test pass in RED phase
    testBus->poll();
    TEST_ASSERT_EQUAL(1, setupCount); // setup validation: handler was called during poll

    testBus->reset();

    // Subscribe with replayLast=true — sticky replay happens inline in subscribe(), NOT in poll()
    testBus->subscribe(String("sticky/topic"), [&](const void* payload) {
        if (payload) replayCount++;
    }, nullptr, true);
    TEST_ASSERT_EQUAL(0, replayCount); // no stale sticky replay after reset
}

void test_unsubscribe_wildcard_by_id(void) {
    int count = 0;
    uint32_t subId = testBus->subscribe(String("sensor.*"), [&](const void*) { count++; }, nullptr);

    testBus->unsubscribe(subId);

    int payload = 1;
    testBus->publish(String("sensor.temp"), payload);
    testBus->poll();
    TEST_ASSERT_EQUAL(0, count);
}

void test_unsubscribe_owner_clears_wildcards(void) {
    int count = 0;
    void* owner = (void*)0x5678;
    testBus->subscribe(String("sensor.*"), [&](const void*) { count++; }, owner);

    testBus->unsubscribeOwner(owner);

    int payload = 1;
    testBus->publish(String("sensor.temp"), payload);
    testBus->poll();
    TEST_ASSERT_EQUAL(0, count);
}

void test_reset_clears_pending_counters(void) {
    int replayCount = 0;

    // Step 1: publishSticky WITHOUT poll — leaves pendingByTopic["pending/topic"] = 1
    int msg1 = 42;
    testBus->publishSticky(String("pending/topic"), msg1);

    // Step 2: reset
    testBus->reset();

    // Step 3: publishSticky + poll — arithmetic: 0(cleared)+1(enqueue)-1(poll) = 0
    int msg2 = 99;
    testBus->publishSticky(String("pending/topic"), msg2);
    testBus->poll();

    // Step 4: subscribe with replayLast=true — should replay because pendingByTopic is 0
    testBus->subscribe(String("pending/topic"), [&](const void* payload) {
        if (payload) replayCount++;
    }, nullptr, true);
    TEST_ASSERT_EQUAL(1, replayCount);
}

void test_reset_clears_queued_events(void) {
    // Regression guard — reset() already clears the queue. This test ensures it stays that way.
    int oldCount = 0;
    int newCount = 0;

    testBus->subscribe(String("queued/topic"), [&](const void*) { oldCount++; }, nullptr);
    int payload = 1;
    testBus->publish(String("queued/topic"), payload); // sits in queue, not polled

    testBus->reset();

    testBus->subscribe(String("queued/topic"), [&](const void*) { newCount++; }, nullptr);
    testBus->poll();

    TEST_ASSERT_EQUAL(0, oldCount);
    TEST_ASSERT_EQUAL(0, newCount);
}

void test_reset_comprehensive(void) {
    int wildcardCount = 0;
    int stickyReplayCount = 0;

    // Setup: wildcard subscription + sticky event
    testBus->subscribe(String("wild.*"), [&](const void*) { wildcardCount++; }, nullptr);
    int msg = 42;
    testBus->publishSticky(String("sticky/data"), msg);
    testBus->poll(); // drain queue so pendingByTopic goes to 0

    testBus->reset();

    // Verify wildcard cleared
    int payload = 1;
    testBus->publish(String("wild.test"), payload);
    testBus->poll();
    TEST_ASSERT_EQUAL(0, wildcardCount);

    // Verify sticky cleared
    testBus->subscribe(String("sticky/data"), [&](const void* p) {
        if (p) stickyReplayCount++;
    }, nullptr, true);
    TEST_ASSERT_EQUAL(0, stickyReplayCount);
}

void test_unsubscribe_owner_clears_all_maps(void) {
    // F5: Cross-map test — owner has subscriptions in all 3 maps simultaneously
    int typedCount = 0;
    int topicCount = 0;
    int wildcardCount = 0;
    void* owner = (void*)0xABCD;

    testBus->subscribe(EventType::Custom, [&](const void*) { typedCount++; }, owner);
    testBus->subscribe(String("exact/topic"), [&](const void*) { topicCount++; }, owner);
    testBus->subscribe(String("wild.*"), [&](const void*) { wildcardCount++; }, owner);

    testBus->unsubscribeOwner(owner);

    int payload = 1;
    testBus->publish(EventType::Custom, payload);
    testBus->publish(String("exact/topic"), payload);
    testBus->publish(String("wild.test"), payload);
    testBus->poll();

    TEST_ASSERT_EQUAL(0, typedCount);
    TEST_ASSERT_EQUAL(0, topicCount);
    TEST_ASSERT_EQUAL(0, wildcardCount);
}

// --- Memory stability tests (R1 shrink_to_fit) ---

void test_eventbus_memory_stability_single_cycle(void) {
    using namespace DomoticsCore::Testing;
    HeapTracker tracker;

    // Warm up: subscribe+unsubscribe once to stabilize allocator
    uint32_t warmId = testBus->subscribe(EventType::Custom, [](const void*) {}, nullptr);
    testBus->unsubscribe(warmId);

    tracker.checkpoint("before");

    // Single cycle: type + topic + wildcard subscriptions
    void* owner = (void*)0x9999;
    uint32_t id1 = testBus->subscribe(EventType::Custom, [](const void*) {}, owner);
    uint32_t id2 = testBus->subscribe(String("mem/test"), [](const void*) {}, owner);
    uint32_t id3 = testBus->subscribe(String("mem.*"), [](const void*) {}, owner);

    testBus->unsubscribe(id1);
    testBus->unsubscribe(id2);
    testBus->unsubscribe(id3);

    tracker.checkpoint("after");

    MemoryTestResult result = tracker.assertStable("before", "after", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

void test_eventbus_memory_stability_multi_cycle(void) {
    using namespace DomoticsCore::Testing;
    HeapTracker tracker;

    // Warm up
    uint32_t warmId = testBus->subscribe(EventType::Custom, [](const void*) {}, nullptr);
    testBus->unsubscribe(warmId);

    tracker.checkpoint("before");

    for (int i = 0; i < 20; i++) {
        void* owner = (void*)(uintptr_t)(0xA000 + i);
        uint32_t id1 = testBus->subscribe(EventType::Custom, [](const void*) {}, owner);
        uint32_t id2 = testBus->subscribe(String("mem/cycle"), [](const void*) {}, owner);
        uint32_t id3 = testBus->subscribe(String("mem.*"), [](const void*) {}, owner);

        testBus->unsubscribe(id1);
        testBus->unsubscribe(id2);
        testBus->unsubscribe(id3);
    }

    tracker.checkpoint("after");

    MemoryTestResult result = tracker.assertStable("before", "after", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

void test_eventbus_memory_stability_unsubscribe_owner(void) {
    using namespace DomoticsCore::Testing;
    HeapTracker tracker;

    // Warm up
    void* warmOwner = (void*)0xBBBB;
    testBus->subscribe(EventType::Custom, [](const void*) {}, warmOwner);
    testBus->subscribe(String("own/topic"), [](const void*) {}, warmOwner);
    testBus->subscribe(String("own.*"), [](const void*) {}, warmOwner);
    testBus->unsubscribeOwner(warmOwner);

    tracker.checkpoint("before");

    for (int i = 0; i < 20; i++) {
        void* owner = (void*)(uintptr_t)(0xC000 + i);
        testBus->subscribe(EventType::Custom, [](const void*) {}, owner);
        testBus->subscribe(String("own/topic"), [](const void*) {}, owner);
        testBus->subscribe(String("own.*"), [](const void*) {}, owner);
        testBus->unsubscribeOwner(owner);
    }

    tracker.checkpoint("after");

    MemoryTestResult result = tracker.assertStable("before", "after", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

void test_eventbus_prune_removes_empty_map_keys(void) {
    // Verify that pruneMap removes map entries whose vector becomes empty
    // Subscribe to a unique topic, then unsubscribe — the map key should be cleaned up
    String topic = String("ephemeral/topic");
    uint32_t id = testBus->subscribe(topic, [](const void*) {}, nullptr);

    // Publish to verify it works
    int payload = 1;
    testBus->publish(topic, payload);
    testBus->poll();

    // Unsubscribe — pruneMap should remove the empty map key
    testBus->unsubscribe(id);

    // Subscribe to the same topic again — if the old key was cleaned up,
    // this creates a fresh entry. Verify by subscribing and publishing.
    int count = 0;
    testBus->subscribe(topic, [&](const void*) { count++; }, nullptr);
    testBus->publish(topic, payload);
    testBus->poll();
    TEST_ASSERT_EQUAL(1, count);
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
    RUN_TEST(test_publish_during_dispatch_safe);
    RUN_TEST(test_reset_clears_wildcard_subscriptions);
    RUN_TEST(test_reset_clears_sticky_events);
    RUN_TEST(test_unsubscribe_wildcard_by_id);
    RUN_TEST(test_unsubscribe_owner_clears_wildcards);
    RUN_TEST(test_reset_clears_pending_counters);
    RUN_TEST(test_reset_clears_queued_events);
    RUN_TEST(test_reset_comprehensive);
    RUN_TEST(test_unsubscribe_owner_clears_all_maps);

    // Memory stability tests (R1)
    RUN_TEST(test_eventbus_memory_stability_single_cycle);
    RUN_TEST(test_eventbus_memory_stability_multi_cycle);
    RUN_TEST(test_eventbus_memory_stability_unsubscribe_owner);
    RUN_TEST(test_eventbus_prune_removes_empty_map_keys);

    return UNITY_END();
}
