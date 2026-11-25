/**
 * @file test_mqtt_event_fields.cpp
 * @brief qos and retain of the publish event read back as they were set.
 */

#include <unity.h>
#include <DomoticsCore/MQTT.h>

using DomoticsCore::Components::MQTTPublishEvent;

void setUp(void) {}
void tearDown(void) {}

void test_qos_and_retain_read_back_after_packing(void) {
    for (uint8_t q = 0; q <= 2; ++q) {
        for (int r = 0; r <= 1; ++r) {
            MQTTPublishEvent ev{};
            ev.qos = q;
            ev.retain = (r == 1);
            MQTTPublishEvent copy;
            memcpy(&copy, &ev, sizeof(ev));
            TEST_ASSERT_EQUAL_UINT8(q, copy.qos);
            TEST_ASSERT_EQUAL(r == 1, copy.retain);
        }
    }
}

void test_a_default_event_is_qos_0_not_retained(void) {
    MQTTPublishEvent ev{};
    TEST_ASSERT_EQUAL_UINT8(0, ev.qos);
    TEST_ASSERT_FALSE(ev.retain);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_qos_and_retain_read_back_after_packing);
    RUN_TEST(test_a_default_event_is_qos_0_not_retained);
    return UNITY_END();
}
