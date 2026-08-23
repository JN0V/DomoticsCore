/**
 * @file test_led_effects.cpp
 * @brief The effect engine and the PWM arithmetic.
 *
 * The stub HAL swallows analogWrite(), so a native test cannot observe a pin.
 * What it can observe is the value computed for that pin — which is where the
 * arithmetic lives, and where BUG-18 (phase reset instead of wrap) hid.
 *
 * Trigonometric expectations are asserted within 1, not exactly: the component
 * truncates a double to uint8_t, so a libm returning 0.9999999 instead of 1.0
 * costs one unit. Exact values (0, and the linear rainbow ramps) are asserted
 * exactly, because they must be.
 */

#include <DomoticsCore/Logger.h>  // Required for DLOG macros used in LED.h
#include <DomoticsCore/LED.h>
#include <unity.h>

using namespace DomoticsCore::Components;

void setUp(void) {}
void tearDown(void) {}

static const uint8_t BASE = 200;

// ============================================================================
// Solid and Rainbow — brightness passes through untouched
// ============================================================================

void test_solid_ignores_phase(void) {
    TEST_ASSERT_EQUAL_UINT8(BASE, LEDComponent::effectBrightness(LEDEffect::Solid, 0.0f, BASE));
    TEST_ASSERT_EQUAL_UINT8(BASE, LEDComponent::effectBrightness(LEDEffect::Solid, 0.5f, BASE));
    TEST_ASSERT_EQUAL_UINT8(BASE, LEDComponent::effectBrightness(LEDEffect::Solid, 1.0f, BASE));
}

void test_rainbow_animates_colour_not_brightness(void) {
    TEST_ASSERT_EQUAL_UINT8(BASE, LEDComponent::effectBrightness(LEDEffect::Rainbow, 0.0f, BASE));
    TEST_ASSERT_EQUAL_UINT8(BASE, LEDComponent::effectBrightness(LEDEffect::Rainbow, 0.7f, BASE));
}

// ============================================================================
// Blink — square wave, half the cycle on
// ============================================================================

void test_blink_is_on_for_the_first_half(void) {
    TEST_ASSERT_EQUAL_UINT8(BASE, LEDComponent::effectBrightness(LEDEffect::Blink, 0.0f, BASE));
    TEST_ASSERT_EQUAL_UINT8(BASE, LEDComponent::effectBrightness(LEDEffect::Blink, 0.25f, BASE));
    TEST_ASSERT_EQUAL_UINT8(BASE, LEDComponent::effectBrightness(LEDEffect::Blink, 0.4999f, BASE));
}

void test_blink_is_off_for_the_second_half(void) {
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Blink, 0.5f, BASE));
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Blink, 0.75f, BASE));
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Blink, 1.0f, BASE));
}

void test_blink_of_a_dark_led_stays_dark(void) {
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Blink, 0.0f, 0));
}

// ============================================================================
// Fade — one full sine, starting mid-scale
// ============================================================================

void test_fade_starts_at_half_brightness(void) {
    TEST_ASSERT_EQUAL_UINT8(100, LEDComponent::effectBrightness(LEDEffect::Fade, 0.0f, BASE));
}

void test_fade_peaks_at_a_quarter_of_the_cycle(void) {
    TEST_ASSERT_UINT8_WITHIN(1, BASE, LEDComponent::effectBrightness(LEDEffect::Fade, 0.25f, BASE));
}

void test_fade_returns_to_half_at_mid_cycle(void) {
    TEST_ASSERT_UINT8_WITHIN(1, 100, LEDComponent::effectBrightness(LEDEffect::Fade, 0.5f, BASE));
}

void test_fade_bottoms_out_at_three_quarters(void) {
    TEST_ASSERT_UINT8_WITHIN(1, 0, LEDComponent::effectBrightness(LEDEffect::Fade, 0.75f, BASE));
}

// ============================================================================
// Pulse — two lobes then a dark tail (heartbeat)
// ============================================================================

void test_pulse_starts_dark(void) {
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Pulse, 0.0f, BASE));
}

void test_pulse_first_lobe_peaks_at_0_15(void) {
    TEST_ASSERT_UINT8_WITHIN(1, BASE, LEDComponent::effectBrightness(LEDEffect::Pulse, 0.15f, BASE));
}

void test_pulse_dips_between_the_two_lobes(void) {
    TEST_ASSERT_UINT8_WITHIN(1, 0, LEDComponent::effectBrightness(LEDEffect::Pulse, 0.3f, BASE));
}

void test_pulse_second_lobe_peaks_at_0_4(void) {
    TEST_ASSERT_UINT8_WITHIN(1, BASE, LEDComponent::effectBrightness(LEDEffect::Pulse, 0.4f, BASE));
}

void test_pulse_is_dark_for_the_whole_second_half(void) {
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Pulse, 0.5f, BASE));
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Pulse, 0.8f, BASE));
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Pulse, 1.0f, BASE));
}

// ============================================================================
// Breathing — one raised cosine, dark at both ends
// ============================================================================

void test_breathing_starts_dark(void) {
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::effectBrightness(LEDEffect::Breathing, 0.0f, BASE));
}

void test_breathing_peaks_at_mid_cycle(void) {
    TEST_ASSERT_UINT8_WITHIN(1, BASE, LEDComponent::effectBrightness(LEDEffect::Breathing, 0.5f, BASE));
}

void test_breathing_is_half_way_up_at_a_quarter(void) {
    TEST_ASSERT_UINT8_WITHIN(1, 100, LEDComponent::effectBrightness(LEDEffect::Breathing, 0.25f, BASE));
}

void test_breathing_ends_dark(void) {
    TEST_ASSERT_UINT8_WITHIN(1, 0, LEDComponent::effectBrightness(LEDEffect::Breathing, 1.0f, BASE));
}

// ============================================================================
// Rainbow colour ramp — three linear segments over a 360 degree hue
// ============================================================================

void test_rainbow_starts_red(void) {
    LEDColor c = LEDComponent::rainbowColor(0.0f);
    TEST_ASSERT_EQUAL_UINT8(255, c.red);
    TEST_ASSERT_EQUAL_UINT8(0, c.green);
    TEST_ASSERT_EQUAL_UINT8(0, c.blue);
}

void test_rainbow_red_to_green_segment(void) {
    // hue 90: three quarters of the way from red to green, blue still absent
    LEDColor c = LEDComponent::rainbowColor(0.25f);
    TEST_ASSERT_EQUAL_UINT8(63, c.red);
    TEST_ASSERT_EQUAL_UINT8(191, c.green);
    TEST_ASSERT_EQUAL_UINT8(0, c.blue);
}

void test_rainbow_green_to_blue_segment(void) {
    // hue 180: halfway from green to blue, red absent
    LEDColor c = LEDComponent::rainbowColor(0.5f);
    TEST_ASSERT_EQUAL_UINT8(0, c.red);
    TEST_ASSERT_EQUAL_UINT8(127, c.green);
    TEST_ASSERT_EQUAL_UINT8(127, c.blue);
}

void test_rainbow_blue_to_red_segment(void) {
    // hue 270: a quarter of the way back from blue to red, green absent
    LEDColor c = LEDComponent::rainbowColor(0.75f);
    TEST_ASSERT_EQUAL_UINT8(63, c.red);
    TEST_ASSERT_EQUAL_UINT8(0, c.green);
    TEST_ASSERT_EQUAL_UINT8(191, c.blue);
}

void test_rainbow_never_goes_dark(void) {
    // Every phase lights at least one channel — a rainbow with a black frame
    // would read as a flicker on the board.
    for (int i = 0; i <= 100; i++) {
        LEDColor c = LEDComponent::rainbowColor(i / 100.0f);
        int sum = (int)c.red + (int)c.green + (int)c.blue;
        TEST_ASSERT_GREATER_THAN_INT(200, sum);
    }
}

// ============================================================================
// PWM arithmetic
// ============================================================================

void test_scale_to_max_maps_full_scale(void) {
    TEST_ASSERT_EQUAL_UINT8(128, LEDComponent::scaleToMax(255, 128));
    TEST_ASSERT_EQUAL_UINT8(255, LEDComponent::scaleToMax(255, 255));
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::scaleToMax(0, 200));
}

void test_scale_to_max_is_identity_against_255(void) {
    TEST_ASSERT_EQUAL_UINT8(128, LEDComponent::scaleToMax(128, 255));
    TEST_ASSERT_EQUAL_UINT8(1, LEDComponent::scaleToMax(1, 255));
}

void test_scale_to_max_truncates_towards_zero(void) {
    // 100/255 of 50 is 19.6 — integer division, so 19. Documented, not fixed:
    // a rounding change would shift every brightness the framework has shipped.
    TEST_ASSERT_EQUAL_UINT8(19, LEDComponent::scaleToMax(100, 50));
}

void test_scale_to_max_of_a_zero_ceiling_is_dark(void) {
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::scaleToMax(255, 0));
}

void test_pwm_value_passes_through_when_not_inverted(void) {
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::pwmValue(0, false));
    TEST_ASSERT_EQUAL_UINT8(200, LEDComponent::pwmValue(200, false));
    TEST_ASSERT_EQUAL_UINT8(255, LEDComponent::pwmValue(255, false));
}

void test_pwm_value_inverts_for_common_anode(void) {
    TEST_ASSERT_EQUAL_UINT8(255, LEDComponent::pwmValue(0, true));
    TEST_ASSERT_EQUAL_UINT8(55, LEDComponent::pwmValue(200, true));
    TEST_ASSERT_EQUAL_UINT8(0, LEDComponent::pwmValue(255, true));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_solid_ignores_phase);
    RUN_TEST(test_rainbow_animates_colour_not_brightness);

    RUN_TEST(test_blink_is_on_for_the_first_half);
    RUN_TEST(test_blink_is_off_for_the_second_half);
    RUN_TEST(test_blink_of_a_dark_led_stays_dark);

    RUN_TEST(test_fade_starts_at_half_brightness);
    RUN_TEST(test_fade_peaks_at_a_quarter_of_the_cycle);
    RUN_TEST(test_fade_returns_to_half_at_mid_cycle);
    RUN_TEST(test_fade_bottoms_out_at_three_quarters);

    RUN_TEST(test_pulse_starts_dark);
    RUN_TEST(test_pulse_first_lobe_peaks_at_0_15);
    RUN_TEST(test_pulse_dips_between_the_two_lobes);
    RUN_TEST(test_pulse_second_lobe_peaks_at_0_4);
    RUN_TEST(test_pulse_is_dark_for_the_whole_second_half);

    RUN_TEST(test_breathing_starts_dark);
    RUN_TEST(test_breathing_peaks_at_mid_cycle);
    RUN_TEST(test_breathing_is_half_way_up_at_a_quarter);
    RUN_TEST(test_breathing_ends_dark);

    RUN_TEST(test_rainbow_starts_red);
    RUN_TEST(test_rainbow_red_to_green_segment);
    RUN_TEST(test_rainbow_green_to_blue_segment);
    RUN_TEST(test_rainbow_blue_to_red_segment);
    RUN_TEST(test_rainbow_never_goes_dark);

    RUN_TEST(test_scale_to_max_maps_full_scale);
    RUN_TEST(test_scale_to_max_is_identity_against_255);
    RUN_TEST(test_scale_to_max_truncates_towards_zero);
    RUN_TEST(test_scale_to_max_of_a_zero_ceiling_is_dark);
    RUN_TEST(test_pwm_value_passes_through_when_not_inverted);
    RUN_TEST(test_pwm_value_inverts_for_common_anode);

    return UNITY_END();
}
