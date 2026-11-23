// Each alone_*.cpp is a translation unit that includes one HA entity header and
// nothing else (LO-23): the suite builds only if every header carries its own
// Logger.h include, whatever the include order.
#include <unity.h>

int ha_header_alone_HALight();
int ha_header_alone_HAButton();
int ha_header_alone_HAAlarmControlPanel();

void setUp() {}
void tearDown() {}

void test_headers_compile_alone() {
    TEST_ASSERT_EQUAL_INT(1, ha_header_alone_HALight());
    TEST_ASSERT_EQUAL_INT(1, ha_header_alone_HAButton());
    TEST_ASSERT_EQUAL_INT(1, ha_header_alone_HAAlarmControlPanel());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_headers_compile_alone);
    return UNITY_END();
}
