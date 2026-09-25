// Board probe: WebUI (or NTP) begun before any network interface exists.
// Prints a heartbeat so a boot loop and a live board read differently, and
// creates the station interface after 8 s without joining any network.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_netif.h>
#include <DomoticsCore/Core.h>
#ifndef PROBE_NTP
#define PROBE_NTP 0
#endif
#if PROBE_NTP
#include <DomoticsCore/NTP.h>
#else
#include <DomoticsCore/WebUI.h>
#endif

using namespace DomoticsCore;

static Core* core = nullptr;
static bool netifMade = false;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("NONETIF probe target=%s reset=%d netifs=%u\n",
                  PROBE_NTP ? "ntp" : "webui", (int)esp_reset_reason(),
                  (unsigned)esp_netif_get_nr_of_ifs());

    core = new Core();
#if PROBE_NTP
    core->addComponent(std::unique_ptr<Components::NTPComponent>(new Components::NTPComponent()));
#else
    core->addComponent(std::unique_ptr<Components::WebUIComponent>(new Components::WebUIComponent()));
#endif
    CoreConfig cfg;
    cfg.deviceName = "NoNetifProbe";
    Serial.println("NONETIF begin");
    core->begin(cfg);
    Serial.printf("NONETIF begun heap=%u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
    core->loop();
    static uint32_t last = 0;
    if (millis() - last >= 2000) {
        last = millis();
        Serial.printf("NONETIF alive t=%lus netifs=%u heap=%u\n", (unsigned long)(millis() / 1000),
                      (unsigned)esp_netif_get_nr_of_ifs(), (unsigned)ESP.getFreeHeap());
    }
    if (!netifMade && millis() > 8000) {
        netifMade = true;
        WiFi.mode(WIFI_STA);  // creates the interface; joins nothing
        Serial.printf("NONETIF station interface created, netifs=%u\n",
                      (unsigned)esp_netif_get_nr_of_ifs());
    }
}
