// WifiComponent's constructor parameters must not shadow its members.
//
// -Wshadow is a compiler diagnostic, not a runtime behaviour, so the test
// compiles Wifi.h with g++ -Wshadow -fsyntax-only and reads what the compiler
// reports. It looks for the repository root and for ArduinoJson from the
// working directory the suite runs in (the component's PlatformIO project).

#include <unity.h>

#include <cstdio>
#include <string>
#include <vector>

void setUp(void) {}
void tearDown(void) {}

// Runs the compiler over a translation unit that only includes Wifi.h and
// returns everything it printed, followed by a line "EXIT:<status>".
static std::string compileWifiHeaderWithShadowWarnings() {
    const char* script = R"SH(
d="$PWD"
while [ ! -f "$d/DomoticsCore-Wifi/include/DomoticsCore/Wifi.h" ] && [ "$d" != "/" ]; do d=$(dirname "$d"); done
aj=$(find "$d/DomoticsCore-Wifi/.pio" "$d/.pio" "$d" -path '*ArduinoJson/src/ArduinoJson.h' 2>/dev/null | head -n 1)
aj=$(dirname "$aj")
printf '#include <DomoticsCore/Wifi.h>\n' | LC_ALL=C ${CXX:-g++} -std=gnu++17 -DCORE_DEBUG_LEVEL=3 \
    -Wshadow -fsyntax-only -x c++ \
    -I"$d/DomoticsCore-Core/include" -I"$d/DomoticsCore-Wifi/include" -I"$aj" - 2>&1
echo "EXIT:$?"
)SH";
    std::string out;
    FILE* pipe = popen(script, "r");
    if (!pipe) return out;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    pclose(pipe);
    return out;
}

static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

// Scenario "No shadow warning":
// Given Wifi.h,
// When it is compiled with -Wshadow,
// Then no warning is reported for ssid or password.
void test_wifi_header_reports_no_shadow_warning_for_ssid_or_password(void) {
    const std::string output = compileWifiHeaderWithShadowWarnings();

    // The compile itself has to have worked, or "no warning" proves nothing.
    TEST_ASSERT_TRUE_MESSAGE(output.find("EXIT:0") != std::string::npos, output.c_str());

    std::string offending;
    for (const std::string& line : splitLines(output)) {
        const bool inWifiH = line.find("Wifi.h:") != std::string::npos;
        const bool isShadow = line.find("-Wshadow") != std::string::npos;
        const bool named = line.find("'ssid'") != std::string::npos ||
                           line.find("'password'") != std::string::npos;
        if (inWifiH && isShadow && named) offending += line + "\n";
    }
    TEST_ASSERT_TRUE_MESSAGE(offending.empty(), offending.c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_header_reports_no_shadow_warning_for_ssid_or_password);
    return UNITY_END();
}
