// OBS brownout probe, ESP32-CAM. Keeps a record in RTC_NOINIT, then draws
// as much current as it can (AP+STA, max TX power, back-to-back scans).
// If the supply sags, the next boot says reason 9 (BROWNOUT) and whether
// the record survived it.
#include <Arduino.h>
#include <WiFi.h>
#include "esp_system.h"

RTC_NOINIT_ATTR struct {
    uint32_t magic, boots, brownouts, alive, lastReason, lastAliveBeforeReset;
} rec;
static const uint32_t MAGIC = 0x0B5C0DE5;
static uint32_t t0;

void setup() {
    Serial.begin(115200);
    delay(300);
    int reason = (int)esp_reset_reason();
    bool kept = (rec.magic == MAGIC);
    if (!kept) { memset(&rec, 0, sizeof(rec)); rec.magic = MAGIC; }
    rec.lastAliveBeforeReset = rec.alive;
    rec.boots++;
    rec.lastReason = reason;
    if (reason == ESP_RST_BROWNOUT) rec.brownouts++;
    Serial.println();
    Serial.printf("=== OBS-PROBE-CAM BOOT reason=%d kept=%d boots=%u brownouts=%u alive_before_reset=%u ===\n",
                  reason, kept, rec.boots, rec.brownouts, rec.lastAliveBeforeReset);
    rec.alive = 0;
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.softAP("obs-probe-cam", "12345678");
    WiFi.begin("obs-nonexistent-ssid", "nothing");
    t0 = millis();
}

void loop() {
    delay(1000);
    rec.alive++;
    int n = -1;
    if (rec.alive % 3 == 0) n = WiFi.scanNetworks(false, true);
    Serial.printf("alive=%us heap=%u scan=%d\n", rec.alive, ESP.getFreeHeap(), n);
    if (rec.alive == 120) Serial.println("NO BROWNOUT IN 120 s");
}
