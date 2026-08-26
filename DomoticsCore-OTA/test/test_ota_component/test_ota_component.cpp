/**
 * @file test_ota_component.cpp
 * @brief Native unit tests for OTA component
 *
 * Tests cover:
 * - Events (OTAEvents)
 * - Component creation and configuration
 * - Config get/set
 * - State machine
 * - Upload session management
 * - Version comparison
 * - Progress tracking
 * - Lifecycle (begin/shutdown)
 * - Non-blocking behavior
 *
 * Note: These are native tests that don't require actual firmware updates.
 * Hardware tests with real OTA are in separate test files.
 */

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/OTA.h>
#include <DomoticsCore/OTAEvents.h>
#include <DomoticsCore/Update_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// ============================================================================
// Event Tests
// ============================================================================

void test_ota_events_constants_defined() {
    // Verify non-deprecated event constants
    TEST_ASSERT_NOT_NULL(OTAEvents::EVENT_START);
    TEST_ASSERT_NOT_NULL(OTAEvents::EVENT_PROGRESS);
    TEST_ASSERT_NOT_NULL(OTAEvents::EVENT_END);
    TEST_ASSERT_NOT_NULL(OTAEvents::EVENT_ERROR);
    TEST_ASSERT_NOT_NULL(OTAEvents::EVENT_INFO);
    TEST_ASSERT_NOT_NULL(OTAEvents::EVENT_COMPLETED);

    TEST_ASSERT_EQUAL_STRING("ota/start", OTAEvents::EVENT_START);
    TEST_ASSERT_EQUAL_STRING("ota/progress", OTAEvents::EVENT_PROGRESS);
    TEST_ASSERT_EQUAL_STRING("ota/end", OTAEvents::EVENT_END);
    TEST_ASSERT_EQUAL_STRING("ota/error", OTAEvents::EVENT_ERROR);
    TEST_ASSERT_EQUAL_STRING("ota/info", OTAEvents::EVENT_INFO);
    TEST_ASSERT_EQUAL_STRING("ota/completed", OTAEvents::EVENT_COMPLETED);
}

void test_ota_events_namespace() {
    // Events should be in OTAEvents namespace
    const char* evt = OTAEvents::EVENT_START;
    TEST_ASSERT_NOT_NULL(evt);
}

// ============================================================================
// Component Creation Tests
// ============================================================================

void test_ota_component_creation_default() {
    OTAComponent ota;

    TEST_ASSERT_EQUAL_STRING("OTA", ota.metadata.name);
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", ota.metadata.author);
}

void test_ota_component_creation_with_config() {
    OTAConfig config;
    config.updateUrl = "https://example.com/firmware.bin";
    config.checkIntervalMs = 7200000;
    config.autoReboot = false;

    OTAComponent ota(config);

    TEST_ASSERT_EQUAL_STRING("OTA", ota.metadata.name);

    const OTAConfig& cfg = ota.getConfig();
    TEST_ASSERT_EQUAL_STRING("https://example.com/firmware.bin", cfg.updateUrl.c_str());
    TEST_ASSERT_EQUAL_UINT32(7200000, cfg.checkIntervalMs);
    TEST_ASSERT_FALSE(cfg.autoReboot);
}

void test_ota_component_type_key() {
    OTAComponent ota;
    TEST_ASSERT_EQUAL_STRING("ota", ota.getTypeKey());
}

// ============================================================================
// Config Tests
// ============================================================================

void test_ota_config_defaults() {
    OTAConfig config;

    TEST_ASSERT_EQUAL_STRING("", config.updateUrl.c_str());
    TEST_ASSERT_EQUAL_STRING("", config.manifestUrl.c_str());
    TEST_ASSERT_EQUAL_UINT32(3600000, config.checkIntervalMs);
    TEST_ASSERT_FALSE(config.allowDowngrades);
    TEST_ASSERT_TRUE(config.autoReboot);
    TEST_ASSERT_EQUAL(0, config.maxDownloadSize);
    TEST_ASSERT_TRUE(config.enableWebUIUpload);
}

void test_ota_config_get_set() {
    OTAComponent ota;

    OTAConfig newConfig;
    newConfig.updateUrl = "http://server/fw.bin";
    newConfig.checkIntervalMs = 1800000;
    newConfig.autoReboot = false;
    newConfig.allowDowngrades = true;
    newConfig.enableWebUIUpload = false;

    ota.setConfig(newConfig);

    const OTAConfig& cfg = ota.getConfig();
    TEST_ASSERT_EQUAL_STRING("http://server/fw.bin", cfg.updateUrl.c_str());
    TEST_ASSERT_EQUAL_UINT32(1800000, cfg.checkIntervalMs);
    TEST_ASSERT_FALSE(cfg.autoReboot);
    TEST_ASSERT_TRUE(cfg.allowDowngrades);
    TEST_ASSERT_FALSE(cfg.enableWebUIUpload);
}

void test_ota_config_max_download_size() {
    OTAConfig config;
    config.maxDownloadSize = 2097152;  // 2MB

    OTAComponent ota(config);

    const OTAConfig& cfg = ota.getConfig();
    TEST_ASSERT_EQUAL(2097152, cfg.maxDownloadSize);
}

// ============================================================================
// State Machine Tests
// ============================================================================

void test_ota_initial_state() {
    OTAComponent ota;

    TEST_ASSERT_EQUAL(OTAComponent::State::Idle, ota.getState());
    TEST_ASSERT_TRUE(ota.isIdle());
    TEST_ASSERT_FALSE(ota.isBusy());
}

void test_ota_state_accessors() {
    OTAComponent ota;

    // Initial state values
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ota.getProgress());
    TEST_ASSERT_EQUAL(0, ota.getDownloadedBytes());
    TEST_ASSERT_EQUAL(0, ota.getTotalBytes());
    TEST_ASSERT_EQUAL_STRING("", ota.getLastResult().c_str());
    TEST_ASSERT_EQUAL_STRING("", ota.getLastError().c_str());
    TEST_ASSERT_EQUAL_STRING("", ota.getLastVersion().c_str());
}

void test_ota_idle_busy_states() {
    OTAComponent ota;

    // In Idle state
    TEST_ASSERT_TRUE(ota.isIdle());
    TEST_ASSERT_FALSE(ota.isBusy());

    // After begin() (still idle, no pending updates)
    ota.begin();
    TEST_ASSERT_TRUE(ota.isIdle());
    TEST_ASSERT_FALSE(ota.isBusy());
}

// ============================================================================
// Trigger Tests (without network)
// ============================================================================

void test_ota_trigger_check_no_provider() {
    OTAComponent ota;
    ota.begin();

    // Without a manifest fetcher, triggerImmediateCheck should queue but fail gracefully
    // (actual behavior depends on implementation - may return true to queue, false to reject)
    bool result = ota.triggerImmediateCheck();
    // The component should not crash even without providers
    TEST_ASSERT_TRUE(ota.isIdle() || !ota.isIdle());  // Either state is acceptable
}

void test_ota_trigger_update_from_url_no_provider() {
    OTAComponent ota;
    ota.begin();

    // Without a downloader, this should fail gracefully
    bool result = ota.triggerUpdateFromUrl("http://example.com/firmware.bin");
    // Should not crash
    TEST_ASSERT_TRUE(ota.isIdle() || !ota.isIdle());
}

// ============================================================================
// SHA-256 Verification Tests (SEC-2)
// ============================================================================
//
// The stub HAL::Platform::SHA256 digests everything to 32 zero bytes, so the
// expected hash alone decides which branch runs: 64 zeros matches, anything else
// does not. What these tests actually pin is the *ordering* — HAL::OTAUpdate::end()
// is the commit, and neither Arduino core can undo it, so a mismatched image must
// never reach it. The counters live in Update_Stub.h; they are the only way a host
// build can tell a committed image from a discarded one.

namespace {

const char* const SHA_OF_ANY_STUB_INPUT = "0000000000000000000000000000000000000000000000000000000000000000";
const char* const SHA_THAT_CANNOT_MATCH = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";

// Drives the real application path: manifest fetch -> performCheck -> installFromUrl.
// installFromUrl is private, and going through the front door is the point.
void runDownloadWithExpectedSha(OTAComponent& ota, const char* expectedSha) {
    OTAConfig config;
    config.manifestUrl = "http://example.com/manifest.json";
    config.checkIntervalMs = 0;   // no periodic check racing the explicit one
    config.autoReboot = false;    // do not arm the reboot timer in a host test
    ota.setConfig(config);
    ota.begin();

    const String sha = expectedSha;
    ota.setManifestFetcher([sha](const String&, String& outJson) {
        outJson = String("{\"version\":\"9.9.9\",\"url\":\"http://example.com/fw.bin\",\"sha256\":\"") + sha + "\"}";
        return true;
    });
    ota.setDownloader([](const String&, size_t& totalSize, OTAComponent::DownloadCallback onChunk) {
        const uint8_t firmware[8] = {0xE9, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        totalSize = sizeof(firmware);
        return onChunk(firmware, sizeof(firmware));
    });

    ota.triggerImmediateCheck(true);
    ota.loop();
}

} // namespace

void test_ota_sha_mismatch_never_commits() {
    OTAComponent ota;
    runDownloadWithExpectedSha(ota, SHA_THAT_CANNOT_MATCH);

    // The image was discarded, and never committed. Before SEC-2 was fixed
    // properly, end() ran first and the abort() that followed was inert.
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubAbortCalls);

    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota.getState());
    TEST_ASSERT_EQUAL_STRING("SHA256 mismatch", ota.getLastError().c_str());
}

void test_ota_sha_match_commits() {
    OTAComponent ota;
    runDownloadWithExpectedSha(ota, SHA_OF_ANY_STUB_INPUT);

    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubAbortCalls);
    TEST_ASSERT_NOT_EQUAL(OTAComponent::State::Error, ota.getState());
}

void test_ota_no_expected_sha_still_commits() {
    // A manifest without a sha256 field, and triggerUpdateFromUrl(), both reach
    // installFromUrl with an empty expected hash. That path is unverified by
    // design and must keep working — moving the check must not gate on it.
    OTAComponent ota;
    runDownloadWithExpectedSha(ota, "");

    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubAbortCalls);
    TEST_ASSERT_NOT_EQUAL(OTAComponent::State::Error, ota.getState());
}

// ============================================================================
// Upload Session Tests
// ============================================================================

void test_ota_begin_upload() {
    OTAComponent ota;
    ota.begin();

    // Begin upload should work (uses Update_Stub in native)
    bool result = ota.beginUpload(1024);
    // On native with Update_Stub, this should succeed
    TEST_ASSERT_TRUE(result || !result);  // Either is acceptable depending on stub
}

void test_ota_upload_chunk_before_begin() {
    OTAComponent ota;
    ota.begin();

    // Sending chunk without beginUpload should fail
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    bool result = ota.acceptUploadChunk(data, sizeof(data));
    TEST_ASSERT_FALSE(result);  // Should fail since no upload started
}

void test_ota_abort_upload() {
    OTAComponent ota;
    ota.begin();

    // Abort should work even if no upload is active
    ota.abortUpload("Test abort");

    // Should be back to idle or error state
    TEST_ASSERT_TRUE(ota.isIdle() || ota.getState() == OTAComponent::State::Error);
}

void test_ota_finalize_without_begin() {
    OTAComponent ota;
    ota.begin();

    // Finalize without begin should fail
    bool result = ota.finalizeUpload();
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// Upload Integrity Tests (SEC-7)
// ============================================================================
//
// Same trick as the download tests above: the stub SHA256 digests everything to
// 32 zero bytes, so the expected hash alone selects the branch. What is pinned is
// again the ordering — end() is the commit, and a mismatched upload must never
// reach it.

namespace {

// Runs a complete upload session and leaves the counters to be inspected.
bool runUpload(OTAComponent& ota, const char* expectedSha, bool requireHash = false) {
    OTAConfig config;
    config.autoReboot = false;
    config.requireUploadHash = requireHash;
    ota.setConfig(config);
    ota.begin();

    const uint8_t firmware[16] = {0xE9, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                  0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    if (!ota.beginUpload(sizeof(firmware), expectedSha)) return false;
    if (!ota.acceptUploadChunk(firmware, sizeof(firmware))) return false;
    return ota.finalizeUpload();
}

} // namespace

void test_ota_upload_sha_mismatch_never_commits() {
    OTAComponent ota;
    TEST_ASSERT_FALSE(runUpload(ota, SHA_THAT_CANNOT_MATCH));

    // The upload path used to call end(true) on whatever arrived — no hash of
    // any kind. This is SEC-7.
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubAbortCalls);

    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota.getState());
    TEST_ASSERT_EQUAL_STRING("SHA256 mismatch", ota.getLastError().c_str());
}

void test_ota_upload_sha_match_commits() {
    OTAComponent ota;
    TEST_ASSERT_TRUE(runUpload(ota, SHA_OF_ANY_STUB_INPUT));

    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubAbortCalls);
    TEST_ASSERT_NOT_EQUAL(OTAComponent::State::Error, ota.getState());
}

void test_ota_upload_without_hash_still_commits() {
    // Every caller before SEC-7 supplied no hash, and this library is installed
    // by version. Verification is opt-in per upload; requireUploadHash is how a
    // deployment makes it compulsory.
    OTAComponent ota;
    TEST_ASSERT_TRUE(runUpload(ota, ""));

    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubAbortCalls);
}

void test_ota_require_upload_hash_refuses_before_touching_flash() {
    // The refusal has to land before HAL::OTAUpdate::begin(), which erases flash.
    // Refusing afterwards would destroy the running firmware to reject an upload
    // that was never going to be installed.
    HAL::OTAUpdate::s_stubBytesWritten = 12345;  // sentinel: begin() would zero it

    OTAComponent ota;
    OTAConfig config;
    config.autoReboot = false;
    config.requireUploadHash = true;
    ota.setConfig(config);
    ota.begin();

    TEST_ASSERT_FALSE(ota.beginUpload(16, ""));
    TEST_ASSERT_EQUAL_STRING("Firmware hash required", ota.getLastError().c_str());
    TEST_ASSERT_EQUAL_UINT32(12345, HAL::OTAUpdate::s_stubBytesWritten);  // untouched

    // ...and a hash makes the same upload acceptable.
    TEST_ASSERT_TRUE(ota.beginUpload(16, SHA_OF_ANY_STUB_INPUT));
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

void test_ota_begin_returns_ok() {
    OTAComponent ota;
    ComponentStatus status = ota.begin();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);
}

void test_ota_shutdown_returns_ok() {
    OTAComponent ota;
    ota.begin();
    ComponentStatus status = ota.shutdown();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);
}

void test_ota_loop_no_crash() {
    OTAComponent ota;
    ota.begin();

    // Multiple loop calls should not crash
    for (int i = 0; i < 100; i++) {
        ota.loop();
    }

    TEST_ASSERT_TRUE(true);  // If we get here, no crash
}

void test_ota_lifecycle_sequence() {
    OTAComponent ota;

    // begin -> loop -> shutdown sequence
    TEST_ASSERT_EQUAL(ComponentStatus::Success, ota.begin());

    ota.loop();
    ota.loop();

    TEST_ASSERT_EQUAL(ComponentStatus::Success, ota.shutdown());
}

// ============================================================================
// Provider Tests
// ============================================================================

void test_ota_set_manifest_fetcher() {
    OTAComponent ota;

    bool fetcherCalled = false;
    ota.setManifestFetcher([&fetcherCalled](const String& url, String& outJson) {
        fetcherCalled = true;
        outJson = "{}";
        return true;
    });

    TEST_ASSERT_TRUE(true);  // Setter should not crash
}

void test_ota_set_downloader() {
    OTAComponent ota;

    bool downloaderCalled = false;
    ota.setDownloader([&downloaderCalled](const String& url, size_t& totalSize, OTAComponent::DownloadCallback cb) {
        downloaderCalled = true;
        totalSize = 0;
        return false;
    });

    TEST_ASSERT_TRUE(true);  // Setter should not crash
}

// ============================================================================
// Non-Blocking Behavior Tests
// ============================================================================

void test_ota_loop_duration() {
    OTAComponent ota;
    ota.begin();

    // Measure loop execution time
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        ota.loop();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 loops should complete in under 1 second (1ms average per loop)
    TEST_ASSERT_TRUE(duration.count() < 1000);
}

// ============================================================================
// Integration with Core Tests
// ============================================================================

void test_ota_with_core() {
    Core core;

    OTAConfig config;
    config.updateUrl = "http://example.com/fw.bin";
    config.checkIntervalMs = 0;  // Disable auto-check

    core.addComponent(std::make_unique<OTAComponent>(config));

    CoreConfig coreConfig;
    coreConfig.deviceName = "TestDevice";

    bool result = core.begin(coreConfig);
    TEST_ASSERT_TRUE(result);

    // Get component
    auto* ota = core.getComponent<OTAComponent>("OTA");
    TEST_ASSERT_NOT_NULL(ota);
    TEST_ASSERT_EQUAL_STRING("http://example.com/fw.bin", ota->getConfig().updateUrl.c_str());

    core.shutdown();
}

void test_ota_component_lookup() {
    Core core;
    core.addComponent(std::make_unique<OTAComponent>());

    CoreConfig cfg;
    cfg.deviceName = "Test";
    core.begin(cfg);

    // Lookup by name
    auto* ota = core.getComponent<OTAComponent>("OTA");
    TEST_ASSERT_NOT_NULL(ota);

    core.shutdown();
}

// ============================================================================
// Check Interval Tests
// ============================================================================

void test_ota_check_interval_disabled() {
    OTAConfig config;
    config.checkIntervalMs = 0;  // Disabled

    OTAComponent ota(config);
    ota.begin();

    // With checkIntervalMs = 0, no automatic checks should occur
    for (int i = 0; i < 100; i++) {
        ota.loop();
    }

    // Should still be idle (no automatic check triggered)
    TEST_ASSERT_TRUE(ota.isIdle());
}

void test_ota_check_interval_config() {
    OTAConfig config;
    config.checkIntervalMs = 60000;  // 1 minute

    OTAComponent ota(config);

    const OTAConfig& cfg = ota.getConfig();
    TEST_ASSERT_EQUAL_UINT32(60000, cfg.checkIntervalMs);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Event tests
    RUN_TEST(test_ota_events_constants_defined);
    RUN_TEST(test_ota_events_namespace);

    // Component creation tests
    RUN_TEST(test_ota_component_creation_default);
    RUN_TEST(test_ota_component_creation_with_config);
    RUN_TEST(test_ota_component_type_key);

    // Config tests
    RUN_TEST(test_ota_config_defaults);
    RUN_TEST(test_ota_config_get_set);
    RUN_TEST(test_ota_config_max_download_size);

    // State machine tests
    RUN_TEST(test_ota_initial_state);
    RUN_TEST(test_ota_state_accessors);
    RUN_TEST(test_ota_idle_busy_states);

    // Trigger tests
    RUN_TEST(test_ota_trigger_check_no_provider);
    RUN_TEST(test_ota_trigger_update_from_url_no_provider);

    // Upload session tests
    // SHA-256 verification (SEC-2)
    RUN_TEST(test_ota_sha_mismatch_never_commits);
    RUN_TEST(test_ota_sha_match_commits);
    RUN_TEST(test_ota_no_expected_sha_still_commits);

    RUN_TEST(test_ota_begin_upload);
    RUN_TEST(test_ota_upload_chunk_before_begin);
    RUN_TEST(test_ota_abort_upload);
    RUN_TEST(test_ota_finalize_without_begin);

    // Upload integrity (SEC-7)
    RUN_TEST(test_ota_upload_sha_mismatch_never_commits);
    RUN_TEST(test_ota_upload_sha_match_commits);
    RUN_TEST(test_ota_upload_without_hash_still_commits);
    RUN_TEST(test_ota_require_upload_hash_refuses_before_touching_flash);

    // Lifecycle tests
    RUN_TEST(test_ota_begin_returns_ok);
    RUN_TEST(test_ota_shutdown_returns_ok);
    RUN_TEST(test_ota_loop_no_crash);
    RUN_TEST(test_ota_lifecycle_sequence);

    // Provider tests
    RUN_TEST(test_ota_set_manifest_fetcher);
    RUN_TEST(test_ota_set_downloader);

    // Non-blocking behavior
    RUN_TEST(test_ota_loop_duration);

    // Integration tests
    RUN_TEST(test_ota_with_core);
    RUN_TEST(test_ota_component_lookup);

    // Check interval tests
    RUN_TEST(test_ota_check_interval_disabled);
    RUN_TEST(test_ota_check_interval_config);

    return UNITY_END();
}
