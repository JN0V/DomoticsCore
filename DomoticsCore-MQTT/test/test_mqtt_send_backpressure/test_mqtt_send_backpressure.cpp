/**
 * @file test_mqtt_send_backpressure.cpp
 * @brief A publish never waits for the broker's acknowledgements.
 *
 * When the client says the socket cannot take a packet without waiting,
 * publish() queues the message and the queue drains once it can; the
 * no-queue path used by telemetry refuses instead.
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

HAL::MQTT::MQTTClientImpl* stubOf(MQTTComponent& mqtt) {
    return static_cast<HAL::MQTT::MQTTClientImpl*>(mqtt.getClientForTest());
}

void connectNow(MQTTComponent& mqtt) {
    HAL::WiFiImpl::setConnectedForTest(true);
    TEST_ASSERT_TRUE(mqtt.connect());
    mqtt.loop();
    TEST_ASSERT_TRUE(mqtt.isConnected());
}

MQTTConfig config() {
    MQTTConfig cfg;
    cfg.broker = "test.local";
    return cfg;
}

}  // namespace

void setUp() {}
void tearDown() {
    HAL::WiFiImpl::setConnectedForTest(false);
    HAL::Platform::resetMillisForTest();
}

void test_a_busy_socket_queues_the_publish_instead_of_writing() {
    MQTTComponent mqtt(config());
    mqtt.begin();
    connectNow(mqtt);
    const uint32_t before = stubOf(mqtt)->getPublishCount();

    stubOf(mqtt)->writable = false;
    TEST_ASSERT_TRUE_MESSAGE(mqtt.publish("a/b", "hello"), "a queued publish is accepted");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before, stubOf(mqtt)->getPublishCount(), "written to a busy socket");
    TEST_ASSERT_EQUAL_UINT32(0, mqtt.getStatistics().publishErrors);
    TEST_ASSERT_EQUAL_UINT32(1, mqtt.getQueuedMessageCount());

    mqtt.loop();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before, stubOf(mqtt)->getPublishCount(), "drained into a busy socket");

    stubOf(mqtt)->writable = true;
    mqtt.loop();
    TEST_ASSERT_EQUAL_UINT32(before + 1, stubOf(mqtt)->getPublishCount());
    TEST_ASSERT_EQUAL_STRING("a/b", stubOf(mqtt)->lastTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("hello", stubOf(mqtt)->lastPayload.c_str());
    mqtt.shutdown();
}

// The client is asked about the packet it would send, framing included.
void test_the_client_is_asked_for_the_whole_packet() {
    MQTTComponent mqtt(config());
    mqtt.begin();
    connectNow(mqtt);
    mqtt.publish("a/b", "hello");
    TEST_ASSERT_EQUAL_UINT32(3 + 5 + 7, stubOf(mqtt)->lastCanWriteLength);
    mqtt.shutdown();
}

// Queued messages leave in order once the socket frees, oldest first.
void test_queued_messages_keep_their_order() {
    MQTTComponent mqtt(config());
    mqtt.begin();
    connectNow(mqtt);
    const uint32_t before = stubOf(mqtt)->getPublishCount();
    stubOf(mqtt)->writable = false;
    mqtt.publish("q/1", "one");
    mqtt.publish("q/2", "two");
    TEST_ASSERT_EQUAL_UINT32(2, mqtt.getQueuedMessageCount());
    stubOf(mqtt)->writable = true;
    mqtt.loop();
    TEST_ASSERT_EQUAL_UINT32(0, mqtt.getQueuedMessageCount());
    TEST_ASSERT_EQUAL_UINT32(before + 2, stubOf(mqtt)->getPublishCount());
    TEST_ASSERT_EQUAL_STRING_MESSAGE("q/2", stubOf(mqtt)->lastTopic.c_str(), "the older message left last");
    mqtt.shutdown();
}

// Telemetry's path never queues: a busy socket refuses the sample.
void test_publish_now_refuses_on_a_busy_socket() {
    MQTTComponent mqtt(config());
    mqtt.begin();
    connectNow(mqtt);
    const uint32_t before = stubOf(mqtt)->getPublishCount();
    stubOf(mqtt)->writable = false;
    TEST_ASSERT_FALSE(mqtt.publishNow("t/x", "1", 1));
    TEST_ASSERT_EQUAL_UINT32(before, stubOf(mqtt)->getPublishCount());
    stubOf(mqtt)->writable = true;
    TEST_ASSERT_TRUE(mqtt.publishNow("t/x", "1", 1));
    mqtt.shutdown();
}

// A queue that cannot drain is bounded in bytes: the next message is refused and counted.
void test_the_queue_refuses_past_its_byte_budget() {
    HAL::Platform::setMillisForTest(10000);
    MQTTConfig cfg = config();
    cfg.maxQueueSize = 1000;   // the count alone would never stop it
    MQTTComponent mqtt(cfg);
    mqtt.begin();
    connectNow(mqtt);
    stubOf(mqtt)->writable = false;
    const String payload(std::string(1000, 'x').c_str());
    size_t accepted = 0;
    while (mqtt.publish("q/big", payload) && accepted < 100) accepted++;
    TEST_ASSERT_TRUE_MESSAGE(accepted > 0 && accepted < 100, "the byte budget never stopped the queue");
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(HAL::MQTT::kQueueByteBudget, mqtt.getQueuedBytes());
    TEST_ASSERT_EQUAL_UINT32(accepted, mqtt.getQueuedMessageCount());
    TEST_ASSERT_EQUAL_UINT32(1, mqtt.getStatistics().publishErrors);

    stubOf(mqtt)->writable = true;
    for (int i = 0; i < 20 && mqtt.getQueuedMessageCount() > 0; ++i) {
        HAL::Platform::advanceMillisForTest(1100);   // the rate limit opens a fresh window each second
        mqtt.loop();
    }
    TEST_ASSERT_EQUAL_UINT32(0, mqtt.getQueuedMessageCount());
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, mqtt.getQueuedBytes(), "the byte count drifted");
    mqtt.shutdown();
}

// A new message never overtakes a queued one, even when the socket could take it.
void test_a_new_publish_waits_behind_the_queue() {
    MQTTComponent mqtt(config());
    mqtt.begin();
    connectNow(mqtt);
    const uint32_t before = stubOf(mqtt)->getPublishCount();
    stubOf(mqtt)->writable = false;
    mqtt.publish("ha/config", "discovery");
    stubOf(mqtt)->writable = true;
    TEST_ASSERT_TRUE(mqtt.publish("ha/state", "on"));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before, stubOf(mqtt)->getPublishCount(), "the state overtook the queue");
    TEST_ASSERT_EQUAL_UINT32(2, mqtt.getQueuedMessageCount());
    mqtt.loop();
    TEST_ASSERT_EQUAL_UINT32(before + 2, stubOf(mqtt)->getPublishCount());
    TEST_ASSERT_EQUAL_STRING("ha/state", stubOf(mqtt)->lastTopic.c_str());
    mqtt.shutdown();
}

// A queued packet the client can never send is dropped, and its bytes leave the count with it.
void test_dropping_an_oversized_packet_releases_its_bytes() {
    MQTTComponent mqtt(config());
    mqtt.begin();
    const String huge(std::string(MQTT_MAX_PACKET_SIZE + 10, 'x').c_str());
    TEST_ASSERT_TRUE(mqtt.publish("q/huge", huge));   // offline: queued without the size test
    TEST_ASSERT_GREATER_THAN(0, mqtt.getQueuedBytes());
    const uint32_t errors = mqtt.getStatistics().publishErrors;
    connectNow(mqtt);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(errors + 1, mqtt.getStatistics().publishErrors, "it was sent, not dropped");
    TEST_ASSERT_EQUAL_UINT32(0, mqtt.getQueuedMessageCount());
    TEST_ASSERT_EQUAL_UINT32(0, mqtt.getQueuedBytes());
    mqtt.shutdown();
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_busy_socket_queues_the_publish_instead_of_writing);
    RUN_TEST(test_the_client_is_asked_for_the_whole_packet);
    RUN_TEST(test_queued_messages_keep_their_order);
    RUN_TEST(test_publish_now_refuses_on_a_busy_socket);
    RUN_TEST(test_the_queue_refuses_past_its_byte_budget);
    RUN_TEST(test_a_new_publish_waits_behind_the_queue);
    RUN_TEST(test_dropping_an_oversized_packet_releases_its_bytes);
    return UNITY_END();
}
