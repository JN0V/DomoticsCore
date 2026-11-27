// tools/local_ci.sh, step 3: the file size check counts code lines only.
//
// The script is run for real, in a scratch directory holding a copy of it and
// one generated .cpp file. Its other steps need PlatformIO, Python and the
// whole tree, so stand-in `python3` and `pio` commands that do nothing and
// succeed are put first on the PATH: the exit status is then step 3's alone.
#include <unity.h>

#include <cstdio>
#include <string>

void setUp(void) {}
void tearDown(void) {}

// Runs tools/local_ci.sh over a scratch tree whose only source file has the
// given number of code lines, blank lines and `//` comment lines. Returns all
// the script printed, followed by a line "EXIT:<status>".
static std::string runLocalCi(int codeLines, int blankLines, int commentLines) {
    const std::string hint = std::string(__FILE__);
    const std::string hintDir = hint.substr(0, hint.find_last_of('/') == std::string::npos ? 0 : hint.find_last_of('/'));
    char script[4096];
    snprintf(script, sizeof(script), R"SH(
d="$PWD"
while [ ! -f "$d/tools/local_ci.sh" ] && [ "$d" != "/" ] && [ "$d" != "." ]; do d=$(dirname "$d"); done
if [ ! -f "$d/tools/local_ci.sh" ]; then
  d="%s"
  while [ ! -f "$d/tools/local_ci.sh" ] && [ "$d" != "/" ] && [ "$d" != "." ]; do d=$(dirname "$d"); done
fi
t=$(mktemp -d)
mkdir -p "$t/tools" "$t/bin" "$t/src"
cp "$d/tools/local_ci.sh" "$t/tools/local_ci.sh"
printf '#!/bin/sh\nexit 0\n' > "$t/bin/python3"
printf '#!/bin/sh\nexit 0\n' > "$t/bin/pio"
chmod +x "$t/bin/python3" "$t/bin/pio"
awk -v code=%d -v blank=%d -v comment=%d 'BEGIN {
  for (i = 0; i < code; i++) print "int value" i " = " i ";"
  for (i = 0; i < blank; i++) print ""
  for (i = 0; i < comment; i++) print "    // note " i
}' > "$t/src/big.cpp"
( cd "$t" && PATH="$t/bin:$PATH" bash tools/local_ci.sh 2>&1 )
echo "EXIT:$?"
rm -rf "$t"
)SH", hintDir.c_str(), codeLines, blankLines, commentLines);

    std::string out;
    FILE* pipe = popen(script, "r");
    if (!pipe) return out;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    pclose(pipe);
    return out;
}

// Scenario "Comments and blanks ignored":
// Given a .cpp file with 700 code lines and 150 blank or comment lines,
// When I run tools/local_ci.sh,
// Then step 3 does not report the file as oversized.
void test_comments_and_blanks_are_not_counted_toward_the_size_limit(void) {
    const std::string out = runLocalCi(700, 75, 75);

    // The script has to have run through step 3, or "no report" proves nothing.
    TEST_ASSERT_TRUE_MESSAGE(out.find("3. Checking file sizes") != std::string::npos, out.c_str());
    TEST_ASSERT_TRUE_MESSAGE(out.find("big.cpp") == std::string::npos, out.c_str());
    TEST_ASSERT_TRUE_MESSAGE(out.find("exceed 800 lines") == std::string::npos, out.c_str());
    TEST_ASSERT_TRUE_MESSAGE(out.find("All files under 800 lines") != std::string::npos, out.c_str());
    TEST_ASSERT_TRUE_MESSAGE(out.find("EXIT:0") != std::string::npos, out.c_str());
}

// Scenario "Real oversize still caught":
// Given a .cpp file with 801 code lines,
// When I run tools/local_ci.sh,
// Then step 3 reports it as exceeding 800 lines and the script exits with 1.
void test_a_file_with_801_code_lines_is_reported_and_the_script_exits_1(void) {
    const std::string out = runLocalCi(801, 0, 0);

    TEST_ASSERT_TRUE_MESSAGE(out.find("3. Checking file sizes") != std::string::npos, out.c_str());
    TEST_ASSERT_TRUE_MESSAGE(out.find("big.cpp") != std::string::npos, out.c_str());
    TEST_ASSERT_TRUE_MESSAGE(out.find("exceed 800 lines") != std::string::npos, out.c_str());
    TEST_ASSERT_TRUE_MESSAGE(out.find("EXIT:1") != std::string::npos, out.c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_comments_and_blanks_are_not_counted_toward_the_size_limit);
    RUN_TEST(test_a_file_with_801_code_lines_is_reported_and_the_script_exits_1);
    return UNITY_END();
}
