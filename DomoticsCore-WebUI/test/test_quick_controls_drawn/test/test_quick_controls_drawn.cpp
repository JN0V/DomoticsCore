/**
 * @file test_quick_controls_drawn.cpp
 * @brief The page's front end draws the QuickControls location.
 */

#include <unity.h>
#include <fstream>
#include <sstream>
#include <string>

void setUp(void) {}
void tearDown(void) {}

// The front end is read from where the suite runs (its project folder), or,
// failing that, from beside this file.
static bool readFrontEnd(std::string& out) {
    std::string here = __FILE__;
    std::string beside;
    const size_t slash = here.find_last_of("/\\");
    if (slash != std::string::npos) {
        beside = here.substr(0, slash) + "/../../../webui_src/app.js";
    }
    const char* candidates[] = {
        "../../webui_src/app.js",
        "DomoticsCore-WebUI/webui_src/app.js",
        "webui_src/app.js",
    };
    std::string paths[4];
    int n = 0;
    for (const char* c : candidates) paths[n++] = c;
    if (!beside.empty()) paths[n++] = beside;
    for (int i = 0; i < n; ++i) {
        std::ifstream in(paths[i].c_str());
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        out = ss.str();
        return true;
    }
    return false;
}

// The text of renderUI(): from its signature to the closing brace that
// stands alone at the method's indent.
static bool renderUIBody(const std::string& src, std::string& body) {
    const std::string head = "renderUI() {";
    const size_t start = src.find(head);
    if (start == std::string::npos) return false;
    const size_t end = src.find("\n    }\n", start);
    if (end == std::string::npos) return false;
    body = src.substr(start, end - start);
    return true;
}

// Drawn
// Given a context made with quickControl(), whose location is QuickControls,
// When the page renders the schema (renderUI()),
// Then the QuickControls location is drawn: renderUI() draws the contexts of
// that location, which the page lays out as cards in a quick-controls area.
void test_quick_control_context_is_drawn_by_render_ui(void) {
    std::string src;
    TEST_ASSERT_TRUE_MESSAGE(readFrontEnd(src), "webui_src/app.js not found");

    std::string body;
    TEST_ASSERT_TRUE_MESSAGE(renderUIBody(src, body), "renderUI() not found in app.js");

    // The location is named by the schema's enum mapping alone today.
    TEST_ASSERT_TRUE_MESSAGE(
        body.find("QuickControls") != std::string::npos,
        "renderUI() never draws the QuickControls location: a control placed with quickControl() is lost");

    // Whatever draws it selects the contexts by that location.
    TEST_ASSERT_TRUE_MESSAGE(
        src.find("WebUILocation.QuickControls") != std::string::npos,
        "app.js never selects contexts by WebUILocation.QuickControls");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_quick_control_context_is_drawn_by_render_ui);
    return UNITY_END();
}
