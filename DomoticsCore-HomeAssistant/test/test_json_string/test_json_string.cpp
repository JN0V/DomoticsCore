/**
 * @file test_json_string.cpp
 * @brief The native String converter reads JSON as ArduinoJson's Arduino
 *        String support does on the boards; the device environments run it too.
 */

#include <unity.h>
#include <DomoticsCore/Platform_HAL.h>
#include <ArduinoJson.h>
#include <DomoticsCore/ArduinoJsonString.h>

void setUp(void) {}
void tearDown(void) {}

void test_a_missing_key_falls_back_to_the_default(void) {
    JsonDocument doc;
    deserializeJson(doc, "{\"state\":\"ON\"}");
    String brightness = doc["brightness"] | String("255");
    TEST_ASSERT_EQUAL_STRING("255", brightness.c_str());
}

void test_a_present_string_wins_over_the_default(void) {
    JsonDocument doc;
    deserializeJson(doc, "{\"state\":\"ON\"}");
    String state = doc["state"] | String("OFF");
    TEST_ASSERT_EQUAL_STRING("ON", state.c_str());
}

void test_a_null_value_falls_back_to_the_default(void) {
    JsonDocument doc;
    deserializeJson(doc, "{\"state\":null}");
    String state = doc["state"] | String("OFF");
    TEST_ASSERT_EQUAL_STRING("OFF", state.c_str());
}

void test_a_number_is_not_a_string(void) {
    JsonDocument doc;
    deserializeJson(doc, "{\"n\":42}");
    TEST_ASSERT_FALSE(doc["n"].is<String>());
    String n = doc["n"] | String("dflt");
    TEST_ASSERT_EQUAL_STRING("dflt", n.c_str());
}

void test_as_string_serializes_what_is_not_a_string(void) {
    JsonDocument doc;
    deserializeJson(doc, "{\"n\":42,\"b\":true}");
    TEST_ASSERT_EQUAL_STRING("42", doc["n"].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("true", doc["b"].as<String>().c_str());
    TEST_ASSERT_EQUAL_STRING("null", doc["missing"].as<String>().c_str());
}

static int runAllTests() {
    UNITY_BEGIN();
    RUN_TEST(test_a_missing_key_falls_back_to_the_default);
    RUN_TEST(test_a_present_string_wins_over_the_default);
    RUN_TEST(test_a_null_value_falls_back_to_the_default);
    RUN_TEST(test_a_number_is_not_a_string);
    RUN_TEST(test_as_string_serializes_what_is_not_a_string);
    return UNITY_END();
}

// The same cases on a board, where ArduinoJson's own String support answers.
#ifdef ARDUINO
void setup() { delay(2000); runAllTests(); }
void loop() {}
#else
int main(int, char**) { return runAllTests(); }
#endif
