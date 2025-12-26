#include "DomoticsCore/OTA.h"
#include "DomoticsCore/Events.h"
#include "DomoticsCore/OTAEvents.h"
#include "DomoticsCore/Platform_HAL.h"  // For HAL::SHA256
#include "DomoticsCore/Update_HAL.h"
#include <ArduinoJson.h>

#include "DomoticsCore/Logger.h"

using namespace DomoticsCore::Components;

namespace {

String toHex(const uint8_t* data, size_t len) {
    static const char* hex = "0123456789ABCDEF";
    String out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out += hex[(data[i] >> 4) & 0x0F];
        out += hex[data[i] & 0x0F];
    }
    return out;
}

const char* stateToString(OTAComponent::State state) {
    switch (state) {
        case OTAComponent::State::Idle: return "idle";
        case OTAComponent::State::Checking: return "checking";
        case OTAComponent::State::Downloading: return "downloading";
        case OTAComponent::State::Applying: return "applying";
        case OTAComponent::State::RebootPending: return "reboot_pending";
        case OTAComponent::State::Error: return "error";
    }
    return "unknown";
}

} // namespace

// Restore constructor and helper methods outside anonymous namespace
OTAComponent::OTAComponent(const OTAConfig& cfg) : config(cfg) {
    metadata.name = "OTA";
    metadata.version = "1.4.0";
    metadata.description = "Secure over-the-air firmware updates";
    metadata.author = "DomoticsCore";
    metadata.category = "system";
}

void OTAComponent::transition(State nextState, const String& reason) {
    state = nextState;
    stateChangeMillis = HAL::Platform::getMillis();
    lastProgressPublishMillis = stateChangeMillis;
    if (!reason.isEmpty()) {
        lastResult = reason;
    }
    const String reasonSuffix = reason.isEmpty() ? String("") : String(" | ") + reason;
    DLOG_I(LOG_OTA, "State -> %s%s", stateToString(state), reasonSuffix.c_str());
}

bool OTAComponent::shouldCheckNow() const {
    if (config.checkIntervalMs == 0) return false;
    return HAL::Platform::getMillis() >= nextCheckMillis;
}

bool OTAComponent::triggerUpdateFromUrl(const String& url, bool force) {
    if (url.isEmpty()) return false;
    pendingUrl = url;
    pendingUrlForce = force;
    pendingUrlUpdate = true;
    return true;
}

ComponentStatus OTAComponent::begin() {
    state = State::Idle;
    stateChangeMillis = HAL::Platform::getMillis();
    lastProgressPublishMillis = stateChangeMillis;
    nextCheckMillis = HAL::Platform::getMillis() + config.checkIntervalMs;
    lastResult = "Idle";
    lastError.clear();
    pendingCheck = false;
    pendingForce = false;
    pendingUrlUpdate = false;
    pendingUrlForce = false;
    pendingUrl.clear();
    uploadSession = UploadSession{};
    progress = 0.0f;
    downloadedBytes = 0;
    totalBytes = 0;
    lastLoggedProgress = -1.0f;
    lastLoggedBytes = 0;
    return ComponentStatus::Success;
}

void OTAComponent::loop() {
    const unsigned long now = HAL::Platform::getMillis();

    // Process buffered upload data if platform requires it (ESP8266)
    // This is safe to call on all platforms - it's a no-op when not needed
    if (uploadSession.active && HAL::OTAUpdate::hasPendingData()) {
        String bufferError;
        int result = HAL::OTAUpdate::processBuffer(bufferError);

        if (result < 0) {
            // Error processing buffer
            lastError = bufferError;
            uploadSession.error = lastError;
            HAL::OTAUpdate::abort();
            uploadSession.active = false;
            transition(State::Error, lastError);
            publishStatusEvent(DomoticsCore::OTAEvents::EVENT_ERROR, [this](JsonDocument& doc){
                doc["success"] = false;
                doc["error"] = lastError.c_str();
                doc["source"] = "upload";
            }, false);
            return;
        } else if (result > 0) {
            // Buffer processing complete - upload finalized
            downloadedBytes = HAL::OTAUpdate::getBytesWritten();
            uploadSession.success = true;
            uploadSession.active = false;
            DLOG_I(LOG_OTA, "Upload finalized | bytes=%lu", static_cast<unsigned long>(downloadedBytes));
            finalizeUpdateOperation("upload", config.autoReboot);
            return;
        }
        // result == 0: continue processing in next loop iteration
        downloadedBytes = HAL::OTAUpdate::getBytesWritten();
    }

    if (pendingUrlUpdate) {
        const bool force = pendingUrlForce;
        const String url = pendingUrl;
        pendingUrlUpdate = false;
        pendingUrlForce = false;
        pendingUrl.clear();
        installFromUrl(url, "", force);
    } else if (pendingCheck) {
        const bool force = pendingForce;
        pendingCheck = false;
        pendingForce = false;
        performCheck(force);
    } else if (shouldCheckNow()) {
        performCheck(false);
    }

    if (state == State::RebootPending && config.autoReboot) {
        if (now - stateChangeMillis > 2000UL) {  // 2 seconds to allow final UI update
            DLOG_I(LOG_OTA, "Rebooting to apply firmware update");
            HAL::Platform::delayMs(100);
            HAL::restart();
        }
    }
}

ComponentStatus OTAComponent::shutdown() {
    if (uploadSession.active) {
        HAL::OTAUpdate::abort();
    }
    state = State::Idle;
    return ComponentStatus::Success;
}

bool OTAComponent::triggerImmediateCheck(bool force) {
    pendingCheck = true;
    pendingForce = force;
    return true;
}

bool OTAComponent::beginUpload(size_t expectedSize) {
    if (uploadSession.active) {
        lastError = "Upload already in progress";
        return false;
    }

    // Initialize HAL upload (handles buffering internally on platforms that need it)
    size_t updateSize = expectedSize > 0 ? expectedSize : UPDATE_SIZE_UNKNOWN;
    if (!HAL::OTAUpdate::begin(updateSize)) {
        lastError = HAL::OTAUpdate::errorString();
        return false;
    }

    uploadSession = UploadSession{};
    uploadSession.active = true;
    uploadSession.expected = expectedSize;
    uploadSession.received = 0;
    uploadSession.success = false;
    uploadSession.error.clear();

    totalBytes = expectedSize;
    downloadedBytes = 0;
    progress = 0.0f;
    lastLoggedProgress = 0.0f;
    lastLoggedBytes = 0;

    transition(State::Downloading, "Manual upload started");
    lastResult = "Uploading firmware";
    if (expectedSize > 0) {
        DLOG_I(LOG_OTA, "Upload started | expected bytes=%lu", static_cast<unsigned long>(expectedSize));
    } else {
        DLOG_I(LOG_OTA, "Upload started | expected bytes=unknown");
    }
    publishStatusEvent(DomoticsCore::OTAEvents::EVENT_INFO, [this](JsonDocument& doc){
        doc["success"] = true;
        doc["message"] = "Upload started";
        doc["source"] = "upload";
    }, false);
    return true;
}

bool OTAComponent::acceptUploadChunk(const uint8_t* data, size_t length) {
    if (!uploadSession.active) {
        lastError = "Upload not active";
        return false;
    }
    if (!data || length == 0) {
        return true;
    }

    // Write to HAL (buffers internally on ESP8266, direct write on ESP32)
    size_t written = HAL::OTAUpdate::write(const_cast<uint8_t*>(data), length);
    if (written != length) {
        // Check for buffer overflow first
        if (HAL::OTAUpdate::hasBufferOverflow()) {
            lastError = "Upload buffer overflow - data arriving faster than flash write";
        } else {
            lastError = HAL::OTAUpdate::errorString();
        }
        uploadSession.error = lastError;
        HAL::OTAUpdate::abort();
        uploadSession.active = false;
        transition(State::Error, lastError);
        publishStatusEvent(DomoticsCore::OTAEvents::EVENT_ERROR, [this](JsonDocument& doc){
            doc["success"] = false;
            doc["error"] = lastError.c_str();
            doc["source"] = "upload";
        }, false);
        return false;
    }

    uploadSession.received += written;

    // On platforms without buffering (ESP32), downloadedBytes = received
    // On platforms with buffering (ESP8266), downloadedBytes is updated in loop()
    if (!HAL::OTAUpdate::requiresBuffering()) {
        downloadedBytes = uploadSession.received;
    }

    // Update progress based on received bytes
    if (uploadSession.expected > 0) {
        progress = (uploadSession.received * 100.0f) / static_cast<float>(uploadSession.expected);
    } else {
        progress = 0.0f;
    }

    // Log progress periodically
    if (uploadSession.expected > 0) {
        float delta = fabs(progress - lastLoggedProgress);
        if (delta >= 10.0f || (uploadSession.received - lastLoggedBytes) >= 256 * 1024) {
            DLOG_I(LOG_OTA, "Upload progress: %.1f%% (%lu/%lu bytes)",
                   static_cast<double>(progress),
                   static_cast<unsigned long>(uploadSession.received),
                   static_cast<unsigned long>(uploadSession.expected));
            lastLoggedProgress = progress;
            lastLoggedBytes = uploadSession.received;
        }
    } else if ((uploadSession.received - lastLoggedBytes) >= 256 * 1024) {
        DLOG_I(LOG_OTA, "Upload received: %lu bytes (no size known)", static_cast<unsigned long>(uploadSession.received));
        lastLoggedBytes = uploadSession.received;
    }

    // Throttle progress broadcasts to avoid EventBus queue overflow (every 1 second)
    const unsigned long now = HAL::Platform::getMillis();
    if ((now - lastProgressPublishMillis) > 1000) {
        publishStatusEvent(DomoticsCore::OTAEvents::EVENT_PROGRESS, [this](JsonDocument& doc){
            doc["success"] = true;
            doc["source"] = "upload";
            doc["bytes"] = uploadSession.received;
            doc["total"] = uploadSession.expected;
        }, false);
        lastProgressPublishMillis = now;
    }
    return true;
}

bool OTAComponent::finalizeUpload() {
    if (!uploadSession.active) {
        lastError = "Upload not active";
        return false;
    }

    DLOG_I(LOG_OTA, "Upload finalizing | received=%lu bytes",
           static_cast<unsigned long>(uploadSession.received));

    // On platforms with buffering, end() just marks as finalizing
    // Actual finalization happens in loop() when buffer is flushed
    if (!HAL::OTAUpdate::end(true)) {
        lastError = HAL::OTAUpdate::errorString();
        uploadSession.error = lastError;
        HAL::OTAUpdate::abort();
        uploadSession.active = false;
        transition(State::Error, lastError);
        publishStatusEvent(DomoticsCore::OTAEvents::EVENT_ERROR, [this](JsonDocument& doc){
            doc["success"] = false;
            doc["error"] = lastError.c_str();
            doc["source"] = "upload";
        }, false);
        return false;
    }

    // On platforms without buffering (ESP32), finalize immediately
    if (!HAL::OTAUpdate::requiresBuffering()) {
        uploadSession.success = true;
        uploadSession.active = false;
        DLOG_I(LOG_OTA, "Upload finalized | bytes=%lu", static_cast<unsigned long>(uploadSession.received));
        finalizeUpdateOperation("upload", config.autoReboot);
    }
    // On platforms with buffering (ESP8266), loop() will complete finalization

    return true;
}

void OTAComponent::abortUpload(const String& reason) {
    if (!uploadSession.active) {
        return;
    }
    HAL::OTAUpdate::abort();
    uploadSession.active = false;
    uploadSession.success = false;
    uploadSession.error = reason;
    lastError = reason;
    transition(State::Error, reason);
    publishStatusEvent(DomoticsCore::OTAEvents::EVENT_ERROR, [this](JsonDocument& doc){
        doc["success"] = false;
        doc["error"] = lastError.c_str();
        doc["source"] = "upload";
    }, false);
}

bool OTAComponent::isIdle() const {
    return state == State::Idle || state == State::Error;
}

bool OTAComponent::isBusy() const {
    return state == State::Checking || state == State::Downloading || state == State::Applying;
}

void OTAComponent::scheduleNextCheck(uint32_t delayMs) {
    if (config.checkIntervalMs == 0 && delayMs == 0) {
        return;
    }
    unsigned long interval = delayMs ? delayMs : config.checkIntervalMs;
    nextCheckMillis = HAL::Platform::getMillis() + interval;
}

bool OTAComponent::performCheck(bool force) {
    if (!force && config.manifestUrl.isEmpty() && config.updateUrl.isEmpty()) {
        lastResult = "No update URL configured";
        return false;
    }

    transition(State::Checking, "Checking for updates");

    ManifestInfo manifest;
    if (!config.manifestUrl.isEmpty()) {
        manifest = fetchManifest();
        if (!manifest.valid) {
            lastError = "Failed to fetch manifest";
            transition(State::Error, lastError);
            scheduleNextCheck();
            return false;
        }
        if (!force && !manifest.version.isEmpty() && !isNewerVersion(manifest.version) && !config.allowDowngrades) {
            lastResult = "Firmware already up to date";
            transition(State::Idle, "No update needed");
            scheduleNextCheck();
            return true;
        }
    } else {
        manifest.url = config.updateUrl;
        manifest.valid = true;
    }

    if (manifest.url.isEmpty()) {
        lastError = "Manifest missing URL";
        transition(State::Error, lastError);
        scheduleNextCheck();
        return false;
    }

    bool ok = installFromUrl(manifest.url, manifest.sha256, force || config.allowDowngrades);
    if (!manifest.version.isEmpty()) {
        lastVersionSeen = manifest.version;
    }
    scheduleNextCheck();
    return ok;
}

OTAComponent::ManifestInfo OTAComponent::fetchManifest() {
    ManifestInfo info;
    if (config.manifestUrl.isEmpty()) {
        return info;
    }
    if (!manifestFetcher) {
        DLOG_E(LOG_OTA, "No manifest fetcher set");
        return info;
    }
    String payload;
    if (!manifestFetcher(config.manifestUrl, payload)) {
        DLOG_E(LOG_OTA, "Manifest fetch failed");
        return info;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload.c_str());
    if (err) {
        DLOG_E(LOG_OTA, "Manifest JSON parse failed: %s", err.c_str());
        return info;
    }

    const char* ver = doc["version"] | "";
    const char* urlStr = doc["url"] | "";
    const char* sha = doc["sha256"] | "";
    const char* sig = doc["signature"] | "";
    info.version = ver;
    info.url = urlStr;
    info.sha256 = sha;
    info.signature = sig;
    info.valid = true;
    return info;
}

bool OTAComponent::installFromUrl(const String& url, const String& expectedSha256, bool allowDowngrade) {
    if (url.isEmpty()) {
        return false;
    }
    if (!downloader) {
        lastError = "No downloader set";
        transition(State::Error, lastError);
        return false;
    }

    // Prepare update
    transition(State::Downloading, "Downloading firmware");
    downloadedBytes = 0;
    progress = 0.0f;

    HAL::SHA256 shaCtx;

    bool started = false;
    size_t announcedSize = 0;
    bool ok = downloader(url, announcedSize, [this, &started, &announcedSize, &shaCtx](const uint8_t* data, size_t len) -> bool {
        if (!started) {
            totalBytes = announcedSize;
            if (config.maxDownloadSize > 0 && totalBytes > config.maxDownloadSize) {
                lastError = "Firmware too large";
                return false;
            }
            size_t updateSize = totalBytes > 0 ? totalBytes : UPDATE_SIZE_UNKNOWN;
            if (!HAL::OTAUpdate::begin(updateSize)) {
                lastError = HAL::OTAUpdate::errorString();
                return false;
            }
            started = true;
        }
        if (len == 0) return true;
        size_t written = HAL::OTAUpdate::write(const_cast<uint8_t*>(data), len);
        if (written != len) {
            lastError = HAL::OTAUpdate::errorString();
            return false;
        }
        shaCtx.update(data, written);
        downloadedBytes += written;
        if (totalBytes > 0) {
            progress = (downloadedBytes * 100.0f) / static_cast<float>(totalBytes);
        }
        // Yield to prevent watchdog timeout during long download operations
        HAL::Platform::yield();
        return true;
    });

    if (!ok) {
        HAL::OTAUpdate::abort();
        shaCtx.abort();
        transition(State::Error, lastError.isEmpty() ? String("Download failed") : lastError);
        publishStatusEvent(DomoticsCore::OTAEvents::EVENT_ERROR, [this](JsonDocument& doc){
            doc["success"] = false;
            doc["error"] = lastError.c_str();
            doc["source"] = "download";
        }, false);
        return false;
    }

    if (!HAL::OTAUpdate::end(true)) {
        lastError = HAL::OTAUpdate::errorString();
        HAL::OTAUpdate::abort();
        shaCtx.abort();
        transition(State::Error, lastError);
        return false;
    }

    uint8_t digest[32];
    shaCtx.finish(digest);

    if (!expectedSha256.isEmpty()) {
        if (!verifySha256(digest, expectedSha256)) {
            lastError = "SHA256 mismatch";
            transition(State::Error, lastError);
            return false;
        }
    }

    finalizeUpdateOperation("download", config.autoReboot);
    return true;
}

bool OTAComponent::verifySha256(const uint8_t* digest, const String& expectedHex) {
    String candidate = toHex(digest, 32);
    String expected = expectedHex;
    expected.toUpperCase();
    expected.replace(" ", "");
    expected.replace(":", "");
    return candidate.equalsIgnoreCase(expected);
}

bool OTAComponent::isNewerVersion(const String& candidate) const {
    // Parse semantic versions (major.minor.patch)
    struct Version {
        int major = 0;
        int minor = 0;
        int patch = 0;

        Version(const String& v) {
            int pos1 = v.indexOf('.');
            int pos2 = pos1 >= 0 ? v.indexOf('.', pos1 + 1) : -1;

            if (pos1 > 0) major = v.substring(0, pos1).toInt();
            if (pos2 > pos1 + 1) {
                minor = v.substring(pos1 + 1, pos2).toInt();
                patch = v.substring(pos2 + 1).toInt();
            } else if (pos1 > 0) {
                minor = v.substring(pos1 + 1).toInt();
            }
        }
    };

    Version current(metadata.version);
    Version cand(candidate);

    if (cand.major > current.major) return true;
    if (cand.major < current.major) return false;
    if (cand.minor > current.minor) return true;
    if (cand.minor < current.minor) return false;
    if (cand.patch > current.patch) return true;
    return false;
}

bool OTAComponent::finalizeUpdateOperation(const String& source, bool autoRebootPending) {
    progress = 100.0f;
    downloadedBytes = totalBytes;  // Ensure bytes match total

    // Final progress update via status event
    publishStatusEvent(DomoticsCore::OTAEvents::EVENT_COMPLETE, [this](JsonDocument& doc){
        doc["success"] = true;
        doc["progress"] = 100.0f;
        doc["bytes"] = totalBytes;
        doc["total"] = totalBytes;
    }, false);

    if (autoRebootPending) {
        transition(State::RebootPending, source + " complete");
        lastResult = "Update complete - rebooting in 2s";
        DLOG_I(LOG_OTA, "%s complete. Reboot scheduled in 2s.", source.c_str());
    } else {
        transition(State::Idle, source + " complete");
        lastResult = "Update applied. Reboot to finish.";
        DLOG_I(LOG_OTA, "%s complete. Manual reboot required.", source.c_str());
    }

    publishStatusEvent(DomoticsCore::OTAEvents::EVENT_COMPLETED, [this, &source](JsonDocument& doc){
        doc["success"] = true;
        doc["source"] = source.c_str();
        doc["autoReboot"] = config.autoReboot;
        doc["bytes"] = downloadedBytes;
        doc["message"] = (config.autoReboot ? "Update complete, rebooting" : "Update complete, reboot manually");
    }, true);

    return true;
}

void OTAComponent::broadcastProgress() {
    JsonDocument doc;
    doc["percent"] = progress;
    doc["downloaded"] = downloadedBytes;
    doc["total"] = totalBytes;
    doc["state"] = stateToString(state);
    String payload;
    serializeJson(doc, payload);
    emit<String>(DomoticsCore::OTAEvents::EVENT_PROGRESS, payload, false);
}

void OTAComponent::publishStatusEvent(const String& topic, std::function<void(JsonDocument&)> fn, bool sticky) {
    JsonDocument doc;
    fn(doc);
    doc["state"] = stateToString(state);
    doc["progress"] = progress;
    doc["lastResult"] = lastResult.c_str();
    String payload;
    serializeJson(doc, payload);
    emit<String>(topic, payload, sticky);
}
