/**
 * @file test_ota_esp8266.cpp
 * @brief SEC-2 on silicon: a firmware whose hash does not match is never staged.
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

// One flash sector. Large enough that Update fills and flushes its buffer, so
// the image really is complete when the tests say it is; small enough to fit
// the free sketch space of a board already carrying this firmware.
constexpr size_t PAYLOAD_SIZE = 4096;

// 4 KB will not fit on the ESP8266 stack.
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

} // namespace

void setUp() {
    eboot_command_clear();
    buildPayload();
}

void tearDown() {
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

void setup() {
    delay(2000);  // let the host attach before Unity starts printing
    UNITY_BEGIN();

    RUN_TEST(test_mismatched_hash_stages_nothing);
    RUN_TEST(test_matching_hash_stages_the_copy);
    RUN_TEST(test_abort_after_a_complete_image_stages_nothing);

    UNITY_END();
}

void loop() {}
