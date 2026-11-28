#include <unity.h>

#include <sys/stat.h>

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

void setUp(void) {}
void tearDown(void) {}

namespace {

// The suite runs from its component's directory, one level under the repository root.
// Find the root by the CHANGELOG, whichever directory the runner starts the program in.
bool findRoot(std::string& prefix, std::string& text) {
    const char* candidates[] = {"", "../", "../../", "../../../"};
    for (const char* c : candidates) {
        std::ifstream in(std::string(c) + "CHANGELOG.md", std::ios::binary);
        if (!in) continue;
        std::ostringstream buf;
        buf << in.rdbuf();
        prefix = c;
        text = buf.str();
        return true;
    }
    return false;
}

bool isPathChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.' || ch == '/' || ch == '-';
}

// Every path the text names under docs/, as written (a trailing sentence dot dropped).
std::set<std::string> docsPaths(const std::string& text) {
    std::set<std::string> paths;
    const std::string needle = "docs/";
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        const bool startsPath = pos == 0 || !isPathChar(text[pos - 1]);
        size_t end = pos;
        while (end < text.size() && isPathChar(text[end])) ++end;
        if (startsPath) {
            std::string path = text.substr(pos, end - pos);
            while (!path.empty() && path.back() == '.') path.pop_back();
            paths.insert(path);
        }
        pos = end > pos ? end : pos + 1;
    }
    return paths;
}

bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

}  // namespace

// Given the CHANGELOG, When I follow each path it names under docs/,
// Then every one exists.
void test_every_docs_path_the_changelog_names_exists(void) {
    std::string prefix, text;
    TEST_ASSERT_TRUE_MESSAGE(findRoot(prefix, text), "CHANGELOG.md was not found from the test's directory");

    const std::set<std::string> paths = docsPaths(text);
    TEST_ASSERT_TRUE_MESSAGE(!paths.empty(), "the CHANGELOG names no path under docs/");

    std::string missing;
    for (const std::string& p : paths) {
        if (!exists(prefix + p)) missing += " " + p;
    }
    const std::string message = "the CHANGELOG names paths under docs/ that do not exist:" + missing;
    TEST_ASSERT_TRUE_MESSAGE(missing.empty(), message.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_docs_path_the_changelog_names_exists);
    return UNITY_END();
}
