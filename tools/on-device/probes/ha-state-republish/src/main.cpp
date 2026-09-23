// Board probe: what Home Assistant reads back after the broker was away.
//
// A periodic sensor proves nothing here — it republishes on its own within one
// interval. The exposure is the entity that publishes on change and on nothing
// else, so this probe drives only those: a retained seed while the link is up,
// then one value per topic while the broker is down, and then silence.
//
// Six slots go into the outage — a binary sensor's state, a light's JSON state,
// three sensor states and one attributes document — so the drain takes two
// passes at four per loop(), and PENDING lines print it moving 6 → 2 → 0. All
// three entry points are covered: publishState, publishStateJson and
// publishAttributes, the last of which had no connectivity guard at all before.
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
#ifndef DC_REPUB_BROKER
#  define DC_REPUB_BROKER DC_MQTT_BROKER
#endif

static System* sys = nullptr;
static bool linkUp = false;
static bool inOutage = false;        // set at the disconnect, cleared at the connect
static bool seeded = false;          // the retained values published while up
static bool outagePublished = false; // the one round published while down
static uint32_t outageSince = 0;
static int lastPending = -1;
static uint32_t lastReport = 0;

static Components::HomeAssistant::HomeAssistantComponent* ha() {
    return sys->getCore().getComponent<
        Components::HomeAssistant::HomeAssistantComponent>("HomeAssistant");
}

static void report(const char* why) {
    auto* h = ha();
    Serial.printf("REPUB t=%lus %s link=%d seeded=%d outage-published=%d pending=%d refused=%lu\n",
                  (unsigned long)(millis() / 1000), why, (int)linkUp,
                  (int)seeded, (int)outagePublished,
                  h ? (int)h->getPendingPublishCount() : -1,
                  h ? (unsigned long)h->getStatistics().statesRefused : 0UL);
}

void setup() {
    HAL::Platform::initializeLogging(115200);
    HAL::Platform::delayMs(500);
    SystemConfig cfg;
    cfg.deviceName = "Republish";
    cfg.firmwareVersion = "0.0.1";
    cfg.enableLED = false;
    cfg.enableStorage = false;
    cfg.wifiSSID = DC_WIFI_SSID;
    cfg.wifiPassword = DC_WIFI_PASSWORD;
    cfg.wifiAutoConfig = false;
    cfg.enableMQTT = DC_REPUB_BROKER[0] != '\0';
    cfg.mqttBroker = DC_REPUB_BROKER;
    cfg.mqttPort = DC_MQTT_PORT;
    cfg.enableHomeAssistant = true;
    sys = new System(cfg);

    auto& bus = sys->getCore().getEventBus();
    bus.subscribe(String(MQTTEvents::EVENT_CONNECTED), [](const void*) {
        linkUp = true;
        inOutage = false;
        report("connected");
    }, nullptr);
    bus.subscribe(String(MQTTEvents::EVENT_DISCONNECTED), [](const void*) {
        linkUp = false;
        inOutage = true;
        outageSince = millis();
        report("disconnected");
    }, nullptr);

    sys->begin();
    if (!cfg.enableMQTT) Serial.println("REPUB no broker: define DC_REPUB_BROKER");

    // A door contact and a lamp: they have a state, they publish only when it
    // changes, and nothing republishes them on a timer.
    if (auto* h = ha()) {
        h->addBinarySensor("door", "Front Door", "door");
        h->addLight("lamp", "Lamp");
        h->addSensor("temp", "Temperature", "°C", "temperature");
        h->addSensor("hum", "Humidity", "%", "humidity");
        h->addSensor("pres", "Pressure", "hPa", "pressure");
    }
    lastReport = millis();
}

// The values Home Assistant would still be showing if the outage's round were
// lost, and the round itself: same topics, different values.
static void publishRound(bool afterTheSeed) {
    auto* h = ha();
    if (!h) return;
    h->publishState("door", afterTheSeed ? "ON" : "OFF");

    JsonDocument light;
    light["state"] = afterTheSeed ? "ON" : "OFF";
    light["brightness"] = afterTheSeed ? 200 : 10;
    h->publishStateJson("lamp", light);

    h->publishState("temp", afterTheSeed ? "23.50" : "18.00");
    h->publishState("hum", afterTheSeed ? "61.00" : "40.00");
    h->publishState("pres", afterTheSeed ? "1013.00" : "990.00");

    JsonDocument attrs;
    attrs["source"] = afterTheSeed ? "outage" : "seed";
    h->publishAttributes("door", attrs);
}

void loop() {
    sys->loop();

    if (linkUp && !seeded) {
        if (auto* h = ha()) {
            if (h->isReady()) {
                publishRound(false);
                seeded = true;
                report("seeded");
            }
        }
    }

    // One round, two seconds into the outage, and never again.
    if (inOutage && seeded && !outagePublished && millis() - outageSince >= 2000) {
        publishRound(true);
        outagePublished = true;
        report("outage-published");
    }

    // Every change of the store, so the paced drain is visible rather than
    // inferred: it should step down by four, not fall to zero in one loop.
    if (auto* h = ha()) {
        const int pending = (int)h->getPendingPublishCount();
        if (pending != lastPending) {
            lastPending = pending;
            report("pending-changed");
        }
    }

    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        report("tick");
    }
}
