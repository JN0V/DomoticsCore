#pragma once

/**
 * @file OTAWebUI.h
 * @brief Composition-based WebUI provider exposing OTA controls and REST endpoints.
 * 
 * @note OTA requires custom REST routes for file uploads (multipart/form-data).
 *       The `init()` method must be called after WebUI server initialization to register these routes.
 *       This differs from simpler components (LED, WiFi) which use only standard WebUI field interactions.
 */

#include <ArduinoJson.h>

#include "DomoticsCore/OTA.h"
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/WebUI.h"
#include "DomoticsCore/BaseWebUIComponents.h"
#include "DomoticsCore/Logger.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief WebUI provider that bridges `OTAComponent` with `WebUIComponent` routes and contexts.
 */
class OTAWebUI : public IWebUIProvider {
public:
    explicit OTAWebUI(OTAComponent* component)
        : ota(component) {}

    /**
     * @brief Initialize routes after construction. Call this once WebUI component is available.
     * @param webuiComponent Pointer to the WebUI component for registering custom upload routes.
     */
    void init(WebUIComponent* webuiComponent) {
        webui = webuiComponent;
        if (webui) {
            registerRoutes();
        }
    }

    String getWebUIName() const override { return ota ? ota->metadata.name : String("OTA"); }
    String getWebUIVersion() const override { return ota ? ota->metadata.version : String("1.0.0"); }

    std::vector<WebUIContext> getWebUIContexts() override {
        std::vector<WebUIContext> contexts;
        if (!ota) return contexts;

        const OTAConfig& cfg = ota->getConfig();

        // Unified OTA card with mode selector
        WebUIContext otaCard = WebUIContext::settings("ota_unified", "Firmware Update");
        if (cfg.enableWebUIUpload) {
            otaCard.customHtml = R"(
                <div class="card-header">
                    <h3 class="card-title">Firmware Update</h3>
                </div>
                <div class="card-content">
                    <!-- Mode Selector -->
                    )" + []() {
                        String options[] = {"remote|Remote URL", "manual|Local File"};
                        return BaseWebUIComponents::radioGroup("otaMode", "Update Method", options, 2, 0);
                    }() + R"(

                    <!-- Common Status Display -->
                    )" + BaseWebUIComponents::fieldRow("Status", "otaStatus", "Ready") + R"(
                    
                    <!-- Progress Bar -->
                    )" + BaseWebUIComponents::progressBar("otaProgressBar", "Progress", true) + R"(

                    <!-- Remote URL Section -->
                    <div id="remoteSection">
                        )" + BaseWebUIComponents::textInput("otaUpdateUrl", "Firmware URL", "https://...") + R"(
                        )" + BaseWebUIComponents::buttonRow(
                            BaseWebUIComponents::button("otaCheckButton", "Check For Updates", false) +
                            BaseWebUIComponents::button("otaInstallButton", "Download & Install", false)
                        ) + R"(
                    </div>

                    <!-- Manual Upload Section -->
                    <div id="manualSection" style="display: none;">
                        )" + BaseWebUIComponents::fileInput("otaUploadInput", "otaFileSelectButton", "otaFileName", "Select Firmware File") + R"(
                        )" + BaseWebUIComponents::buttonRow(
                            BaseWebUIComponents::button("otaUploadButton", "Upload Firmware", false)
                        ) + R"(
                    </div>

                    <!-- Auto Reboot Toggle -->
                    )" + BaseWebUIComponents::toggleSwitch("otaAutoReboot", "Auto Reboot", true) + R"(
                </div>
            )";
            otaCard.customJs = R"(
                (function(){
                    if (window.__dcOtaUnifiedInit) return;
                    const ensureInit = () => {
                        const modeRemote = document.getElementById('otaMode_remote');
                        const modeManual = document.getElementById('otaMode_manual');
                        const remoteSection = document.getElementById('remoteSection');
                        const manualSection = document.getElementById('manualSection');
                        const statusEl = document.getElementById('otaStatus');
                        const progressBar = document.getElementById('otaProgressBar');
                        const progressText = document.getElementById('otaProgressBar_text');
                        const updateUrlInput = document.getElementById('otaUpdateUrl');
                        const checkButton = document.getElementById('otaCheckButton');
                        const installButton = document.getElementById('otaInstallButton');
                        const input = document.getElementById('otaUploadInput');
                        const selectButton = document.getElementById('otaFileSelectButton');
                        const uploadButton = document.getElementById('otaUploadButton');
                        const fileNameEl = document.getElementById('otaFileName');
                        const autoRebootCheckbox = document.getElementById('otaAutoReboot');
                        
                        if (!modeRemote || !statusEl || !progressBar || !progressText) {
                            setTimeout(ensureInit, 50);
                            return;
                        }

                        if (window.__dcOtaUnifiedInit) return;
                        window.__dcOtaUnifiedInit = true;

                        // Mode toggle handler
                        const toggleMode = () => {
                            if (modeRemote.checked) {
                                remoteSection.style.display = 'block';
                                manualSection.style.display = 'none';
                            } else {
                                remoteSection.style.display = 'none';
                                manualSection.style.display = 'block';
                            }
                        };
                        
                        if (modeRemote && modeManual) {
                            modeRemote.addEventListener('change', toggleMode);
                            modeManual.addEventListener('change', toggleMode);
                            toggleMode(); // Initialize visibility
                        }

                        // Auto-reboot is read-only (configured at compile time)
                        if (autoRebootCheckbox) {
                            autoRebootCheckbox.disabled = true;
                        }

                        // Initialize URL field from config
                        fetch('/api/ota/unified')
                            .then(resp => resp.json())
                            .then(data => {
                                if (data.update_url && updateUrlInput) {
                                    updateUrlInput.value = data.update_url;
                                }
                                if (typeof data.auto_reboot !== 'undefined' && autoRebootCheckbox) {
                                    autoRebootCheckbox.checked = (data.auto_reboot === true || data.auto_reboot === 'true');
                                }
                            })
                            .catch(() => {});

                        // Remote URL handlers
                        if (checkButton) {
                            checkButton.addEventListener('click', async () => {
                                statusEl.textContent = 'Checking for updates...';
                                try {
                                    const resp = await fetch('/api/ota/check', { method: 'POST' });
                                    const data = await resp.json();
                                    statusEl.textContent = data.success ? 'Check initiated' : 'Check failed';
                                } catch (err) {
                                    statusEl.textContent = 'Error: ' + err;
                                }
                            });
                        }

                        if (installButton) {
                            installButton.addEventListener('click', async () => {
                                const url = updateUrlInput.value.trim();
                                if (!url) {
                                    statusEl.textContent = 'Please enter a firmware URL';
                                    return;
                                }
                                statusEl.textContent = 'Starting download...';
                                try {
                                    const resp = await fetch('/api/ota/update', {
                                        method: 'POST',
                                        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                                        body: 'url=' + encodeURIComponent(url) + '&force=true'
                                    });
                                    const data = await resp.json();
                                    statusEl.textContent = data.success ? 'Download started' : (data.error || 'Download failed');
                                } catch (err) {
                                    statusEl.textContent = 'Error: ' + err;
                                }
                            });
                        }

                        // Manual upload handlers
                        if (selectButton) {
                            selectButton.addEventListener('click', () => {
                                input.click();
                            });
                        }

                        if (input && fileNameEl && uploadButton) {
                            input.addEventListener('change', () => {
                                if (input.files && input.files.length > 0) {
                                    const file = input.files[0];
                                    fileNameEl.textContent = file.name + ' (' + (file.size / 1024 / 1024).toFixed(2) + ' MB)';
                                    fileNameEl.style.color = 'var(--text-primary)';
                                    uploadButton.disabled = false;
                                } else {
                                    fileNameEl.textContent = 'No file selected';
                                    fileNameEl.style.color = 'var(--text-secondary)';
                                    uploadButton.disabled = true;
                                }
                            });
                        }

                        if (uploadButton) {
                            uploadButton.addEventListener('click', async () => {
                                if (!input.files || input.files.length === 0) return;
                                const file = input.files[0];
                                uploadButton.disabled = true;
                                statusEl.textContent = 'Uploading...';
                                try {
                                    const formData = new FormData();
                                    formData.append('firmware', file);
                                    const resp = await fetch('/api/ota/upload', {
                                        method: 'POST',
                                        body: formData
                                    });
                                    const data = await resp.json();
                                    statusEl.textContent = data.success ? (data.message || 'Upload successful.') : (data.error || 'Upload failed');
                                    if (data.success && fileNameEl) {
                                        fileNameEl.textContent = 'No file selected';
                                        fileNameEl.style.color = 'var(--text-secondary)';
                                        input.value = '';
                                    }
                                } catch (err) {
                                    statusEl.textContent = 'Upload error: ' + err;
                                } finally {
                                    uploadButton.disabled = false;
                                }
                            });
                        }

                        // Listen for component status updates (only from ota_unified)
                        document.addEventListener('component-status-update', (event) => {
                            const { contextId, data } = event.detail || {};
                            if (contextId !== 'ota_unified') return;

                            const detail = data || {};
                            console.log('[OTA] Update received:', detail);
                            
                            // Update status text
                            if (detail.message) {
                                statusEl.textContent = detail.message;
                            }
                            if (detail.error) {
                                statusEl.textContent = 'Error: ' + detail.error;
                            }
                            
                            // Update progress bar and text
                            let progressPercent = 0;
                            let progressInfo = '0.0% - 0 bytes';
                            
                            // Get progress from backend
                            if (typeof detail.progress === 'number') {
                                progressPercent = detail.progress;
                            }
                            
                            // Calculate display info
                            if (typeof detail.bytes === 'number') {
                                const bytesMB = (detail.bytes / 1024 / 1024).toFixed(2);
                                if (typeof detail.total === 'number' && detail.total > 0) {
                                    const totalMB = (detail.total / 1024 / 1024).toFixed(2);
                                    progressPercent = ((detail.bytes / detail.total) * 100);
                                    progressInfo = progressPercent.toFixed(1) + '% - ' + bytesMB + ' MB / ' + totalMB + ' MB';
                                    if (statusEl && detail.state !== 'idle') statusEl.textContent = 'Transferring firmware...';
                                } else {
                                    progressInfo = progressPercent.toFixed(1) + '% - ' + bytesMB + ' MB';
                                }
                            } else {
                                progressInfo = progressPercent.toFixed(1) + '% - 0 bytes';
                            }
                            
                            if (progressBar) progressBar.style.width = progressPercent.toFixed(1) + '%';
                            if (progressText) progressText.textContent = progressInfo;
                            
                            // Handle reboot pending - auto refresh after 3s
                            if (detail.state === 'reboot_pending') {
                                statusEl.textContent = 'Update complete! Device rebooting...';
                                if (progressBar) progressBar.style.width = '100%';
                                if (progressText) progressText.textContent = '100% - Complete';
                                setTimeout(() => {
                                    window.location.reload();
                                }, 3000);  // Reload after 3s (device reboots in 2s)
                            } else if (detail.state === 'idle') {
                                if (progressBar) progressBar.style.width = '0%';
                                if (progressText) progressText.textContent = '0.0% - 0 bytes';
                            }
                            
                            // Update auto-reboot checkbox from backend state
                            if (typeof detail.auto_reboot !== 'undefined' && autoRebootCheckbox) {
                                autoRebootCheckbox.checked = (detail.auto_reboot === true || detail.auto_reboot === 'true');
                            }
                            
                            // Reset file selection after successful upload
                            if (detail.resetSelection && fileNameEl) {
                                fileNameEl.textContent = 'No file selected';
                                fileNameEl.style.color = 'var(--text-secondary)';
                            }
                            
                            // Control button states
                            if (typeof detail.buttonEnabled === 'boolean' && uploadButton) {
                                uploadButton.disabled = !detail.buttonEnabled;
                            }
                        });
                    };

                    ensureInit();
                })();
            )";
            otaCard.withRealTime(2000).withAPI("/api/ota/unified");
        } else {
            // Remote-only mode (no upload)
            otaCard
                .withField(WebUIField("status", "Status", WebUIFieldType::Display, ota->getLastResult(), "", true))
                .withField(WebUIField("progress", "Progress", WebUIFieldType::Display, formatProgress(), "", true))
                .withField(WebUIField("downloaded", "Downloaded", WebUIFieldType::Display, String(ota->getDownloadedBytes()), " bytes", true))
                .withField(WebUIField("update_url", "Firmware URL", WebUIFieldType::Text, cfg.updateUrl))
                .withField(WebUIField("check_now", "Check For Updates", WebUIFieldType::Button, ""))
                .withField(WebUIField("start_update", "Download & Install", WebUIFieldType::Button, ""))
                .withField(WebUIField("auto_reboot", "Auto Reboot", WebUIFieldType::Boolean, cfg.autoReboot ? "true" : "false"))
                .withRealTime(2000)
                .withAPI("/api/ota/update");
        }
        contexts.push_back(otaCard);

        return contexts;
    }

    String getWebUIData(const String& contextId) override {
        if (!ota) return "{}";

        JsonDocument doc;

        if (contextId == "ota_unified") {
            // Unified card data
            doc["state"] = stateToString(ota->getState());
            doc["message"] = ota->getLastResult();
            doc["progress"] = ota->getProgress(); // Send numeric value
            doc["bytes"] = ota->getDownloadedBytes();
            doc["total"] = ota->getTotalBytes();
            doc["update_url"] = ota->getConfig().updateUrl;
            doc["auto_reboot"] = ota->getConfig().autoReboot;
            doc["buttonEnabled"] = !ota->isBusy();
        } else {
            return "{}";
        }

        String json;
        if (serializeJson(doc, json) == 0) {
            return "{}";
        }
        return json;
    }

    bool hasDataChanged(const String& contextId) override {
        if (!ota) return false;
        
        if (contextId == "ota_unified") {
            // Only send updates when OTA state or progress changes
            OTAState current = {
                ota->getState(),
                ota->getProgress(),
                ota->getDownloadedBytes()
            };
            return otaState.hasChanged(current);
        }
        
        return true;  // Other contexts: always send
    }

    String handleWebUIRequest(const String& contextId, const String& /*endpoint*/, const String& method,
                              const std::map<String, String>& params) override {
        if (!ota) {
            return "{\"success\":false}";
        }

        if (method == "GET") {
            // Real-time updates handled by getWebUIData()
            return "{\"success\":true}";
        }

        if (method != "POST") {
            return "{\"success\":false}";
        }

        auto fieldIt = params.find("field");
        auto valueIt = params.find("value");
        if (fieldIt == params.end()) {
            return "{\"success\":false}";
        }
        const String field = fieldIt->second;
        const String value = (valueIt != params.end()) ? valueIt->second : String();

        if (contextId == "ota_unified" || contextId == "ota_manager") {
            if (field == "update_url") {
                // Use Get → Override → Set pattern
                OTAConfig cfg = ota->getConfig();
                cfg.updateUrl = value;
                ota->setConfig(cfg);
                return "{\"success\":true}";
            }
            if (field == "check_now") {
                ota->triggerImmediateCheck(true);
                return "{\"success\":true}";
            }
            if (field == "start_update") {
                String url = value.isEmpty() || value == "clicked" ? ota->getConfig().updateUrl : value;
                if (url.isEmpty()) {
                    return "{\"success\":false,\"error\":\"No firmware URL configured\"}";
                }
                ota->triggerUpdateFromUrl(url, true);
                return "{\"success\":true}";
            }
            if (field == "auto_reboot") {
                bool enable = (value == "true" || value == "1" || value == "on");
                // Use Get → Override → Set pattern
                OTAConfig cfg = ota->getConfig();
                cfg.autoReboot = enable;
                ota->setConfig(cfg);
                return "{\"success\":true}";
            }
        }

        return "{\"success\":false}";
    }

private:
    OTAComponent* ota = nullptr;      //!< Non-owning pointer to the OTA component
    WebUIComponent* webui = nullptr;  //!< Non-owning pointer to the WebUI component (set via init())

    struct UploadState {
        bool active = false;
        bool success = false;
        String error;
        String filename;
        size_t total = 0;
    } uploadState;
    
    // State tracking for change detection
    struct OTAState {
        OTAComponent::State state;
        float progress;
        size_t bytes;
        
        bool operator==(const OTAState& other) const {
            return state == other.state && 
                   progress == other.progress && 
                   bytes == other.bytes;
        }
        bool operator!=(const OTAState& other) const { return !(*this == other); }
    };
    LazyState<OTAState> otaState;

    static String stateToString(OTAComponent::State state) {
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

    String formatProgress() const {
        return String(ota ? ota->getProgress() : 0.0f, 1) + "%";
    }

    void registerRoutes() {
        if (!webui) return;

        // Unified API endpoint for OTA card (GET for current state, POST for updates)
        webui->registerApiRoute("/api/ota/unified", HTTP_GET, [this](AsyncWebServerRequest* request) {
            respondJson(request, [this](JsonDocument& doc) {
                if (!ota) return;
                doc["state"] = stateToString(ota->getState());
                doc["message"] = ota->getLastResult();
                doc["progress"] = ota->getProgress();
                doc["bytes"] = ota->getDownloadedBytes();
                doc["total"] = ota->getTotalBytes();
                doc["update_url"] = ota->getConfig().updateUrl;
                doc["auto_reboot"] = ota->getConfig().autoReboot;
            });
        });

        webui->registerApiRoute("/api/ota/unified", HTTP_POST, [this](AsyncWebServerRequest* request) {
            if (!ota) {
                respondJson(request, [](JsonDocument& doc) {
                    doc["success"] = false;
                    doc["error"] = "OTA unavailable";
                });
                return;
            }

            // Return current state (auto_reboot is read-only from config)
            respondJson(request, [this](JsonDocument& doc) {
                doc["state"] = stateToString(ota->getState());
                doc["message"] = ota->getLastResult();
                doc["progress"] = ota->getProgress();
                doc["bytes"] = ota->getDownloadedBytes();
                doc["total"] = ota->getTotalBytes();
            });
        });

        webui->registerApiRoute("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
            respondJson(request, [this](JsonDocument& doc) {
                if (!ota) return;
                doc["state"] = stateToString(ota->getState());
                doc["progress"] = ota->getProgress();
                doc["downloaded"] = ota->getDownloadedBytes();
                doc["total"] = ota->getTotalBytes();
                doc["lastResult"] = ota->getLastResult();
                doc["lastVersion"] = ota->getLastVersion();
                doc["autoReboot"] = ota->getConfig().autoReboot;
            });
        });

        webui->registerApiRoute("/api/ota/check", HTTP_POST, [this](AsyncWebServerRequest* request) {
            if (ota) ota->triggerImmediateCheck(true);
            respondJson(request, [](JsonDocument& doc) {
                doc["success"] = true;
            });
        });

        webui->registerApiRoute("/api/ota/update", HTTP_POST, [this](AsyncWebServerRequest* request) {
            if (!ota) {
                respondJson(request, [](JsonDocument& doc){
                    doc["success"] = false;
                    doc["error"] = "OTA unavailable";
                });
                return;
            }
            
            // Check if this is a real-time update request (no parameters)
            bool hasParams = request->hasParam("url", true) || request->hasParam("force", true) || request->hasParam("action", true);
            
            if (!hasParams) {
                // Real-time update: return current field values
                respondJson(request, [this](JsonDocument& doc) {
                    doc["status"] = ota->getLastResult();
                    doc["progress"] = formatProgress();
                    doc["downloaded"] = String(ota->getDownloadedBytes());
                    doc["update_url"] = ota->getConfig().updateUrl;
                    doc["auto_reboot"] = ota->getConfig().autoReboot ? "true" : "false";
                });
                return;
            }
            
            // Action request: trigger update
            String url = request->hasParam("url", true) ? request->getParam("url", true)->value() : ota->getConfig().updateUrl;
            bool force = false;
            if (request->hasParam("force", true)) {
                String v = request->getParam("force", true)->value();
                force = (v == "true" || v == "1" || v == "on");
            }
            bool ok = !url.isEmpty() && ota->triggerUpdateFromUrl(url, force);
            respondJson(request, [ok](JsonDocument& doc) {
                doc["success"] = ok;
                if (!ok) doc["error"] = "Missing or invalid URL";
            });
        });

        if (ota && ota->getConfig().enableWebUIUpload) {
            // Simple HTML upload page for demonstration
            webui->registerApiRoute("/ota/upload", HTTP_GET, [this](AsyncWebServerRequest* request){
                const char* html =
                    "<!DOCTYPE html><html><head><meta charset='utf-8'><title>OTA Upload</title>"
                    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                    "<style>body{font-family:sans-serif;margin:2rem;} .card{max-width:480px;padding:1rem;border:1px solid #ccc;border-radius:8px;} button{padding:.5rem 1rem;} input{margin:.5rem 0;}</style>"
                    "</head><body><div class='card'>"
                    "<h2>Firmware Upload</h2>"
                    "<form method='POST' action='/api/ota/upload' enctype='multipart/form-data'>"
                    "<input type='file' name='firmware' accept='.bin,.bin.gz' required><br>"
                    "<button type='submit'>Upload & Install</button>"
                    "</form>"
                    "<p>After a successful upload, the device may reboot automatically.</p>"
                    "</div></body></html>";
                request->send(200, "text/html", html);
            });
            webui->registerApiUploadRoute(
                "/api/ota/upload",
                [this](AsyncWebServerRequest* request) {
                    respondJson(request, [this](JsonDocument& doc) {
                        doc["success"] = uploadState.success;
                        if (!uploadState.success) {
                            doc["error"] = uploadState.error;
                        } else {
                            doc["message"] = "Upload successful";
                        }
                    });
                },
                [this](AsyncWebServerRequest* request, const String& filename, uint32_t index, uint8_t* data, size_t len, bool final) {
                    if (index == 0) {
                        uploadState = UploadState{};
                        uploadState.active = true;
                        uploadState.filename = filename;
                        uploadState.total = 0;
                        // Get content length from request for progress tracking
                        size_t expectedSize = request->contentLength();
                        ota->beginUpload(expectedSize);
                    }
                    if (data && len > 0) {
                        uploadState.total += len;
                        if (!ota->acceptUploadChunk(data, len)) {
                            uploadState.success = false;
                            uploadState.error = ota->getLastError();
                        }
                    }
                    if (final) {
                        uploadState.active = false;
                        uploadState.success = ota->finalizeUpload();
                        if (!uploadState.success) {
                            uploadState.error = ota->getLastError();
                        }
                    }
                }
            );
        }
    }

    template<typename Fn>
    static void respondJson(AsyncWebServerRequest* request, Fn fn) {
        AsyncResponseStream* response = request->beginResponseStream("application/json");
        JsonDocument doc;
        fn(doc);
        serializeJson(doc, *response);
        request->send(response);
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
