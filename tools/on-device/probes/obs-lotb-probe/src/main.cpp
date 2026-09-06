// OBS Lot B probe: System (console, Storage, SystemInfo, STA WiFi) with a
// stopwatch around loop(). "LOOPCOST" lines every 5 s give the mean and the
// maximum iteration, and say whether the recorder's tick is compiled in.
#include <Arduino.h>
#include <DomoticsCore/System.h>
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
static uint32_t iters = 0, maxUs = 0, lastReport = 0;

void setup() {
    HAL::Platform::initializeLogging(115200);
    HAL::Platform::delayMs(500);
    SystemConfig cfg;
    cfg.deviceName = "ObsLotB";
    cfg.firmwareVersion = "0.0.1";
    cfg.enableLED = false;
    cfg.enableConsole = true;
    cfg.enableStorage = true;
    cfg.enableSystemInfo = true;
    cfg.wifiSSID = DC_WIFI_SSID;
    cfg.wifiPassword = DC_WIFI_PASSWORD;
    cfg.wifiAutoConfig = false;
    sys = new System(cfg);
    sys->begin();
#ifdef DEBUG_ESP_OOM
    Serial.setDebugOutput(true);   // the core's ":oom(size)@file:line" line needs os_print on
#endif
    // OBS-4 S5 coverage check: grow a String until its realloc fails. A stock
    // build records nothing for that path; under DEBUG_ESP_OOM the latch
    // counts it. The String dies before the reply is built.
    sys->registerCommand("bigstring", [](const String&) -> String {
        size_t grew = 0;
        {
            String s;
            for (int i = 0; i < 8192; ++i) {
                if (!s.concat(F("0123456789abcdef0123456789abcdef"))) break;
                grew = s.length();
            }
        }
        return String("grew to ") + grew + " B, then the realloc failed\n";
    });
    lastReport = millis();
}

void loop() {
    uint32_t t0 = micros();
    sys->loop();
    uint32_t dt = micros() - t0;
    iters++;
    if (dt > maxUs) maxUs = dt;
    if (millis() - lastReport >= 5000) {
        uint32_t span = millis() - lastReport;
        Serial.printf("LOOPCOST t=%lus iters=%lu mean=%lu us max=%lu us heap=%u tick=%d\n",
                      (unsigned long)(millis() / 1000), (unsigned long)iters,
                      (unsigned long)(span * 1000UL / (iters ? iters : 1)), (unsigned long)maxUs,
                      HAL::Platform::getFreeHeap(), (int)DOMOTICS_FLIGHT_RECORDER_TICK);
        iters = 0; maxUs = 0; lastReport = millis();
    }
}
