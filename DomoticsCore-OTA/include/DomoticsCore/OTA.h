#pragma once

/**
 * @file OTA.h
 * @brief Declares the DomoticsCore OTA component providing secure firmware updates.
 */

#include <Arduino.h>
#include <functional>

#include "DomoticsCore/IComponent.h"
#include <ArduinoJson.h>

namespace DomoticsCore {
namespace Components {

/**
 * @brief Configuration options for the OTA component.
 */
struct OTAConfig {
    String updateUrl = "";              //!< Direct firmware URL
    String manifestUrl = "";            //!< Optional manifest endpoint providing metadata
    String bearerToken = "";            //!< Optional HTTP bearer token
    String basicAuthUser = "";          //!< Optional basic-auth username
    String basicAuthPassword = "";      //!< Optional basic-auth password
    String rootCA = "";                 //!< Optional PEM-encoded root CA certificate
    String signaturePublicKey = "";     //!< Optional public key for signature validation (PEM)
    uint32_t checkIntervalMs = 3600000;  //!< Automatic periodic check interval (0 = disabled)
    bool requireTLS = true;              //!< Enforce HTTPS when true
    bool allowDowngrades = false;        //!< Permit installing lower semantic versions
    bool autoReboot = true;              //!< Reboot immediately after a successful update
    size_t maxDownloadSize = 0;          //!< Reject binaries larger than this (0 = unlimited)
    bool enableWebUIUpload = true;       //!< Allow manual firmware upload via WebUI helpers
};

/**
 * @brief OTA component handling secure firmware downloads and manual upload helpers.
 */
class OTAComponent : public IComponent {
public:
    // Transport/provider concepts to keep OTA network-agnostic
    using ManifestFetcher = std::function<bool(const String& manifestUrl, String& outJson)>;
    using DownloadCallback = std::function<bool(const uint8_t* data, size_t len)>;
    using Downloader = std::function<bool(const String& url, size_t& totalSize, DownloadCallback onChunk)>;
    enum class State {
        Idle,
        Checking,
        Downloading,
        Applying,
        RebootPending,
        Error
    };

    OTAComponent(const OTAConfig& cfg = OTAConfig());

    // IComponent implementation
    ComponentStatus begin() override;
    void loop() override;
    ComponentStatus shutdown() override;
    const char* getTypeKey() const override { return "ota"; }

    // Control API
    bool triggerImmediateCheck(bool force = false);
    bool triggerUpdateFromUrl(const String& url, bool force = false);

    // Manual upload helpers (used by WebUI provider or OTA tooling)
    bool beginUpload(size_t expectedSize = 0);
    bool acceptUploadChunk(const uint8_t* data, size_t length);
    bool finalizeUpload();
    void abortUpload(const String& reason);

    // State accessors
    bool isIdle() const;
    bool isBusy() const;
    State getState() const { return state; }
    float getProgress() const { return progress; }
    size_t getDownloadedBytes() const { return downloadedBytes; }
    size_t getTotalBytes() const { return totalBytes; }
    const String& getLastResult() const { return lastResult; }
    const String& getLastError() const { return lastError; }
    const String& getLastVersion() const { return lastVersionSeen; }
    OTAConfig& mutableConfig() { return config; }
    const OTAConfig& getOTAConfig() const { return config; }

    // Set providers. If not set, network-based features are disabled and will error gracefully.
    void setManifestFetcher(ManifestFetcher fetcher) { manifestFetcher = std::move(fetcher); }
    void setDownloader(Downloader dl) { downloader = std::move(dl); }

private:
    struct ManifestInfo {
        String version;
        String url;
        String sha256;
        String signature;
        bool valid = false;
    };

    struct UploadSession {
        bool active = false;
        bool success = false;
        bool initialized = false;
        String error;
        size_t received = 0;
        size_t expected = 0;
    };

    OTAConfig config;
    State state = State::Idle;
    unsigned long stateChangeMillis = 0;
    unsigned long lastProgressPublishMillis = 0;
    unsigned long nextCheckMillis = 0;
    float progress = 0.0f;
    size_t downloadedBytes = 0;
    size_t totalBytes = 0;
    String lastVersionSeen;
    String lastResult;
    String lastError;
    UploadSession uploadSession;
    float lastLoggedProgress = -1.0f;
    size_t lastLoggedBytes = 0;

    bool pendingCheck = false;
    bool pendingForce = false;
    bool pendingUrlUpdate = false;
    bool pendingUrlForce = false;
    String pendingUrl;

    // Pluggable providers (unset by default)
    ManifestFetcher manifestFetcher = nullptr;
    Downloader downloader = nullptr;

    // Helpers
    void scheduleNextCheck(uint32_t delayMs = 0);
    void transition(State nextState, const String& reason = "");
    bool shouldCheckNow() const;
    bool performCheck(bool force);
    ManifestInfo fetchManifest();
    bool installFromUrl(const String& url, const String& expectedSha256, bool allowDowngrade);
    bool installFromStream(Stream& stream, size_t size, const String& expectedSha256);
    bool finalizeUpdateOperation(const String& source, bool autoRebootPending);
    bool verifySha256(const uint8_t* digest, const String& expectedHex);
    bool isNewerVersion(const String& candidate) const;
    void broadcastProgress();
    void publishStatusEvent(const String& topicSuffix, std::function<void(JsonDocument&)> fn, bool sticky);
};

} // namespace Components
} // namespace DomoticsCore
