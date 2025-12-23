/**
 * @file main.cpp
 * @brief MQTT with WebUI Example
 * 
 * Demonstrates:
 * - MQTT client with web-based configuration
 * - Real-time connection status in WebUI
 * - Statistics monitoring
 * - Interactive MQTT testing through browser
 * - Using DomoticsCore logging system
 * - Non-blocking timers
 * 
 * Hardware: ESP32 Dev Board
 * 
 * Access:
 * - WebUI: http://<device-ip>
 * - Configure MQTT broker via Settings tab
 * - Monitor statistics in Components tab
 * 
 * Note: This example uses ESP32 native WiFi (not DomoticsCore-WiFi component)
 * for simplicity. In production, use DomoticsCore-WiFi for advanced features
 * like automatic reconnection, network scanning, and credential management.
 */

#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Wifi_HAL.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/MQTTWebUI.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::WebUI;

// ========== Configuration ==========

// WiFi credentials
const char* WIFI_SSID = "YourWifiSSID";
const char* WIFI_PASSWORD = "YourWifiPassword";

// Default MQTT configuration (can be changed via WebUI)
const char* MQTT_BROKER = "mqtt.example.com";
const uint16_t MQTT_PORT = 1883;

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay publishTimer(10000);  // Publish every 10 seconds

// ========== Setup ==========

void setup() {
    // Initialize logging system before any DLOG calls
    HAL::Platform::initializeLogging(115200);

    // ============================================================================
    // EXAMPLE: MQTT with WebUI Configuration
    // ============================================================================
    // This example demonstrates MQTT with web-based configuration interface:
    // - Web interface for MQTT broker configuration
    // - Real-time connection status monitoring in browser
    // - Statistics dashboard (messages sent/received, uptime)
    // - Interactive testing through WebUI
    // - Periodic telemetry publishing (JSON format)
    // - EventBus-based event handling
    // Expected Console Output:
    //   [APP] === MQTT with WebUI ===
    //   [APP] MQTT with web-based configuration
    //   [APP] - Web interface for broker config
    //   [APP] - Real-time status monitoring
    //   [APP] - Statistics dashboard
    //   [APP] - Telemetry publishing (JSON, every 10s)
    //   [APP] ==============================
    //   [APP] Connecting to WiFi: YourWifiSSID
    //   [APP] ✓ WiFi connected! IP: 192.168.1.XXX
    //   [WebUI] Initializing on port 80
    //   [MQTT] Initializing
    //   [MQTT] Connected to mqtt.example.com:1883
    //   [APP] 📡 MQTT Connected!
    //   [APP]   ✓ Published online status
    //   [APP]   ✓ Subscribed to commands
    //   [APP] ✓ MQTT WebUI provider registered
    //   [APP] WebUI: http://192.168.1.XXX
    //   (Every 10 seconds: telemetry JSON published)
    //   [APP] 📤 Published telemetry
    //
    // To test command handling, publish to commands topic:
    //   mosquitto_pub -h mqtt.example.com -t "esp32-webui-XXXX/command/test" -m "hello"
    //
    // Expected output when receiving commands:
    //   [APP] 📨 Command received: esp32-webui-XXXX/command/test = hello
    //
    // Expected WebUI Access:
    //   - Browse to http://<device-ip>
    //   - Configure MQTT broker in Settings tab
    //   - Monitor statistics in Components tab
    // ============================================================================

    DLOG_I(LOG_APP, "=== MQTT with WebUI ===");
    DLOG_I(LOG_APP, "MQTT with web-based configuration");
    DLOG_I(LOG_APP, "- Web interface for broker config");
    DLOG_I(LOG_APP, "- Real-time status monitoring");
    DLOG_I(LOG_APP, "- Statistics dashboard");
    DLOG_I(LOG_APP, "- Telemetry publishing (JSON, every 10s)");
    DLOG_I(LOG_APP, "==============================");

    // Connect to WiFi using HAL (ESP32/ESP8266 compatible)
    // In production, use DomoticsCore-WiFi component for advanced features
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    HAL::WiFiHAL::init();
    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);
    HAL::WiFiHAL::connect(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (!HAL::WiFiHAL::isConnected() && attempts < 20) {
        HAL::Platform::delayMs(500);
        attempts++;
    }

    if (!HAL::WiFiHAL::isConnected()) {
        DLOG_W(LOG_APP, "✗ WiFi connection failed!");
        DLOG_I(LOG_APP, "Starting AP mode for configuration...");
        HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::AccessPoint);
        HAL::WiFiHAL::startAP("MQTT-WebUI-Setup", "");
        DLOG_I(LOG_APP, "AP IP: %s", HAL::WiFiHAL::getAPIP().c_str());
    } else {
        DLOG_I(LOG_APP, "✓ WiFi connected! IP: %s", HAL::WiFiHAL::getLocalIP().c_str());
    }
    
    // Configure WebUI
    WebUIConfig webConfig;
    webConfig.deviceName = "ESP32 MQTT Device";
    webConfig.wsUpdateInterval = 2000;
    auto webui = std::make_unique<WebUIComponent>(webConfig);
    auto* webuiPtr = webui.get();
    core.addComponent(std::move(webui));
    
    // Configure MQTT
    MQTTConfig mqttConfig;
    mqttConfig.broker = MQTT_BROKER;
    mqttConfig.port = MQTT_PORT;
    mqttConfig.clientId = "mqtt-webui-" + String((uint32_t)HAL::Platform::getChipId(), HEX);
    mqttConfig.enabled = true;
    mqttConfig.autoReconnect = true;
    mqttConfig.enableLWT = true;
    mqttConfig.lwtTopic = mqttConfig.clientId + "/status";
    mqttConfig.lwtMessage = "offline";
    mqttConfig.lwtQoS = 1;
    mqttConfig.lwtRetain = true;
    
    auto mqtt = std::make_unique<MQTTComponent>(mqttConfig);
    auto* mqttPtr = mqtt.get();  // Keep for WebUI provider
    core.addComponent(std::move(mqtt));
    
    // Register EventBus listeners (capture clientId before config is moved)
    String clientId = mqttConfig.clientId;

    // Note: core is global, no need to capture it by reference
    core.on<bool>("mqtt/connected", [clientId](const bool&) {
        DLOG_I(LOG_APP, "📡 MQTT Connected!");

        // Publish online status via EventBus (pointer-based event)
        static String statusTopic = clientId + "/status";
        DomoticsCore::Components::MQTTPublishEvent pubEv{};
        pubEv.topic = statusTopic.c_str();
        pubEv.payload = "online";
        pubEv.qos = 1;
        pubEv.retain = true;
        core.emit("mqtt/publish", pubEv);

        // Subscribe to command topics via EventBus (pointer-based event)
        static String commandTopic = clientId + "/command/#";
        DomoticsCore::Components::MQTTSubscribeEvent subEv{};
        subEv.topic = commandTopic.c_str();
        subEv.qos = 1;
        core.emit("mqtt/subscribe", subEv);

        DLOG_I(LOG_APP, "  ✓ Published online status");
        DLOG_I(LOG_APP, "  ✓ Subscribed to commands");
    });
    
    core.on<bool>("mqtt/disconnected", [](const bool&) {
        DLOG_W(LOG_APP, "📡 MQTT Disconnected");
    });
    
    core.on<DomoticsCore::Components::MQTTMessageEvent>("mqtt/message", [](const DomoticsCore::Components::MQTTMessageEvent& ev) {
        DLOG_I(LOG_APP, "📨 Command received: %s = %s", ev.topic, ev.payload);
    });
    
    // Initialize components
    DLOG_I(LOG_APP, "Initializing components...");
    core.begin();
    
    // Register WebUI provider for MQTT
    if (webuiPtr && mqttPtr) {
        webuiPtr->registerProviderWithComponent(new MQTTWebUI(mqttPtr), mqttPtr);
        DLOG_I(LOG_APP, "✓ MQTT WebUI provider registered");
    }
    
    DLOG_I(LOG_APP, "✓ Setup complete!");

    if (HAL::WiFiHAL::isConnected()) {
        DLOG_I(LOG_APP, "========================================");
        DLOG_I(LOG_APP, "WebUI: http://%s", HAL::WiFiHAL::getLocalIP().c_str());
        DLOG_I(LOG_APP, "========================================");
    } else {
        DLOG_I(LOG_APP, "========================================");
        DLOG_I(LOG_APP, "WebUI: http://%s", HAL::WiFiHAL::getAPIP().c_str());
        DLOG_I(LOG_APP, "========================================");
    }
}

// ========== Loop ==========

void loop() {
    core.loop();
    
    // Publish telemetry periodically using NonBlockingDelay
    if (publishTimer.isReady()) {
        auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
        if (mqtt && mqtt->isConnected()) {
            // Publish JSON telemetry
            JsonDocument doc;
            doc["uptime"] = HAL::Platform::getMillis() / 1000;
            doc["freeHeap"] = HAL::Platform::getFreeHeap();
            doc["rssi"] = HAL::WiFiHAL::getRSSI();
            doc["temperature"] = 20.0 + (random(0, 100) / 10.0);
            
            String telemetryTopic = mqtt->getConfig().clientId + "/telemetry";
            if (mqtt->publishJSON(telemetryTopic, doc, 0, false)) {
                DLOG_I(LOG_APP, "📤 Published telemetry");
            }
        }
    }
    
    // No delay() - fully non-blocking
}
