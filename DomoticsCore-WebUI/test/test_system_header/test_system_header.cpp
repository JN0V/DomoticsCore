/**
 * @file test_system_header.cpp
 * @brief BUG-32: the WebUI update header must survive any device name.
 *
 * `buildSystemHeader` (extracted from `WebUIComponent::buildUpdateJson` so this
 * suite can reach it — the private member and its `wsBuffer_` are unreachable
 * from any native translation unit) interpolated `device_name` with a raw `%s`.
 * A `"` or `\` in the name corrupted every WebSocket and polling update for every
 * client, persistently after reboot.
 *
 * Every discriminating test here fails against the pre-fix raw-`%s` builder: the
 * closing `}}` is appended to make the header a complete document, and a raw quote
 * would make `deserializeJson` reject it. The tight-buffer and crowding tests pin
 * the two properties the fix must not get wrong — never a partial escape, and the
 * escaping's cost against the 1024-byte ESP8266 buffer.
 */

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>

#include <DomoticsCore/WebUI/JsonEscape.h>
#include <DomoticsCore/WebUI/SystemHeader.h>

using namespace DomoticsCore::Components::WebUI;

static char buf[4096];

// Build the header, close its trailing `"contexts":{` with `}}` so it is a whole
// document, and parse it. Returns true when the result is valid JSON.
static bool parseHeader(JsonDocument& doc, const char* deviceName,
                        char* out = buf, size_t cap = sizeof(buf)) {
    int n = buildSystemHeader(out, cap, 12345u, 45000u, 2, deviceName);
    if (n < 0) return false;
    if ((size_t)n + 3 > cap) return false;  // room for "}}" plus NUL
    out[n] = '}'; out[n + 1] = '}'; out[n + 2] = '\0';
    return deserializeJson(doc, out) == DeserializationError::Ok;
}

void setUp() {}
void tearDown() {}

// --- the header is well-formed and carries the fixed fields --------------------
void test_plain_name_produces_valid_json(void) {
    JsonDocument doc;
    TEST_ASSERT_TRUE(parseHeader(doc, "Living Room"));
    TEST_ASSERT_EQUAL_STRING("Living Room", doc["system"]["device_name"]);
    TEST_ASSERT_EQUAL_UINT32(12345u, doc["system"]["uptime"]);
    TEST_ASSERT_EQUAL_UINT32(45000u, doc["system"]["heap"]);
    TEST_ASSERT_EQUAL_INT(2, doc["system"]["clients"]);
}

// --- a quote no longer breaks the payload (fails vs raw %s) ---------------------
void test_a_quote_is_escaped(void) {
    JsonDocument doc;
    TEST_ASSERT_TRUE(parseHeader(doc, "a\"b"));
    TEST_ASSERT_EQUAL_STRING("a\"b", doc["system"]["device_name"]);
}

// --- a backslash no longer breaks the payload (fails vs raw %s) -----------------
void test_a_backslash_is_escaped(void) {
    JsonDocument doc;
    TEST_ASSERT_TRUE(parseHeader(doc, "a\\b"));
    TEST_ASSERT_EQUAL_STRING("a\\b", doc["system"]["device_name"]);
}

// --- a crafted name cannot inject a sibling key (fails vs raw %s) ---------------
void test_an_injection_attempt_stays_one_string(void) {
    JsonDocument doc;
    // Raw, this would close device_name and add {"injected":"x"} to system.
    const char* evil = "\",\"injected\":\"x";
    TEST_ASSERT_TRUE(parseHeader(doc, evil));
    TEST_ASSERT_EQUAL_STRING(evil, doc["system"]["device_name"]);
    TEST_ASSERT_TRUE(doc["system"]["injected"].isNull());
}

// --- 31 control characters escape whole, with no partial sequence --------------
// Discriminating on the escaped FORM, not just round-trip: ArduinoJson accepts raw
// control bytes in a string, so a raw-%s builder would still parse and round-trip.
// The test asserts the bytes are actually escaped in the built header — the escaped
// six-char form is present, a raw 0x01 byte is not — then that it round-trips.
void test_thirtyone_control_chars_produce_no_partial_escape(void) {
    char name[32];
    memset(name, 0x01, 31);
    name[31] = '\0';

    char raw[512];
    int n = buildSystemHeader(raw, sizeof(raw), 12345u, 45000u, 2, name);
    TEST_ASSERT_GREATER_THAN_INT(0, n);
    const char* escForm = "\\u0001";
    TEST_ASSERT_NOT_NULL(strstr(raw, escForm));         // it was escaped
    TEST_ASSERT_NULL(memchr(raw, 0x01, (size_t)n));     // no raw control byte
    // Exactly 31 whole escape sequences (186 bytes), none partial.
    int count = 0;
    for (const char* p = strstr(raw, escForm); p; p = strstr(p + 1, escForm)) count++;
    TEST_ASSERT_EQUAL_INT(31, count);

    // And it is still valid JSON that round-trips to the 31 original bytes.
    JsonDocument doc;
    TEST_ASSERT_TRUE(parseHeader(doc, name));
    const char* got = doc["system"]["device_name"];
    TEST_ASSERT_EQUAL_UINT(31u, strlen(got));
    for (int i = 0; i < 31; ++i) TEST_ASSERT_EQUAL_HEX8(0x01, (unsigned char)got[i]);
}

// --- jsonEscape never writes a partial escape when the buffer runs out ---------
void test_escape_stops_clean_when_the_buffer_is_too_small(void) {
    // One 0x01 escapes to six bytes. A cap of 4 cannot hold it, so nothing is
    // written rather than a truncated "\u0" — which would later break parsing.
    char out[4];
    memset(out, 'Z', sizeof(out));
    size_t w = jsonEscape(out, sizeof(out), "\x01");
    TEST_ASSERT_EQUAL_UINT(0u, w);
    TEST_ASSERT_EQUAL('\0', out[0]);

    // A cap that fits exactly one escape takes one and stops before the second,
    // leaving a clean, terminated result.
    char out2[8];
    size_t w2 = jsonEscape(out2, sizeof(out2), "\x01\x01");
    TEST_ASSERT_EQUAL_UINT(6u, w2);
    TEST_ASSERT_EQUAL_STRING("\\u0001", out2);
}

// --- the escaping's cost against the 1024-byte ESP8266 buffer ------------------
// Pins the mechanism behind the owed board observation: a worst-case name eats
// ~185 bytes of the update that would otherwise hold context data. Not a board
// test — it measures the header the board would build.
void test_worst_case_name_crowds_the_1024_buffer(void) {
    char small[1024];
    char name[32];
    memset(name, 0x01, 31);
    name[31] = '\0';

    int worst = buildSystemHeader(small, sizeof(small), 12345u, 45000u, 2, name);
    int plain = buildSystemHeader(small, sizeof(small), 12345u, 45000u, 2, "d");
    TEST_ASSERT_GREATER_THAN_INT(0, worst);
    TEST_ASSERT_GREATER_THAN_INT(0, plain);

    // 31 control chars cost 31*6 = 186 escaped bytes vs 1 plain, so the header
    // grows by ~185 — room taken straight out of the 1024-byte update.
    TEST_ASSERT_GREATER_THAN_INT(150, worst - plain);
    // The header itself still fits; the crowding is of contexts, not a truncated
    // header (which the caller rejects as pos >= bufSize).
    TEST_ASSERT_LESS_THAN_INT((int)sizeof(small), worst);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_plain_name_produces_valid_json);
    RUN_TEST(test_a_quote_is_escaped);
    RUN_TEST(test_a_backslash_is_escaped);
    RUN_TEST(test_an_injection_attempt_stays_one_string);
    RUN_TEST(test_thirtyone_control_chars_produce_no_partial_escape);
    RUN_TEST(test_escape_stops_clean_when_the_buffer_is_too_small);
    RUN_TEST(test_worst_case_name_crowds_the_1024_buffer);
    return UNITY_END();
}
