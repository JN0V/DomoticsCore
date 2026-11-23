// The library's field names must stay out of the identifiers the C++ standard
// reserves for the compiler and the standard library: a name holding a double
// underscore. The Core headers are read as text and searched.
#include <unity.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <vector>

void setUp(void) {}
void tearDown(void) {}

static bool isDirectory(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Every regular file under dir, subdirectories included.
static void listFiles(const std::string& dir, std::vector<std::string>& out) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    while (struct dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string full = dir + "/" + name;
        if (isDirectory(full)) listFiles(full, out);
        else out.push_back(full);
    }
    closedir(d);
}

// The Core headers' directory, found from the test's working directory or its own path.
static std::string coreIncludeDir() {
    std::vector<std::string> candidates = {
        "include/DomoticsCore",
        "DomoticsCore-Core/include/DomoticsCore",
    };
    std::string self = __FILE__;
    size_t slash = self.find_last_of('/');
    if (slash != std::string::npos) {
        std::string here = self.substr(0, slash);
        candidates.push_back(here + "/../../include/DomoticsCore");
        candidates.push_back(here + "/../../DomoticsCore-Core/include/DomoticsCore");
    }
    for (const std::string& c : candidates) {
        if (isDirectory(c)) return c;
    }
    return "";
}

// Given the Core headers,
// When I search them for names starting with the reserved double-underscore "dc" prefix,
// Then none is found.
void test_core_headers_hold_no_reserved_dc_names(void) {
    const std::string dir = coreIncludeDir();
    TEST_ASSERT_FALSE_MESSAGE(dir.empty(), "the Core include directory was not found");

    std::vector<std::string> files;
    listFiles(dir, files);
    TEST_ASSERT_TRUE_MESSAGE(files.size() > 10, "too few Core headers found to search");

    // Built in two pieces so this file never holds the name itself.
    const std::string needle = std::string("__") + "dc_";

    std::string found;
    size_t hits = 0;
    for (const std::string& path : files) {
        std::ifstream in(path);
        std::string line;
        size_t lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            if (line.find(needle) == std::string::npos) continue;
            ++hits;
            if (hits <= 3) {
                found += path + ":" + std::to_string(lineNo) + " ";
            }
        }
    }
    std::string message = "reserved names found, first at: " + found;
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, hits, message.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_core_headers_hold_no_reserved_dc_names);
    return UNITY_END();
}
