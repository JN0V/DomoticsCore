/**
 * @file test_led_webui_version.cpp
 * @brief The LEDWebUI provider reports the component's own version, never an
 *        old number written into the provider.
 */

#include <DomoticsCore/Logger.h>  // Required for DLOG macros used in LED.h
#include <DomoticsCore/LEDWebUI.h>
#include <unity.h>
#include <cstring>

using namespace DomoticsCore::Components;

void setUp(void) {}
void tearDown(void) {}

// Scenario: No stale literal
// Given a provider whose component is missing,
// When getWebUIVersion() is read,
// Then the result is not the version number once hard-coded in the provider.
void test_led_missing_component_reports_no_stale_version(void) {
    LEDWebUI ui(nullptr);
    const String version = ui.getWebUIVersion();
    TEST_ASSERT_TRUE_MESSAGE(strcmp(version.c_str(), "1.3.0") != 0, version.c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_led_missing_component_reports_no_stale_version);
    return UNITY_END();
}
