/**
 * @file test_ntp_sync_interval_note.cpp
 * @brief The ESP8266 NTP backend ignores setSyncInterval(): the NTP context
 *        doc and the backend header must both say so.
 *
 * The tests read the two files from the repository; the program runs from the
 * component's directory, and the test file's own path gives a second way in.
 */

#include <unity.h>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Reads a repository file, given from the repository's root; empty when none
// of the places it may be found holds it.
std::string readRepoFile(const std::string& fromRoot) {
    std::vector<std::string> candidates;
    candidates.push_back("../" + fromRoot);
    candidates.push_back(fromRoot);
    std::string here = __FILE__;
    size_t slash = here.find_last_of('/');
    if (slash != std::string::npos) {
        // <component>/test/<suite>/<file>.cpp: three levels up is the root.
        candidates.push_back(here.substr(0, slash) + "/../../../" + fromRoot);
    }
    for (const auto& path : candidates) {
        std::ifstream in(path.c_str());
        if (in) {
            std::stringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }
    }
    return "";
}

}  // namespace

void setUp() {}
void tearDown() {}

// Given the NTP project-context doc,
// When I read the HAL::NTP section,
// Then it states that setSyncInterval has no effect on ESP8266.
void test_the_hal_ntp_section_states_set_sync_interval_has_no_effect_on_esp8266() {
    std::string doc = readRepoFile("docs/components/ntp/project-context.md");
    TEST_ASSERT_FALSE_MESSAGE(doc.empty(), "docs/components/ntp/project-context.md not found");

    const std::string heading = "### `DomoticsCore::HAL::NTP`";
    size_t start = doc.find(heading);
    TEST_ASSERT_TRUE_MESSAGE(start != std::string::npos, "no HAL::NTP section in the doc");
    size_t end = doc.size();
    size_t nextHeading = doc.find("\n### ", start + heading.size());
    size_t nextRule = doc.find("\n---", start + heading.size());
    if (nextHeading != std::string::npos && nextHeading < end) end = nextHeading;
    if (nextRule != std::string::npos && nextRule < end) end = nextRule;
    std::string section = lower(doc.substr(start, end - start));

    TEST_ASSERT_TRUE_MESSAGE(section.find("esp8266") != std::string::npos,
                             "the HAL::NTP section does not mention ESP8266");
    TEST_ASSERT_TRUE_MESSAGE(section.find("no effect") != std::string::npos,
                             "the HAL::NTP section does not say setSyncInterval has no effect");
}

// Given NTP_ESP8266.h,
// When I read setSyncInterval,
// Then a comment says the interval is ignored on this platform.
void test_the_esp8266_set_sync_interval_comment_says_the_interval_is_ignored() {
    std::string header = readRepoFile("DomoticsCore-NTP/include/DomoticsCore/NTP_ESP8266.h");
    TEST_ASSERT_FALSE_MESSAGE(header.empty(), "NTP_ESP8266.h not found");

    size_t fn = header.find("inline void setSyncInterval");
    TEST_ASSERT_TRUE_MESSAGE(fn != std::string::npos, "no setSyncInterval in NTP_ESP8266.h");
    size_t after = header.find("inline void stop", fn);
    TEST_ASSERT_TRUE_MESSAGE(after != std::string::npos, "no stop() after setSyncInterval");
    // From the previous function's end, so a comment above counts too.
    size_t before = header.rfind("inline time_t now", fn);
    if (before == std::string::npos) before = 0;

    std::istringstream lines(header.substr(before, after - before));
    std::string line;
    std::string comments;
    while (std::getline(lines, line)) {
        size_t c = line.find("//");
        if (c != std::string::npos) comments += line.substr(c) + "\n";
        size_t b = line.find("/*");
        if (b != std::string::npos) comments += line.substr(b) + "\n";
        else if (line.find_first_not_of(" \t") != std::string::npos &&
                 line[line.find_first_not_of(" \t")] == '*') comments += line + "\n";
    }
    comments = lower(comments);

    TEST_ASSERT_TRUE_MESSAGE(comments.find("ignored") != std::string::npos,
                             "no comment at setSyncInterval says the interval is ignored");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_hal_ntp_section_states_set_sync_interval_has_no_effect_on_esp8266);
    RUN_TEST(test_the_esp8266_set_sync_interval_comment_says_the_interval_is_ignored);
    return UNITY_END();
}
