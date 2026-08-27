/**
 * @file test_ota_esp8266.cpp
 * @brief SEC-2 and SEC-7 on silicon: a firmware whose hash does not match is
 *        never staged, whether it was downloaded or uploaded.
 *
 * The host suite proves the *ordering* — that `HAL::OTAUpdate::end()` is not
 * reached when the digest mismatches — by counting calls into a stub. It cannot
 * prove what `end()` would have done, because on a host it does nothing. That
 * gap is the whole reason SEC-2 sat resolved for two releases while being inert:
 * the fix read correctly, compiled, and passed every native test.
 *
 * On an ESP8266 the commit is observable. `end()` writes an eboot
 * `ACTION_COPY_RAW` into RTC memory, and the bootloader acts on it at the next
 * reset by copying the staged image over the running sketch. `eboot_command_read()`
 * reads it back. That command — present or absent — is the only thing that
 * decides what boots next, and it is what these tests assert on. Update's own
 * state is gone by then either way, so nothing else would tell them apart.
 *
 * No network. The downloader is synthetic and the digest is computed on the
 * board, so the suite needs nothing but the board.
 *
 * ## This suite deliberately arms the bootloader, once
 *
 * `test_matching_hash_stages_the_copy` commits a real image and reads the staged
 * command back, because a test that only ever asserts "nothing is staged" passes
 * just as well when staging is impossible — which would make the other two tests
 * worthless. Between the commit and `eboot_command_clear()` there are two
 * statements; a reset in that window copies this file's synthetic payload over
 * the sketch and the board needs reflashing over serial. `tearDown()` clears the
 * command after every test as a second line of defence, and the clear happens
 * before any assertion that could longjmp past it.
 */

#include <unity.h>
#include <Arduino.h>
#include <Updater.h>
#include <eboot_command.h>

#include <DomoticsCore/OTA.h>
#include <DomoticsCore/Update_HAL.h>
#include <DomoticsCore/Platform_HAL.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

namespace {

// Two flash sectors, and the second one is not decoration.
//
// This was one sector, on the reasoning that a single sector is enough for the
// Updater to fill and flush its buffer. It is not, and at exactly one sector it
// only looked like it. `Updater.cpp:460` flushes the tail when
// `_bufferLen == remaining()`, i.e. only when the buffered bytes complete the
// *announced* size exactly — which a 4096-byte payload announced as 4096 does,
// on its very first buffer, having never flushed once. `progress()` was
// therefore 0 until `end()`, and every test passed because its announcement
// happened to match its delivery to the byte.
//
// TEST-8 needs the shape that does not match — announce the multipart envelope,
// deliver the firmware — and at one sector that shape leaves the whole image
// unflushed, `progress()` at 0, and `end()` refusing it as UPDATE_ERROR_NO_DATA
// before it reaches the flush three lines further down. Real firmware is never
// one sector; a 475 KB upload flushes 115 times before it gets there. At two
// sectors the suite behaves the way silicon does.
constexpr size_t PAYLOAD_SIZE = 8192;

// 8 KB will not fit on the ESP8266 stack.
uint8_t g_payload[PAYLOAD_SIZE];

const char* const HASH_THAT_CANNOT_MATCH =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";

/**
 * @brief Fill g_payload with something the Updater will accept as firmware.
 *
 * _verifyEnd() reads the first bytes back from flash and rejects anything whose
 * magic byte is not 0xE9 or whose flash-size nibble disagrees with the chip.
 * Copying that header from the running sketch satisfies both without hard-coding
 * anything about this particular board — and a rejection there would fail the
 * commit for a reason that has nothing to do with what is being tested.
 */
void buildPayload() {
    uint32_t header[4];
    ESP.flashRead(0, header, sizeof(header));
    memcpy(g_payload, header, sizeof(header));
    for (size_t i = sizeof(header); i < PAYLOAD_SIZE; ++i) {
        g_payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
}

/// True when a copy command is armed — i.e. the next reset would reflash.
bool copyCommandStaged() {
    eboot_command cmd;
    if (eboot_command_read(&cmd) != 0) {
        return false;  // no valid command in RTC memory
    }
    return cmd.action == ACTION_COPY_RAW;
}

String payloadHash() {
    HAL::Platform::SHA256 sha;
    sha.update(g_payload, PAYLOAD_SIZE);
    uint8_t digest[32];
    sha.finish(digest);
    return HAL::Platform::SHA256::toHex(digest, 32);
}

/**
 * @brief Drive one download to completion through the public path.
 *
 * Manifest fetch -> performCheck -> installFromUrl, the route an application
 * takes. autoReboot stays false: a reboot mid-suite would end the run and,
 * worse, act on anything staged.
 */
struct Download {
    OTAComponent ota;

    explicit Download(const String& expectedHash) {
        OTAConfig cfg;
        cfg.manifestUrl = "http://localhost/manifest.json";
        cfg.checkIntervalMs = 0;
        cfg.autoReboot = false;
        ota.setConfig(cfg);
        ota.begin();

        ota.setManifestFetcher([expectedHash](const String&, String& outJson) {
            outJson = String("{\"version\":\"9.9.9\","
                             "\"url\":\"http://localhost/firmware.bin\","
                             "\"sha256\":\"") + expectedHash + "\"}";
            return true;
        });
        ota.setDownloader([](const String&, size_t& totalSize, OTAComponent::DownloadCallback onChunk) {
            totalSize = PAYLOAD_SIZE;
            // Chunked, the way an HTTP body actually arrives.
            for (size_t offset = 0; offset < PAYLOAD_SIZE; offset += 512) {
                if (!onChunk(g_payload + offset, 512)) return false;
                yield();
            }
            return true;
        });

        ota.triggerImmediateCheck(true);
        ota.loop();
    }
};

/**
 * @brief Drive one upload to completion through the public path (SEC-7).
 *
 * The chunk sequence a WebUI multipart POST produces, minus the HTTP.
 */
struct Upload {
    OTAComponent ota;
    bool opened = false;
    bool committed = false;

    Upload(const String& expectedHash, bool requireHash) {
        OTAConfig cfg;
        cfg.autoReboot = false;
        cfg.requireUploadHash = requireHash;
        ota.setConfig(cfg);
        ota.begin();

        opened = ota.beginUpload(PAYLOAD_SIZE, expectedHash);
        if (!opened) return;
        for (size_t offset = 0; offset < PAYLOAD_SIZE; offset += 512) {
            if (!ota.acceptUploadChunk(g_payload + offset, 512)) return;
            yield();
        }
        committed = ota.finalizeUpload();
    }
};

/**
 * @brief A download whose length the server never announced.
 *
 * TEST-8. `Download` above announces PAYLOAD_SIZE and delivers it, so its image
 * is finished and `end()`'s argument does not matter. Content-Length is optional
 * and a chunked response has none, so `installFromUrl()` opens the update at
 * UPDATE_SIZE_UNKNOWN — the whole free sketch space — and the image is short by
 * everything it does not fill. Only `end(true)` commits it.
 */
struct UnknownLengthDownload {
    OTAComponent ota;

    UnknownLengthDownload() {
        OTAConfig cfg;
        cfg.manifestUrl = "http://localhost/manifest.json";
        cfg.checkIntervalMs = 0;
        cfg.autoReboot = false;
        ota.setConfig(cfg);
        ota.begin();

        const String hash = payloadHash();
        ota.setManifestFetcher([hash](const String&, String& outJson) {
            outJson = String("{\"version\":\"9.9.9\","
                             "\"url\":\"http://localhost/firmware.bin\","
                             "\"sha256\":\"") + hash + "\"}";
            return true;
        });
        ota.setDownloader([](const String&, size_t& totalSize, OTAComponent::DownloadCallback onChunk) {
            totalSize = 0;  // "unknown" — what a chunked response gives you
            for (size_t offset = 0; offset < PAYLOAD_SIZE; offset += 512) {
                if (!onChunk(g_payload + offset, 512)) return false;
                yield();
            }
            return true;
        });

        ota.triggerImmediateCheck(true);
        ota.loop();
    }
};

/**
 * @brief The shape every browser upload has, which no suite had.
 *
 * TEST-8. `Upload` above announces exactly what it delivers, so the image is
 * already finished by the Updater's definition when end() is called and the
 * `evenIfRemaining` argument is never load-bearing. A multipart POST is not like
 * that: `request->contentLength()` measures the encoded body, 220 bytes more
 * than the firmware on this board (SEC-9), so `_size` is never reached and the
 * commit happens only because `finalizeUpload()` passes `true`.
 *
 * SEC-9 pinned that argument natively, where the stub's end() returns true
 * whatever it is given — so the pin stopped at the call site. This is the same
 * claim against the real Updater.
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

        // The digest covers what is delivered, not what is announced — the same
        // asymmetry SEC-7 established, and the reason an over-announcement is not
        // an integrity problem.
        opened = ota.beginUpload(PAYLOAD_SIZE + MULTIPART_OVERHEAD, payloadHash());
        if (!opened) return;
        for (size_t offset = 0; offset < PAYLOAD_SIZE; offset += 512) {
            if (!ota.acceptUploadChunk(g_payload + offset, 512)) return;
            yield();
        }
        committed = ota.finalizeUpload();
    }
};

} // namespace


void setUp() {
    eboot_command_clear();
    buildPayload();
}

void tearDown() {
    // Release an update left open by a test that longjmped out of a failed
    // assertion. Without this, one failure cascades: the next beginUpload() gets
    // a refusal from the Updater and the test after it reports a failure it never
    // actually reached. The SEC-8 removal check on 2026-08-27 was read that way
    // for a day — the second test looked like it had demonstrated something when
    // all it had done was inherit the first one's open update.
    //
    // HAL::OTAUpdate::abort() rather than Update.end() directly: on this platform
    // the release is end(false), which stages the image once every announced byte
    // has been written, so the HAL clears the eboot command behind it. See the
    // contract in Update_HAL.h.
    if (Update.isRunning()) {
        HAL::OTAUpdate::abort();
    }
    // Never leave the bootloader armed, whatever a test did or how it failed.
    eboot_command_clear();
}

// ============================================================================
// SEC-2
// ============================================================================

void test_mismatched_hash_stages_nothing() {
    Download dl(HASH_THAT_CANNOT_MATCH);

    TEST_ASSERT_EQUAL(OTAComponent::State::Error, dl.ota.getState());
    TEST_ASSERT_EQUAL_STRING("SHA256 mismatch", dl.ota.getLastError().c_str());

    // The assertion SEC-2 is about. Before the fix this was staged, and the
    // abort() that followed could not take it back.
    TEST_ASSERT_FALSE(copyCommandStaged());
}

void test_matching_hash_stages_the_copy() {
    // Proves the negative assertions above are not vacuous: on this board, a
    // committed image really does arm the bootloader. Disarmed immediately —
    // see the file header.
    Download dl(payloadHash());

    TEST_ASSERT_NOT_EQUAL(OTAComponent::State::Error, dl.ota.getState());

    const bool staged = copyCommandStaged();
    eboot_command_clear();  // BEFORE any assertion: a failure here longjmps out

    TEST_ASSERT_TRUE(staged);
    TEST_ASSERT_FALSE(copyCommandStaged());
}

void test_abort_after_a_complete_image_stages_nothing() {
    // The ESP8266-specific trap the fix had to close. The Updater exposes no
    // abort(), so the HAL calls end(false) to release the buffer — safe while
    // the image is incomplete, but once every announced byte is written that
    // call clears the !isFinished() guard and runs to completion, staging the
    // very image it was asked to discard. This is that exact state, reached
    // directly rather than through a hash mismatch.
    TEST_ASSERT_TRUE(HAL::OTAUpdate::begin(PAYLOAD_SIZE));
    TEST_ASSERT_EQUAL(PAYLOAD_SIZE, HAL::OTAUpdate::write(g_payload, PAYLOAD_SIZE));
    TEST_ASSERT_TRUE(Update.isRunning());
    TEST_ASSERT_TRUE(Update.isFinished());  // the dangerous precondition

    HAL::OTAUpdate::abort();

    TEST_ASSERT_FALSE(copyCommandStaged());
    TEST_ASSERT_FALSE(Update.isRunning());
}

// ============================================================================
// SEC-7 — the upload path, same property, different entry point
// ============================================================================

void test_upload_with_mismatched_hash_stages_nothing() {
    // Until SEC-7 the upload path hashed nothing at all: finalizeUpload() called
    // end(true) on whatever arrived. On this board that armed the bootloader.
    Upload up(HASH_THAT_CANNOT_MATCH, false);

    // The flash consequence first — it is the property, and asserting it before
    // the bookkeeping means a regression reports the armed bootloader rather
    // than a return value that merely implies it.
    TEST_ASSERT_FALSE(copyCommandStaged());

    TEST_ASSERT_TRUE(up.opened);
    TEST_ASSERT_FALSE(up.committed);
    TEST_ASSERT_EQUAL(OTAComponent::State::Error, up.ota.getState());
    TEST_ASSERT_EQUAL_STRING("SHA256 mismatch", up.ota.getLastError().c_str());
}

void test_upload_with_matching_hash_stages_the_copy() {
    // Non-vacuity for the upload path, on the same terms as the download one.
    Upload up(payloadHash(), false);

    TEST_ASSERT_TRUE(up.committed);

    const bool staged = copyCommandStaged();
    eboot_command_clear();  // BEFORE any assertion: a failure here longjmps out

    TEST_ASSERT_TRUE(staged);
}

void test_require_upload_hash_refuses_before_erasing_flash() {
    // The refusal must land before Update.begin(), which erases the staging
    // sectors. Refusing after that point would have destroyed the staged region
    // to reject an upload that was never going to be installed.
    Upload up("", true);

    TEST_ASSERT_FALSE(copyCommandStaged());
    TEST_ASSERT_FALSE(Update.isRunning());  // never opened an update at all

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
// merely on the refusal: the Updater has size limits of its own, and a test that
// only checks "it said no" would pass on the platform's reasons rather than ours
// — the trap the ESP32 suite walked into with the synthetic payload.
//
// BUG-21's events are not tested here. Nothing about emitting them is
// platform-specific, and the native suite pins the topics and their ordering.

namespace {

OTAComponent* configureCappedUpload(OTAComponent& ota, size_t cap) {
    OTAConfig cfg;
    cfg.autoReboot = false;
    cfg.maxDownloadSize = cap;
    ota.setConfig(cfg);
    ota.begin();
    return &ota;
}

} // namespace

void test_upload_over_the_cap_is_refused_before_erasing_flash() {
    OTAComponent ota;
    configureCappedUpload(ota, PAYLOAD_SIZE / 2);

    TEST_ASSERT_FALSE(ota.beginUpload(PAYLOAD_SIZE, payloadHash()));

    // Never opened an update at all: the staging sectors are untouched, on the
    // same ordering SEC-7 established for the hash refusal.
    TEST_ASSERT_FALSE(Update.isRunning());
    TEST_ASSERT_FALSE(copyCommandStaged());
    TEST_ASSERT_EQUAL_STRING("Firmware too large", ota.getLastError().c_str());
}

void test_upload_streaming_past_the_cap_releases_flash_and_stages_nothing() {
    // The case an announced-size check cannot see. Content-Length is optional, so
    // beginUpload(0) opens an update sized to the whole free sketch space — the
    // Updater will accept every one of these 4 KB. Only counting what arrives
    // stops it, and by then flash is erased, which is the ESP8266 trap SEC-2
    // documented: releasing an update must not stage the copy.
    OTAComponent ota;
    configureCappedUpload(ota, PAYLOAD_SIZE / 2);

    TEST_ASSERT_TRUE(ota.beginUpload(0));
    TEST_ASSERT_TRUE_MESSAGE(Update.isRunning(), "flash was never opened: nothing to release");

    bool refused = false;
    for (size_t offset = 0; offset < PAYLOAD_SIZE; offset += 512) {
        if (!ota.acceptUploadChunk(g_payload + offset, 512)) { refused = true; break; }
        yield();
    }

    TEST_ASSERT_TRUE_MESSAGE(refused, "4 KB went past a 2 KB ceiling");
    TEST_ASSERT_EQUAL_STRING("Firmware too large", ota.getLastError().c_str());
    TEST_ASSERT_FALSE(copyCommandStaged());
    TEST_ASSERT_FALSE_MESSAGE(Update.isRunning(), "the update was left open after the refusal");
    TEST_ASSERT_EQUAL(OTAComponent::State::Error, ota.getState());
}

// ============================================================================
// TEST-8 — an image short of Update's _size still commits
// ============================================================================
//
// Both of these fail if HAL::OTAUpdate::end() is ever handed `false`, on this
// board, at the HAL rather than at the call site SEC-9 could reach. They are the
// two shapes real transfers actually have and no suite had: an upload that
// announces its multipart envelope, and a download whose length is not known in
// advance.

void test_upload_short_of_the_announced_size_still_stages_the_copy() {
    ShortUpload up;

    TEST_ASSERT_TRUE_MESSAGE(up.opened, "the update never opened: nothing below means anything");
    TEST_ASSERT_TRUE_MESSAGE(up.committed, up.ota.getLastError().c_str());
    TEST_ASSERT_TRUE_MESSAGE(copyCommandStaged(),
                             "finalizeUpload() returned true and the bootloader was not armed");
    TEST_ASSERT_EQUAL_STRING("", up.ota.getLastError().c_str());

    // SEC-9: the announced envelope must not survive into what the device
    // reports. 4096 delivered, 4316 announced.
    TEST_ASSERT_EQUAL_MESSAGE(PAYLOAD_SIZE, up.ota.getTotalBytes(),
                              "the completion figures still carry the multipart envelope");
}

void test_download_of_unknown_length_still_stages_the_copy() {
    // Content-Length is optional and chunked transfer-encoding has none, so
    // installFromUrl() opens the update at UPDATE_SIZE_UNKNOWN — the whole free
    // sketch space. The image is then short by everything it does not fill, and
    // only end(true) commits it.
    UnknownLengthDownload dl;

    TEST_ASSERT_EQUAL_MESSAGE(OTAComponent::State::Idle, dl.ota.getState(),
                              dl.ota.getLastError().c_str());
    TEST_ASSERT_TRUE_MESSAGE(copyCommandStaged(),
                             "a download that announced no size was not committed");

    // TEST-8, hole 3, measured here rather than argued: the transfer succeeded
    // and the device reports having downloaded nothing. finalizeUpdateOperation()
    // does downloadedBytes = totalBytes, and totalBytes is the size the *server*
    // announced — zero, on a chunked response. SEC-9 narrowed the upload path's
    // equivalent; this is the download path's, and it is not this lot's to fix.
    // Asserted so that fixing it fails here and is noticed, rather than looking
    // like nobody knew.
    TEST_ASSERT_EQUAL_MESSAGE(0, dl.ota.getDownloadedBytes(),
                              "the download byte count was fixed — good; update TEST-8 hole 3");
}

void setup() {
    delay(2000);  // let the host attach before Unity starts printing
    UNITY_BEGIN();

    RUN_TEST(test_mismatched_hash_stages_nothing);
    RUN_TEST(test_matching_hash_stages_the_copy);
    RUN_TEST(test_abort_after_a_complete_image_stages_nothing);

    RUN_TEST(test_upload_with_mismatched_hash_stages_nothing);
    RUN_TEST(test_upload_with_matching_hash_stages_the_copy);
    RUN_TEST(test_require_upload_hash_refuses_before_erasing_flash);

    RUN_TEST(test_upload_over_the_cap_is_refused_before_erasing_flash);
    RUN_TEST(test_upload_streaming_past_the_cap_releases_flash_and_stages_nothing);

    RUN_TEST(test_upload_short_of_the_announced_size_still_stages_the_copy);
    RUN_TEST(test_download_of_unknown_length_still_stages_the_copy);

    UNITY_END();
}

void loop() {}
