#include <DomoticsCore/Platform_HAL.h>
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
    // Initialize logging system before any DLOG calls
    HAL::Platform::initializeLogging(115200);

    // ============================================================================
    // EXAMPLE: MQTT + WiFi + WebUI Full Integration
    // ============================================================================
    // This example demonstrates full integration of MQTT, WiFi, and WebUI:
    // - WiFi component with automatic AP mode fallback (no credentials = AP mode)
    // - Web interface for both WiFi and MQTT configuration
    // - Persistent configuration storage (WiFi and MQTT settings saved)
    // - Automatic reconnection for both WiFi and MQTT
    // - Real-time status monitoring through WebUI
    // - Periodic telemetry publishing (uptime, heap, RSSI, temperature)
    // Expected Console Output (First Boot - AP Mode):
    //   [APP] === MQTT + WiFi + WebUI Full Integration ===
    //   [APP] Complete IoT device with web configuration
    //   [APP] - WiFi with automatic AP mode fallback
    //   [APP] - Web interface for WiFi and MQTT config
    //   [APP] - Persistent storage (settings saved)
    //   [APP] - Automatic reconnection
    //   [APP] - Telemetry publishing (JSON, every 10s)
    //   [APP] ============================================
    //   [Wifi] No credentials configured, starting AP mode
    //   [Wifi] AP started: MQTT-Wifi-WebUI-XXXX
    //   [WebUI] Initializing on port 80
    //   [MQTT] No broker configured - component disabled
    //   [APP] Setup complete
    //   [APP] Configure WiFi via WebUI at http://192.168.4.1
    // Expected Console Output (After WiFi Configured):
    //   [Wifi] Connecting to YourSSID...
    //   [Wifi] Connected! IP: 192.168.1.XXX
    //   [MQTT] Connecting to broker... (if configured)
    //   [APP] MQTT connected
    //   (Every 10 seconds: telemetry published)
    // Expected WebUI Access:
    //   - First boot: http://192.168.4.1 (AP mode)
    //   - After config: http://<device-ip>
    //   - Configure WiFi in WiFi tab
    //   - Configure MQTT in MQTT/Settings tab
    //   - Monitor status in Components tab
    // ============================================================================

    DLOG_I(LOG_APP, "=== MQTT + WiFi + WebUI Full Integration ===");
    DLOG_I(LOG_APP, "Complete IoT device with web configuration");
    DLOG_I(LOG_APP, "- WiFi with automatic AP mode fallback");
    DLOG_I(LOG_APP, "- Web interface for WiFi and MQTT config");
    DLOG_I(LOG_APP, "- Persistent storage (settings saved)");
    DLOG_I(LOG_APP, "- Automatic reconnection");
    DLOG_I(LOG_APP, "- Telemetry publishing (JSON, every 10s)");
    DLOG_I(LOG_APP, "============================================");

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
    mqttCfg.clientId = "mqtt-wifi-webui-" + String((uint32_t)HAL::Platform::getChipId(), HEX);
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

        // Publish online status via EventBus (pointer-based event)
        static String statusTopic = clientId + "/status";
        DomoticsCore::Components::MQTTPublishEvent pubEv{};
        pubEv.topic = statusTopic.c_str();
        pubEv.payload = "online";
        pubEv.qos = 1;
        pubEv.retain = true;
        core.emit("mqtt/publish", pubEv);

        // Subscribe to command topics via EventBus (pointer-based event)
        static String cmdTopic = clientId + "/command/#";
        DomoticsCore::Components::MQTTSubscribeEvent subEv{};
        subEv.topic = cmdTopic.c_str();
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
            doc["uptime"] = HAL::Platform::getMillis() / 1000;
            doc["freeHeap"] = HAL::Platform::getFreeHeap();
            if (wifi) doc["rssi"] = wifi->getRSSI();
            doc["temp"] = 20.0 + (random(0, 100) / 10.0);
            String topic = mqtt->getConfig().clientId + "/telemetry";
            mqtt->publishJSON(topic, doc, 0, false);
        }
    }
}
