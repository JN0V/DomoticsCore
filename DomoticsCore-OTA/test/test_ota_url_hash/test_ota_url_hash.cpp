/**
 * @file test_ota_url_hash.cpp
 * @brief A URL install can carry the SHA-256 its image must match, and can be required to.
 *
 * The stub SHA-256 digests everything to 32 zero bytes, so 64 zeros matches and
 * anything else does not. HAL::OTAUpdate::end() is the commit; the stub counts it.
 */

#include <unity.h>
#include <DomoticsCore/OTA.h>
#include <DomoticsCore/Update_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

const char* const SHA_OF_ANY_STUB_INPUT = "0000000000000000000000000000000000000000000000000000000000000000";
const char* const SHA_THAT_CANNOT_MATCH = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";

void startWithDownloader(OTAComponent& ota, bool requireHash) {
    OTAConfig config;
    config.checkIntervalMs = 0;
    config.autoReboot = false;
    config.requireDownloadHash = requireHash;
    ota.setConfig(config);
    ota.begin();
    ota.setDownloader([](const String&, size_t& totalSize, OTAComponent::DownloadCallback onChunk) {
        const uint8_t firmware[8] = {0xE9, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        totalSize = sizeof(firmware);
        return onChunk(firmware, sizeof(firmware));
    });
}

}  // namespace

void setUp() {
    HAL::OTAUpdate::s_stubBeginCalls = 0;
    HAL::OTAUpdate::s_stubEndCalls = 0;
    HAL::OTAUpdate::s_stubAbortCalls = 0;
}
void tearDown() {}

// A literal digest must reach the String overload, not convert to the bool `force`.
void test_a_literal_digest_is_checked_against_the_image() {
    OTAComponent ota;
    startWithDownloader(ota, false);
    TEST_ASSERT_TRUE(ota.triggerUpdateFromUrl("http://example.invalid/fw.bin", SHA_THAT_CANNOT_MATCH));
    ota.loop();
    TEST_ASSERT_EQUAL_STRING("SHA256 mismatch", ota.getLastError().c_str());
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, HAL::OTAUpdate::s_stubEndCalls, "a mismatched image was committed");
}

void test_a_matching_digest_commits() {
    OTAComponent ota;
    startWithDownloader(ota, false);
    TEST_ASSERT_TRUE(ota.triggerUpdateFromUrl(String("http://example.invalid/fw.bin"), String(SHA_OF_ANY_STUB_INPUT), true));
    ota.loop();
    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_NOT_EQUAL(OTAComponent::State::Error, ota.getState());
}

void test_without_the_requirement_an_unhashed_install_still_commits() {
    OTAComponent ota;
    startWithDownloader(ota, false);
    TEST_ASSERT_TRUE(ota.triggerUpdateFromUrl("http://example.invalid/fw.bin", true));
    ota.loop();
    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubEndCalls);
}

void test_the_requirement_refuses_an_unhashed_trigger_at_once() {
    OTAComponent ota;
    startWithDownloader(ota, true);
    TEST_ASSERT_FALSE(ota.triggerUpdateFromUrl("http://example.invalid/fw.bin", true));
    TEST_ASSERT_EQUAL_STRING("Firmware hash required", ota.getLastError().c_str());
    ota.loop();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, HAL::OTAUpdate::s_stubBeginCalls, "a refused trigger still opened an update");
}

void test_the_requirement_accepts_a_hashed_trigger() {
    OTAComponent ota;
    startWithDownloader(ota, true);
    TEST_ASSERT_TRUE(ota.triggerUpdateFromUrl("http://example.invalid/fw.bin", SHA_OF_ANY_STUB_INPUT));
    ota.loop();
    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubEndCalls);
}

// A manifest without a sha256 field reaches the same install path, and is refused before any flash is opened.
void test_the_requirement_refuses_a_manifest_without_a_digest() {
    OTAComponent ota;
    startWithDownloader(ota, true);
    OTAConfig config = ota.getConfig();
    config.manifestUrl = "http://example.invalid/manifest.json";
    ota.setConfig(config);
    ota.setManifestFetcher([](const String&, String& outJson) {
        outJson = "{\"version\":\"9.9.9\",\"url\":\"http://example.invalid/fw.bin\"}";
        return true;
    });
    ota.triggerImmediateCheck(true);
    ota.loop();
    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota.getState());
    TEST_ASSERT_EQUAL_STRING("Firmware hash required", ota.getLastError().c_str());
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubBeginCalls);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_literal_digest_is_checked_against_the_image);
    RUN_TEST(test_a_matching_digest_commits);
    RUN_TEST(test_without_the_requirement_an_unhashed_install_still_commits);
    RUN_TEST(test_the_requirement_refuses_an_unhashed_trigger_at_once);
    RUN_TEST(test_the_requirement_accepts_a_hashed_trigger);
    RUN_TEST(test_the_requirement_refuses_a_manifest_without_a_digest);
    return UNITY_END();
}
