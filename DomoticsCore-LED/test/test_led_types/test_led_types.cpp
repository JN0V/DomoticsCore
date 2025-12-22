// Tests unitaires pour les types LED (LEDColor, LEDEffect, LEDConfig)
// Ces tests ne dépendent pas de Core - ils testent la logique pure

#include <DomoticsCore/Logger.h>  // Required for DLOG macros used in LED.h
#include <DomoticsCore/LED.h>
#include <unity.h>

using namespace DomoticsCore::Components;

// ============================================================================
// LEDColor Tests
// ============================================================================

void test_led_color_default_constructor(void) {
    LEDColor color;
    TEST_ASSERT_EQUAL_UINT8(0, color.red);
    TEST_ASSERT_EQUAL_UINT8(0, color.green);
    TEST_ASSERT_EQUAL_UINT8(0, color.blue);
}

void test_led_color_rgb_constructor(void) {
    LEDColor color(100, 150, 200);
    TEST_ASSERT_EQUAL_UINT8(100, color.red);
    TEST_ASSERT_EQUAL_UINT8(150, color.green);
    TEST_ASSERT_EQUAL_UINT8(200, color.blue);
}

void test_led_color_predefined_white(void) {
    LEDColor white = LEDColor::White();
    TEST_ASSERT_EQUAL_UINT8(255, white.red);
    TEST_ASSERT_EQUAL_UINT8(255, white.green);
    TEST_ASSERT_EQUAL_UINT8(255, white.blue);
}

void test_led_color_predefined_red(void) {
    LEDColor red = LEDColor::Red();
    TEST_ASSERT_EQUAL_UINT8(255, red.red);
    TEST_ASSERT_EQUAL_UINT8(0, red.green);
    TEST_ASSERT_EQUAL_UINT8(0, red.blue);
}

void test_led_color_predefined_green(void) {
    LEDColor green = LEDColor::Green();
    TEST_ASSERT_EQUAL_UINT8(0, green.red);
    TEST_ASSERT_EQUAL_UINT8(255, green.green);
    TEST_ASSERT_EQUAL_UINT8(0, green.blue);
}

void test_led_color_predefined_blue(void) {
    LEDColor blue = LEDColor::Blue();
    TEST_ASSERT_EQUAL_UINT8(0, blue.red);
    TEST_ASSERT_EQUAL_UINT8(0, blue.green);
    TEST_ASSERT_EQUAL_UINT8(255, blue.blue);
}

void test_led_color_predefined_off(void) {
    LEDColor off = LEDColor::Off();
    TEST_ASSERT_EQUAL_UINT8(0, off.red);
    TEST_ASSERT_EQUAL_UINT8(0, off.green);
    TEST_ASSERT_EQUAL_UINT8(0, off.blue);
}

// ============================================================================
// LEDEffect Tests
// ============================================================================

void test_led_effect_solid(void) {
    LEDEffect effect = LEDEffect::Solid;
    TEST_ASSERT_EQUAL(LEDEffect::Solid, effect);
}

void test_led_effect_blink(void) {
    LEDEffect effect = LEDEffect::Blink;
    TEST_ASSERT_EQUAL(LEDEffect::Blink, effect);
}

void test_led_effect_fade(void) {
    LEDEffect effect = LEDEffect::Fade;
    TEST_ASSERT_EQUAL(LEDEffect::Fade, effect);
}

void test_led_effect_pulse(void) {
    LEDEffect effect = LEDEffect::Pulse;
    TEST_ASSERT_EQUAL(LEDEffect::Pulse, effect);
}

void test_led_effect_breathing(void) {
    LEDEffect effect = LEDEffect::Breathing;
    TEST_ASSERT_EQUAL(LEDEffect::Breathing, effect);
}

// ============================================================================
// LEDConfig Tests
// ============================================================================

void test_led_config_default_values(void) {
    LEDConfig config;
    TEST_ASSERT_EQUAL(-1, config.pin);
    TEST_ASSERT_FALSE(config.isRGB);
    TEST_ASSERT_EQUAL(-1, config.redPin);
    TEST_ASSERT_EQUAL(-1, config.greenPin);
    TEST_ASSERT_EQUAL(-1, config.bluePin);
    TEST_ASSERT_EQUAL_UINT8(255, config.maxBrightness);
    TEST_ASSERT_FALSE(config.invertLogic);
}

void test_led_config_single_led(void) {
    LEDConfig config;
    config.pin = 2;
    config.name = "TestLED";
    config.maxBrightness = 128;
    
    TEST_ASSERT_EQUAL(2, config.pin);
    TEST_ASSERT_EQUAL_STRING("TestLED", config.name.c_str());
    TEST_ASSERT_EQUAL_UINT8(128, config.maxBrightness);
}

void test_led_config_rgb_led(void) {
    LEDConfig config;
    config.isRGB = true;
    config.redPin = 25;
    config.greenPin = 26;
    config.bluePin = 27;
    config.name = "RGBLed";
    
    TEST_ASSERT_TRUE(config.isRGB);
    TEST_ASSERT_EQUAL(25, config.redPin);
    TEST_ASSERT_EQUAL(26, config.greenPin);
    TEST_ASSERT_EQUAL(27, config.bluePin);
}

// ============================================================================
// LEDState Tests
// ============================================================================

void test_led_state_default_values(void) {
    LEDState state;
    TEST_ASSERT_EQUAL_UINT8(0, state.brightness);
    TEST_ASSERT_EQUAL(LEDEffect::Solid, state.effect);
    TEST_ASSERT_EQUAL(1000, state.effectSpeed);
    TEST_ASSERT_TRUE(state.enabled);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // LEDColor tests
    RUN_TEST(test_led_color_default_constructor);
    RUN_TEST(test_led_color_rgb_constructor);
    RUN_TEST(test_led_color_predefined_white);
    RUN_TEST(test_led_color_predefined_red);
    RUN_TEST(test_led_color_predefined_green);
    RUN_TEST(test_led_color_predefined_blue);
    RUN_TEST(test_led_color_predefined_off);

    // LEDEffect tests
    RUN_TEST(test_led_effect_solid);
    RUN_TEST(test_led_effect_blink);
    RUN_TEST(test_led_effect_fade);
    RUN_TEST(test_led_effect_pulse);
    RUN_TEST(test_led_effect_breathing);

    // LEDConfig tests
    RUN_TEST(test_led_config_default_values);
    RUN_TEST(test_led_config_single_led);
    RUN_TEST(test_led_config_rgb_led);

    // LEDState tests
    RUN_TEST(test_led_state_default_values);

    return UNITY_END();
}
