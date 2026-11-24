/**
 * @file test_ntp_timezone_once.cpp
 * @brief The timezone table of the NTP web page is one object in the program,
 *        however many source files include NTPWebUI.h.
 *
 * This suite is two translation units: this one, and tz_other_unit.cpp,
 * which includes the same header and hands back the address of the table
 * it sees. One table in the linked image means the same address from both.
 */

#include <unity.h>

#include <DomoticsCore/NTP.h>
#include <DomoticsCore/NTPWebUI.h>

using namespace DomoticsCore::Components;

// Defined in tz_other_unit.cpp.
const void* timezoneTableSeenByTheOtherUnit();

void setUp() {}
void tearDown() {}

// Given two source files that include the NTP web page header,
// When the address of the timezone table is taken in each,
// Then both are the same address: the table is in the image once.
void test_the_timezone_table_exists_once_across_translation_units() {
    const void* here = &WebUI::TIMEZONE_LOOKUP[0];
    const void* there = timezoneTableSeenByTheOtherUnit();
    TEST_ASSERT_NOT_NULL(there);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(here, there,
                                  "each source file including NTPWebUI.h carries its own copy of the timezone table");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_timezone_table_exists_once_across_translation_units);
    return UNITY_END();
}
