// SystemInfo.h log strings stay plain ASCII: a serial console or telnet client
// without UTF-8 shows garbage for an emoji (#167).

#include <unity.h>
#include <fstream>
#include <sstream>
#include <string>

void setUp(void) {}
void tearDown(void) {}

static bool readHeader(std::string& out) {
    const char* paths[] = {
        "include/DomoticsCore/SystemInfo.h",
        "DomoticsCore-SystemInfo/include/DomoticsCore/SystemInfo.h",
        "../DomoticsCore-SystemInfo/include/DomoticsCore/SystemInfo.h",
        "../../include/DomoticsCore/SystemInfo.h",
    };
    for (const char* p : paths) {
        std::ifstream f(p, std::ios::binary);
        if (f) { std::stringstream ss; ss << f.rdbuf(); out = ss.str(); return true; }
    }
    return false;
}

// Every DLOG_x(...) statement, from its macro name to its closing ';'.
void test_dlog_statements_hold_only_ascii_bytes() {
    std::string src;
    TEST_ASSERT_TRUE_MESSAGE(readHeader(src), "SystemInfo.h not found");
    int statements = 0;
    size_t pos = 0;
    while ((pos = src.find("DLOG_", pos)) != std::string::npos) {
        size_t end = src.find(';', pos);
        TEST_ASSERT_TRUE(end != std::string::npos);
        statements++;
        for (size_t i = pos; i < end; i++) {
            if (static_cast<unsigned char>(src[i]) > 0x7F) {
                std::string msg = "non-ASCII byte in a DLOG statement at offset " + std::to_string(i);
                TEST_FAIL_MESSAGE(msg.c_str());
            }
        }
        pos = end;
    }
    TEST_ASSERT_GREATER_THAN(0, statements);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_dlog_statements_hold_only_ascii_bytes);
    return UNITY_END();
}
