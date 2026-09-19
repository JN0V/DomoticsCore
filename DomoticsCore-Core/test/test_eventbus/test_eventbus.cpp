#include <unity.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Core.h>
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

// ---------------------------------------------------------------------------
// BUG-41: the queue is bounded by the bytes it holds, not by an entry count.
// Every expected number below is derived from QueueCost, never written as a
// literal: the native stub String is not the boards' String, so these test the
// model, not the silicon.
// ---------------------------------------------------------------------------
using DomoticsCore::Utils::QueueCost;

// This project is built WITHOUT -DDOMOTICS_EVENTBUS_QUEUE_BYTES, so it pins the
// default arm of the #ifdef; the flag's own arm is pinned by the project in
// test/test_queue_budget_override, which is built with it.
static_assert(QueueCost::kBudgetBytes == 32 * QueueCost::kReference,
              "the default budget is no longer 32 reference events");

// A topic that stays inside SSO on every target, so a small event costs node+payload only.
static const char* SMALL_TOPIC = "t/small";           // 7 chars
static const char* REF_TOPIC   = "mqtt/publish";      // 12 chars, the reference event's

// How many events of this class fit in the budget.
static size_t fits(size_t payload, size_t topicLen) {
    return QueueCost::kBudgetBytes / QueueCost::of(payload, topicLen);
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
    // BUG-41: a storm past the BUDGET keeps the most recent, in order, and counts
    // what it dropped. The capacity is derived from QueueCost, never written down.
    std::vector<int> received;
    testBus->subscribe(String(REF_TOPIC), [&](const void* p) {
        if (p) received.push_back(*static_cast<const int*>(p));
    }, nullptr);
    const size_t CAP = QueueCost::kBudgetBytes / QueueCost::of(830, strlen(REF_TOPIC));
    const int N = 100;
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(CAP, (size_t)N, "the storm must exceed the budget or it storms nothing");
    std::vector<uint8_t> buf(830, 0);
    for (int i = 0; i < N; i++) {
        memcpy(buf.data(), &i, sizeof(int));
        testBus->publish(String(REF_TOPIC), buf.data(), 830);
    }
    for (int i = 0; i < 40; i++) testBus->poll();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(CAP, received.size(), "the budget did not hold what it models");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((size_t)N - CAP, testBus->getDroppedCount(), "the drop count and the survivors disagree");
    for (size_t i = 0; i < CAP; i++) TEST_ASSERT_EQUAL_INT((int)((size_t)N - CAP + i), received[i]);
}

// BUG-36: on overflow enqueue() popped the oldest event without decrementing
// its topic's pending counter, so a topic that had ever been oldest when
// another topic stormed the queue never replayed its sticky value again.
void test_bug36_topic_dropped_on_overflow_still_replays_sticky(void) {
    int b = 7;
    testBus->publishSticky(String("topic/B"), b);           // B is the oldest queued event
    // BUG-41: the storm has to be in BYTES now. Forty small events no longer
    // overflow anything, and this test would pass while reaching nothing.
    const size_t CAP = QueueCost::kBudgetBytes / QueueCost::of(830, strlen("topic/A"));
    std::vector<uint8_t> buf(830, 0x55);
    for (size_t i = 0; i < CAP + 8; i++) testBus->publish(String("topic/A"), buf.data(), 830);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, testBus->getDroppedCount(),
                                            "nothing was evicted: this test measured nothing");
    for (int i = 0; i < 80; i++) testBus->poll();          // drain everything that survived

    int replayed = 0;
    testBus->subscribe(String("topic/B"), [&](const void* payload) {
        if (payload) replayed = *static_cast<const int*>(payload);
    }, nullptr, true);                                      // replayLast: must see B's sticky value
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, replayed, "sticky replay of a topic dropped on overflow");
}

void test_bug36_drop_counter_counts_every_overflow(void) {
    TEST_ASSERT_EQUAL_UINT32(0, testBus->getDroppedCount());
    const size_t CAP = QueueCost::kBudgetBytes / QueueCost::of(830, strlen("topic/A"));
    std::vector<uint8_t> buf(830, 0x44);
    for (size_t i = 0; i < CAP + 8; i++) testBus->publish(String("topic/A"), buf.data(), 830);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(8, testBus->getDroppedCount(), "eight past the budget, eight counted");
    testBus->reset();
    TEST_ASSERT_EQUAL_UINT32(0, testBus->getDroppedCount());
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getQueuedBytes(), "reset() left bytes on the books");
}


void test_burst_of_small_events_beyond_32_is_not_dropped(void) {
    const size_t N = 64;                                  // past the old entry cap, far inside the budget
    TEST_ASSERT_GREATER_THAN_UINT32(N, fits(sizeof(int), strlen(SMALL_TOPIC)));
    std::vector<int> got;
    testBus->subscribe(String(SMALL_TOPIC), [&](const void* p) {
        if (p) got.push_back(*static_cast<const int*>(p));
    }, nullptr);
    for (size_t i = 0; i < N; ++i) { int v = (int)i; testBus->publish(String(SMALL_TOPIC), v); }
    for (int i = 0; i < 20; ++i) testBus->poll();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getDroppedCount(), "a burst well inside the budget still dropped");
    TEST_ASSERT_EQUAL_UINT32(N, got.size());
    for (size_t i = 0; i < N; ++i) TEST_ASSERT_EQUAL_INT((int)i, got[i]);
}

void test_late_subscriber_receives_every_event_published_before_it_subscribed(void) {
    const size_t N = 40;                                  // C1 + C3: nothing drains before the first loop()
    std::vector<int> got;
    for (size_t i = 0; i < N; ++i) { int v = (int)i; testBus->publish(String(SMALL_TOPIC), v); }
    testBus->subscribe(String(SMALL_TOPIC), [&](const void* p) {
        if (p) got.push_back(*static_cast<const int*>(p));
    }, nullptr);
    for (int i = 0; i < 20; ++i) testBus->poll();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(N, got.size(), "a subscriber posted after the burst lost its head");
    for (size_t i = 0; i < N; ++i) TEST_ASSERT_EQUAL_INT((int)i, got[i]);
}

// A measured boot: the events a FullStack application emits between the first
// component begin() and the first Core::loop(), component subscribers posted
// before, sketch subscribers after. All of it must survive one loop.
void test_boot_sequence_is_delivered_in_full(void) {
    struct E { const char* topic; size_t payload; };
    std::vector<E> boot;
    for (int i = 0; i < 13; ++i) boot.push_back({"component/ready", 4});
    boot.push_back({"system/ready", 0});
    boot.push_back({"storage/ready", 9});
    boot.push_back({"mqtt/subscribe", 129});
    boot.push_back({REF_TOPIC, 830});
    for (int i = 0; i < 18; ++i) boot.push_back({"ha/entity_added", 96});
    boot.push_back({"app/io/pulse_completed", 1});   // a sketch topic, 24 chars
    TEST_ASSERT_EQUAL_UINT32(36, boot.size());   // 13 ready + system + storage + sub + pub + 18 entities + pulse

    size_t modelled = 0;
    for (const E& e : boot) modelled += QueueCost::of(e.payload, strlen(e.topic));
    TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(QueueCost::kBudgetBytes, modelled, "the boot burst no longer fits the budget");

    size_t received = 0;
    testBus->subscribe(String("component/ready"), [&](const void*) { received++; }, nullptr);
    std::vector<uint8_t> buf(830, 0x5A);
    for (const E& e : boot) {
        if (e.payload) testBus->publish(String(e.topic), buf.data(), e.payload);
        else           testBus->publish(String(e.topic));
    }
    testBus->subscribe(String(REF_TOPIC), [&](const void*) { received++; }, nullptr);   // the sketch, after begin()
    for (int i = 0; i < 20; ++i) testBus->poll();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getDroppedCount(), "the boot burst still drops events");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(14, received, "13 component/ready and the sketch's own event");
}

// The cliff has to stay exactly where it is today: the budget is 32 reference
// events, so the 33rd costs one and not before.
void test_cliff_is_where_it_was(void) {
    std::vector<uint8_t> buf(830, 0x11);
    for (int i = 0; i < 32; ++i) testBus->publish(String(REF_TOPIC), buf.data(), 830);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getDroppedCount(), "32 reference events must fit exactly");
    testBus->publish(String(REF_TOPIC), buf.data(), 830);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, testBus->getDroppedCount(), "the 33rd must cost exactly one");
}

// One large event evicts as many of the oldest as it needs — and BUG-36's
// release still runs for every one of them.
void test_one_large_event_evicts_as_many_oldest_as_needed_and_keeps_sticky_replayable(void) {
    // Medium events, so the byte budget binds well before the entry guard rail:
    // what is under test is eviction by bytes, not by count.
    const size_t MED = 200;
    std::vector<uint8_t> med(MED, 0x33), big(830, 0x22);
    const size_t medCost = QueueCost::of(MED, strlen(SMALL_TOPIC));
    const size_t bigCost = QueueCost::of(830, strlen(REF_TOPIC));
    TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(QueueCost::kMaxEntries, QueueCost::kBudgetBytes / medCost,
                                         "this class would hit the entry guard first");

    int b = 7;
    testBus->publishSticky(String("topic/B"), b);            // oldest, and sticky
    size_t queued = QueueCost::of(sizeof(int), strlen("topic/B"));
    size_t n = 0;
    while (queued + medCost + bigCost <= QueueCost::kBudgetBytes) {
        testBus->publish(String(SMALL_TOPIC), med.data(), MED);
        queued += medCost; n++;
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getDroppedCount(), "filling to the budget must not drop");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(queued, testBus->getQueuedBytes(), "the bus and the model disagree on what is queued");

    // One more medium plus the big one cannot both fit: the big one evicts the
    // oldest until it does, and topic/B is the oldest.
    testBus->publish(String(SMALL_TOPIC), med.data(), MED);
    testBus->publish(String(REF_TOPIC), big.data(), 830);
    const uint32_t dropped = testBus->getDroppedCount();
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, dropped, "the big event evicted nothing");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(bigCost / medCost + 2, dropped, "it evicted far more than it needed");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(QueueCost::kBudgetBytes, testBus->getQueuedBytes(),
                                             "the queue is over its budget after an eviction");

    for (int i = 0; i < 200; ++i) testBus->poll();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getQueuedBytes(), "a drained queue must account zero bytes");
    int replayed = 0;
    testBus->subscribe(String("topic/B"), [&](const void* p) {
        if (p) replayed = *static_cast<const int*>(p);
    }, nullptr, true);
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, replayed, "sticky replay of a topic evicted by a large event");
}


void test_budget_is_accounted_on_publish_on_dispatch_and_on_reset(void) {
    const size_t cost = QueueCost::of(sizeof(int), strlen(SMALL_TOPIC));
    TEST_ASSERT_EQUAL_UINT32(0, testBus->getQueuedBytes());
    const size_t N = 40;                                   // past the old entry cap, inside the budget
    for (size_t i = 0; i < N; ++i) { int v = (int)i; testBus->publish(String(SMALL_TOPIC), v); }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(N * cost, testBus->getQueuedBytes(), "publish did not bill the queue");
    testBus->poll();                                       // maxPerPoll is 8
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((N - 8) * cost, testBus->getQueuedBytes(), "dispatch did not credit it back");
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, testBus->getQueueHighWaterPct(), "the high water mark never moved");
    testBus->reset();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getQueuedBytes(), "reset() left bytes on the books");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getQueueHighWaterPct(), "reset() kept the old high water mark");
}

// An event bigger than the whole budget can never be queued: evicting for it
// would empty the queue and still fail. It is refused, counted, and named.
void test_oversized_event_is_refused_counted_and_not_queued(void) {
    int keep = 3;
    testBus->publish(String("topic/keep"), keep);
    const uint16_t before = testBus->getQueuedBytes();
    std::vector<uint8_t> huge(QueueCost::kBudgetBytes + 1, 0x77);
    testBus->publish(String("topic/huge"), huge.data(), huge.size());
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, testBus->getDroppedCount(), "the oversized event was not counted");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before, testBus->getQueuedBytes(),
                                     "the oversized event emptied the queue on its way out");
    size_t kept = 0;
    testBus->subscribe(String("topic/keep"), [&](const void*) { kept++; }, nullptr);
    for (int i = 0; i < 8; ++i) testBus->poll();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, kept, "the event that was already queued did not survive");
}

// The entry guard rail: a class of events cheap enough that the budget would
// hold more of them than the model is trusted for.
void test_entry_guard_rail_bounds_a_cheap_class(void) {
    const size_t cheap = QueueCost::of(0, 4);              // no payload, a topic inside SSO
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(QueueCost::kMaxEntries, QueueCost::kBudgetBytes / cheap,
                                            "this class is not cheap enough to reach the guard rail");
    for (size_t i = 0; i < QueueCost::kMaxEntries; ++i) testBus->publish(String("t/gr"));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getDroppedCount(), "the guard rail fired early");
    testBus->publish(String("t/gr"));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, testBus->getDroppedCount(), "the guard rail did not fire");
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

// M11: Core::emit() sticky parameter tests
void test_core_emit_sticky_with_payload(void) {
    Core core;
    String topic = String("core/sticky");
    int payload = 42;
    core.emit(topic, payload, true);

    int received = 0;
    core.on<int>(topic, [&](const int& val) { received = val; }, true);
    core.getEventBus().poll();
    TEST_ASSERT_EQUAL(42, received);
}

void test_core_emit_non_sticky_default(void) {
    Core core;
    String topic = String("core/nonsticky");
    int payload = 99;
    core.emit(topic, payload);

    // Flush the queue so the event is dispatched (to no subscribers)
    core.getEventBus().poll();

    int received = 0;
    core.on<int>(topic, [&](const int& val) { received = val; }, true);
    core.getEventBus().poll();
    TEST_ASSERT_EQUAL(0, received);
}


// BUG-41: the refusal has to happen before the payload is copied. Inside
// enqueue() the heap has already been asked for the buffer, which on an ESP8266
// is where the OOM lands — the guard would then be accounting, not protection.
// Natively the observable is the sticky store: a payload the queue refuses must
// not become replayable either.
void test_an_oversized_sticky_payload_is_not_stored_for_replay(void) {
    std::vector<uint8_t> huge(QueueCost::kBudgetBytes + 1, 0x5A);
    testBus->publishSticky(String("t/huge"), huge.data(), huge.size());

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, testBus->getDroppedCount(),
                                     "the oversized sticky publish was not counted as refused");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getQueuedBytes(),
                                     "it reached the queue");

    bool replayed = false;
    testBus->subscribe(String("t/huge"), [&](const void* p) { if (p) replayed = true; },
                       nullptr, /*replayLast=*/true);
    TEST_ASSERT_FALSE_MESSAGE(replayed,
        "a payload the queue refuses was stored whole in the sticky map and replayed "
        "to a late subscriber");
}

// The typed overload carries no topic, so the refusal used to name nothing.
void test_an_oversized_typed_event_is_refused_and_named(void) {
    struct Huge { uint8_t bytes[QueueCost::kBudgetBytes + 1]; };
    Huge h{};
    testBus->publish(EventType::Custom, h);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, testBus->getDroppedCount(),
                                     "the oversized typed event was not counted");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getQueuedBytes(), "it reached the queue");
}

// The boundary the guard is written on: cost == budget must be ACCEPTED, and it
// must evict everything else to make room. Nothing tested that side of `>`.
void test_an_event_costing_exactly_the_budget_is_accepted(void) {
    const size_t topicLen = strlen(SMALL_TOPIC);
    // largest payload whose total cost is still within the budget
    size_t payload = QueueCost::kBudgetBytes;
    while (payload > 0 && QueueCost::of(payload, topicLen) > QueueCost::kBudgetBytes) --payload;
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, payload, "no payload fits: the test is vacuous");

    int filler = 1;
    testBus->publish(String(SMALL_TOPIC), filler);
    TEST_ASSERT_GREATER_THAN_UINT32(0, testBus->getQueuedBytes());

    std::vector<uint8_t> big(payload, 0x11);
    testBus->publish(String(SMALL_TOPIC), big.data(), big.size());

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(QueueCost::of(payload, topicLen), testBus->getQueuedBytes(),
        "the largest event that fits was refused, or did not evict what stood in its way");
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
    RUN_TEST(test_bug36_topic_dropped_on_overflow_still_replays_sticky);
    RUN_TEST(test_bug36_drop_counter_counts_every_overflow);
    RUN_TEST(test_burst_of_small_events_beyond_32_is_not_dropped);
    RUN_TEST(test_late_subscriber_receives_every_event_published_before_it_subscribed);
    RUN_TEST(test_boot_sequence_is_delivered_in_full);
    RUN_TEST(test_cliff_is_where_it_was);
    RUN_TEST(test_one_large_event_evicts_as_many_oldest_as_needed_and_keeps_sticky_replayable);
    RUN_TEST(test_budget_is_accounted_on_publish_on_dispatch_and_on_reset);
    RUN_TEST(test_oversized_event_is_refused_counted_and_not_queued);
    RUN_TEST(test_an_oversized_sticky_payload_is_not_stored_for_replay);
    RUN_TEST(test_an_oversized_typed_event_is_refused_and_named);
    RUN_TEST(test_an_event_costing_exactly_the_budget_is_accepted);
    RUN_TEST(test_entry_guard_rail_bounds_a_cheap_class);
    RUN_TEST(test_publish_during_dispatch_safe);
    RUN_TEST(test_reset_clears_wildcard_subscriptions);
    RUN_TEST(test_reset_clears_sticky_events);
    RUN_TEST(test_unsubscribe_wildcard_by_id);
    RUN_TEST(test_unsubscribe_owner_clears_wildcards);
    RUN_TEST(test_reset_clears_pending_counters);
    RUN_TEST(test_reset_clears_queued_events);
    RUN_TEST(test_reset_comprehensive);
    RUN_TEST(test_unsubscribe_owner_clears_all_maps);

    // M11: Core::emit() sticky parameter
    RUN_TEST(test_core_emit_sticky_with_payload);
    RUN_TEST(test_core_emit_non_sticky_default);

    // Memory stability tests (R1)
    RUN_TEST(test_eventbus_memory_stability_single_cycle);
    RUN_TEST(test_eventbus_memory_stability_multi_cycle);
    RUN_TEST(test_eventbus_memory_stability_unsubscribe_owner);
    RUN_TEST(test_eventbus_prune_removes_empty_map_keys);

    return UNITY_END();
}
