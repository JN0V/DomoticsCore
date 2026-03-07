/**
 * @file test_ha_entity.cpp
 * @brief Native unit tests for HAEntity topic generation methods
 *
 * Tests cover:
 * - Zero heap allocation for all 5 topic methods (AC1, AC2)
 * - Topic format correctness (all methods)
 * - Truncation safety with oversized inputs (AC4)
 */

#include <unity.h>
#include <DomoticsCore/Testing/HeapTracker.h>
#include <DomoticsCore/HAEntity.h>
#include <cstring>
#include <string>

using namespace DomoticsCore::Components::HomeAssistant;
using namespace DomoticsCore::Testing;

void setUp() {}
void tearDown() {}

// ============================================================================
// Test: Zero heap allocation for topic methods (AC1, AC2)
// ============================================================================

void test_ha_topic_methods_zero_heap() {
    // Setup: String member allocations happen here (before checkpoint)
    HAEntity entity("front_door", "Front Door", "alarm_control_panel");
    char buf[HA_TOPIC_BUF_SIZE];

    HeapTracker tracker;
    HEAP_CHECKPOINT(tracker, "before");

    // Call all 5 topic methods 100 times each — pure stack ops
    for (int i = 0; i < 100; i++) {
        entity.getDiscoveryTopic(buf, sizeof(buf), "mynode", "homeassistant");
        entity.getStateTopic(buf, sizeof(buf), "mynode", "homeassistant");
        entity.getCommandTopic(buf, sizeof(buf), "mynode", "homeassistant");
        entity.getAttributesTopic(buf, sizeof(buf), "mynode", "homeassistant");
        entity.getUniqueId(buf, sizeof(buf), "mynode");
    }

    HEAP_CHECKPOINT(tracker, "after");
    // Tolerance 200: HeapTracker's own checkpoint bookkeeping uses ~144 bytes on native.
    // Old String-concat API would add ~500+ bytes per iteration (5000+ total). 200 catches real leaks.
    HEAP_ASSERT_STABLE(tracker, "before", "after", 200);
}

// ============================================================================
// Test: Topic format correctness
// ============================================================================

void test_ha_topic_format_correctness() {
    HAEntity entity("front_door", "Front Door", "alarm_control_panel");
    char buf[HA_TOPIC_BUF_SIZE];

    entity.getDiscoveryTopic(buf, sizeof(buf), "mynode", "homeassistant");
    TEST_ASSERT_EQUAL_STRING("homeassistant/alarm_control_panel/mynode/front_door/config", buf);

    entity.getStateTopic(buf, sizeof(buf), "mynode", "homeassistant");
    TEST_ASSERT_EQUAL_STRING("homeassistant/alarm_control_panel/mynode/front_door/state", buf);

    entity.getCommandTopic(buf, sizeof(buf), "mynode", "homeassistant");
    TEST_ASSERT_EQUAL_STRING("homeassistant/alarm_control_panel/mynode/front_door/set", buf);

    entity.getAttributesTopic(buf, sizeof(buf), "mynode", "homeassistant");
    TEST_ASSERT_EQUAL_STRING("homeassistant/alarm_control_panel/mynode/front_door/attributes", buf);

    entity.getUniqueId(buf, sizeof(buf), "mynode");
    TEST_ASSERT_EQUAL_STRING("mynode_front_door", buf);
}

// ============================================================================
// Test: Truncation safety with oversized inputs (AC4)
// ============================================================================

void test_ha_topic_truncation_safety() {
    // Create entity with long id and long nodeId to exceed 127 chars
    // Format: "homeassistant/alarm_control_panel/{nodeId}/{entityId}/config"
    // "homeassistant/alarm_control_panel/" = 34 chars, "/config" = 7 chars = 41 fixed
    // Need nodeId + entityId + 1 (separator) > 86 to exceed 127
    String longId(std::string(60, 'e').c_str());
    String longNodeId(std::string(60, 'n').c_str());
    HAEntity entity(longId, "Test", "alarm_control_panel");
    char buf[HA_TOPIC_BUF_SIZE];

    entity.getDiscoveryTopic(buf, sizeof(buf), longNodeId.c_str(), "homeassistant");

    // Must be truncated to 127 chars (null terminator at [127])
    TEST_ASSERT_EQUAL_UINT32(127, strlen(buf));
    // Must not overflow
    TEST_ASSERT_EQUAL_CHAR('\0', buf[127]);
}

// ============================================================================
// Unity main
// ============================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ha_topic_methods_zero_heap);
    RUN_TEST(test_ha_topic_format_correctness);
    RUN_TEST(test_ha_topic_truncation_safety);
    return UNITY_END();
}
