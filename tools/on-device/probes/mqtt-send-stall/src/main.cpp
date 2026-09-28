// Board probe: times loop() and MQTTComponent::publish() against a slow or
// lossy broker. Read the STALL and SLOWLOOP lines.
#include <Arduino.h>
#include <DomoticsCore/System.h>
#include <DomoticsCore/HomeAssistant.h>
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
#ifndef DC_STALL_BROKER
#  define DC_STALL_BROKER ""
#endif
#ifndef DC_STALL_PORT
#  define DC_STALL_PORT 1883
#endif
#ifndef DC_STALL_BURST
#  define DC_STALL_BURST 4
#endif
#ifndef DC_STALL_ENTITIES
#  define DC_STALL_ENTITIES 0
#endif
#ifndef DC_STALL_PAYLOAD
#  define DC_STALL_PAYLOAD 60
#endif

static System* sys = nullptr;
static Components::MQTTComponent* mqtt = nullptr;
static uint32_t lastBurst = 0, lastReport = 0, seq = 0;
static size_t peakQueuedBytes = 0;
static uint32_t maxLoopUs = 0, maxPublishUs = 0, loops = 0, over50 = 0, over200 = 0, publishes = 0;

void setup() {
    HAL::Platform::initializeLogging(115200);
    HAL::Platform::delayMs(500);
    SystemConfig cfg;
    cfg.deviceName = "SendStall";
    cfg.firmwareVersion = "0.0.1";
    cfg.enableLED = false;
    cfg.enableStorage = false;
    cfg.wifiSSID = DC_WIFI_SSID;
    cfg.wifiPassword = DC_WIFI_PASSWORD;
    cfg.wifiAutoConfig = false;
    cfg.enableMQTT = DC_STALL_BROKER[0] != '\0';
    cfg.mqttBroker = DC_STALL_BROKER;
    cfg.mqttPort = DC_STALL_PORT;
    cfg.enableHomeAssistant = true;
    sys = new System(cfg);
    sys->begin();
    if (!cfg.enableMQTT) Serial.println("STALL no broker: define DC_STALL_BROKER");
    mqtt = sys->getCore().getComponent<Components::MQTTComponent>("MQTT");
    auto* ha = sys->getCore().getComponent<
        Components::HomeAssistant::HomeAssistantComponent>("HomeAssistant");
    if (ha) {
        ha->addSensor("uptime", "Uptime", "s");
        ha->addSensor("seq", "Sequence", "");
        for (int i = 0; i < DC_STALL_ENTITIES; ++i) {
            char id[16], name[24];
            snprintf(id, sizeof(id), "extra_%d", i);
            snprintf(name, sizeof(name), "Extra sensor %d", i);
            ha->addSensor(id, name, "W");
        }
    }
    lastBurst = lastReport = millis();
}

void loop() {
    const uint32_t t0 = micros();
    sys->loop();

    if (mqtt && mqtt->getQueuedBytes() > peakQueuedBytes) peakQueuedBytes = mqtt->getQueuedBytes();
    if (DC_STALL_BURST > 0 && mqtt && mqtt->isConnected() && millis() - lastBurst >= 2000) {
        lastBurst = millis();
        for (int i = 0; i < DC_STALL_BURST; ++i) {
            String payload;
            payload.reserve(DC_STALL_PAYLOAD + 32);
            payload = String("{\"seq\":") + seq++ + ",\"i\":" + i + ",\"pad\":\"";
            while (payload.length() < DC_STALL_PAYLOAD) payload += 'x';
            payload += "\"}";
            const uint32_t p0 = micros();
            mqtt->publish("stall/events", payload);
            const uint32_t dp = micros() - p0;
            publishes++;
            if (dp > maxPublishUs) maxPublishUs = dp;
            if (dp > 100000) Serial.printf("SLOWPUBLISH t=%lums %lums\n", (unsigned long)millis(), (unsigned long)(dp / 1000));
        }
    }

    const uint32_t dl = micros() - t0;
    loops++;
    if (dl > maxLoopUs) maxLoopUs = dl;
    if (dl > 50000) over50++;
    if (dl > 200000) over200++;
    if (dl > 100000) Serial.printf("SLOWLOOP t=%lums %lums\n", (unsigned long)millis(), (unsigned long)(dl / 1000));

    if (millis() - lastReport >= 10000) {
        lastReport = millis();
        Serial.printf("STALL t=%lus connected=%d loops=%lu maxLoop=%lums over50=%lu over200=%lu publishes=%lu maxPublish=%lums"
                      " peakQueued=%uB errors=%lu sent=%lu heap=%u\n",
                      (unsigned long)(millis() / 1000), mqtt ? (int)mqtt->isConnected() : -1,
                      (unsigned long)loops, (unsigned long)(maxLoopUs / 1000), (unsigned long)over50,
                      (unsigned long)over200, (unsigned long)publishes, (unsigned long)(maxPublishUs / 1000),
                      (unsigned)peakQueuedBytes, mqtt ? (unsigned long)mqtt->getStatistics().publishErrors : 0UL,
                      mqtt ? (unsigned long)mqtt->getStatistics().publishCount : 0UL, (unsigned)HAL::Platform::getFreeHeap());
        maxLoopUs = maxPublishUs = 0;
        loops = over50 = over200 = publishes = 0;
    }
}
