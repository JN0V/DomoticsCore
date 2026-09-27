/**
 * @file test_ota_status_allocations.cpp
 * @brief What a progress event allocates, counted at malloc.
 *
 * The progress event is published once a second for the whole upload, on a
 * platform with little heap, so its allocations are counted rather than argued.
 * glibc's malloc is interposed for the duration of one call; operator new
 * reaches it too.
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/OTA.h>
#include <DomoticsCore/OTAEvents.h>
#include <cstring>

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

void setUp() {}
void tearDown() { HAL::Platform::resetMillisForTest(); }

static uint8_t chunk[256];

static long allocationsOfOneChunk(OTAComponent* ota) {
    allocations = 0;
    counting = true;
    ota->acceptUploadChunk(chunk, sizeof(chunk));
    counting = false;
    return allocations;
}

// A chunk that publishes nothing allocates nothing; one that publishes progress
// pays the JSON pool, the two strings ArduinoJson copies and the payload the
// queue entry owns (the topic fits the host's small-string buffer).
// A payload serialized into a growing String would add its reallocations.
void test_a_progress_event_allocates_only_what_the_queue_and_the_document_need() {
    Core core;
    auto owned = std::make_unique<OTAComponent>(OTAConfig());
    OTAComponent* ota = owned.get();
    core.addComponent(std::move(owned));
    core.begin();
    memset(chunk, 0xE9, sizeof(chunk));
    HAL::Platform::setMillisForTest(10000);
    TEST_ASSERT_TRUE(ota->beginUpload(1 << 20));
    ota->acceptUploadChunk(chunk, sizeof(chunk));
    for (int i = 0; i < 5; i++) core.loop();

    HAL::Platform::advanceMillisForTest(10);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, allocationsOfOneChunk(ota), "a chunk that publishes nothing allocated");

    // The fewest over several progress events: the queue's deque takes a new
    // block now and then, which is its cost, not the event's.
    long published = -1;
    for (int i = 0; i < 4; i++) {
        HAL::Platform::advanceMillisForTest(1500);
        const long n = allocationsOfOneChunk(ota);
        if (published < 0 || n < published) published = n;
        for (int j = 0; j < 5; j++) core.loop();
    }
    TEST_ASSERT_TRUE_MESSAGE(published > 0, "nothing was published, so the count measures nothing");
    TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(4, published, "the progress event allocates more than its document and queue entry");

    ota->abortUpload("test");
    core.shutdown();
}

// A status longer than the stack buffer takes the heap and is published whole.
void test_a_status_over_the_stack_buffer_is_published_whole() {
    Core core;
    auto owned = std::make_unique<OTAComponent>(OTAConfig());
    OTAComponent* ota = owned.get();
    core.addComponent(std::move(owned));
    core.begin();
    String received;
    core.getEventBus().subscribe(String(OTAEvents::EVENT_ERROR),
        [&](const void* data) { received = static_cast<const char*>(data); }, nullptr);

    String reason;
    for (int i = 0; i < 20; i++) reason += "0123456789";
    TEST_ASSERT_TRUE(ota->beginUpload(1024));
    ota->abortUpload(reason);
    for (int i = 0; i < 5; i++) core.loop();

    JsonDocument doc;
    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, deserializeJson(doc, received).code(),
                              "the long status did not arrive as JSON");
    TEST_ASSERT_EQUAL_STRING(reason.c_str(), doc["lastResult"].as<const char*>());
    core.shutdown();
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_progress_event_allocates_only_what_the_queue_and_the_document_need);
    RUN_TEST(test_a_status_over_the_stack_buffer_is_published_whole);
    return UNITY_END();
}
