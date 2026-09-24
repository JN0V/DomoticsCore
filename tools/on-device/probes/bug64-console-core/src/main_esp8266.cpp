// ESP8266 half: a scan every twenty seconds is what makes the SDK narrate, and
// `core on` over the console is what makes that narration exist at all.
#include <Arduino.h>
#include <DomoticsCore/System.h>
#include <DomoticsCore/CoreLog_HAL.h>
#include <ESP8266WiFi.h>
using namespace DomoticsCore;

#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif
#ifndef DC_WIFI_SSID
#  define DC_WIFI_SSID ""
#endif
#ifndef DC_WIFI_PASSWORD
#  define DC_WIFI_PASSWORD ""
#endif

static System* sys = nullptr;
static uint32_t lastScan = 0;

void setup() {
    Serial.begin(115200);
    delay(300);

    SystemConfig cfg;
    cfg.deviceName = "CoreLogProbe";
    cfg.wifiSSID = DC_WIFI_SSID;
    cfg.wifiPassword = DC_WIFI_PASSWORD;
    cfg.enableConsole = true;
    cfg.enableMQTT = false;
    cfg.enableWebUI = false;
    cfg.enableOTA = false;

    sys = new System(cfg);
    sys->begin();
}

void loop() {
    if (sys) sys->loop();

    const uint32_t now = millis();
    if (now - lastScan >= 20000) {
        lastScan = now;
        WiFi.scanNetworks(true);   // async: the SDK narrates scandone when it ends
    }
}
