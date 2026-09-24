// Board probe: the discovery document a code-less alarm panel actually puts on
// the wire. The device-side half of the measurement — the other half is Home
// Assistant being asked to arm the panel, which is the action that fails when
// the requirement keys are absent.
//
// The panel carries no code and requires none, which is what the library's own
// defaults describe, with the two arm modes a real panel carries.
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
#ifndef DC_PANEL_BROKER
#  define DC_PANEL_BROKER DC_MQTT_BROKER
#endif

static System* sys = nullptr;

void setup() {
    Serial.begin(115200);
    delay(300);

    SystemConfig cfg;
    cfg.deviceName = "PanelProbe";
    cfg.wifiSSID = DC_WIFI_SSID;
    cfg.wifiPassword = DC_WIFI_PASSWORD;
    cfg.enableMQTT = true;
    cfg.mqttBroker = DC_PANEL_BROKER;
    cfg.mqttPort = DC_MQTT_PORT;
    cfg.enableHomeAssistant = true;
    cfg.enableWebUI = false;
    cfg.enableOTA = false;

    sys = new System(cfg);
    sys->begin();

    auto* ha = sys->getCore().getComponent<Components::HomeAssistant::HomeAssistantComponent>("HomeAssistant");
    if (!ha) { Serial.println("PANEL no HomeAssistant component"); return; }

    using namespace Components::HomeAssistant;
    ha->addAlarmControlPanel("alarm_control", "Alarm Control", "mdi:shield-home",
                             AlarmFeature::ArmAway | AlarmFeature::ArmNight);

    sys->getCore().on<int>(DomoticsCore::HAEvents::EVENT_DISCOVERY_PUBLISHED, [](const int& n) {
        Serial.printf("PANEL discovery published for %d entities\n", n);
    });
}

void loop() {
    if (sys) sys->loop();
}
