// ESP32 half: one line of each shape, every five seconds, read over telnet.
#include <Arduino.h>
#include <DomoticsCore/System.h>
#include <DomoticsCore/CoreLog_HAL.h>
#include <Preferences.h>
#include <driver/gpio.h>
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
static uint32_t lastProvoke = 0;

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

#if PROBE_RELEASE_CAPTURE
    // The state before this capture existed, through public API rather than an
    // edit: the lines below then reach the serial port and nothing else.
    HAL::CoreLog::removeCapture();
    Serial.println("PROBE capture released");
#endif
}

void loop() {
    if (sys) sys->loop();

    const uint32_t now = millis();
    if (now - lastProvoke >= 5000) {
        lastProvoke = now;
        // An ESP-IDF library line: driver/gpio.c refuses an input-only pin in its
        // own words, through esp_log_write.
        gpio_set_direction((gpio_num_t)99, GPIO_MODE_OUTPUT);
        // An Arduino core line: Preferences.cpp refuses a namespace over the NVS
        // limit, through log_printf and ets_printf.
        Preferences prefs;
        prefs.begin("a_namespace_name_far_too_long", false);
    }
}
