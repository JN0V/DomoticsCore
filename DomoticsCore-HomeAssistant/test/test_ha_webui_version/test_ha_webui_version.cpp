/**
 * @file test_ha_webui_version.cpp
 * @brief The HomeAssistantWebUI provider reports the component's own version,
 *        never an old number written into the provider.
 */

#include <unity.h>
#include <DomoticsCore/HomeAssistantWebUI.h>
#include <cstring>

using namespace DomoticsCore::Components;
using DomoticsCore::Components::WebUI::HomeAssistantWebUI;

void setUp(void) {}
void tearDown(void) {}

// Scenario: No stale literal
// Given a provider whose component is missing,
// When getWebUIVersion() is read,
// Then the result is not the version number once hard-coded in the provider.
void test_ha_missing_component_reports_no_stale_version(void) {
    HomeAssistantWebUI ui(nullptr);
    const String version = ui.getWebUIVersion();
    TEST_ASSERT_TRUE_MESSAGE(strcmp(version.c_str(), "1.4.0") != 0, version.c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ha_missing_component_reports_no_stale_version);
    return UNITY_END();
}
