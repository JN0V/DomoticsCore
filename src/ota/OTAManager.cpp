#include <DomoticsCore/OTAManager.h>
#include <DomoticsCore/WebConfig.h>
#include <DomoticsCore/SystemUtils.h>
#include <DomoticsCore/Logger.h>
#include "OTAPages.h"
#include <Update.h>

OTAManager::OTAManager(AsyncWebServer* srv, WebConfig* webCfg) : server(srv), webConfig(webCfg) {
  otaError = "";
}

void OTAManager::begin() {
  setupRoutes();
}

void OTAManager::setupRoutes() {
  // OTA update page
  server->on("/update", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (!webConfig->authenticate(request)) return;
    String page = FPSTR(HTML_OTA_PAGE);
    String errorHtml = "";
    if (otaError != "") {
      errorHtml = "<div class='error'><h3>Update Failed</h3><p>" + otaError + "</p></div>";
    }
    page.replace("%ERROR%", errorHtml);
    request->send(200, "text/html", page);
  });

  // OTA update POST handler
  server->on("/update", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!webConfig->authenticate(request)) return;
    bool shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", 
      shouldReboot ? "Update successful! Rebooting..." : "Update failed!");
    response->addHeader("Connection", "close");
    request->send(response);
    
    if (shouldReboot) {
      SystemUtils::watchdogSafeDelay(1000);
      ESP.restart();
    }
  }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (!index) {
      DLOG_I(LOG_OTA, "Update Start: %s", filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        otaError = "Cannot start update: " + String(Update.errorString());
        DLOG_E(LOG_OTA, "Update begin failed: %s", Update.errorString());
      }
    }
    
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        otaError = "Write failed: " + String(Update.errorString());
        DLOG_E(LOG_OTA, "Update write failed: %s", Update.errorString());
      }
    }
    
    if (final) {
      if (Update.end(true)) {
        DLOG_I(LOG_OTA, "Update Success: %uB", index+len);
        otaError = "";
      } else {
        otaError = "Update failed: " + String(Update.errorString());
        DLOG_E(LOG_OTA, "Update end failed: %s", Update.errorString());
      }
    }
  });
}
