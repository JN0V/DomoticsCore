#include "DomoticsCore/OTA.h"
#include "DomoticsCore/Events.h"

#include <Update.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>

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
    metadata.version = "1.2.1";
    metadata.description = "Secure over-the-air firmware updates";
    metadata.author = "DomoticsCore";
    metadata.category = "system";
}

void OTAComponent::transition(State nextState, const String& reason) {
    state = nextState;
    stateChangeMillis = millis();
    lastProgressPublishMillis = stateChangeMillis;
    if (!reason.isEmpty()) {
        lastResult = reason;
    }
    const String reasonSuffix = reason.isEmpty() ? String("") : String(" | ") + reason;
    DLOG_I(LOG_OTA, "State -> %s%s", stateToString(state), reasonSuffix.c_str());
}

bool OTAComponent::shouldCheckNow() const {
    if (config.checkIntervalMs == 0) return false;
    return millis() >= nextCheckMillis;
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
    stateChangeMillis = millis();
    lastProgressPublishMillis = stateChangeMillis;
    nextCheckMillis = millis() + config.checkIntervalMs;
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
    const unsigned long now = millis();

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
            delay(100);
            ESP.restart();
        }
    }
}

ComponentStatus OTAComponent::shutdown() {
    if (uploadSession.active) {
        Update.abort();
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

    size_t updateSize = expectedSize > 0 ? expectedSize : UPDATE_SIZE_UNKNOWN;
    if (!Update.begin(updateSize)) {
        lastError = Update.errorString();
        return false;
    }

    uploadSession = UploadSession{};
    uploadSession.active = true;
    uploadSession.initialized = true;
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
    publishStatusEvent(DomoticsCore::Events::EVENT_OTA_INFO, [this](JsonDocument& doc){
        doc["success"] = true;
        doc["message"] = "Upload started";
        doc["source"] = "upload";
    }, false);
    // Don't broadcast progress here - will be sent on first chunk
    return true;
}

bool OTAComponent::acceptUploadChunk(const uint8_t* data, size_t length) {
    if (!uploadSession.active || !uploadSession.initialized) {
        lastError = "Upload not initialized";
        return false;
    }
    if (!data || length == 0) {
        return true;
    }

    size_t written = Update.write(const_cast<uint8_t*>(data), length);
    if (written != length) {
        lastError = Update.errorString();
        uploadSession.error = lastError;
        Update.abort();
        uploadSession.active = false;
        transition(State::Error, lastError);
        publishStatusEvent(DomoticsCore::Events::EVENT_OTA_ERROR, [this](JsonDocument& doc){
            doc["success"] = false;
            doc["error"] = lastError;
            doc["source"] = "upload";
        }, false);
        return false;
    }

    uploadSession.received += written;
    downloadedBytes = uploadSession.received;
    if (uploadSession.expected > 0) {
        progress = (uploadSession.received * 100.0f) / static_cast<float>(uploadSession.expected);
    } else {
        progress = 0.0f;
    }
    // Log progress more frequently for visibility
    if (uploadSession.expected > 0) {
        float delta = fabs(progress - lastLoggedProgress);
        if (delta >= 10.0f || (downloadedBytes - lastLoggedBytes) >= 256 * 1024) {
            DLOG_I(LOG_OTA, "Upload progress: %.1f%% (%lu/%lu bytes)",
                   static_cast<double>(progress),
                   static_cast<unsigned long>(downloadedBytes),
                   static_cast<unsigned long>(uploadSession.expected));
            lastLoggedProgress = progress;
            lastLoggedBytes = downloadedBytes;
        }
    } else if ((downloadedBytes - lastLoggedBytes) >= 256 * 1024) {
        DLOG_I(LOG_OTA, "Upload received: %lu bytes (no size known)", static_cast<unsigned long>(downloadedBytes));
        lastLoggedBytes = downloadedBytes;
    }
    // Throttle progress broadcasts to avoid EventBus queue overflow (every 1 second)
    const unsigned long now = millis();
    if ((now - lastProgressPublishMillis) > 1000) {
        publishStatusEvent(DomoticsCore::Events::EVENT_OTA_PROGRESS, [this](JsonDocument& doc){
            doc["success"] = true;
            doc["source"] = "upload";
            doc["bytes"] = downloadedBytes;
            doc["total"] = uploadSession.expected;
        }, false);
        lastProgressPublishMillis = now;
    }
    // Note: broadcastProgress() removed here - already covered by publishStatusEvent above
    return true;
}

bool OTAComponent::finalizeUpload() {
    if (!uploadSession.active || !uploadSession.initialized) {
        lastError = "Upload not initialized";
        return false;
    }

    if (!Update.end(true)) {
        lastError = Update.errorString();
        uploadSession.error = lastError;
        Update.abort();
        uploadSession.active = false;
        transition(State::Error, lastError);
        publishStatusEvent(DomoticsCore::Events::EVENT_OTA_ERROR, [this](JsonDocument& doc){
            doc["success"] = false;
            doc["error"] = lastError;
            doc["source"] = "upload";
        }, false);
        return false;
    }

    uploadSession.success = true;
    uploadSession.active = false;
    DLOG_I(LOG_OTA, "Upload finalized | bytes=%lu", static_cast<unsigned long>(uploadSession.received));
    finalizeUpdateOperation("upload", config.autoReboot);
    return true;
}

void OTAComponent::abortUpload(const String& reason) {
    if (!uploadSession.active) {
        return;
    }
    Update.abort();
    uploadSession.active = false;
    uploadSession.success = false;
    uploadSession.error = reason;
    lastError = reason;
    transition(State::Error, reason);
    publishStatusEvent(DomoticsCore::Events::EVENT_OTA_ERROR, [this](JsonDocument& doc){
        doc["success"] = false;
        doc["error"] = lastError;
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
    nextCheckMillis = millis() + interval;
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
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        DLOG_E(LOG_OTA, "Manifest JSON parse failed: %s", err.c_str());
        return info;
    }

    info.version = doc["version"].as<String>();
    info.url = doc["url"].as<String>();
    info.sha256 = doc["sha256"].as<String>();
    info.signature = doc["signature"].as<String>();
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

    mbedtls_sha256_context shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts_ret(&shaCtx, 0);

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
            if (!Update.begin(updateSize)) {
                lastError = Update.errorString();
                return false;
            }
            started = true;
        }
        if (len == 0) return true;
        size_t written = Update.write(const_cast<uint8_t*>(data), len);
        if (written != len) {
            lastError = Update.errorString();
            return false;
        }
        mbedtls_sha256_update_ret(&shaCtx, data, written);
        downloadedBytes += written;
        if (totalBytes > 0) {
            progress = (downloadedBytes * 100.0f) / static_cast<float>(totalBytes);
        }
        // Progress broadcast throttled in acceptUploadChunk - don't spam EventBus
        return true;
    });

    if (!ok) {
        Update.abort();
        mbedtls_sha256_free(&shaCtx);
        transition(State::Error, lastError.isEmpty() ? String("Download failed") : lastError);
        publishStatusEvent(DomoticsCore::Events::EVENT_OTA_ERROR, [this](JsonDocument& doc){
            doc["success"] = false;
            doc["error"] = lastError;
            doc["source"] = "download";
        }, false);
        return false;
    }

    if (!Update.end(true)) {
        lastError = Update.errorString();
        Update.abort();
        mbedtls_sha256_free(&shaCtx);
        transition(State::Error, lastError);
        return false;
    }

    uint8_t digest[32];
    mbedtls_sha256_finish_ret(&shaCtx, digest);
    mbedtls_sha256_free(&shaCtx);

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
    publishStatusEvent(DomoticsCore::Events::EVENT_OTA_COMPLETE, [this](JsonDocument& doc){
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

    publishStatusEvent(DomoticsCore::Events::EVENT_OTA_COMPLETED, [this, &source](JsonDocument& doc){
        doc["success"] = true;
        doc["source"] = source;
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
    emit<String>(DomoticsCore::Events::EVENT_OTA_PROGRESS, payload, false);
}

void OTAComponent::publishStatusEvent(const String& topic, std::function<void(JsonDocument&)> fn, bool sticky) {
    JsonDocument doc;
    fn(doc);
    doc["state"] = stateToString(state);
    doc["progress"] = progress;
    doc["lastResult"] = lastResult;
    String payload;
    serializeJson(doc, payload);
    emit<String>(topic, payload, sticky);
}
