/**
 * @file test_mqtt_topic_match.cpp
 * @brief topicMatches() follows the MQTT 3.1.1 filter rules and never allocates.
 *
 * Allocation is counted by replacing the global operator new for this suite:
 * the native String is a std::string, so a substring or a vector shows up there.
 */

#include <unity.h>
#include <cstdlib>
#include <new>
#include <DomoticsCore/MQTT.h>

using DomoticsCore::Components::MQTTComponent;

static bool countingNew = false;
static size_t newCalls = 0;

void* operator new(std::size_t size) {
    if (countingNew) newCalls++;
    void* p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static bool matches(const char* filter, const char* topic) {
    return MQTTComponent::topicMatches(String(filter), String(topic));
}

void setUp(void) {}
void tearDown(void) { countingNew = false; }

void test_literal_filters(void) {
    TEST_ASSERT_TRUE(matches("home/light", "home/light"));
    TEST_ASSERT_FALSE(matches("home/light", "home/lights"));
    TEST_ASSERT_FALSE(matches("home/light", "home"));
    TEST_ASSERT_FALSE(matches("home", "home/light"));
    TEST_ASSERT_FALSE(matches("a", "a/"));
    TEST_ASSERT_FALSE(matches("a/", "a"));
}

void test_single_level_wildcard(void) {
    TEST_ASSERT_TRUE(matches("home/+/temp", "home/living/temp"));
    TEST_ASSERT_FALSE(matches("home/+/temp", "home/living/room/temp"));
    TEST_ASSERT_FALSE(matches("home/+/temp", "home/living/hum"));
    TEST_ASSERT_TRUE(matches("+", "a"));
    TEST_ASSERT_FALSE(matches("+", "a/b"));
    TEST_ASSERT_TRUE(matches("a/+", "a/"));
    TEST_ASSERT_FALSE(matches("a/+", "a"));
    TEST_ASSERT_TRUE(matches("+/+", "/a"));
    TEST_ASSERT_TRUE(matches("+/b/+", "a/b/c"));
}

void test_multi_level_wildcard(void) {
    TEST_ASSERT_TRUE(matches("#", "a"));
    TEST_ASSERT_TRUE(matches("#", "a/b/c"));
    TEST_ASSERT_TRUE(matches("home/sensors/#", "home/sensors/temp"));
    TEST_ASSERT_TRUE(matches("home/sensors/#", "home/sensors/a/b"));
    TEST_ASSERT_FALSE(matches("home/sensors/#", "home/other"));
    TEST_ASSERT_FALSE(matches("home/sensors/#", "home/sensorsX/temp"));
}

// MQTT 3.1.1 4.7.1.2: "sport/#" also matches the parent level "sport".
void test_multi_level_wildcard_matches_its_parent(void) {
    TEST_ASSERT_TRUE(matches("home/sensors/#", "home/sensors"));
    TEST_ASSERT_TRUE(matches("+/#", "a"));
    TEST_ASSERT_FALSE(matches("home/sensors/#", "home"));
}

// MQTT 3.1.1 4.7.2: a filter starting with a wildcard does not match a '$' topic.
void test_wildcards_skip_dollar_topics(void) {
    TEST_ASSERT_FALSE(matches("#", "$SYS/broker/uptime"));
    TEST_ASSERT_FALSE(matches("+/broker/uptime", "$SYS/broker/uptime"));
    TEST_ASSERT_TRUE(matches("$SYS/#", "$SYS/broker/uptime"));
    TEST_ASSERT_TRUE(matches("$SYS/+/uptime", "$SYS/broker/uptime"));
}

// MQTT 3.1.1 4.7.1: '#' alone and last, '+' alone in its level; a topic has a character.
void test_malformed_filters_and_empty_strings_match_nothing(void) {
    TEST_ASSERT_FALSE(matches("#x", "a"));
    TEST_ASSERT_FALSE(matches("a/#b", "a/x"));
    TEST_ASSERT_FALSE(matches("a/#/b", "a/x"));
    TEST_ASSERT_FALSE(matches("a/+b/c", "a/x//c"));
    TEST_ASSERT_FALSE(matches("+x#", "a"));
    TEST_ASSERT_FALSE(matches("#", ""));
    TEST_ASSERT_FALSE(matches("+", ""));
    TEST_ASSERT_FALSE(matches("", ""));
    TEST_ASSERT_FALSE(matches("", "a"));
}

void test_matching_allocates_nothing(void) {
    // Segments longer than the native String's inline capacity, so any copy shows.
    const String filter("domotics-living-room/+/sensors-and-actuators/#");
    const String topic("domotics-living-room/esp32-0123456789/sensors-and-actuators/temp/raw");
    const String literal("domotics-living-room/esp32-0123456789/sensors-and-actuators/temp/raw");

    newCalls = 0;
    countingNew = true;
    bool wildcard = MQTTComponent::topicMatches(filter, topic);
    bool exact = MQTTComponent::topicMatches(literal, topic);
    bool miss = MQTTComponent::topicMatches(filter, String());
    countingNew = false;

    TEST_ASSERT_TRUE(wildcard);
    TEST_ASSERT_TRUE(exact);
    TEST_ASSERT_FALSE(miss);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)newCalls);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_literal_filters);
    RUN_TEST(test_single_level_wildcard);
    RUN_TEST(test_multi_level_wildcard);
    RUN_TEST(test_multi_level_wildcard_matches_its_parent);
    RUN_TEST(test_wildcards_skip_dollar_topics);
    RUN_TEST(test_malformed_filters_and_empty_strings_match_nothing);
    RUN_TEST(test_matching_allocates_nothing);
    return UNITY_END();
}
