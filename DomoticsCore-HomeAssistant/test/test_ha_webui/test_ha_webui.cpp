/**
 * @file test_ha_webui.cpp
 * @brief The HomeAssistantWebUI provider — the only path by which a user's
 *        browser reaches the Home Assistant configuration.
 *
 * BUG-31. The handler used to read six parameters by their own names
 * (`node_id`, `device_name`, …) while the framework's only dispatcher
 * (`WebUI.h:865-876`) supplies exactly `field` and `value`. No read could hit,
 * so no setting could be saved — and `setConfig`, the flash-writing persistence
 * callback and `publishDiscovery()` ran on every request regardless, with the
 * answer `{"success":true}`.
 *
 * What each test would do against that unfixed handler is recorded above it,
 * and the whole suite was run against it: 18 of the 20 fail. The two that pass
 * are marked as guards rather than as evidence — the discovery counter (which
 * tests the component, not the handler) and the topic-left-alone check (which
 * passes because the unfixed handler changes nothing at all).
 *
 * The observable for "discovery was republished" is
 * `getStatistics().discoveryCount`: `publishDiscovery()` increments it before
 * any MQTT work and the publish path goes through `emit()`, which is a no-op
 * without an EventBus. `test_the_discovery_counter_is_a_usable_observable`
 * pins that, because the whole suite rests on it. Flash itself is not
 * observable here — persistence is reached only through the `onConfigSaved`
 * callback, whose `putString` calls live in a lambda at
 * `SystemWebUISetup.h:381-386` that no native project compiles.
 */

#include <unity.h>
#include <DomoticsCore/HomeAssistantWebUI.h>
#include <DomoticsCore/ArduinoJsonString.h>
#include <cstring>
#include <map>
#include <string>

using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;
using DomoticsCore::Components::WebUI::HomeAssistantWebUI;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Fixture
// ============================================================================

static std::map<String, String> fieldValue(const char* field, const char* value) {
    std::map<String, String> params;
    params[String("field")] = String(field);
    params[String("value")] = String(value);
    return params;
}

/**
 * A component with default identity, its provider, and counters for the two
 * side effects the fix is about.
 */
struct Fixture {
    HomeAssistantComponent ha;
    HomeAssistantWebUI ui;
    int saveCount = 0;
    HAConfig lastSaved;

    Fixture() : ui(&ha) {
        ui.setConfigSaveCallback([this](const HAConfig& cfg) {
            saveCount++;
            lastSaved = cfg;
        });
    }

    explicit Fixture(const HAConfig& cfg) : ha(cfg), ui(&ha) {
        ui.setConfigSaveCallback([this](const HAConfig& c) {
            saveCount++;
            lastSaved = c;
        });
    }

    uint32_t discoveries() const { return ha.getStatistics().discoveryCount; }

    String post(const char* field, const char* value) {
        return ui.handleWebUIRequest(String("ha_settings"), String("/api/ui/action"),
                                     String("POST"), fieldValue(field, value));
    }
};

static bool contains(const String& haystack, const char* needle) {
    return strstr(haystack.c_str(), needle) != nullptr;
}

static bool succeeded(const String& response) {
    return contains(response, "\"success\":true");
}

// A refusal says `success:false` and carries no `error` key: app.js inspects
// only `data.error`, so an error key pops a modal alert where the refusal is
// meant to be silent.
static void assertRefused(const String& response) {
    TEST_ASSERT_FALSE(succeeded(response));
    TEST_ASSERT_TRUE(contains(response, "\"success\":false"));
    TEST_ASSERT_FALSE(contains(response, "error"));
}

// ============================================================================
// The observable the rest of the suite rests on
// ============================================================================

// Would pass without the fix — it tests the component, not the handler.
void test_the_discovery_counter_is_a_usable_observable(void) {
    Fixture f;
    TEST_ASSERT_EQUAL_UINT32(0, f.discoveries());
    f.ha.publishDiscovery();
    // No MQTT, no EventBus, no entities — the counter still moves, so "was
    // discovery republished?" is answerable natively.
    TEST_ASSERT_EQUAL_UINT32(1, f.discoveries());
}

// ============================================================================
// A setting can be saved at all
// ============================================================================

// FAILS without the fix: `node_id` is never read, so nodeId keeps its default.
void test_node_id_is_saved(void) {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", "lab01")));
    TEST_ASSERT_EQUAL_STRING("lab01", f.ha.getConfig().nodeId);
}

// FAILS without the fix: none of the six values reaches the config.
void test_every_settings_field_is_saved(void) {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", "lab01")));
    TEST_ASSERT_TRUE(succeeded(f.post("device_name", "Lab Sensor")));
    TEST_ASSERT_TRUE(succeeded(f.post("manufacturer", "Acme")));
    TEST_ASSERT_TRUE(succeeded(f.post("model", "AC-1")));
    TEST_ASSERT_TRUE(succeeded(f.post("discovery_prefix", "hass")));
    TEST_ASSERT_TRUE(succeeded(f.post("suggested_area", "Garage")));

    const HAConfig& cfg = f.ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("lab01", cfg.nodeId);
    TEST_ASSERT_EQUAL_STRING("Lab Sensor", cfg.deviceName);
    TEST_ASSERT_EQUAL_STRING("Acme", cfg.manufacturer);
    TEST_ASSERT_EQUAL_STRING("AC-1", cfg.model);
    TEST_ASSERT_EQUAL_STRING("hass", cfg.discoveryPrefix);
    TEST_ASSERT_EQUAL_STRING("Garage", cfg.suggestedArea);
}

// FAILS without the fix: nothing changes, so "only that field" is vacuous —
// the assertion that carries it is that nodeId did change.
void test_only_the_named_field_moves(void) {
    Fixture f;
    HAConfig before = f.ha.getConfig();
    TEST_ASSERT_TRUE(succeeded(f.post("model", "AC-1")));

    const HAConfig& cfg = f.ha.getConfig();
    TEST_ASSERT_EQUAL_STRING("AC-1", cfg.model);
    TEST_ASSERT_EQUAL_STRING(before.nodeId, cfg.nodeId);
    TEST_ASSERT_EQUAL_STRING(before.deviceName, cfg.deviceName);
    TEST_ASSERT_EQUAL_STRING(before.manufacturer, cfg.manufacturer);
    TEST_ASSERT_EQUAL_STRING(before.discoveryPrefix, cfg.discoveryPrefix);
    TEST_ASSERT_EQUAL_STRING(before.suggestedArea, cfg.suggestedArea);
}

// FAILS without the fix: the callback fires but carries the unchanged config.
void test_saving_fires_the_persistence_callback_once_with_the_new_config(void) {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", "lab01")));
    TEST_ASSERT_EQUAL_INT(1, f.saveCount);
    TEST_ASSERT_EQUAL_STRING("lab01", f.lastSaved.nodeId);
}

// FAILS without the fix: the config is unchanged, so the republished discovery
// carries the old identity. The counter assertion alone would pass; the nodeId
// assertion is what makes this discriminating.
void test_saving_republishes_discovery(void) {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", "lab01")));
    TEST_ASSERT_EQUAL_UINT32(1, f.discoveries());
    TEST_ASSERT_EQUAL_STRING("lab01", f.ha.getConfig().nodeId);
}

// ============================================================================
// A request that changes nothing does nothing
// ============================================================================

// FAILS without the fix: today every request writes flash and republishes.
void test_an_unchanged_value_has_no_side_effects(void) {
    Fixture f;
    const char* currentName = f.ha.getConfig().deviceName;
    TEST_ASSERT_TRUE(succeeded(f.post("device_name", currentName)));
    TEST_ASSERT_EQUAL_INT(0, f.saveCount);
    TEST_ASSERT_EQUAL_UINT32(0, f.discoveries());
}

// FAILS without the fix: two identical requests would be two saves and two
// republishes. This is the third acceptance criterion.
void test_the_same_value_twice_costs_one_save_and_one_republish(void) {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", "lab01")));
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", "lab01")));
    TEST_ASSERT_EQUAL_INT(1, f.saveCount);
    TEST_ASSERT_EQUAL_UINT32(1, f.discoveries());
}

// ============================================================================
// Refusals
// ============================================================================

// FAILS without the fix: an unknown field returns `{"success":true}` today,
// having written flash and republished on the way.
void test_an_unknown_field_is_refused_and_changes_nothing(void) {
    Fixture f;
    HAConfig before = f.ha.getConfig();
    assertRefused(f.post("colour", "blue"));
    TEST_ASSERT_EQUAL_STRING(before.nodeId, f.ha.getConfig().nodeId);
    TEST_ASSERT_EQUAL_INT(0, f.saveCount);
    TEST_ASSERT_EQUAL_UINT32(0, f.discoveries());
}

// FAILS without the fix: no `field` parameter is exactly what the dispatcher
// used to look like to this handler, and it answered `success` with the full
// set of side effects.
void test_a_request_without_a_field_is_refused(void) {
    Fixture f;
    std::map<String, String> params;
    params[String("value")] = String("lab01");
    assertRefused(f.ui.handleWebUIRequest(String("ha_settings"), String(""),
                                          String("POST"), params));
    TEST_ASSERT_EQUAL_INT(0, f.saveCount);
    TEST_ASSERT_EQUAL_UINT32(0, f.discoveries());
}

// FAILS without the fix, for the same reason.
void test_a_request_without_a_value_is_refused(void) {
    Fixture f;
    std::map<String, String> params;
    params[String("field")] = String("node_id");
    assertRefused(f.ui.handleWebUIRequest(String("ha_settings"), String(""),
                                          String("POST"), params));
    TEST_ASSERT_EQUAL_INT(0, f.saveCount);
    TEST_ASSERT_EQUAL_UINT32(0, f.discoveries());
}

// Would pass without the fix — the old handler also required "POST". A guard,
// not evidence. Note that the route is registered HTTP_GET (`WebUI.h:544`) and
// the dispatcher fabricates the string "POST" at `:871`, so this check does not
// mean a GET cannot reach the handler.
void test_a_non_post_method_is_refused(void) {
    Fixture f;
    assertRefused(f.ui.handleWebUIRequest(String("ha_settings"), String(""),
                                          String("GET"), fieldValue("node_id", "lab01")));
    TEST_ASSERT_EQUAL_INT(0, f.saveCount);
    TEST_ASSERT_EQUAL_UINT32(0, f.discoveries());
}

// Would pass without the fix in outcome, but not in shape: the old handler
// answered `{"error":"Unsupported operation"}`, which pops a modal alert in
// app.js. The no-error-key assertion is what makes it discriminating.
void test_another_context_is_refused(void) {
    Fixture f;
    assertRefused(f.ui.handleWebUIRequest(String("ha_status"), String(""),
                                          String("POST"), fieldValue("node_id", "lab01")));
    TEST_ASSERT_EQUAL_INT(0, f.saveCount);
    TEST_ASSERT_EQUAL_UINT32(0, f.discoveries());
}

// Would pass without the fix in outcome, not in shape: the old guard returned
// `{"error":"Component not available"}`. Neither version dereferences null.
void test_a_null_component_is_refused_rather_than_dereferenced(void) {
    HomeAssistantWebUI ui(nullptr);
    assertRefused(ui.handleWebUIRequest(String("ha_settings"), String(""),
                                        String("POST"), fieldValue("node_id", "lab01")));
}

// ============================================================================
// The availability topic follows the node it names
// ============================================================================

// FAILS without the fix — and would fail against a fix that only routed the
// value, because setConfig regenerates the topic only when it is empty
// (`HomeAssistant.h:465-475`). Without this the device keeps publishing
// availability on the previous node's topic.
void test_changing_the_node_id_regenerates_the_availability_topic(void) {
    Fixture f;
    TEST_ASSERT_EQUAL_STRING("homeassistant/myDeviceId/availability",
                             f.ha.getConfig().availabilityTopic);
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", "lab01")));
    TEST_ASSERT_EQUAL_STRING("homeassistant/lab01/availability",
                             f.ha.getConfig().availabilityTopic);
}

// FAILS without the fix, same reason, through the other field the topic is
// built from.
void test_changing_the_discovery_prefix_regenerates_the_availability_topic(void) {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("discovery_prefix", "hass")));
    TEST_ASSERT_EQUAL_STRING("hass/myDeviceId/availability",
                             f.ha.getConfig().availabilityTopic);
}

// FAILS without the fix (nothing is saved at all). It pins the boundary of the
// regeneration: a topic somebody set deliberately is not the handler's to
// rewrite, which is the contract `test_ha_component.cpp:126-135` already holds
// setConfig to.
void test_a_custom_availability_topic_survives_a_node_id_change(void) {
    HAConfig cfg;
    HA::setField(cfg.availabilityTopic, "custom/availability/topic", sizeof(cfg.availabilityTopic));
    Fixture f(cfg);

    TEST_ASSERT_TRUE(succeeded(f.post("node_id", "lab01")));
    TEST_ASSERT_EQUAL_STRING("lab01", f.ha.getConfig().nodeId);
    TEST_ASSERT_EQUAL_STRING("custom/availability/topic", f.ha.getConfig().availabilityTopic);
}

// Would pass without the fix — measured, not assumed: the unfixed handler
// changes nothing at all, so of course the topic stays put. It is a guard on
// the regeneration above, not evidence.
void test_changing_the_model_leaves_the_availability_topic_alone(void) {
    Fixture f;
    TEST_ASSERT_TRUE(succeeded(f.post("model", "AC-1")));
    TEST_ASSERT_EQUAL_STRING("homeassistant/myDeviceId/availability",
                             f.ha.getConfig().availabilityTopic);
}

// ============================================================================
// Length
// ============================================================================

// FAILS without the fix: nothing is stored today, truncated or otherwise.
void test_an_over_long_node_id_is_truncated_by_set_field(void) {
    Fixture f;
    std::string sixtyFour(64, 'x');
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", sixtyFour.c_str())));
    // HA::MAX_NODE_ID is 33 — 32 characters plus the terminator — and setField
    // warns as it truncates.
    TEST_ASSERT_EQUAL_size_t(HA::MAX_NODE_ID - 1, strlen(f.ha.getConfig().nodeId));
}

// FAILS without the fix. Two over-long values that truncate to the same stored
// string are one change, not two: the comparison happens after the truncation.
void test_a_value_that_truncates_to_the_stored_one_is_not_a_change(void) {
    Fixture f;
    std::string sixtyFour(64, 'x');
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", sixtyFour.c_str())));
    TEST_ASSERT_EQUAL_INT(1, f.saveCount);

    std::string longer(80, 'x');
    TEST_ASSERT_TRUE(succeeded(f.post("node_id", longer.c_str())));
    TEST_ASSERT_EQUAL_INT(1, f.saveCount);
    TEST_ASSERT_EQUAL_UINT32(1, f.discoveries());
}

// ============================================================================

int runAllTests(void) {
    UNITY_BEGIN();

    RUN_TEST(test_the_discovery_counter_is_a_usable_observable);

    RUN_TEST(test_node_id_is_saved);
    RUN_TEST(test_every_settings_field_is_saved);
    RUN_TEST(test_only_the_named_field_moves);
    RUN_TEST(test_saving_fires_the_persistence_callback_once_with_the_new_config);
    RUN_TEST(test_saving_republishes_discovery);

    RUN_TEST(test_an_unchanged_value_has_no_side_effects);
    RUN_TEST(test_the_same_value_twice_costs_one_save_and_one_republish);

    RUN_TEST(test_an_unknown_field_is_refused_and_changes_nothing);
    RUN_TEST(test_a_request_without_a_field_is_refused);
    RUN_TEST(test_a_request_without_a_value_is_refused);
    RUN_TEST(test_a_non_post_method_is_refused);
    RUN_TEST(test_another_context_is_refused);
    RUN_TEST(test_a_null_component_is_refused_rather_than_dereferenced);

    RUN_TEST(test_changing_the_node_id_regenerates_the_availability_topic);
    RUN_TEST(test_changing_the_discovery_prefix_regenerates_the_availability_topic);
    RUN_TEST(test_a_custom_availability_topic_survives_a_node_id_change);
    RUN_TEST(test_changing_the_model_leaves_the_availability_topic_alone);

    RUN_TEST(test_an_over_long_node_id_is_truncated_by_set_field);
    RUN_TEST(test_a_value_that_truncates_to_the_stored_one_is_not_a_change);

    return UNITY_END();
}

#ifdef ARDUINO
void setup() { runAllTests(); }
void loop() {}
#else
int main(int argc, char** argv) { return runAllTests(); }
#endif
