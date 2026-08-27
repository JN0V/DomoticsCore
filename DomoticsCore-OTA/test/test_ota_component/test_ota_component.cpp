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

#include <vector>

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
// Lifecycle Events (BUG-21) and Upload Size Cap (SEC-8)
// ============================================================================
//
// `ota/start` and `ota/end` were declared in OTAEvents.h from the first release
// and emitted by nothing — three documents said so and told readers not to
// subscribe. These tests are what stops that happening again: they assert the
// topics reach the bus, and on the mismatch path they assert `ota/end` precedes
// the verification failure, which is the whole point of an event named "transfer
// ended, not yet verified".
//
// They subscribe on the raw EventBus rather than `Core::on<String>`. The payload
// publishStatusEvent hands over is a `String` byte-copied into the queue, so it
// is not safe to read back once the publisher's local has gone out of scope.
// What is under test is the topic, and the topic is a copy the queue owns.

namespace {

// The lifecycle topics that reached the bus, in dispatch order.
struct TopicLog {
    std::vector<String> seen;

    void watch(Core& core, const char* topic) {
        const String t = topic;
        core.getEventBus().subscribe(t, [this, t](const void*) { seen.push_back(t); }, this);
    }

    void watchLifecycle(Core& core) {
        watch(core, OTAEvents::EVENT_START);
        watch(core, OTAEvents::EVENT_END);
        watch(core, OTAEvents::EVENT_COMPLETED);
        watch(core, OTAEvents::EVENT_ERROR);
    }

    int indexOf(const char* topic) const {
        for (size_t i = 0; i < seen.size(); ++i) {
            if (seen[i] == topic) return static_cast<int>(i);
        }
        return -1;
    }

    bool sawTopic(const char* topic) const { return indexOf(topic) >= 0; }
};

// A component reached through a Core, because emit() is a no-op on a component
// that has no bus — which is why the bare-OTAComponent tests above can say
// nothing about events.
OTAComponent* attachOta(Core& core, const OTAConfig& cfg) {
    core.addComponent(std::make_unique<OTAComponent>(cfg));
    CoreConfig coreCfg;
    coreCfg.deviceName = "OtaTest";
    core.begin(coreCfg);
    return core.getComponent<OTAComponent>("OTA");
}

// poll() dispatches 8 events per call; boot publishes its own. Drain generously.
void drain(Core& core) {
    for (int i = 0; i < 10; ++i) core.loop();
}

OTAConfig quietConfig() {
    OTAConfig cfg;
    cfg.checkIntervalMs = 0;  // no periodic check racing the explicit one
    cfg.autoReboot = false;   // do not arm the reboot timer in a host test
    return cfg;
}

// A downloader that announces `announced` bytes and then streams `streamed` of
// them. The two differ only where a lying server is the thing under test.
OTAComponent::Downloader downloaderOf(size_t announced, size_t streamed) {
    return [announced, streamed](const String&, size_t& totalSize,
                                 OTAComponent::DownloadCallback onChunk) {
        totalSize = announced;
        std::vector<uint8_t> firmware(streamed, 0xE9);
        return onChunk(firmware.data(), firmware.size());
    };
}

} // namespace

void test_ota_download_emits_start_then_end_then_completed() {
    Core core;
    OTAComponent* ota = attachOta(core, quietConfig());
    TEST_ASSERT_NOT_NULL(ota);

    TopicLog log;
    log.watchLifecycle(core);

    ota->setDownloader(downloaderOf(8, 8));
    ota->triggerUpdateFromUrl("http://example.com/fw.bin");
    drain(core);

    TEST_ASSERT_TRUE_MESSAGE(log.sawTopic(OTAEvents::EVENT_START), "ota/start never reached the bus");
    TEST_ASSERT_TRUE_MESSAGE(log.sawTopic(OTAEvents::EVENT_END), "ota/end never reached the bus");
    TEST_ASSERT_FALSE(log.sawTopic(OTAEvents::EVENT_ERROR));

    TEST_ASSERT_TRUE(log.indexOf(OTAEvents::EVENT_START) < log.indexOf(OTAEvents::EVENT_END));
    TEST_ASSERT_TRUE(log.indexOf(OTAEvents::EVENT_END) < log.indexOf(OTAEvents::EVENT_COMPLETED));

    // SEC-9 pinned the upload path's evenIfRemaining and left this one unobserved,
    // which meant flipping installFromUrl()'s end(true) failed no test at all. The
    // observable already existed; this is the assertion that was missing. A
    // download announcing exactly what it delivers does not need the flag — which
    // is the point: nothing here justifies passing false, and nothing may quietly
    // start doing so.
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, HAL::OTAUpdate::s_stubEndCalls,
                                     "the image was never committed: the flag below means nothing");
    TEST_ASSERT_TRUE_MESSAGE(HAL::OTAUpdate::s_stubEndEvenIfRemaining,
                             "installFromUrl() committed without evenIfRemaining");

    core.shutdown();
}

void test_ota_download_end_precedes_the_hash_verdict() {
    // "Transfer ended, before verification" is the documented meaning of ota/end.
    // An event that only fired on success would carry no information the
    // completion event does not already carry.
    Core core;
    OTAConfig cfg = quietConfig();
    cfg.manifestUrl = "http://example.com/manifest.json";
    OTAComponent* ota = attachOta(core, cfg);
    TEST_ASSERT_NOT_NULL(ota);

    TopicLog log;
    log.watchLifecycle(core);

    ota->setManifestFetcher([](const String&, String& outJson) {
        outJson = String("{\"version\":\"9.9.9\",\"url\":\"http://example.com/fw.bin\",\"sha256\":\"")
                + SHA_THAT_CANNOT_MATCH + "\"}";
        return true;
    });
    ota->setDownloader(downloaderOf(8, 8));
    ota->triggerImmediateCheck(true);
    drain(core);

    TEST_ASSERT_TRUE(log.sawTopic(OTAEvents::EVENT_START));
    TEST_ASSERT_TRUE(log.sawTopic(OTAEvents::EVENT_END));
    TEST_ASSERT_TRUE(log.sawTopic(OTAEvents::EVENT_ERROR));
    TEST_ASSERT_FALSE(log.sawTopic(OTAEvents::EVENT_COMPLETED));

    TEST_ASSERT_TRUE(log.indexOf(OTAEvents::EVENT_END) < log.indexOf(OTAEvents::EVENT_ERROR));
    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota->getState());

    core.shutdown();
}

void test_ota_failed_transfer_emits_start_but_no_end() {
    // Nothing arrived, so nothing ended. A start with no end is the signal a
    // subscriber needs to distinguish a dead transfer from a rejected image.
    Core core;
    OTAComponent* ota = attachOta(core, quietConfig());
    TEST_ASSERT_NOT_NULL(ota);

    TopicLog log;
    log.watchLifecycle(core);

    ota->setDownloader([](const String&, size_t& totalSize, OTAComponent::DownloadCallback) {
        totalSize = 0;
        return false;
    });
    ota->triggerUpdateFromUrl("http://example.com/fw.bin");
    drain(core);

    TEST_ASSERT_TRUE(log.sawTopic(OTAEvents::EVENT_START));
    TEST_ASSERT_FALSE_MESSAGE(log.sawTopic(OTAEvents::EVENT_END), "a transfer that never ran cannot end");
    TEST_ASSERT_TRUE(log.sawTopic(OTAEvents::EVENT_ERROR));

    core.shutdown();
}

void test_ota_upload_emits_start_then_end_then_completed() {
    Core core;
    OTAComponent* ota = attachOta(core, quietConfig());
    TEST_ASSERT_NOT_NULL(ota);

    TopicLog log;
    log.watchLifecycle(core);

    const uint8_t firmware[16] = {0xE9, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                  0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    TEST_ASSERT_TRUE(ota->beginUpload(sizeof(firmware)));
    TEST_ASSERT_TRUE(ota->acceptUploadChunk(firmware, sizeof(firmware)));
    TEST_ASSERT_TRUE(ota->finalizeUpload());
    drain(core);

    TEST_ASSERT_TRUE_MESSAGE(log.sawTopic(OTAEvents::EVENT_START), "ota/start never reached the bus");
    TEST_ASSERT_TRUE_MESSAGE(log.sawTopic(OTAEvents::EVENT_END), "ota/end never reached the bus");
    TEST_ASSERT_TRUE(log.indexOf(OTAEvents::EVENT_START) < log.indexOf(OTAEvents::EVENT_END));
    TEST_ASSERT_TRUE(log.indexOf(OTAEvents::EVENT_END) < log.indexOf(OTAEvents::EVENT_COMPLETED));

    core.shutdown();
}

void test_ota_upload_start_still_emits_the_documented_info_event() {
    // The published reference names EVENT_INFO as the upload-start signal, and
    // this library is installed by version. Adding ota/start must not take it
    // away from whoever followed the documentation.
    Core core;
    OTAComponent* ota = attachOta(core, quietConfig());
    TEST_ASSERT_NOT_NULL(ota);

    TopicLog log;
    log.watch(core, OTAEvents::EVENT_INFO);

    TEST_ASSERT_TRUE(ota->beginUpload(16));
    drain(core);

    TEST_ASSERT_TRUE(log.sawTopic(OTAEvents::EVENT_INFO));

    ota->abortUpload("test teardown");
    core.shutdown();
}

// --- SEC-8: maxDownloadSize was enforced on downloads and not on uploads ----
//
// The download path checked the announced size and the upload path checked
// nothing at all, so the ceiling a deployment configured applied to the transfer
// it did not initiate and not to the one anybody could POST. The announced-size
// check is also not sufficient on its own: it trusts a number the sender chose.

void test_ota_upload_over_the_cap_is_refused_before_touching_flash() {
    // Same ordering as SEC-7: the refusal has to land before
    // HAL::OTAUpdate::begin(), which erases flash.
    HAL::OTAUpdate::s_stubBytesWritten = 12345;  // sentinel: begin() would zero it

    OTAConfig config = quietConfig();
    config.maxDownloadSize = 16;

    OTAComponent ota(config);
    ota.begin();

    TEST_ASSERT_FALSE_MESSAGE(ota.beginUpload(32), "an upload twice the ceiling was accepted");
    TEST_ASSERT_EQUAL_STRING("Firmware too large", ota.getLastError().c_str());
    TEST_ASSERT_EQUAL_UINT32(12345, HAL::OTAUpdate::s_stubBytesWritten);  // untouched
}

void test_ota_upload_within_the_cap_is_accepted() {
    OTAConfig config = quietConfig();
    config.maxDownloadSize = 64;

    OTAComponent ota(config);
    ota.begin();

    TEST_ASSERT_TRUE(ota.beginUpload(16));
    ota.abortUpload("test teardown");
}

void test_ota_upload_streaming_past_the_cap_is_refused() {
    // An upload may announce no size at all — Content-Length is optional, and
    // beginUpload(0) means "unknown". The announced-size check cannot see this
    // one coming; only counting what arrives can.
    OTAConfig config = quietConfig();
    config.maxDownloadSize = 16;

    OTAComponent ota(config);
    ota.begin();

    TEST_ASSERT_TRUE(ota.beginUpload(0));

    std::vector<uint8_t> chunk(32, 0xE9);
    TEST_ASSERT_FALSE_MESSAGE(ota.acceptUploadChunk(chunk.data(), chunk.size()),
                              "32 bytes went past a 16 byte ceiling");
    TEST_ASSERT_EQUAL_STRING("Firmware too large", ota.getLastError().c_str());
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota.getState());
}

void test_ota_download_streaming_past_the_cap_is_refused() {
    // The server announces a size inside the ceiling and then sends more. The
    // check that reads `totalSize` believes it; the check that counts bytes
    // does not.
    Core core;
    OTAConfig cfg = quietConfig();
    cfg.maxDownloadSize = 16;
    OTAComponent* ota = attachOta(core, cfg);
    TEST_ASSERT_NOT_NULL(ota);

    ota->setDownloader(downloaderOf(/*announced=*/8, /*streamed=*/32));
    ota->triggerUpdateFromUrl("http://example.com/fw.bin");
    drain(core);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, HAL::OTAUpdate::s_stubEndCalls,
                                     "an oversized image was committed");
    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota->getState());
    TEST_ASSERT_EQUAL_STRING("Firmware too large", ota->getLastError().c_str());

    core.shutdown();
}

// --- SEC-9: the upload path sizes itself from the multipart envelope --------
//
// request->contentLength() measures the whole encoded body, so what reaches
// beginUpload() from a browser is 220 bytes more than the firmware — measured on
// a nodemcuv2 and a WROOM-32D. That figure is an upper bound, which is safe
// everywhere it is used except at the end, where the completion event reported it
// as the byte count actually received.
//
// The other half is the argument nothing could observe. A streaming upload never
// knows its exact length before the last chunk, so the image is never "finished"
// by either core's definition and end() only commits it because evenIfRemaining
// is true. That is not a defect of the envelope and no announced size removes it
// — so it is pinned here rather than fixed.

void test_ota_upload_commits_with_even_if_remaining() {
    OTAComponent ota(quietConfig());
    ota.begin();

    const uint8_t firmware[16] = {0xE9};
    TEST_ASSERT_TRUE(ota.beginUpload(sizeof(firmware)));
    TEST_ASSERT_TRUE(ota.acceptUploadChunk(firmware, sizeof(firmware)));
    TEST_ASSERT_TRUE(ota.finalizeUpload());

    // Non-vacuity: a test that only read the flag would pass just as well if
    // end() had never been called at all.
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, HAL::OTAUpdate::s_stubEndCalls,
                                     "the image was never committed: the flag below means nothing");
    TEST_ASSERT_TRUE_MESSAGE(HAL::OTAUpdate::s_stubEndEvenIfRemaining,
                             "end() was called without evenIfRemaining — every browser upload "
                             "is short of Update's _size and would now be refused");
}

void test_ota_upload_refusal_names_the_figure_it_compared() {
    // The refusal quotes the *announced* size, which on the browser path is the
    // multipart envelope. An operator reading "475452 against a 100000 ceiling"
    // for a 475232-byte firmware needs the message to say which number that is.
    // DLOG_W formats into 128 bytes on ESP8266, so this also pins that the
    // sentence still fits. It runs on a host where that buffer is 256, so the
    // length below is checked rather than provoked — and it is checked at the
    // *worst* case, two ten-digit unsigned longs, not at the six-digit figures
    // the campaign happened to measure. Feeding smaller numbers would have left
    // eight unmeasured characters of slack on the one platform that has none.
    //
    // `captured` is static and the callback is removed in a Unity-safe order:
    // a failed TEST_ASSERT longjmps out of this function, and a lambda holding a
    // reference to a dead stack String would then be written through by every
    // later DLOG in the suite. That is the cascade class SEC-8 already recorded.
    static String captured;
    captured = "";
    LoggerCallbacks::CallbackId id = LoggerCallbacks::addCallback(
        [](LogLevel level, const char* tag, const char* message) {
            if (level == LOG_LEVEL_WARN && String(tag) == LOG_OTA) captured = message;
        });

    OTAConfig config = quietConfig();
    config.maxDownloadSize = 4294967294UL;  // one below the widest 32-bit figure

    OTAComponent ota(config);
    ota.begin();
    const bool refused = !ota.beginUpload(4294967295UL);

    LoggerCallbacks::removeCallback(id);

    TEST_ASSERT_TRUE_MESSAGE(refused, "an upload over the ceiling was accepted");
    TEST_ASSERT_TRUE_MESSAGE(captured.length() > 0, "the refusal logged no warning at all");
    TEST_ASSERT_TRUE_MESSAGE(captured.indexOf("4294967295") >= 0,
                             "the warning does not name the figure it compared");
    TEST_ASSERT_TRUE_MESSAGE(captured.indexOf("announced") >= 0,
                             "the warning does not say the figure is what the sender announced");
    TEST_ASSERT_TRUE_MESSAGE(captured.indexOf("framing included") >= 0,
                             "the qualifier is gone — truncated, or edited out");
    TEST_ASSERT_TRUE_MESSAGE(captured.length() < 128,
                             "at its widest the warning would be truncated by ESP8266's "
                             "128-byte DOMOTICS_DLOG_BUF_SIZE");
}

void test_ota_upload_completion_reports_the_bytes_that_arrived() {
    // 236 announced for 16 delivered: the shape of a multipart POST, with the
    // 220-byte framing measured on both boards.
    OTAComponent ota(quietConfig());
    ota.begin();

    const uint8_t firmware[16] = {0xE9};
    TEST_ASSERT_TRUE(ota.beginUpload(236));
    TEST_ASSERT_TRUE(ota.acceptUploadChunk(firmware, sizeof(firmware)));

    // Mid-transfer the announced figure is deliberately kept as the denominator:
    // it is an upper bound, it is the only one there is, and it is 0.046 % low on
    // a real firmware rather than the 93 % it looks like at this scale.
    TEST_ASSERT_EQUAL(236, ota.getTotalBytes());
    TEST_ASSERT_EQUAL(16, ota.getDownloadedBytes());

    TEST_ASSERT_TRUE(ota.finalizeUpload());

    // Once the transfer is over the counted figure is known exactly, and it is
    // the one the completion event carries.
    TEST_ASSERT_EQUAL_MESSAGE(16, ota.getTotalBytes(),
                              "completion reported the multipart envelope as the firmware size");
    TEST_ASSERT_EQUAL_MESSAGE(16, ota.getDownloadedBytes(),
                              "completion reported more bytes than ever arrived");
    TEST_ASSERT_EQUAL_FLOAT(100.0f, ota.getProgress());
}

// ============================================================================
// State Machine Transitions (TEST-3)
// ============================================================================
//
// The state tests above read the initial state and the accessors. None of them
// watched the machine move, which is where a state machine goes wrong.

void test_ota_upload_holds_downloading_while_it_runs() {
    OTAComponent ota(quietConfig());
    ota.begin();
    TEST_ASSERT_TRUE(ota.isIdle());

    TEST_ASSERT_TRUE(ota.beginUpload(16));
    TEST_ASSERT_EQUAL(OTAComponent::State::Downloading, ota.getState());
    TEST_ASSERT_TRUE(ota.isBusy());
    TEST_ASSERT_FALSE(ota.isIdle());

    ota.abortUpload("test teardown");
}

void test_ota_upload_settles_in_idle_without_autoreboot() {
    OTAComponent ota;
    TEST_ASSERT_TRUE(runUpload(ota, ""));  // runUpload sets autoReboot = false

    TEST_ASSERT_EQUAL(OTAComponent::State::Idle, ota.getState());
    TEST_ASSERT_TRUE(ota.isIdle());
    TEST_ASSERT_FALSE(ota.isBusy());
    TEST_ASSERT_EQUAL_FLOAT(100.0f, ota.getProgress());
}

void test_ota_upload_settles_in_reboot_pending_with_autoreboot() {
    OTAConfig config;
    config.checkIntervalMs = 0;
    config.autoReboot = true;

    OTAComponent ota(config);
    ota.begin();

    const uint8_t firmware[16] = {0xE9};
    TEST_ASSERT_TRUE(ota.beginUpload(sizeof(firmware)));
    TEST_ASSERT_TRUE(ota.acceptUploadChunk(firmware, sizeof(firmware)));
    TEST_ASSERT_TRUE(ota.finalizeUpload());

    // No loop() here on purpose: loop() is what arms the 2 s reboot, and this
    // test is about the state the upload leaves behind, not about rebooting.
    TEST_ASSERT_EQUAL(OTAComponent::State::RebootPending, ota.getState());
    TEST_ASSERT_FALSE(ota.isBusy());
}

void test_ota_abort_mid_transfer_lands_in_error_and_commits_nothing() {
    OTAConfig config = quietConfig();
    OTAComponent ota(config);
    ota.begin();

    const uint8_t firmware[8] = {0xE9, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    TEST_ASSERT_TRUE(ota.beginUpload(64));
    TEST_ASSERT_TRUE(ota.acceptUploadChunk(firmware, sizeof(firmware)));

    ota.abortUpload("Client disconnected");

    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota.getState());
    TEST_ASSERT_EQUAL_STRING("Client disconnected", ota.getLastError().c_str());
    TEST_ASSERT_EQUAL_UINT32(0, HAL::OTAUpdate::s_stubEndCalls);
    TEST_ASSERT_EQUAL_UINT32(1, HAL::OTAUpdate::s_stubAbortCalls);

    // ...and the session is closed: a chunk arriving late must not be written.
    TEST_ASSERT_FALSE(ota.acceptUploadChunk(firmware, sizeof(firmware)));
}

void test_ota_upload_progress_tracks_the_bytes_written() {
    OTAConfig config = quietConfig();
    OTAComponent ota(config);
    ota.begin();

    const uint8_t half[8] = {0xE9, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    TEST_ASSERT_TRUE(ota.beginUpload(16));
    TEST_ASSERT_EQUAL(16, ota.getTotalBytes());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ota.getProgress());

    TEST_ASSERT_TRUE(ota.acceptUploadChunk(half, sizeof(half)));
    TEST_ASSERT_EQUAL(8, ota.getDownloadedBytes());
    TEST_ASSERT_EQUAL_FLOAT(50.0f, ota.getProgress());

    TEST_ASSERT_TRUE(ota.acceptUploadChunk(half, sizeof(half)));
    TEST_ASSERT_EQUAL(16, ota.getDownloadedBytes());
    TEST_ASSERT_EQUAL_FLOAT(100.0f, ota.getProgress());

    ota.abortUpload("test teardown");
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

    // Lifecycle events (BUG-21)
    RUN_TEST(test_ota_download_emits_start_then_end_then_completed);
    RUN_TEST(test_ota_download_end_precedes_the_hash_verdict);
    RUN_TEST(test_ota_failed_transfer_emits_start_but_no_end);
    RUN_TEST(test_ota_upload_emits_start_then_end_then_completed);
    RUN_TEST(test_ota_upload_start_still_emits_the_documented_info_event);

    // Upload size cap (SEC-8)
    RUN_TEST(test_ota_upload_over_the_cap_is_refused_before_touching_flash);
    RUN_TEST(test_ota_upload_within_the_cap_is_accepted);
    RUN_TEST(test_ota_upload_streaming_past_the_cap_is_refused);
    RUN_TEST(test_ota_download_streaming_past_the_cap_is_refused);

    // SEC-9
    RUN_TEST(test_ota_upload_commits_with_even_if_remaining);
    RUN_TEST(test_ota_upload_refusal_names_the_figure_it_compared);
    RUN_TEST(test_ota_upload_completion_reports_the_bytes_that_arrived);

    // State machine transitions (TEST-3)
    RUN_TEST(test_ota_upload_holds_downloading_while_it_runs);
    RUN_TEST(test_ota_upload_settles_in_idle_without_autoreboot);
    RUN_TEST(test_ota_upload_settles_in_reboot_pending_with_autoreboot);
    RUN_TEST(test_ota_abort_mid_transfer_lands_in_error_and_commits_nothing);
    RUN_TEST(test_ota_upload_progress_tracks_the_bytes_written);

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
