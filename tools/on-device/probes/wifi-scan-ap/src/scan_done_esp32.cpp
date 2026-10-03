// ESP32 only (excluded from the ESP8266 build): the SDK's own SCAN_DONE, so a
// scan the core's 6 s deadline called failed can be seen to have finished.
#include <Arduino.h>
#include <WiFi.h>

void listenForScanDone(const uint8_t* round, const uint32_t* startedAt) {
    WiFi.onEvent([round, startedAt](arduino_event_id_t, arduino_event_info_t info) {
        Serial.printf("SCANAP sdk_done round=%u t=%lums status=%u count=%u\n", (unsigned)*round,
                      (unsigned long)(millis() - *startedAt), (unsigned)info.wifi_scan_done.status,
                      (unsigned)info.wifi_scan_done.number);
    }, ARDUINO_EVENT_WIFI_SCAN_DONE);
}
