/**
 * @file test_led_component.cpp
 * @brief Tests for LED metadata.name fix (M12)
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/LED.h>
#include <DomoticsCore/Platform_Stub.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

void setUp(void) {}
void tearDown(void) {}

void test_led_metadata_name_is_short(void) {
    LEDComponent led;
    TEST_ASSERT_EQUAL_STRING("LED", led.metadata.name);
}

void test_led_component_lookup_by_short_name(void) {
    Core core;
    core.addComponent(std::make_unique<LEDComponent>());
    core.begin();

    auto* ledComp = core.getComponent<LEDComponent>("LED");
    TEST_ASSERT_NOT_NULL(ledComp);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_led_metadata_name_is_short);
    RUN_TEST(test_led_component_lookup_by_short_name);

    return UNITY_END();
}
