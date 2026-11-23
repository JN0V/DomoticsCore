// What the EventBus byte budget does not bound: the per-topic maps
// (lastByTopic, pendingByTopic) that a sticky publish fills, one entry per
// distinct topic. Each test states what it needs to hold; the docs it reads
// are located from the working directory or from this file's own path.
#include <unity.h>
#include <DomoticsCore/EventBus.h>
#include <DomoticsCore/Platform_Stub.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace DomoticsCore::Utils;

static const char* REFERENCE_PAGE = "docs/components/core/technical-reference.md";
static const char* EVENTBUS_HEADER = "DomoticsCore-Core/include/DomoticsCore/EventBus.h";

EventBus* testBus = nullptr;

void setUp(void) { testBus = new EventBus(); }
void tearDown(void) { delete testBus; testBus = nullptr; }

// Reads a file given from the repository's root, whichever directory the test
// program runs from: the working directory and its parents, then the
// directories above this source file.
static bool readRepoFile(const char* rel, std::vector<std::string>& lines) {
    std::vector<std::string> bases = {"", "../", "../../", "../../../", "../../../../"};
    std::string here = __FILE__;
    for (size_t cut = here.rfind('/'); cut != std::string::npos && cut > 0; cut = here.rfind('/', cut - 1)) {
        bases.push_back(here.substr(0, cut + 1));
    }
    for (const std::string& base : bases) {
        std::ifstream in(base + rel);
        if (!in.good()) continue;
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
        return true;
    }
    return false;
}

// The text of one markdown section: from its heading to the next heading or rule.
static std::string sectionText(const std::vector<std::string>& lines, const char* heading) {
    std::string out;
    bool inside = false;
    for (const std::string& l : lines) {
        if (!inside) { if (l.rfind(heading, 0) == 0) inside = true; continue; }
        if (l.rfind("#", 0) == 0 || l.rfind("---", 0) == 0) break;
        out += l + "\n";
    }
    return out;
}

// Sticky growth
// Given a caller that publishes sticky events on many distinct topics,
// When the count passes the stated bound,
// Then memory stops growing (the bus no longer keeps a value per topic),
//      or the bound is documented (the Sticky Events section names lastByTopic).
void test_sticky_topics_are_bounded_or_the_bound_is_documented(void) {
    const int N = 1024;
    char name[16];
    for (int i = 0; i < N; ++i) {
        snprintf(name, sizeof(name), "s/%04d", i);
        testBus->publishSticky(String(name), i);
        testBus->poll();                       // drained at once: the queue itself stays small
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, testBus->getQueuedBytes(), "the queue was not drained");

    size_t replayable = 0;
    for (int i = 0; i < N; ++i) {
        snprintf(name, sizeof(name), "s/%04d", i);
        testBus->subscribe(String(name), [&replayable](const void* p) { if (p) ++replayable; },
                           nullptr, /*replayLast=*/true);
    }
    if (replayable < (size_t)N) return;        // memory stopped growing: a bound holds

    std::vector<std::string> page;
    TEST_ASSERT_TRUE_MESSAGE(readRepoFile(REFERENCE_PAGE, page), "the EventBus reference page was not found");
    const std::string sticky = sectionText(page, "### Sticky Events");
    TEST_ASSERT_TRUE_MESSAGE(sticky.find("lastByTopic") != std::string::npos,
        "1024 distinct sticky topics are all kept, and the reference page's Sticky Events "
        "section does not state the bound on lastByTopic");
}

// Burst of small events
// Given 256 small events on 256 distinct topics,
// When they are queued,
// Then the memory held by the topic map is counted in the budget (the queue
//      bills more than the events themselves, or evicts),
//      or its bound is stated (the Backpressure section names lastByTopic).
void test_topic_map_memory_is_in_the_budget_or_its_bound_is_stated(void) {
    const size_t N = 256;
    const size_t eventCost = QueueCost::of(sizeof(int), 5);   // "s/NNN": inside SSO
    TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(QueueCost::kBudgetBytes, N * eventCost,
                                         "the events alone overflow the budget: this test is vacuous");
    char name[16];
    for (size_t i = 0; i < N; ++i) {
        snprintf(name, sizeof(name), "s/%03u", (unsigned)i);
        testBus->publishSticky(String(name), (int)i);
    }
    if (testBus->getQueuedBytes() > N * eventCost) return;    // the map is billed too
    if (testBus->getDroppedCount() > 0) return;               // or the budget evicted for it

    std::vector<std::string> page;
    TEST_ASSERT_TRUE_MESSAGE(readRepoFile(REFERENCE_PAGE, page), "the EventBus reference page was not found");
    const std::string backpressure = sectionText(page, "### Backpressure");
    TEST_ASSERT_TRUE_MESSAGE(backpressure.find("lastByTopic") != std::string::npos,
        "256 sticky events on 256 topics are billed only for the queued events, and the "
        "reference page's Backpressure section does not state the bound on the topic map");
}

// Build flag
// Given the reference page and the header comment,
// When I read how to set DOMOTICS_EVENTBUS_QUEUE_BYTES,
// Then both name build_flags as the place for it.
void test_both_name_build_flags_as_the_place_for_the_queue_budget_flag(void) {
    // The reference page: the paragraph that names the flag also names build_flags.
    std::vector<std::string> page;
    TEST_ASSERT_TRUE_MESSAGE(readRepoFile(REFERENCE_PAGE, page), "the EventBus reference page was not found");
    bool pageNames = false;
    for (size_t i = 0; i < page.size(); ++i) {
        if (page[i].find("DOMOTICS_EVENTBUS_QUEUE_BYTES") == std::string::npos) continue;
        size_t from = i, to = i;
        while (from > 0 && !page[from - 1].empty()) --from;
        while (to + 1 < page.size() && !page[to + 1].empty()) ++to;
        for (size_t k = from; k <= to; ++k)
            if (page[k].find("build_flags") != std::string::npos) pageNames = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(pageNames,
        "the reference page's text on DOMOTICS_EVENTBUS_QUEUE_BYTES does not name build_flags");

    // The header: a comment beside the #ifdef that reads the flag names build_flags.
    std::vector<std::string> header;
    TEST_ASSERT_TRUE_MESSAGE(readRepoFile(EVENTBUS_HEADER, header), "EventBus.h was not found");
    size_t at = header.size();
    for (size_t i = 0; i < header.size(); ++i)
        if (header[i].find("#ifdef DOMOTICS_EVENTBUS_QUEUE_BYTES") != std::string::npos) { at = i; break; }
    TEST_ASSERT_TRUE_MESSAGE(at < header.size(), "EventBus.h no longer reads DOMOTICS_EVENTBUS_QUEUE_BYTES");
    bool headerNames = false;
    const size_t lo = at > 30 ? at - 30 : 0, hi = at + 30 < header.size() ? at + 30 : header.size() - 1;
    for (size_t k = lo; k <= hi; ++k) {
        const std::string& l = header[k];
        const size_t c = l.find("//") != std::string::npos ? l.find("//") : l.find(" * ");
        if (c != std::string::npos && l.find("build_flags", c) != std::string::npos) headerNames = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(headerNames,
        "the comment on DOMOTICS_EVENTBUS_QUEUE_BYTES in EventBus.h does not name build_flags");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sticky_topics_are_bounded_or_the_bound_is_documented);
    RUN_TEST(test_topic_map_memory_is_in_the_budget_or_its_bound_is_stated);
    RUN_TEST(test_both_name_build_flags_as_the_place_for_the_queue_budget_flag);
    return UNITY_END();
}
