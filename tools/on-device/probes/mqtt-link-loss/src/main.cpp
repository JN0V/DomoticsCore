// Board probe: counts mqtt/connected and mqtt/disconnected as the EventBus
// delivers them, with the MQTT state and HomeAssistant::isReady() beside them.
// Restart the broker under it and read the LINKLOSS lines.
#include <Arduino.h>
#include <DomoticsCore/System.h>
#include <DomoticsCore/MQTTEvents.h>
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
#ifndef DC_MQTT_BROKER
#  define DC_MQTT_BROKER ""
#endif
#ifndef DC_MQTT_PORT
#  define DC_MQTT_PORT 1883
#endif
// The broker has to be one you can restart. Name it on the build line rather
// than in secrets.h, which every other example on the ladder reads.
#ifndef DC_LINK_LOSS_BROKER
#  define DC_LINK_LOSS_BROKER DC_MQTT_BROKER
#endif

static System* sys = nullptr;
static uint32_t connects = 0, disconnects = 0, lastReport = 0;

static void report(const char* why) {
    auto* ha = sys->getCore().getComponent<
        Components::HomeAssistant::HomeAssistantComponent>("HomeAssistant");
    auto* mqtt = sys->getCore().getComponent<Components::MQTTComponent>("MQTT");
    Serial.printf("LINKLOSS t=%lus %s connected=%lu disconnected=%lu state=%s ready=%d\n",
                  (unsigned long)(millis() / 1000), why,
                  (unsigned long)connects, (unsigned long)disconnects,
                  mqtt ? mqtt->getStateString().c_str() : "?",
                  ha ? (int)ha->isReady() : -1);
}

void setup() {
    HAL::Platform::initializeLogging(115200);
    HAL::Platform::delayMs(500);
    SystemConfig cfg;
    cfg.deviceName = "LinkLoss";
    cfg.firmwareVersion = "0.0.1";
    cfg.enableLED = false;
    cfg.enableStorage = false;
    cfg.wifiSSID = DC_WIFI_SSID;
    cfg.wifiPassword = DC_WIFI_PASSWORD;
    cfg.wifiAutoConfig = false;
    cfg.enableMQTT = DC_LINK_LOSS_BROKER[0] != '\0';
    cfg.mqttBroker = DC_LINK_LOSS_BROKER;
    cfg.mqttPort = DC_MQTT_PORT;
    cfg.enableHomeAssistant = true;
    sys = new System(cfg);

    // Before begin(), or the boot connection is counted only because the bus
    // defers dispatch — an assumption about timing rather than a reading.
    auto& bus = sys->getCore().getEventBus();
    bus.subscribe(String(MQTTEvents::EVENT_CONNECTED),
                  [](const void*) { connects++; report("connected"); }, nullptr);
    bus.subscribe(String(MQTTEvents::EVENT_DISCONNECTED),
                  [](const void*) { disconnects++; report("disconnected"); }, nullptr);

    sys->begin();
    if (!cfg.enableMQTT) Serial.println("LINKLOSS no broker: define DC_LINK_LOSS_BROKER");

    // One entity, so the component has discovery and state to publish and its
    // isReady() guard sits on a path that matters.
    auto* ha = sys->getCore().getComponent<
        Components::HomeAssistant::HomeAssistantComponent>("HomeAssistant");
    if (ha) ha->addSensor("uptime", "Uptime", "s");
    lastReport = millis();
}

void loop() {
    sys->loop();
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        report("tick");
        auto* ha = sys->getCore().getComponent<
            Components::HomeAssistant::HomeAssistantComponent>("HomeAssistant");
        if (ha) ha->publishState("uptime", String(millis() / 1000));
    }
}
