/**
 * @file test_generated_header.cpp
 * @brief No native suite can see the generated WebUIAssets.h before the mock.
 *
 * embed_webui.py writes DomoticsCore/Generated/WebUIAssets.h into
 * DomoticsCore-WebUI/include and the file is gitignored. A native manifest
 * that puts that include directory on its path without the mock directory
 * ahead of it builds against the generated file where one is left over from
 * a WebUI example build, and fails on a clean checkout where there is none.
 * The manifests are read as they are tracked.
 */

#include <unity.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

const char* const MOCK_HEADER = "tests/mocks/libraries/DomoticsCore/Generated/WebUIAssets.h";

struct Manifest {
    std::string name;               // from the repository's root
    std::vector<fs::path> includes; // -I directories of [env:native], in order
    std::vector<fs::path> deps;     // symlink:// targets of [env:native]
};

std::string trim(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

fs::path normal(const fs::path& p) {
    std::error_code ec;
    fs::path out = fs::weakly_canonical(p, ec);
    if (ec) out = p.lexically_normal();
    if (!out.has_filename()) out = out.parent_path();
    return out;
}

/** The repository's root: the first directory up from here holding the mock. */
fs::path repoRoot() {
    std::error_code ec;
    fs::path dir = fs::current_path(ec);
    while (!dir.empty()) {
        if (fs::exists(dir / MOCK_HEADER, ec)) return dir;
        if (dir == dir.parent_path()) break;
        dir = dir.parent_path();
    }
    return fs::path();
}

void addFlags(const fs::path& dir, const std::string& text, Manifest& out) {
    std::istringstream words(text);
    std::string word;
    bool wantsPath = false;
    while (words >> word) {
        if (wantsPath) {
            out.includes.push_back(normal(dir / word));
            wantsPath = false;
        } else if (word == "-I") {
            wantsPath = true;
        } else if (word.rfind("-I", 0) == 0) {
            out.includes.push_back(normal(dir / word.substr(2)));
        }
    }
}

/** Reads [env:native]'s build_flags and lib_deps; false when there is no such environment. */
bool readNative(const fs::path& ini, Manifest& out) {
    std::ifstream in(ini);
    if (!in) return false;
    const fs::path dir = ini.parent_path();
    enum class Key { None, Flags, Deps };
    Key key = Key::None;
    bool inNative = false;
    bool seenNative = false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') continue;
        const bool indented = line[0] == ' ' || line[0] == '\t';
        std::string value = t;
        if (!indented) {
            key = Key::None;
            if (t[0] == '[') {
                inNative = (t == "[env:native]");
                seenNative = seenNative || inNative;
                continue;
            }
            if (!inNative) continue;
            const size_t eq = t.find('=');
            if (eq == std::string::npos) continue;
            const std::string name = trim(t.substr(0, eq));
            if (name == "build_flags") key = Key::Flags;
            else if (name == "lib_deps") key = Key::Deps;
            else continue;
            value = trim(t.substr(eq + 1));
        }
        if (!inNative || key == Key::None || value.empty()) continue;
        if (key == Key::Flags) {
            addFlags(dir, value, out);
        } else if (value.rfind("symlink://", 0) == 0) {
            out.deps.push_back(normal(dir / trim(value.substr(10))));
        }
    }
    return seenNative;
}

/** Every tracked-style manifest with a native environment, examples and build output left out. */
std::vector<Manifest> nativeManifests(const fs::path& root) {
    std::vector<Manifest> found;
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
        const std::string leaf = it->path().filename().string();
        if (it->is_directory(ec)) {
            if (leaf == ".pio" || leaf == ".git" || leaf == "node_modules" || leaf == "examples") {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (leaf != "platformio.ini") continue;
        Manifest m;
        if (!readNative(it->path(), m)) continue;
        m.name = fs::relative(it->path(), root, ec).generic_string();
        found.push_back(m);
    }
    return found;
}

std::vector<Manifest> loadManifests(fs::path& root) {
    root = repoRoot();
    TEST_ASSERT_FALSE_MESSAGE(root.empty(), "the repository's root was not found from the test's working directory");
    std::vector<Manifest> all = nativeManifests(normal(root));
    TEST_ASSERT_TRUE_MESSAGE(all.size() > 5, "fewer than six native manifests were found: the walk is wrong");
    return all;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// Given every native manifest of the repository, When the ones that reach the
// WebUI include directory or the WebUI package are read, Then each one carries
// the mock directory on its include path, so a clean checkout compiles.
void test_every_native_manifest_that_reaches_webui_carries_the_mock(void) {
    fs::path root;
    const std::vector<Manifest> all = loadManifests(root);
    const fs::path mockDir = normal(root / "tests/mocks/libraries");
    const fs::path webuiDir = normal(root / "DomoticsCore-WebUI");
    const fs::path webuiInclude = normal(root / "DomoticsCore-WebUI/include");

    std::string offenders;
    for (const Manifest& m : all) {
        bool reaches = false;
        bool hasMock = false;
        for (const fs::path& p : m.includes) {
            reaches = reaches || p == webuiInclude;
            hasMock = hasMock || p == mockDir;
        }
        for (const fs::path& p : m.deps) reaches = reaches || p == webuiDir;
        if (reaches && !hasMock) offenders += "\n  " + m.name;
    }
    const std::string message =
        "native manifests that reach the WebUI package without tests/mocks/libraries on their include path:" + offenders;
    TEST_ASSERT_TRUE_MESSAGE(offenders.empty(), message.c_str());
}

// Given a generated WebUIAssets.h left by an earlier WebUI example build, When a
// native suite includes DomoticsCore/Generated/WebUIAssets.h, Then it gets the
// mock: in every native manifest the mock directory comes before the WebUI
// include directory, where the generated file lives.
void test_the_mock_comes_before_the_webui_include_in_every_native_manifest(void) {
    fs::path root;
    const std::vector<Manifest> all = loadManifests(root);
    const fs::path mockDir = normal(root / "tests/mocks/libraries");
    const fs::path webuiInclude = normal(root / "DomoticsCore-WebUI/include");

    std::string offenders;
    for (const Manifest& m : all) {
        size_t webuiAt = m.includes.size();
        size_t mockAt = m.includes.size();
        for (size_t i = 0; i < m.includes.size(); i++) {
            if (m.includes[i] == webuiInclude && webuiAt == m.includes.size()) webuiAt = i;
            if (m.includes[i] == mockDir && mockAt == m.includes.size()) mockAt = i;
        }
        if (webuiAt == m.includes.size()) continue;  // never reaches the generated header
        if (mockAt > webuiAt) offenders += "\n  " + m.name;
    }
    const std::string message =
        "native manifests whose include path reaches DomoticsCore-WebUI/include before the mock:" + offenders;
    TEST_ASSERT_TRUE_MESSAGE(offenders.empty(), message.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_native_manifest_that_reaches_webui_carries_the_mock);
    RUN_TEST(test_the_mock_comes_before_the_webui_include_in_every_native_manifest);
    return UNITY_END();
}
