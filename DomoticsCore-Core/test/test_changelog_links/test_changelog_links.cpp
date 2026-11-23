#include <unity.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

void setUp(void) {}
void tearDown(void) {}

// The suite runs from its component's directory (DomoticsCore-Core), the
// CHANGELOG sits at the repository's root.
static bool readChangelog(std::vector<std::string>& lines) {
    const char* candidates[] = {"../CHANGELOG.md", "CHANGELOG.md", "../../CHANGELOG.md",
                                "../../../CHANGELOG.md"};
    for (const char* path : candidates) {
        std::ifstream in(path);
        if (!in.is_open()) continue;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        return true;
    }
    return false;
}

// Given the CHANGELOG,
// When I click a version heading (`## [x.y.z] - date`),
// Then I reach that version's changes: the heading's `[x.y.z]` resolves to a
//      reference link definition `[x.y.z]: https://...` naming that version.
void test_every_version_heading_has_a_reference_link(void) {
    std::vector<std::string> lines;
    TEST_ASSERT_TRUE_MESSAGE(readChangelog(lines), "CHANGELOG.md not found");

    std::vector<std::string> versions;
    for (const std::string& l : lines) {
        if (l.compare(0, 4, "## [") != 0) continue;
        size_t close = l.find(']', 4);
        if (close == std::string::npos) continue;
        versions.push_back(l.substr(4, close - 4));
    }
    TEST_ASSERT_TRUE_MESSAGE(!versions.empty(), "no version heading found");

    std::string missing;
    for (const std::string& v : versions) {
        const std::string prefix = "[" + v + "]: ";
        bool found = false;
        for (const std::string& l : lines) {
            if (l.compare(0, prefix.size(), prefix) != 0) continue;
            const std::string url = l.substr(prefix.size());
            if (url.compare(0, 8, "https://") == 0 && url.find(v) != std::string::npos) {
                found = true;
                break;
            }
        }
        if (!found) missing += (missing.empty() ? "" : ", ") + v;
    }
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", missing.c_str(),
                                     "versions with no reference link definition");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_version_heading_has_a_reference_link);
    return UNITY_END();
}
