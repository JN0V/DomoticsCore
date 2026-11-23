/**
 * @file test_mqtt_event_size.cpp
 * @brief The MQTT publish event is smaller than the 830 bytes it was.
 */

#include <unity.h>
#include <DomoticsCore/MQTT.h>

using DomoticsCore::Components::MQTTPublishEvent;

void setUp(void) {}
void tearDown(void) {}

void test_the_publish_event_is_below_830_bytes(void) {
    // Given the MQTT publish event type
    // When its sizeof is read
    // Then it is below the 830 bytes it was
    TEST_ASSERT_LESS_THAN_UINT32((uint32_t)830, (uint32_t)sizeof(MQTTPublishEvent));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_publish_event_is_below_830_bytes);
    return UNITY_END();
}
