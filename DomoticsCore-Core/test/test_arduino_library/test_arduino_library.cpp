// The Arduino IDE builds a library from one src/ at its root. This repository
// keeps its modular layout, so tools/make_arduino_library.py builds the copy
// the Arduino registry ships. The test builds that copy and looks for what
// a sketch that includes <DomoticsCore/System.h> needs to compile.
#include <unity.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

void setUp(void) {}
void tearDown(void) {}

static bool isDirectory(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool isRegularFile(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// The repository's root, found from the project directory (pio test runs
// there) or from this file's own location.
static std::string repoRoot() {
    std::string here = __FILE__;
    std::string fromFile = here.substr(0, here.find_last_of('/') == std::string::npos ? 0 : here.find_last_of('/'));
    const std::vector<std::string> candidates = {
        "..",
        fromFile + "/../../..",
    };
    for (const std::string& c : candidates) {
        if (isDirectory(c + "/DomoticsCore-Core") && isRegularFile(c + "/library.json")) return c;
    }
    return "";
}

// Given the release archive installed in the Arduino IDE,
// When a sketch includes DomoticsCore/System.h,
// Then it compiles: the copy the generator builds holds one src/ in which
// System.h, every header it includes without a __has_include guard, the
// umbrella header DomoticsCore.h and the generated WebUIAssets.h are all
// reachable as <DomoticsCore/X.h> from the src/ folder.
void test_arduino_copy_has_one_src_where_system_h_builds(void) {
    const std::string root = repoRoot();
    TEST_ASSERT_FALSE_MESSAGE(root.empty(), "the repository root was not found");
    TEST_ASSERT_TRUE_MESSAGE(isRegularFile(root + "/tools/make_arduino_library.py"),
                             "tools/make_arduino_library.py does not exist");

    char tmpl[] = "/tmp/domotics_arduino_lib_XXXXXX";
    char* made = mkdtemp(tmpl);
    TEST_ASSERT_NOT_NULL_MESSAGE(made, "no temporary folder for the copy");
    const std::string tmp = made;
    const std::string out = tmp + "/DomoticsCore";

    const std::string run = "cd '" + root + "' && python3 tools/make_arduino_library.py '" + out + "' > '" +
                            tmp + "/generator.log' 2>&1";
    const int status = std::system(run.c_str());

    const std::string src = out + "/src";
    const std::vector<std::string> headers = {
        "DomoticsCore.h",
        "DomoticsCore/System.h",
        "DomoticsCore/SystemConfig.h",
        "DomoticsCore/SystemPersistence.h",
        "DomoticsCore/SystemWebUISetup.h",
        "DomoticsCore/SystemTelemetry.h",
        "DomoticsCore/SystemTelemetrySetup.h",
        "DomoticsCore/Core.h",
        "DomoticsCore/Logger.h",
        "DomoticsCore/Events.h",
        "DomoticsCore/Platform_HAL.h",
        "DomoticsCore/LED.h",
        "DomoticsCore/RemoteConsole.h",
        "DomoticsCore/Wifi.h",
        "DomoticsCore/WebUI.h",
        "DomoticsCore/Generated/WebUIAssets.h",
    };
    std::string missing;
    for (const std::string& h : headers) {
        if (!isRegularFile(src + "/" + h)) missing += " " + h;
    }
    // The root of the copy holds no second src/ tree beside the one.
    const bool nested = isDirectory(src + "/src");

    const std::string cleanup = "rm -rf '" + tmp + "'";
    std::system(cleanup.c_str());

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, status, "tools/make_arduino_library.py <output folder> failed");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", missing.c_str(), "headers missing from the copy's src/");
    TEST_ASSERT_FALSE_MESSAGE(nested, "the copy holds a src/src folder");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_arduino_copy_has_one_src_where_system_h_builds);
    return UNITY_END();
}
