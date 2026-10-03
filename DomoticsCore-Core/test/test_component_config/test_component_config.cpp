// ComponentStatus and ComponentMetadata, and the decimal rule user input is held to.
// ComponentConfig.h is included first on purpose: the header must stand alone.
#include <DomoticsCore/ComponentConfig.h>
#include <DomoticsCore/StringParse.h>
#include <unity.h>

using namespace DomoticsCore::Components;
using DomoticsCore::Utils::digitsOnly;

void setUp(void) {}
void tearDown(void) {}

void test_status_to_string_names_all_nine_and_the_unknown(void) {
    TEST_ASSERT_EQUAL_STRING("Success", statusToString(ComponentStatus::Success));
    TEST_ASSERT_EQUAL_STRING("Configuration Error", statusToString(ComponentStatus::ConfigError));
    TEST_ASSERT_EQUAL_STRING("Hardware Error", statusToString(ComponentStatus::HardwareError));
    TEST_ASSERT_EQUAL_STRING("Dependency Error", statusToString(ComponentStatus::DependencyError));
    TEST_ASSERT_EQUAL_STRING("Network Error", statusToString(ComponentStatus::NetworkError));
    TEST_ASSERT_EQUAL_STRING("Memory Error", statusToString(ComponentStatus::MemoryError));
    TEST_ASSERT_EQUAL_STRING("Timeout Error", statusToString(ComponentStatus::TimeoutError));
    TEST_ASSERT_EQUAL_STRING("Invalid State", statusToString(ComponentStatus::InvalidState));
    TEST_ASSERT_EQUAL_STRING("Not Supported", statusToString(ComponentStatus::NotSupported));
    TEST_ASSERT_EQUAL_STRING("Unknown Error", statusToString(static_cast<ComponentStatus>(99)));
}

void test_component_metadata_defaults_and_constructor(void) {
    ComponentMetadata d;
    TEST_ASSERT_EQUAL_STRING("", d.name);
    TEST_ASSERT_EQUAL_STRING("1.0.0", d.version);
    TEST_ASSERT_EQUAL_STRING("", d.author);
    TEST_ASSERT_EQUAL_STRING("", d.description);
    TEST_ASSERT_EQUAL_STRING("", d.category);
    TEST_ASSERT_TRUE(d.tags.empty());
    ComponentMetadata m("LED", "2.1.0", "me", "blinks");
    TEST_ASSERT_EQUAL_STRING("LED", m.name);
    TEST_ASSERT_EQUAL_STRING("2.1.0", m.version);
    TEST_ASSERT_EQUAL_STRING("me", m.author);
    TEST_ASSERT_EQUAL_STRING("blinks", m.description);
}

// toInt() would read every one of the refused strings as some number.
void test_digits_only_takes_decimal_digits_and_nothing_else(void) {
    const char* good[] = {"0", "0080", "1234567890"};
    for (const char* v : good) TEST_ASSERT_TRUE_MESSAGE(digitsOnly(v), v);
    const char* bad[] = {"", "80x", "+80", "-0", " 80", "80 ", "abc", "1.5", "0x10"};
    for (const char* v : bad) TEST_ASSERT_FALSE_MESSAGE(digitsOnly(v), v);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_status_to_string_names_all_nine_and_the_unknown);
    RUN_TEST(test_component_metadata_defaults_and_constructor);
    RUN_TEST(test_digits_only_takes_decimal_digits_and_nothing_else);
    return UNITY_END();
}
