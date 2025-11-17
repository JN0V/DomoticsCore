#include <Arduino.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/WifiWebUI.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/MQTTWebUI.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

#define LOG_APP "APP"

// Default MQTT configuration (can be changed via WebUI)
static const char* DEFAULT_MQTT_BROKER = "";
static const uint16_t DEFAULT_MQTT_PORT = 1883;

Core core;
Utils::NonBlockingDelay publishTimer(10000); // 10s telemetry

void setup() {
    Serial.begin(115200);
    delay(500);

    DLOG_I(LOG_APP, "=== DomoticsCore MQTT + Wifi + WebUI Example ===");

    // WebUI component
    WebUIConfig webCfg;
    webCfg.deviceName = "MQTT Wifi WebUI";
    webCfg.wsUpdateInterval = 2000;
    auto webui = std::make_unique<WebUIComponent>(webCfg);
    auto* webuiPtr = webui.get();
    core.addComponent(std::move(webui));

    // Wifi component: start in AP-only (empty creds) for initial config
    auto wifi = std::make_unique<WifiComponent>("", "");
    auto* wifiPtr = wifi.get();
    core.addComponent(std::move(wifi));

    // MQTT component
    MQTTConfig mqttCfg;
    mqttCfg.broker = DEFAULT_MQTT_BROKER;
    mqttCfg.port = DEFAULT_MQTT_PORT;
    mqttCfg.clientId = "mqtt-wifi-webui-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    mqttCfg.enabled = true;
    mqttCfg.autoReconnect = true;
    mqttCfg.enableLWT = true;
    mqttCfg.lwtTopic = mqttCfg.clientId + "/status";
    mqttCfg.lwtMessage = "offline";
    mqttCfg.lwtQoS = 1;
    mqttCfg.lwtRetain = true;

    auto mqtt = std::make_unique<MQTTComponent>(mqttCfg);
    auto* mqttPtr = mqtt.get();  // Keep for WebUI provider
    core.addComponent(std::move(mqtt));

    // Register EventBus listeners (capture clientId before config is moved)
    String clientId = mqttCfg.clientId;
    
    core.on<bool>("mqtt/connected", [&core, clientId](const bool&) {
        DLOG_I(LOG_APP, "MQTT connected");
        
        // Publish online status via EventBus
        DomoticsCore::Components::MQTTPublishEvent pubEv{};
        String statusTopic = clientId + "/status";
        strncpy(pubEv.topic, statusTopic.c_str(), sizeof(pubEv.topic) - 1);
        strncpy(pubEv.payload, "online", sizeof(pubEv.payload) - 1);
        pubEv.qos = 1;
        pubEv.retain = true;
        core.emit("mqtt/publish", pubEv);
        
        // Subscribe to command topics via EventBus
        DomoticsCore::Components::MQTTSubscribeEvent subEv{};
        String cmdTopic = clientId + "/command/#";
        strncpy(subEv.topic, cmdTopic.c_str(), sizeof(subEv.topic) - 1);
        subEv.qos = 1;
        core.emit("mqtt/subscribe", subEv);
    });
    
    core.on<bool>("mqtt/disconnected", [](const bool&) { 
        DLOG_W(LOG_APP, "MQTT disconnected"); 
    });

    // Initialize components
    DLOG_I(LOG_APP, "Initializing components...");
    core.begin();

    // Register WebUI providers
    if (webuiPtr && wifiPtr) {
        webuiPtr->registerProviderWithComponent(new WifiWebUI(wifiPtr), wifiPtr);
    }
    if (webuiPtr && mqttPtr) {
        webuiPtr->registerProviderWithComponent(new MQTTWebUI(mqttPtr), mqttPtr);
    }

    DLOG_I(LOG_APP, "Setup complete");
}

void loop() {
    core.loop();

    if (publishTimer.isReady()) {
        auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
        auto* wifi = core.getComponent<WifiComponent>("Wifi");
        if (mqtt && mqtt->isConnected()) {
            JsonDocument doc;
            doc["uptime"] = millis() / 1000;
            doc["freeHeap"] = ESP.getFreeHeap();
            if (wifi) doc["rssi"] = wifi->getRSSI();
            doc["temp"] = 20.0 + (random(0, 100) / 10.0);
            String topic = mqtt->getConfig().clientId + "/telemetry";
            mqtt->publishJSON(topic, doc, 0, false);
        }
    }
}
