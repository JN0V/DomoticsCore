/**
 * @file test_espressif32_platform.cpp
 * @brief The ESP32 platform pin: every ESP32 build the project declares runs
 *        on espressif32 8.x.
 *
 * The builds themselves need a toolchain and run in CI. What a person changes
 * to move them is the `platform` line of each environment, and that is what is
 * read here, from the files that CI builds.
 */

#include <unity.h>
#include <fstream>
#include <sstream>
#include <string>

void setUp(void) {}
void tearDown(void) {}

// The repository root, relative to where the suite runs: the project
// directory of DomoticsCore-System, one level below it.
static const std::string ROOT = []() {
    for (const char* prefix : {"", "../", "../../", "../../../"}) {
        std::ifstream probe(std::string(prefix) + ".github/workflows/test-github-install.yml");
        if (probe) return std::string(prefix);
    }
    return std::string();
}();

static std::string slurp(const std::string& path) {
    std::ifstream in(ROOT + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// True when the [env:<env>] section of `text` (the whole text when env is
// empty) has at least one `platform = espressif32@...` line, and every such
// line pins version 8 (8.x, ^8.0.0, 8.1.0, ...).
static bool onEspressif32_8(const std::string& text, const std::string& env) {
    size_t from = 0, to = text.size();
    if (!env.empty()) {
        from = text.find("[env:" + env + "]");
        if (from == std::string::npos) return false;
        to = text.find("[env:", from + 1);
        if (to == std::string::npos) to = text.size();
    }
    int found = 0;
    std::istringstream in(text.substr(from, to - from));
    std::string line;
    while (std::getline(in, line)) {
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos || line.compare(a, 8, "platform") != 0) continue;
        size_t eq = line.find_first_not_of(" \t", a + 8);
        if (eq == std::string::npos || line[eq] != '=') continue;
        size_t v = line.find_first_not_of(" \t", eq + 1);
        if (v == std::string::npos || line.compare(v, 11, "espressif32") != 0) continue;
        found++;
        size_t at = line.find('@', v);
        if (at == std::string::npos) return false;
        size_t d = line.find_first_not_of("^~>= ", at + 1);
        if (d == std::string::npos || line[d] != '8') return false;
        if (d + 1 < line.size() && line[d + 1] >= '0' && line[d + 1] <= '9') return false;
    }
    return found > 0;
}

/**
 * Scenario "FullStack fits":
 * Given espressif32 8.x,
 * When the FullStack example is built for esp32dev and esp32c3,
 * Then the build passes and the image fits its partition: both environments
 * pin espressif32 8.x and name the partition table the image is sized against.
 */
void test_fullstack_builds_for_esp32dev_and_esp32c3_on_espressif32_8(void) {
    const std::string ini = slurp("DomoticsCore-System/examples/FullStack/platformio.ini");
    TEST_ASSERT_FALSE_MESSAGE(ini.empty(), "FullStack platformio.ini not found");

    TEST_ASSERT_TRUE_MESSAGE(onEspressif32_8(ini, "esp32dev"),
                             "FullStack [env:esp32dev] is not on espressif32 8.x");
    TEST_ASSERT_TRUE_MESSAGE(onEspressif32_8(ini, "esp32c3"),
                             "FullStack [env:esp32c3] is not on espressif32 8.x");

    // The partition table is chosen explicitly in each ESP32 environment.
    const size_t dev = ini.find("[env:esp32dev]");
    const size_t c3 = ini.find("[env:esp32c3]");
    const std::string devSection = ini.substr(dev, ini.find("[env:", dev + 1) - dev);
    const std::string c3Section = ini.substr(c3);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(std::string::npos, devSection.find("board_build.partitions"),
                                  "esp32dev names no partition table");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(std::string::npos, c3Section.find("board_build.partitions"),
                                  "esp32c3 names no partition table");
}

/**
 * Scenario "Install test fits":
 * Given espressif32 8.x,
 * When a new project installs the library from GitHub and builds for esp32dev,
 * Then the build passes: the project the install test generates pins
 * espressif32 8.x for esp32dev.
 */
void test_install_test_project_builds_for_esp32dev_on_espressif32_8(void) {
    const std::string wf = slurp(".github/workflows/test-github-install.yml");
    TEST_ASSERT_FALSE_MESSAGE(wf.empty(), "test-github-install.yml not found");
    TEST_ASSERT_TRUE_MESSAGE(onEspressif32_8(wf, "esp32dev"),
                             "the install test's [env:esp32dev] is not on espressif32 8.x");
}

/**
 * Scenario "On-device suites build":
 * Given espressif32 8.x,
 * When the on-device suites are built for esp32dev and esp32cam,
 * Then each build passes: every ESP32 environment of those suites pins
 * espressif32 8.x.
 */
void test_on_device_suites_build_for_esp32dev_and_esp32cam_on_espressif32_8(void) {
    struct Suite { const char* file; const char* env; };
    const Suite suites[] = {
        {"DomoticsCore-HomeAssistant/platformio.ini", "esp32dev"},
        {"DomoticsCore-RemoteConsole/platformio.ini", "esp32dev"},
        {"DomoticsCore-OTA/platformio.ini", "esp32cam"},
        {"tools/on-device/probes/obs-probe/platformio-esp32.ini", "esp32dev"},
        {"tools/on-device/probes/obs-probe/platformio-esp32cam.ini", "esp32cam"},
        {"tools/on-device/probes/allocator-shape/platformio.ini", "esp32dev"},
        {"tools/on-device/probes/obs-loopmax/platformio.ini", "esp32dev"},
        {"tools/on-device/probes/webui-buffer-race/platformio.ini", "esp32dev"},
    };
    for (const Suite& s : suites) {
        const std::string text = slurp(s.file);
        TEST_ASSERT_FALSE_MESSAGE(text.empty(), s.file);
        TEST_ASSERT_TRUE_MESSAGE(onEspressif32_8(text, s.env), s.file);
    }
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_fullstack_builds_for_esp32dev_and_esp32c3_on_espressif32_8);
    RUN_TEST(test_install_test_project_builds_for_esp32dev_on_espressif32_8);
    RUN_TEST(test_on_device_suites_build_for_esp32dev_and_esp32cam_on_espressif32_8);

    return UNITY_END();
}
