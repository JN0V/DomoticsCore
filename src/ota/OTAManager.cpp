#include <DomoticsCore/OTAManager.h>
#include <DomoticsCore/WebConfig.h>
#include <DomoticsCore/SystemUtils.h>
#include <DomoticsCore/Logger.h>
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
    String html = "<!DOCTYPE html><html><head><title>OTA Update</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body{font-family:Arial;margin:40px;background:#f0f0f0}";
    html += ".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}";
    html += "h1{color:#333;text-align:center}";
    html += ".error{background:#ffe7e7;padding:15px;border-radius:5px;margin:10px 0;color:#d00}";
    html += ".success{background:#e7ffe7;padding:15px;border-radius:5px;margin:10px 0;color:#0a0}";
    html += ".button{display:inline-block;padding:10px 20px;margin:5px;background:#007cba;color:white;text-decoration:none;border-radius:5px;border:none;cursor:pointer}";
    html += "input[type=file]{margin:10px 0}";
    html += "</style></head><body>";
    html += "<div class='container'><h1>OTA Firmware Update</h1>";
    
    if (otaError != "") {
      html += "<div class='error'><h3>Update Failed</h3><p>" + otaError + "</p></div>";
    }
    
    html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<p>Select firmware file (.bin):</p>";
    html += "<input type='file' name='update' accept='.bin'>";
    html += "<br><br><input type='submit' value='Upload Firmware' class='button'>";
    html += "</form>";
    
    html += "<br><a href='/' class='button'>Back to Main</a>";
    html += "</div></body></html>";
    request->send(200, "text/html", html);
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
      delay(1000);
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
