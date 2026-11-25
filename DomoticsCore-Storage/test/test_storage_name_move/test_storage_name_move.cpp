// Regression test for issue #171: a StorageComponent built with a custom componentName must still report that name in metadata.name after it is copied or moved, once the source object is destroyed.
#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Storage.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

void setUp(void) {}
void tearDown(void) {}

static StorageConfig named() {
    StorageConfig c;
    c.componentName = "short-name";
    return c;
}

void test_copy_keeps_name() {
    StorageComponent* a = new StorageComponent(named());
    StorageComponent b(*a);
    TEST_ASSERT_TRUE(b.metadata.name != a->metadata.name);
    delete a;
    TEST_ASSERT_EQUAL_STRING("short-name", b.metadata.name);
}

void test_move_keeps_name() {
    StorageComponent* a = new StorageComponent(named());
    StorageComponent b(std::move(*a));
    TEST_ASSERT_TRUE(b.metadata.name != a->metadata.name);
    delete a;
    TEST_ASSERT_EQUAL_STRING("short-name", b.metadata.name);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_copy_keeps_name);
    RUN_TEST(test_move_keeps_name);
    return UNITY_END();
}
