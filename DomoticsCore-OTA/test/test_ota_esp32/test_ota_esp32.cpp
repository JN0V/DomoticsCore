/**
 * @file test_ota_esp32.cpp
 * @brief SEC-2 and SEC-7 on an ESP32, where the commit is a boot-partition switch.
 *
 * The ESP8266 suite reads back the eboot command. There is no such thing here.
 * On an ESP32 `Update.end(true)` reaches `_verifyEnd()`, which calls
 * `esp_ota_set_boot_partition()` — so the observable is which partition the
 * bootloader will pick, and `esp_ota_get_boot_partition()` answers that
 * directly. A rejected image must leave it pointing at the running partition.
 *
 * Both halves of SEC-2 were argued from the vendored `Updater.cpp`, and only the
 * ESP8266 half was ever run on silicon. This is the other half.
 *
 * ## Why the commit test copies the running application
 *
 * `esp_ota_set_boot_partition()` validates the image before accepting it, so a
 * synthetic payload with a plausible header cannot be committed at all — `end()`
 * fails with UPDATE_ERROR_ACTIVATE and proves nothing about the ordering. The
 * only readily available *valid* image is the one already running, so the commit
 * test streams the running partition into the spare OTA slot.
 *
 * That also makes it the safest possible payload: if the restore below ever
 * failed and the board booted the copy, it would boot exactly what it is running
 * now. The negative tests, which must never reach `end()`, use a cheap synthetic
 * payload instead — there is no reason to move a megabyte to prove a rejection.
 */

#include <unity.h>
#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include <DomoticsCore/OTA.h>
#include <DomoticsCore/Update_HAL.h>
#include <DomoticsCore/Platform_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

const esp_partition_t* g_running = nullptr;

/**
 * One whole flash block, not one sector.
 *
 * `Update::_writeBuffer()` picks its erase strategy from the declared size. At a
 * single sector it takes the `part_tail_sectors` branch — the one meant for a
 * partition's unaligned tail — and `esp_flash_erase_region()` calls `abort()`
 * from underneath it, panicking the board. A block-aligned size takes the
 * ordinary `block_erase` path instead, which is the path every real firmware
 * takes. No firmware is 4 KB; a test that pretends otherwise tests a code path
 * that never runs in production and crashes on the way.
 */
constexpr size_t SYNTH_SIZE = 65536;
constexpr size_t CHUNK_SIZE = 512;

// Generated per chunk rather than held in a 64 KB array: this board has PSRAM,
// but a test has no business reserving that much DRAM to prove a rejection.
uint8_t g_chunk[CHUNK_SIZE];

const char* const HASH_THAT_CANNOT_MATCH =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";

/// True when the bootloader would start something other than what is running.
bool bootPartitionMoved() {
    return esp_ota_get_boot_partition() != g_running;
}

void restoreBootPartition() {
    if (g_running) {
        esp_ota_set_boot_partition(g_running);
    }
}

/**
 * @brief Fill g_chunk with the synthetic payload's bytes at `offset`.
 *
 * A payload Update will accept writes for but never commit: the magic byte gets
 * it past `_writeBuffer`'s header check, which is as far as the negative tests
 * need to go. Deterministic, so the same bytes come back on every pass.
 */
void fillChunk(size_t offset) {
    for (size_t i = 0; i < CHUNK_SIZE; ++i) {
        g_chunk[i] = static_cast<uint8_t>((offset + i) & 0xFF);
    }
    if (offset == 0) {
        g_chunk[0] = ESP_IMAGE_HEADER_MAGIC;
    }
}

/// Hand the synthetic payload to a sink, one chunk at a time.
template <typename Sink>
bool streamSynthetic(Sink sink) {
    for (size_t offset = 0; offset < SYNTH_SIZE; offset += CHUNK_SIZE) {
        fillChunk(offset);
        if (!sink(g_chunk, CHUNK_SIZE)) return false;
        yield();
    }
    return true;
}

/// Stream the running application, hashing it and/or handing it to a sink.
template <typename Sink>
bool streamRunningApp(Sink sink, HAL::SHA256* sha) {
    static uint8_t buf[1024];
    const size_t total = ESP.getSketchSize();
    for (size_t offset = 0; offset < total; offset += sizeof(buf)) {
        const size_t len = (total - offset) < sizeof(buf) ? (total - offset) : sizeof(buf);
        if (esp_partition_read(g_running, offset, buf, len) != ESP_OK) return false;
        if (sha) sha->update(buf, len);
        if (!sink(buf, len)) return false;
        yield();
    }
    return true;
}

String runningAppHash() {
    HAL::SHA256 sha;
    streamRunningApp([](const uint8_t*, size_t) { return true; }, &sha);
    uint8_t digest[32];
    sha.finish(digest);
    return HAL::SHA256::toHex(digest, 32);
}

/// Drive a download through the public path: manifest -> check -> installFromUrl.
struct Download {
    OTAComponent ota;

    Download(const String& expectedHash, bool useRunningApp) {
        OTAConfig cfg;
        cfg.manifestUrl = "http://localhost/manifest.json";
        cfg.checkIntervalMs = 0;
        cfg.autoReboot = false;  // a reboot mid-suite would act on anything committed
        ota.setConfig(cfg);
        ota.begin();

        ota.setManifestFetcher([expectedHash](const String&, String& outJson) {
            outJson = String("{\"version\":\"9.9.9\","
                             "\"url\":\"http://localhost/firmware.bin\","
                             "\"sha256\":\"") + expectedHash + "\"}";
            return true;
        });
        ota.setDownloader([useRunningApp](const String&, size_t& totalSize,
                                          OTAComponent::DownloadCallback onChunk) {
            if (useRunningApp) {
                totalSize = ESP.getSketchSize();
                return streamRunningApp(
                    [&onChunk](const uint8_t* d, size_t n) { return onChunk(d, n); }, nullptr);
            }
            totalSize = SYNTH_SIZE;
            return streamSynthetic([&onChunk](const uint8_t* d, size_t n) { return onChunk(d, n); });
        });

        ota.triggerImmediateCheck(true);
        ota.loop();
    }
};

/// Drive an upload through the public path (SEC-7).
struct Upload {
    OTAComponent ota;
    bool opened = false;
    bool committed = false;

    Upload(const String& expectedHash, bool requireHash, bool useRunningApp = false) {
        OTAConfig cfg;
        cfg.autoReboot = false;
        cfg.requireUploadHash = requireHash;
        ota.setConfig(cfg);
        ota.begin();

        auto sink = [this](const uint8_t* d, size_t n) { return ota.acceptUploadChunk(d, n); };

        opened = ota.beginUpload(useRunningApp ? ESP.getSketchSize() : SYNTH_SIZE, expectedHash);
        if (!opened) return;
        if (!(useRunningApp ? streamRunningApp(sink, nullptr) : streamSynthetic(sink))) return;
        committed = ota.finalizeUpload();
    }
};

/**
 * @brief The shape every browser upload has, which no suite had.
 *
 * TEST-8. `Upload` above announces exactly what it delivers, so `_progress ==
 * _size` holds by the time `end()` is called and the `evenIfRemaining` argument
 * is never load-bearing. A multipart POST is not like that: SEC-9 measured
 * `request->contentLength()` at 220 bytes over the firmware on this board, so
 * `_size` is never reached and the commit happens only because
 * `finalizeUpload()` passes `true`.
 *
 * SEC-9 pinned that argument natively, where the stub's `end()` returns true
 * whatever it is given — so the pin stopped at the call site. This is the same
 * claim against the real Updater, and it uses the running application as the
 * payload for the reason the mismatched-hash test does: a synthetic image is
 * refused by `esp_ota_set_boot_partition()` itself, and the test would then pass
 * for the platform's reasons rather than ours.
 */
struct ShortUpload {
    OTAComponent ota;
    bool opened = false;
    bool committed = false;

    static constexpr size_t MULTIPART_OVERHEAD = 220;

    ShortUpload() {
        OTAConfig cfg;
        cfg.autoReboot = false;
        ota.setConfig(cfg);
        ota.begin();

        auto sink = [this](const uint8_t* d, size_t n) { return ota.acceptUploadChunk(d, n); };

        // The digest covers what is delivered, not what is announced.
        opened = ota.beginUpload(ESP.getSketchSize() + MULTIPART_OVERHEAD, runningAppHash());
        if (!opened) return;
        if (!streamRunningApp(sink, nullptr)) return;
        committed = ota.finalizeUpload();
    }
};

} // namespace

void setUp() {
    g_running = esp_ota_get_running_partition();
    restoreBootPartition();
}

void tearDown() {
    // Release an update left open by a test that longjmped out of a failed
    // assertion. Without this, one failure cascades: the next beginUpload() gets
    // "already running" from Update and the test after it reports a failure it
    // never actually reached. The SEC-8 removal check on 2026-08-27 was read that
    // way for a day — the second test looked like it had demonstrated something
    // when all it had done was inherit the first one's open partition.
    if (Update.isRunning()) {
        Update.abort();
    }
    // Never leave the bootloader pointed anywhere but here, however a test ended.
    restoreBootPartition();
}

// ============================================================================
// SEC-2 — the download path
// ============================================================================

void test_two_app_slots_and_the_running_one_is_the_boot_one() {
    // Everything below reads as "unchanged"; this is what unchanged means.
    TEST_ASSERT_NOT_NULL(g_running);
    TEST_ASSERT_FALSE(bootPartitionMoved());

    // OTA needs somewhere else to write. On a single-app table — huge_app.csv,
    // which is what this board ships with — esp_ota_get_next_update_partition()
    // does NOT return null: it returns the *running* partition, because there is
    // no other OTA slot to cycle to. Update then tries to erase the code it is
    // executing from and ESP-IDF calls abort() out of CHECK_WRITE_ADDRESS, which
    // reads as a mysterious panic several frames deep in spi_flash.
    //
    // Asserting non-null is what this test did first, and it passed while every
    // other test on the board panicked. The invariant is that the two are
    // different.
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    TEST_ASSERT_NOT_NULL(next);
    TEST_ASSERT_TRUE_MESSAGE(next != g_running,
        "single app slot: OTA has nowhere to write. Needs a two-slot table "
        "(board_build.partitions = default.csv)");
}

void test_mismatched_hash_leaves_the_boot_partition_alone() {
    // A *valid* image with the wrong hash — deliberately, and this is the whole
    // point of the test. esp_ota_set_boot_partition() verifies the image, so a
    // synthetic payload is refused by the platform whatever our code does: the
    // test then passes without the fix and proves nothing. Only a well-formed
    // image the caller did not ask for isolates our check from the platform's.
    //
    // That distinction is the threat model. ESP-IDF stops a *corrupt* download.
    // It cannot stop a *valid image that is not the one requested* — a
    // substituted or misrouted firmware — and that is what SHA-256 is for.
    Download dl(HASH_THAT_CANNOT_MATCH, true);

    // The property, asserted before the bookkeeping that merely implies it.
    TEST_ASSERT_FALSE(bootPartitionMoved());

    TEST_ASSERT_EQUAL(OTAComponent::State::Error, dl.ota.getState());
    TEST_ASSERT_EQUAL_STRING("SHA256 mismatch", dl.ota.getLastError().c_str());
}

void test_matching_hash_switches_the_boot_partition() {
    // Non-vacuity: proves a commit really does move the boot partition on this
    // board, so the "unchanged" assertions elsewhere are worth something.
    // Restored immediately — and the payload is a copy of the running app, so
    // even a failed restore boots identical firmware.
    Download dl(runningAppHash(), true);

    const bool moved = bootPartitionMoved();
    restoreBootPartition();  // BEFORE asserting: a failure here longjmps out

    TEST_ASSERT_NOT_EQUAL(OTAComponent::State::Error, dl.ota.getState());
    TEST_ASSERT_TRUE(moved);
    TEST_ASSERT_FALSE(bootPartitionMoved());
}

void test_abort_after_a_complete_image_leaves_the_boot_partition_alone() {
    // On ESP32 abort() before end() is genuinely correct: Update withholds the
    // image's first 16 bytes until _verifyEnd(), so the partition never becomes
    // bootable and esp_ota_set_boot_partition() is never reached.
    TEST_ASSERT_TRUE(HAL::OTAUpdate::begin(SYNTH_SIZE));
    TEST_ASSERT_TRUE(streamSynthetic([](const uint8_t* d, size_t n) {
        return HAL::OTAUpdate::write(const_cast<uint8_t*>(d), n) == n;
    }));

    HAL::OTAUpdate::abort();

    TEST_ASSERT_FALSE(bootPartitionMoved());
}

// ============================================================================
// SEC-7 — the upload path
// ============================================================================

void test_upload_with_mismatched_hash_leaves_the_boot_partition_alone() {
    // Valid image, wrong hash — see the download case for why a synthetic
    // payload would let this pass without the fix.
    Upload up(HASH_THAT_CANNOT_MATCH, false, true);

    TEST_ASSERT_FALSE(bootPartitionMoved());

    TEST_ASSERT_TRUE(up.opened);
    TEST_ASSERT_FALSE(up.committed);
    TEST_ASSERT_EQUAL_STRING("SHA256 mismatch", up.ota.getLastError().c_str());
}

void test_require_upload_hash_refuses_before_erasing_flash() {
    Upload up("", true);

    TEST_ASSERT_FALSE(bootPartitionMoved());

    TEST_ASSERT_FALSE(up.opened);
    TEST_ASSERT_FALSE(up.committed);
    TEST_ASSERT_EQUAL_STRING("Firmware hash required", up.ota.getLastError().c_str());
}

// ============================================================================
// SEC-8 — the size ceiling, on the path anybody can POST to
// ============================================================================
//
// maxDownloadSize was enforced on the transfer this device initiates and not on
// the one arriving from outside. Both tests assert on the error *message*, not
// merely on the refusal: Update has size limits of its own, and a test content
// with "it said no" would be crediting the platform for our check — the trap
// the synthetic payload set for the SHA tests above.
//
// BUG-21's events are not tested here. Nothing about emitting them is
// platform-specific, and the native suite pins the topics and their ordering.

namespace {

void configureCappedUpload(OTAComponent& ota, size_t cap) {
    OTAConfig cfg;
    cfg.autoReboot = false;
    cfg.maxDownloadSize = cap;
    ota.setConfig(cfg);
    ota.begin();
}

} // namespace

void test_upload_over_the_cap_is_refused_before_erasing_flash() {
    OTAComponent ota;
    configureCappedUpload(ota, SYNTH_SIZE / 2);

    TEST_ASSERT_FALSE(ota.beginUpload(SYNTH_SIZE, HASH_THAT_CANNOT_MATCH));

    TEST_ASSERT_FALSE(bootPartitionMoved());
    TEST_ASSERT_FALSE(Update.isRunning());  // never opened an update at all
    TEST_ASSERT_EQUAL_STRING("Firmware too large", ota.getLastError().c_str());
}

void test_upload_streaming_past_the_cap_leaves_the_boot_partition_alone() {
    // The case an announced-size check cannot see: Content-Length is optional, so
    // beginUpload(0) opens an update sized to the whole OTA partition and Update
    // will take every one of these 64 KB. Only counting what arrives stops it.
    OTAComponent ota;
    configureCappedUpload(ota, SYNTH_SIZE / 2);

    TEST_ASSERT_TRUE(ota.beginUpload(0));
    TEST_ASSERT_TRUE_MESSAGE(Update.isRunning(), "flash was never opened: nothing to release");

    const bool refused = !streamSynthetic([&ota](const uint8_t* d, size_t n) {
        return ota.acceptUploadChunk(d, n);
    });

    TEST_ASSERT_TRUE_MESSAGE(refused, "64 KB went past a 32 KB ceiling");
    TEST_ASSERT_EQUAL_STRING("Firmware too large", ota.getLastError().c_str());
    TEST_ASSERT_FALSE(bootPartitionMoved());
    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota.getState());
}

// ============================================================================
// TEST-8 — an image short of Update's _size still commits
// ============================================================================

void test_upload_short_of_the_announced_size_still_switches_the_boot_partition() {
    ShortUpload up;

    TEST_ASSERT_TRUE_MESSAGE(up.opened, "the update never opened: nothing below means anything");
    TEST_ASSERT_TRUE_MESSAGE(up.committed, up.ota.getLastError().c_str());
    TEST_ASSERT_TRUE_MESSAGE(bootPartitionMoved(),
                             "an upload 220 bytes short of its announced size did not switch the "
                             "boot partition — this is every browser upload");

    // SEC-9: the announced envelope must not survive into what the device reports.
    TEST_ASSERT_EQUAL_MESSAGE(ESP.getSketchSize(), up.ota.getTotalBytes(),
                              "the completion figures still carry the multipart envelope");

    restoreBootPartition();
}

void setup() {
    delay(2000);  // let the host attach before Unity starts printing
    UNITY_BEGIN();

    RUN_TEST(test_two_app_slots_and_the_running_one_is_the_boot_one);
    RUN_TEST(test_mismatched_hash_leaves_the_boot_partition_alone);
    RUN_TEST(test_matching_hash_switches_the_boot_partition);
    RUN_TEST(test_abort_after_a_complete_image_leaves_the_boot_partition_alone);

    RUN_TEST(test_upload_with_mismatched_hash_leaves_the_boot_partition_alone);
    RUN_TEST(test_require_upload_hash_refuses_before_erasing_flash);

    RUN_TEST(test_upload_over_the_cap_is_refused_before_erasing_flash);
    RUN_TEST(test_upload_streaming_past_the_cap_leaves_the_boot_partition_alone);

    RUN_TEST(test_upload_short_of_the_announced_size_still_switches_the_boot_partition);

    UNITY_END();
}

void loop() {}
