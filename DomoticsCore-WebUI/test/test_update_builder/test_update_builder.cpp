/**
 * @file test_update_builder.cpp
 * @brief Native tests for WebUI::buildUpdateJson (SIZE-1 extraction).
 *
 * Until this extraction the update assembly was a private WebUIComponent
 * method compiled only against ESPAsyncWebServer; only its header step
 * (buildSystemHeader, BUG-32) was natively testable. These tests pin the
 * assembly itself: the delta/forceNext skip logic, the empty-data skip, and
 * the crowding behaviour — contexts that no longer fit are dropped cleanly,
 * never truncated into invalid JSON.
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IWebUIProvider.h>
#include <DomoticsCore/WebUI/UpdateBuilder.h>
#include <ArduinoJson.h>

#include <map>
#include <string>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

namespace {

// A provider whose data and change-report are both controllable. The
// IWebUIProvider default hasDataChanged() returns true — a fixture that did
// not override it could never exhibit a delta skip, and the delta tests
// below would discriminate nothing.
class ScriptedProvider : public IWebUIProvider {
public:
    String data = "{\"v\":1}";
    bool changed = true;
    int dataCalls = 0;

    String getWebUIName() const override { return "Scripted"; }
    String getWebUIVersion() const override { return "1.0.0"; }
    void forEachContext(std::function<bool(const WebUIContext&)>) override {}
    size_t getContextCount() override { return 0; }
    bool getContextAt(size_t, WebUIContext&) override { return false; }
    WebUIContext getWebUIContext(const String&) override { return WebUIContext(); }
    String getWebUIData(const String&) override {
        dataCalls++;
        return data;
    }
    String handleWebUIRequest(const String&, const String&, const String&,
                              const std::map<String, String>&) override {
        return "{}";
    }
    bool hasDataChanged(const String&) override { return changed; }
};

} // namespace

void setUp() {}
void tearDown() {}

void test_full_update_parses_and_carries_contexts() {
    ScriptedProvider p1, p2;
    p1.data = "{\"temp\":21}";
    p2.data = "{\"state\":\"on\"}";
    std::map<String, IWebUIProvider*> providers;
    providers["ctx_a"] = &p1;
    providers["ctx_b"] = &p2;

    char buf[1024];
    int len = buildUpdateJson(buf, sizeof(buf), providers, "Device",
                              123456, 40000, 2, true, false);
    TEST_ASSERT_TRUE(len > 0);

    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, buf) == DeserializationError::Ok);
    TEST_ASSERT_EQUAL_UINT32(123456, doc["system"]["uptime"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(40000, doc["system"]["heap"].as<uint32_t>());
    TEST_ASSERT_EQUAL_INT(2, doc["system"]["clients"].as<int>());
    TEST_ASSERT_EQUAL_STRING("Device", doc["system"]["device_name"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(21, doc["contexts"]["ctx_a"]["temp"].as<int>());
    TEST_ASSERT_EQUAL_STRING("on", doc["contexts"]["ctx_b"]["state"].as<const char*>());
}

void test_delta_skips_unchanged_and_forcenext_overrides() {
    ScriptedProvider changedP, unchangedP;
    changedP.data = "{\"v\":1}";
    unchangedP.data = "{\"v\":2}";
    unchangedP.changed = false;
    std::map<String, IWebUIProvider*> providers;
    providers["changed"] = &changedP;
    providers["unchanged"] = &unchangedP;

    char buf[1024];

    // Delta path: the unchanged provider is excluded and its data never built.
    int len = buildUpdateJson(buf, sizeof(buf), providers, "D",
                              1, 1, 0, false, false);
    TEST_ASSERT_TRUE(len > 0);
    {
        JsonDocument doc;
        TEST_ASSERT_TRUE(deserializeJson(doc, buf) == DeserializationError::Ok);
        TEST_ASSERT_TRUE(doc["contexts"]["changed"].is<JsonObject>());
        TEST_ASSERT_FALSE(doc["contexts"]["unchanged"].is<JsonObject>());
    }
    TEST_ASSERT_EQUAL_INT(0, unchangedP.dataCalls);

    // forceNext overrides the delta check.
    len = buildUpdateJson(buf, sizeof(buf), providers, "D",
                          1, 1, 0, false, true);
    TEST_ASSERT_TRUE(len > 0);
    {
        JsonDocument doc;
        TEST_ASSERT_TRUE(deserializeJson(doc, buf) == DeserializationError::Ok);
        TEST_ASSERT_TRUE(doc["contexts"]["unchanged"].is<JsonObject>());
    }

    // forceFull (polling) includes it too.
    len = buildUpdateJson(buf, sizeof(buf), providers, "D",
                          1, 1, 0, true, false);
    TEST_ASSERT_TRUE(len > 0);
    {
        JsonDocument doc;
        TEST_ASSERT_TRUE(deserializeJson(doc, buf) == DeserializationError::Ok);
        TEST_ASSERT_TRUE(doc["contexts"]["unchanged"].is<JsonObject>());
    }
}

void test_empty_data_skipped() {
    ScriptedProvider emptyP, bracesP, realP;
    emptyP.data = "";
    bracesP.data = "{}";
    realP.data = "{\"ok\":true}";
    std::map<String, IWebUIProvider*> providers;
    providers["a_empty"] = &emptyP;
    providers["b_braces"] = &bracesP;
    providers["c_real"] = &realP;

    char buf[1024];
    int len = buildUpdateJson(buf, sizeof(buf), providers, "D",
                              1, 1, 0, true, false);
    TEST_ASSERT_TRUE(len > 0);
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, buf) == DeserializationError::Ok);
    TEST_ASSERT_EQUAL(1, doc["contexts"].as<JsonObject>().size());
    TEST_ASSERT_TRUE(doc["contexts"]["c_real"]["ok"].as<bool>());
}

// The crowding behaviour BUG-32 documented: with a small buffer, later
// contexts are dropped SILENTLY — but the output must stay complete, valid
// JSON at every buffer size, with no comma dangling where a context was
// dropped and nothing written past the returned length.
void test_crowding_drops_contexts_but_never_corrupts() {
    ScriptedProvider providers_data[6];
    std::map<String, IWebUIProvider*> providers;
    char names[6][16];
    for (int i = 0; i < 6; i++) {
        snprintf(names[i], sizeof(names[i]), "ctx_%02d", i);
        providers_data[i].data = "{\"payload\":\"0123456789012345678901234567890123456789\"}";
        providers[names[i]] = &providers_data[i];
    }
    // One payload larger than the 512-byte early-break slack: for it, the
    // per-context `needed` bound is the ONLY guard — without it the comma is
    // placed, the write truncates, and the update ends in `,}}`. Real
    // contexts this size exist (customHtml panels).
    {
        String big = "{\"payload\":\"";
        for (int i = 0; i < 60; i++) big += "0123456789";
        big += "\"}";
        providers_data[2].data = big;
    }

    // Reference: a huge buffer carries all six.
    char big[4096];
    int lenBig = buildUpdateJson(big, sizeof(big), providers, "Device",
                                 1, 1, 0, true, false);
    TEST_ASSERT_TRUE(lenBig > 0);
    {
        JsonDocument doc;
        TEST_ASSERT_TRUE(deserializeJson(doc, big) == DeserializationError::Ok);
        TEST_ASSERT_EQUAL(6, doc["contexts"].as<JsonObject>().size());
    }

    // Squeeze: every buffer size from just-above-header to full must yield
    // either 0 (nothing fit) or complete, valid JSON with a subset of
    // contexts in map order and untouched memory past the returned length.
    int sawPartial = 0;
    for (size_t cap = 96; cap <= sizeof(big); cap += 7) {
        char buf[4096];
        memset(buf, 0x7E, sizeof(buf));
        int len = buildUpdateJson(buf, cap, providers, "Device",
                                  1, 1, 0, true, false);
        if (len == 0) continue;
        TEST_ASSERT_TRUE(len < (int)cap);
        TEST_ASSERT_EQUAL_CHAR(0x7E, buf[cap]);  // never writes past cap
        JsonDocument doc;
        if (deserializeJson(doc, buf) != DeserializationError::Ok) {
            char msg[64];
            snprintf(msg, sizeof(msg), "invalid JSON at cap %u", (unsigned)cap);
            TEST_FAIL_MESSAGE(msg);
        }
        size_t n = doc["contexts"].as<JsonObject>().size();
        TEST_ASSERT_TRUE(n <= 6);
        if (n < 6) sawPartial++;
    }
    TEST_ASSERT_TRUE_MESSAGE(sawPartial > 0,
        "no buffer size produced a crowded (partial) update - the drop path is untested");
}

void test_header_too_small_returns_zero() {
    std::map<String, IWebUIProvider*> providers;
    char buf[16];
    TEST_ASSERT_EQUAL_INT(0, buildUpdateJson(buf, sizeof(buf), providers, "Device",
                                             1, 1, 0, true, false));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_full_update_parses_and_carries_contexts);
    RUN_TEST(test_delta_skips_unchanged_and_forcenext_overrides);
    RUN_TEST(test_empty_data_skipped);
    RUN_TEST(test_crowding_drops_contexts_but_never_corrupts);
    RUN_TEST(test_header_too_small_returns_zero);
    return UNITY_END();
}
