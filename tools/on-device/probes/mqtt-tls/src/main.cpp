// Board probe: an MQTT session over TLS against a CA the firmware carries.
// Read the TLS lines.
#include <Arduino.h>
#include <time.h>
#include <DomoticsCore/System.h>
using namespace DomoticsCore;

#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#  if __has_include("mqtt_tls_ca.h")
#    include "mqtt_tls_ca.h"
#  endif
#endif
#ifndef DC_WIFI_SSID
#  define DC_WIFI_SSID ""
#endif
#ifndef DC_WIFI_PASSWORD
#  define DC_WIFI_PASSWORD ""
#endif
#ifndef DC_TLS_BROKER
#  define DC_TLS_BROKER ""
#endif
#ifndef DC_TLS_PORT
#  define DC_TLS_PORT 8883
#endif

#ifdef DC_TLS_CA_PEM
static const char kCa[] = DC_TLS_CA_PEM;
static const char* const kCaOrNull = kCa;
#else
static const char* const kCaOrNull = nullptr;
#endif

static System* sys = nullptr;
static Components::MQTTComponent* mqtt = nullptr;
static uint32_t lastReport = 0;

void setup() {
    HAL::Platform::initializeLogging(115200);
    HAL::Platform::delayMs(500);
    SystemConfig cfg;
    cfg.deviceName = "TlsProbe";
    cfg.firmwareVersion = "0.0.1";
    cfg.enableLED = false;
    cfg.enableStorage = false;
    cfg.wifiSSID = DC_WIFI_SSID;
    cfg.wifiPassword = DC_WIFI_PASSWORD;
    cfg.wifiAutoConfig = false;
    cfg.enableNTP = true;
    cfg.enableMQTT = DC_TLS_BROKER[0] != '\0';
    cfg.mqttBroker = DC_TLS_BROKER;
    cfg.mqttPort = DC_TLS_PORT;
    cfg.mqttUseTLS = true;
    cfg.mqttCaCert = kCaOrNull;
    sys = new System(cfg);
    sys->begin();
    if (!cfg.enableMQTT) Serial.println("TLS no broker: define DC_TLS_BROKER");
    Serial.printf("TLS ca=%s\n", kCaOrNull ? "yes" : "none");
    mqtt = sys->getCore().getComponent<Components::MQTTComponent>("MQTT");
    lastReport = millis();
}

void loop() {
    sys->loop();
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        const auto st = mqtt ? mqtt->getStatistics() : Components::MQTTStatistics{};
        Serial.printf("TLS t=%lus connected=%d connects=%lu clock=%ld err='%s' heap=%u\n",
                      (unsigned long)(millis() / 1000), mqtt ? (int)mqtt->isConnected() : -1,
                      (unsigned long)st.connectCount, (long)time(nullptr),
                      mqtt ? mqtt->getLastError().c_str() : "no component",
                      (unsigned)HAL::Platform::getFreeHeap());
        if (mqtt && mqtt->isConnected()) mqtt->publish("tls/probe", String(millis()));
    }
}
