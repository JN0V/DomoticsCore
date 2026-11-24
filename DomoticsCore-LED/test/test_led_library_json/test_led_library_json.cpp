/**
 * @file test_led_library_json.cpp
 * @brief The LED library manifest's dependency on Core.
 *
 * The manifest is read as text: the test finds the "version" given for the
 * DomoticsCore-Core dependency and checks the style of its constraint.
 */

#include <unity.h>
#include <fstream>
#include <sstream>
#include <string>

void setUp(void) {}
void tearDown(void) {}

// The manifest's text, wherever the runner's working directory is: the LED
// project's root, the repository's root, or the test file's own place.
static std::string readManifest() {
    std::string here = __FILE__;
    std::string fromFile;
    size_t slash = here.find_last_of("/\\");
    if (slash != std::string::npos) {
        fromFile = here.substr(0, slash) + "/../../library.json";
    }
    const char* candidates[] = {
        "library.json",
        "DomoticsCore-LED/library.json",
        "../DomoticsCore-LED/library.json",
    };
    std::ifstream in;
    for (const char* path : candidates) {
        in.open(path);
        if (in.good()) break;
        in.close();
        in.clear();
    }
    if (!in.is_open() && !fromFile.empty()) {
        in.open(fromFile);
    }
    std::stringstream buf;
    if (in.is_open()) buf << in.rdbuf();
    return buf.str();
}

// The constraint written for DomoticsCore-Core in the manifest's dependencies.
static std::string coreConstraint(const std::string& manifest) {
    size_t deps = manifest.find("\"dependencies\"");
    if (deps == std::string::npos) return "";
    size_t core = manifest.find("\"DomoticsCore-Core\"", deps);
    if (core == std::string::npos) return "";
    size_t key = manifest.find("\"version\"", core);
    if (key == std::string::npos) return "";
    size_t colon = manifest.find(':', key);
    if (colon == std::string::npos) return "";
    size_t open = manifest.find('"', colon);
    if (open == std::string::npos) return "";
    size_t close = manifest.find('"', open + 1);
    if (close == std::string::npos) return "";
    return manifest.substr(open + 1, close - open - 1);
}

// Scenario: Constraint style
// Given the LED library.json,
// When I read its dependency,
// Then it uses >= like the other components.
void test_core_dependency_uses_a_minimum_constraint(void) {
    std::string manifest = readManifest();
    TEST_ASSERT_FALSE_MESSAGE(manifest.empty(), "LED library.json could not be read");

    std::string constraint = coreConstraint(manifest);
    TEST_ASSERT_FALSE_MESSAGE(constraint.empty(), "no version found for the DomoticsCore-Core dependency");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, (int)constraint.find(">="),
                                  "the Core dependency does not start with >=");
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, (int)constraint.find('^'),
                                  "the Core dependency still carries a ^ constraint");
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_core_dependency_uses_a_minimum_constraint);
    return UNITY_END();
}
