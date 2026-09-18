#include <unity.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Platform_Stub.h>

using namespace DomoticsCore::Utils;

// ---------------------------------------------------------------------------
// BUG-41: -DDOMOTICS_EVENTBUS_QUEUE_BYTES lowers the queue budget. It was
// announced, passed by DomoticsCore-Storage's esp8266dev environment, and read
// by nothing — kBudgetBytes was 32 reference events unconditionally. This
// project is built with the flag at 2048 and is the only place that sees it.
//
// Two things the suite deliberately does not test: the pathological values the
// static_asserts in EventBus.h refuse (a budget of zero, a budget under one
// reference event) cannot be exercised from a suite that has to compile.
// ---------------------------------------------------------------------------

static const size_t kOverride = 2048;

// A topic inside SSO on every target, so a small event costs node + payload.
static const char* SMALL_TOPIC = "t/small";

// The flag is honoured at compile time or not at all: if the #ifdef is ever
// removed again, this fails before a single test runs.
static_assert(QueueCost::kBudgetBytes == kOverride,
              "-DDOMOTICS_EVENTBUS_QUEUE_BYTES is not read by QueueCost");

EventBus* bus = nullptr;
void setUp(void) { bus = new EventBus(); }
void tearDown(void) { delete bus; bus = nullptr; }

void test_override_replaces_the_default_budget(void) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(kOverride, QueueCost::kBudgetBytes,
                                     "the budget is not the flag's value");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(32 * QueueCost::kReference, QueueCost::kBudgetBytes,
                                  "the budget is still 32 reference events");
}

// The behavioural half: the cliff has to MOVE, not just the constant. The
// event count below overflows the override and is far short of what the shipped
// budget holds, so a dead flag leaves the queue whole and this test red.
void test_the_cliff_sits_at_the_overridden_budget(void) {
    const size_t cost = QueueCost::of(sizeof(int), strlen(SMALL_TOPIC));
    const size_t fitsOverride = kOverride / cost;
    const size_t fitsDefault  = (32 * QueueCost::kReference) / cost;
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(fitsOverride + 1, fitsDefault,
                                            "the two budgets do not separate: this test is vacuous");

    int payload = 7;
    for (size_t i = 0; i <= fitsOverride; ++i) bus->publish(String(SMALL_TOPIC), payload);

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, bus->getDroppedCount(),
                                            "nothing was evicted: the queue is still on the default budget");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(kOverride, bus->getQueuedBytes(),
                                             "the queue holds more than the overridden budget");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(fitsOverride * cost, bus->getQueuedBytes(),
                                     "the queue did not settle on the budget's capacity");
}

// The high-water mark divides by the budget, so the flag reaches it too.
void test_high_water_is_a_percentage_of_the_overridden_budget(void) {
    const size_t cost = QueueCost::of(sizeof(int), strlen(SMALL_TOPIC));
    int payload = 7;
    const size_t half = (kOverride / 2) / cost;
    for (size_t i = 0; i < half; ++i) bus->publish(String(SMALL_TOPIC), payload);

    const uint8_t expected = (uint8_t)(half * cost * 100u / kOverride);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected, bus->getQueueHighWaterPct(),
                                    "the peak is not measured against the overridden budget");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_override_replaces_the_default_budget);
    RUN_TEST(test_the_cliff_sits_at_the_overridden_budget);
    RUN_TEST(test_high_water_is_a_percentage_of_the_overridden_budget);
    return UNITY_END();
}
