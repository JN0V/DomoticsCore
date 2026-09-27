/**
 * @file test_emit_topic_allocations.cpp
 * @brief A topic given as const char* is copied once, into the queued event.
 *
 * Counted at glibc's malloc. The topic is longer than the host's 15-character
 * small-string buffer, as most topics are longer than the ESP8266's 10.
 */

#include <unity.h>
#include <DomoticsCore/Core.h>

extern "C" void* __libc_malloc(size_t);
extern "C" void* __libc_realloc(void*, size_t);
extern "C" void* __libc_calloc(size_t, size_t);

static bool counting = false;
static long allocations = 0;

extern "C" void* malloc(size_t n) {
    if (counting) allocations++;
    return __libc_malloc(n);
}
extern "C" void* realloc(void* p, size_t n) {
    if (counting) allocations++;
    return __libc_realloc(p, n);
}
extern "C" void* calloc(size_t a, size_t b) {
    if (counting) allocations++;
    return __libc_calloc(a, b);
}

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

constexpr const char* kTopic = "component/long_topic";  // 20 characters

class Emitter : public IComponent {
public:
    ComponentStatus begin() override { return ComponentStatus::Success; }
    void loop() override {}
    ComponentStatus shutdown() override { return ComponentStatus::Success; }
    using IComponent::emit;
};

struct Fixture {
    Core core;
    Emitter* emitter = nullptr;
    Fixture() {
        auto owned = std::make_unique<Emitter>();
        emitter = owned.get();
        core.addComponent(std::move(owned));
        core.begin();
        core.loop();
    }
    // The fewest over several publishes: the queue's deque takes a new block
    // now and then, which is its cost, not the topic's.
    long count(const std::function<void()>& publish) {
        long fewest = -1;
        for (int i = 0; i < 6; i++) {
            allocations = 0;
            counting = true;
            publish();
            counting = false;
            core.loop();
            if (fewest < 0 || allocations < fewest) fewest = allocations;
        }
        return fewest;
    }
};

}  // namespace

void setUp() {}
void tearDown() {}

// The queued event owns a topic String and a payload vector: two allocations.
void test_bytes_under_a_const_char_topic_allocate_the_queue_entry_only() {
    Fixture f;
    const char payload[] = "{\"x\":1}";
    const long n = f.count([&] { f.emitter->emit(kTopic, payload, sizeof(payload), false); });
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, n, "a const char* topic was copied more than once");
}

void test_a_typed_payload_under_a_const_char_topic_allocates_the_queue_entry_only() {
    Fixture f;
    const uint32_t value = 42;
    const long n = f.count([&] { f.emitter->emit(kTopic, value); });
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, n, "a const char* topic was copied more than once");
}

// A String topic still costs its one copy into the queue.
void test_a_string_topic_still_publishes() {
    Fixture f;
    String received;
    f.core.getEventBus().subscribe(String(kTopic),
        [&](const void* data) { received = static_cast<const char*>(data); }, nullptr);
    const String topic(kTopic);
    f.emitter->emit(topic, "hello", 6, false);
    f.core.loop();
    TEST_ASSERT_EQUAL_STRING("hello", received.c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_bytes_under_a_const_char_topic_allocate_the_queue_entry_only);
    RUN_TEST(test_a_typed_payload_under_a_const_char_topic_allocates_the_queue_entry_only);
    RUN_TEST(test_a_string_topic_still_publishes);
    return UNITY_END();
}
