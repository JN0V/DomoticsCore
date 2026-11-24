#include <unity.h>
#include <dirent.h>
#include <sys/stat.h>
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

// The repository's root examples/ folder, found from the project directory
// (pio test runs there) or from this file's own location.
static std::string rootExamplesDir() {
    std::string here = __FILE__;
    std::string fromFile = here.substr(0, here.find_last_of('/') == std::string::npos ? 0 : here.find_last_of('/'));
    const std::vector<std::string> candidates = {
        "../examples",
        fromFile + "/../../../examples",
    };
    for (const std::string& c : candidates) {
        if (isDirectory(c) && isRegularFile(c + "/README.md")) return c;
    }
    return "";
}

// Given the root examples/ folder,
// When the library is opened in the Arduino Library Manager,
// Then at least one example is listed: the Library Manager lists a folder of
// examples/ that holds a sketch of the same name, <name>/<name>.ino.
void test_root_examples_lists_at_least_one_example(void) {
    const std::string dir = rootExamplesDir();
    TEST_ASSERT_FALSE_MESSAGE(dir.empty(), "the root examples/ folder was not found");

    int listed = 0;
    DIR* d = opendir(dir.c_str());
    TEST_ASSERT_NOT_NULL_MESSAGE(d, "the root examples/ folder cannot be read");
    while (struct dirent* e = readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        const std::string folder = dir + "/" + name;
        if (isDirectory(folder) && isRegularFile(folder + "/" + name + ".ino")) ++listed;
    }
    closedir(d);

    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(
        1, listed, "examples/ holds no <name>/<name>.ino sketch for the Library Manager to list");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_root_examples_lists_at_least_one_example);
    return UNITY_END();
}
