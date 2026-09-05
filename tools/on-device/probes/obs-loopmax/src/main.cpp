// OBS-7 residual 6: the longest System::loop() iteration under real load.
// FullStack configuration, credentials from the repository's secrets.h, and a
// stopwatch around every domotics->loop(). The loop watchdog stays at its
// shipped default (30 s) so the measurement is taken under the shipped
// condition. Drive an OTA with tools/on-device/ota_upload_check.py --commit
// while reading the serial port; the "LOOPMAX" lines are the figure.
#include <Arduino.h>
#include <vector>
#include <DomoticsCore/System.h>
using namespace DomoticsCore;
using namespace DomoticsCore::Components;
#define LOG_APP "APP"

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
#ifndef DC_MQTT_BROKER
#  define DC_MQTT_BROKER ""
#endif
#ifndef DC_MQTT_PORT
#  define DC_MQTT_PORT 1883
#endif
#ifndef DC_MQTT_USER
#  define DC_MQTT_USER ""
#endif
#ifndef DC_MQTT_PASSWORD
#  define DC_MQTT_PASSWORD ""
#endif

static System* domotics = nullptr;
static uint32_t maxIterUs = 0, iters = 0, lastReport = 0;
static bool scanDone = false;

void setup() {
    HAL::Platform::initializeLogging(115200);
    HAL::Platform::delayMs(1000);
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "LoopMaxProbe";
    config.firmwareVersion = "0.0.1";
    config.wifiSSID = DC_WIFI_SSID;
    config.wifiPassword = DC_WIFI_PASSWORD;
    config.mqttBroker = DC_MQTT_BROKER;
    config.mqttPort = DC_MQTT_PORT;
    config.mqttUser = DC_MQTT_USER;
    config.mqttPassword = DC_MQTT_PASSWORD;
    config.mqttClientId = config.deviceName;
#ifdef LOOPMAX_WDT
    config.loopWatchdogSeconds = LOOPMAX_WDT;
#endif
    domotics = new System(config);
    uint32_t t0 = micros();
    bool ok = domotics->begin();
    Serial.printf("LOOPMAX begin() took %lu us, ok=%d, loopWatchdogSeconds=%lu\n",
                  (unsigned long)(micros() - t0), (int)ok, (unsigned long)config.loopWatchdogSeconds);
}

void loop() {
    uint32_t t0 = micros();
    domotics->loop();
    uint32_t dt = micros() - t0;
    iters++;
    if (dt > maxIterUs) {
        maxIterUs = dt;
        if (dt > 50000) Serial.printf("LOOPMAX new max %lu us at t=%lus\n", (unsigned long)dt, (unsigned long)(millis() / 1000));
    }
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        Serial.printf("LOOPMAX t=%lus max=%lu us iters=%lu heap=%u\n",
                      (unsigned long)(millis() / 1000), (unsigned long)maxIterUs, (unsigned long)iters, HAL::Platform::getFreeHeap());
    }
    // One synchronous scan, 45 s after boot, timed on its own: the blocking
    // call a user might make from loop(), and what it costs against 30 s.
    if (!scanDone && millis() > 45000) {
        scanDone = true;
        uint32_t s0 = micros();
        int n = HAL::WiFiHAL::scanNetworks(false);
        uint32_t sdt = micros() - s0;
        HAL::WiFiHAL::scanDelete();
        Serial.printf("LOOPMAX sync scan: %lu us, %d networks\n", (unsigned long)sdt, n);
    }
}
