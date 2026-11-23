/**
 * @file test_webui_polling_counter.cpp
 * @brief The WebUI polling client counter is safe to use from several tasks.
 *
 * The counter is read from the source text of the WebSocket handler header: a
 * plain `volatile int` gives no atomicity, so `onPollRequest()` and `loop()`
 * running on different tasks could miscount it.
 */

#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

namespace {

const char* const kHeader = "DomoticsCore/WebUI/WebSocketHandler.h";

/** Reads the header from wherever the runner's working directory puts it. */
bool readHeader(std::string& out) {
    const std::string rel = std::string("include/") + kHeader;
    const std::string here = __FILE__;
    const std::string fromSource = here.substr(0, here.find_last_of("/\\") + 1) + "../../../" + rel;
    const std::string candidates[] = {
        "../../" + rel,                    // project directory of this suite
        "../../../" + rel,
        "DomoticsCore-WebUI/" + rel,       // repository root
        rel,                               // component directory
        fromSource,
    };
    for (const std::string& path : candidates) {
        std::ifstream in(path.c_str());
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        out = ss.str();
        return true;
    }
    return false;
}

/** The first line naming `pollingClients`: its declaration. */
std::string declarationLine(const std::string& source) {
    const std::string::size_type at = source.find("pollingClients");
    if (at == std::string::npos) return "";
    const std::string::size_type start = source.rfind('\n', at);
    const std::string::size_type end = source.find('\n', at);
    return source.substr(start == std::string::npos ? 0 : start + 1,
                         end == std::string::npos ? std::string::npos
                                                  : end - (start == std::string::npos ? 0 : start + 1));
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// Counter type: Given the WebUI source, When I read the pollingClients
// declaration, Then it is not a plain volatile int.
void test_polling_clients_is_not_a_plain_volatile_int(void) {
    std::string source;
    TEST_ASSERT_TRUE_MESSAGE(readHeader(source), "WebSocketHandler.h could not be read");

    const std::string line = declarationLine(source);
    TEST_ASSERT_TRUE_MESSAGE(!line.empty(), "no pollingClients declaration found");
    TEST_ASSERT_TRUE_MESSAGE(line.find("volatile int") == std::string::npos,
                             "pollingClients is still declared as a plain volatile int");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_polling_clients_is_not_a_plain_volatile_int);
    return UNITY_END();
}
