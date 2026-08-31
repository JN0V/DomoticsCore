/**
 * @file test_schema_chunking.cpp
 * @brief Native tests for SchemaChunkState::writeChunk — the schema array
 *        assembly loop (SIZE-1 extraction).
 *
 * Until this extraction the loop lived twice inside WebUI.h route lambdas,
 * compiled only against ESPAsyncWebServer, and no test had ever executed it —
 * the existing "schema" tests hand-rolled their own approximation. These tests
 * drive the real thing: comma placement, the comma backtrack
 * (contextIndexInProvider--), provider skipping, and the stall contract that
 * the HTTP wrappers' RESPONSE_TRY_AGAIN policy depends on (BUG-34).
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/IWebUIProvider.h>
#include <DomoticsCore/WebUI/ProviderRegistry.h>
#include <ArduinoJson.h>

#include <cstring>
#include <string>
#include <vector>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

namespace {

class FakeProvider : public CachingWebUIProvider {
private:
    String name_;
    std::vector<WebUIContext> pending_;
    bool enabled_ = true;

protected:
    void buildContexts(std::vector<WebUIContext>& contexts) override {
        for (const auto& ctx : pending_) contexts.push_back(ctx);
    }

public:
    explicit FakeProvider(const String& n) : name_(n) {}

    void addContext(const WebUIContext& ctx) {
        pending_.push_back(ctx);
        invalidateContextCache();
    }

    String getWebUIName() const override { return name_; }
    String getWebUIVersion() const override { return "1.0.0"; }
    String getWebUIData(const String&) override { return "{}"; }
    String handleWebUIRequest(const String&, const String&, const String&,
                              const std::map<String, String>&) override {
        return "{\"success\":true}";
    }
    bool isWebUIEnabled() override { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
};

// The maximal fixture: escapes (including a control character, whose 
// escape is the six-byte atomic unit that sets the sweep floor), options,
// option labels, a multiselect, custom HTML/CSS/JS.
void addMaximalContexts(FakeProvider& p) {
    std::vector<String> opts = {"one", "two", "three"};
    std::vector<String> sel = {"one", "three"};

    p.addContext(WebUIContext::dashboard("chunk_dash", "Da\"sh\\board", "dc-test")
        .withCustomHtml("<div class=\"x\">ctl:\x01</div>")
        .withCustomCss(".x { content: \"\\\\\"; }")
        .withCustomJs("if (a < 1) { b(\"\x02\"); }")
        .withField(WebUIField("esc", "Esc\"aped", WebUIFieldType::Text, "va\x01l\"ue", "u\"nit"))
        .withField(WebUIField("multi", "Multi", WebUIFieldType::Multiselect, "")
            .choices(opts)
            .values(sel)));

    WebUIField sel_field("mode", "Mode", WebUIFieldType::Select, "two");
    sel_field.choices(opts);
    sel_field.optionLabels["one"] = "O\"ne";
    sel_field.optionLabels["two"] = "Tw\x03o";
    p.addContext(WebUIContext::settings("chunk_settings", "Settings").withField(sel_field));
}

// Drive writeChunk to completion at a fixed chunk size, retrying on zero
// returns exactly as the HTTP layer's RESPONSE_TRY_AGAIN does. writeChunk
// returns 0 with finished still false in two reproduced shapes:
//  - TRANSIENT: the serializer's check-state chain was exited in a call that
//    had written nothing yet — state advanced, the retry writes. This happens
//    at ordinary chunk sizes on ordinary content (BUG-34's easy shape).
//  - PERMANENT: an atomic escape sequence does not fit maxLen — the retry at
//    the same size writes nothing either; only a larger buffer recovers.
// zeroReturns counts every 0; a permanent stall (3 consecutive zeros) fails
// unless permanentStalls is non-null, in which case it is counted and the
// drive recovers with a large buffer, as a healthy HTTP client eventually
// would when the TCP window opens.
std::string driveToCompletion(ProviderRegistry::SchemaChunkState& state,
                              size_t chunkSize,
                              int* zeroReturns = nullptr,
                              int* permanentStalls = nullptr) {
    std::string out;
    std::vector<uint8_t> buf(chunkSize);
    int guard = 200000;
    int consecutiveZeros = 0;
    while (!state.finished && guard-- > 0) {
        size_t n = state.writeChunk(buf.data(), chunkSize);
        if (n == 0 && !state.finished) {
            if (zeroReturns) (*zeroReturns)++;
            if (++consecutiveZeros >= 3) {
                if (!permanentStalls) {
                    char msg[192];
                    snprintf(msg, sizeof(msg),
                             "writeChunk stalled permanently: chunkSize=%u offset=%u "
                             "tail='%.12s' serCtx=%d needComma=%d",
                             (unsigned)chunkSize, (unsigned)out.size(),
                             out.size() >= 12 ? out.c_str() + out.size() - 12 : out.c_str(),
                             (int)state.serializingContext, (int)state.needComma);
                    TEST_FAIL_MESSAGE(msg);
                }
                (*permanentStalls)++;
                std::vector<uint8_t> big(4096);
                size_t m = state.writeChunk(big.data(), big.size());
                TEST_ASSERT_TRUE_MESSAGE(m > 0, "stall did not recover with a larger buffer");
                out.append(reinterpret_cast<char*>(big.data()), m);
                consecutiveZeros = 0;
            }
            continue;
        }
        consecutiveZeros = 0;
        out.append(reinterpret_cast<char*>(buf.data()), n);
    }
    TEST_ASSERT_TRUE_MESSAGE(guard > 0, "writeChunk livelocked");
    return out;
}

std::string oneShotReference(ProviderRegistry& registry) {
    auto state = registry.prepareSchemaGeneration();
    std::vector<uint8_t> buf(16384);
    std::string out;
    int guard = 1000;
    while (!state->finished && guard-- > 0) {
        size_t n = state->writeChunk(buf.data(), buf.size());
        out.append(reinterpret_cast<char*>(buf.data()), n);
    }
    TEST_ASSERT_TRUE(guard > 0);
    return out;
}

} // namespace

void setUp() {}
void tearDown() {}

// ============================================================================
// Sweep: every chunk size from 6 (the longest escape in the content is the
// six-byte \u00XX form, and escapes are atomic within one write) to 64 must
// produce output byte-identical to a one-shot run under the retry policy.
// Transient zero returns are expected and counted — they are BUG-34's
// ordinary shape; a permanent stall at these sizes fails.
// ============================================================================
void test_chunk_sweep_byte_identical_to_one_shot() {
    ProviderRegistry registry;
    FakeProvider provider("Maximal");
    addMaximalContexts(provider);
    FakeProvider disabled("Disabled");
    disabled.addContext(WebUIContext::dashboard("never", "Never", "dc-x"));
    disabled.setEnabled(false);
    FakeProvider contextless("Empty");

    registry.registerProvider(&provider);
    registry.registerProvider(&disabled);
    registry.registerProvider(&contextless);

    std::string reference = oneShotReference(registry);
    TEST_ASSERT_TRUE(reference.size() > 100);

    // The reference must parse and must not contain the disabled context.
    {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, reference.c_str());
        TEST_ASSERT_TRUE_MESSAGE(err == DeserializationError::Ok, err.c_str());
        TEST_ASSERT_EQUAL(2, doc.as<JsonArray>().size());
    }
    TEST_ASSERT_TRUE(reference.find("never") == std::string::npos);

    int zeroReturns = 0;
    for (size_t size = 6; size <= 64; size++) {
        auto state = registry.prepareSchemaGeneration();
        std::string out = driveToCompletion(*state, size, &zeroReturns);
        if (out != reference) {
            char msg[64];
            snprintf(msg, sizeof(msg), "sweep diverged at chunk size %u", (unsigned)size);
            TEST_FAIL_MESSAGE(msg);
        }
    }

    // The transient zero returns are BUG-34's ordinary shape: they occur at
    // everyday chunk sizes, and each one is a point where /api/ui/schema
    // truncated before its wrapper carried the retry policy. If this stops
    // firing, the TRY_AGAIN policy has become dead code worth re-examining.
    TEST_ASSERT_TRUE_MESSAGE(zeroReturns > 0,
        "no transient zero returns across the sweep - retry policy untestable");
}

// ============================================================================
// Stall contract (BUG-34's core precondition): with maxLen below the atomic
// escape length, writeChunk returns 0 with finished == false — the exact
// return the HTTP wrapper must map to RESPONSE_TRY_AGAIN rather than
// end-of-response — and a larger buffer recovers, completing byte-identically.
// ============================================================================
void test_stall_returns_zero_then_recovers() {
    ProviderRegistry registry;
    FakeProvider provider("Maximal");
    addMaximalContexts(provider);
    registry.registerProvider(&provider);

    std::string reference = oneShotReference(registry);

    auto state = registry.prepareSchemaGeneration();
    int zeroReturns = 0;
    int permanentStalls = 0;
    std::string out = driveToCompletion(*state, 1, &zeroReturns, &permanentStalls);

    TEST_ASSERT_TRUE_MESSAGE(permanentStalls > 0,
        "content with a \\u00XX escape never stalled permanently at maxLen 1 — "
        "the escape-atomicity floor is untested");
    TEST_ASSERT_TRUE(out == reference);
}

// ============================================================================
// Skip logic on a hand-built state: a null provider slot and a context with an
// empty id are both skipped without corrupting the array.
// ============================================================================
void test_null_provider_and_empty_context_id_skipped() {
    FakeProvider real("Real");
    real.addContext(WebUIContext::dashboard("real_ctx", "Real", "dc-r"));
    real.addContext(WebUIContext());  // default-constructed: empty contextId

    ProviderRegistry::SchemaChunkState state;
    state.providers = {nullptr, &real};

    std::vector<uint8_t> buf(8192);
    size_t n = state.writeChunk(buf.data(), buf.size());
    TEST_ASSERT_TRUE(state.finished);

    std::string out(reinterpret_cast<char*>(buf.data()), n);
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, out.c_str()) == DeserializationError::Ok);
    TEST_ASSERT_EQUAL(1, doc.as<JsonArray>().size());
    TEST_ASSERT_EQUAL_STRING("real_ctx", doc[0]["contextId"].as<const char*>());
}

// ============================================================================
// No providers at all: the array still opens and closes.
// ============================================================================
void test_no_providers_yields_empty_array() {
    ProviderRegistry::SchemaChunkState state;

    std::vector<uint8_t> buf(16);
    size_t n = state.writeChunk(buf.data(), buf.size());
    TEST_ASSERT_TRUE(state.finished);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_MEMORY("[]", buf.data(), 2);

    // A finished state stays finished and writes nothing more.
    TEST_ASSERT_EQUAL(0, state.writeChunk(buf.data(), buf.size()));
}

// ============================================================================
// Comma backtrack: with escape-free ASCII content every size down to 1 must
// make progress, and the sizes where the comma lands exactly on the buffer
// boundary exercise the contextIndexInProvider-- rewind. Both contexts must
// survive at every size.
// ============================================================================
void test_comma_backtrack_loses_no_context() {
    ProviderRegistry registry;
    FakeProvider provider("Plain");
    provider.addContext(WebUIContext::dashboard("a", "A", "i")
        .withField(WebUIField("f", "F", WebUIFieldType::Display, "1")));
    provider.addContext(WebUIContext::dashboard("b", "B", "i")
        .withField(WebUIField("g", "G", WebUIFieldType::Display, "2")));
    registry.registerProvider(&provider);

    std::string reference = oneShotReference(registry);
    {
        JsonDocument doc;
        TEST_ASSERT_TRUE(deserializeJson(doc, reference.c_str()) == DeserializationError::Ok);
        TEST_ASSERT_EQUAL(2, doc.as<JsonArray>().size());
    }

    for (size_t size = 1; size <= 32; size++) {
        auto state = registry.prepareSchemaGeneration();
        std::string out = driveToCompletion(*state, size);
        if (out != reference) {
            char msg[64];
            snprintf(msg, sizeof(msg), "backtrack diverged at chunk size %u", (unsigned)size);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_chunk_sweep_byte_identical_to_one_shot);
    RUN_TEST(test_stall_returns_zero_then_recovers);
    RUN_TEST(test_null_provider_and_empty_context_id_skipped);
    RUN_TEST(test_no_providers_yields_empty_array);
    RUN_TEST(test_comma_backtrack_loses_no_context);
    return UNITY_END();
}
