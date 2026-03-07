/**
 * @file main.cpp
 * @brief Basic Home Assistant MQTT Discovery Example
 *
 * Demonstrates:
 * - Automatic entity discovery in Home Assistant
 * - Sensor state publishing (temperature, humidity, uptime)
 * - Switch control (relay)
 * - Button trigger (restart)
 * - Alarm control panel with complete state lifecycle:
 *   disarmed → arming → armed_away → pending → triggered → disarmed
 * - Device information and availability
 *
 * Requirements:
 * - WiFi network
 * - MQTT broker
 * - Home Assistant with MQTT integration enabled
 */

#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Wifi_HAL.h> // Using WiFi HAL for simple connection (Full DomoticsCore-Wifi not needed for basic examples)
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/HomeAssistant.h>
#include <DomoticsCore/Timer.h>

using namespace DomoticsCore;

// Custom application log tag
#define LOG_APP "APP"
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;

// ========== Configuration ==========
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// MQTT broker
const char* MQTT_BROKER = "mqtt.example.com";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "";          // Leave empty if no auth
const char* MQTT_PASSWORD = "";

// Hardware pins
const int SENSOR_UPDATE_INTERVAL = 30000;  // Update sensors every 30 seconds

// ========== Global Variables ==========

Core core;
Utils::NonBlockingDelay sensorTimer(SENSOR_UPDATE_INTERVAL);
Utils::NonBlockingDelay aliveTimer(5000);       // System alive message every 5 seconds
HomeAssistantComponent* haPtr = nullptr;
bool lastRelayState = false;  // Track relay state changes
bool initialStatePublished = false;  // Track if initial state sent to HA

// ========== Alarm Panel Demo State Machine ==========
// Demonstrates the complete alarm lifecycle with intermediate states.
// All state transitions and delays are consumer responsibility — DomoticsCore
// is a pure MQTT transport bridge with no internal alarm state tracking.
enum class AlarmDemoState { Disarmed, Arming, ArmedAway, Pending, Triggered };
AlarmDemoState alarmState = AlarmDemoState::Disarmed;
unsigned long alarmDelayStart = 0;
const unsigned long ALARM_EXIT_DELAY = 5000;   // 5s exit delay (arming → armed)
const unsigned long ALARM_ENTRY_DELAY = 5000;  // 5s entry delay (pending → triggered)
const unsigned long ALARM_TRIGGER_DURATION = 10000; // 10s siren then auto-disarm

// Simulated sensor readings (replace with real sensors)
float getTemperature() {
    // Simulate temperature reading (replace with real sensor)
    return 20.0 + (random(0, 100) / 10.0);  // 20-30°C
}

float getHumidity() {
    // Simulate humidity reading (replace with real sensor)
    return 40.0 + (random(0, 200) / 10.0);  // 40-60%
}


void setup() {
    HAL::Platform::initializeLogging(115200);
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "DomoticsCore - Home Assistant Integration - Basic example");
    DLOG_I(LOG_APP, "========================================");
    
    // Initialize GPIO
    HAL::Platform::pinMode(LED_BUILTIN, OUTPUT);
    HAL::Platform::digitalWrite(LED_BUILTIN, HAL::ledBuiltinOff());
    
    // Connect to WiFi using HAL
    DLOG_I(LOG_APP, "Connecting to WiFi: %s", WIFI_SSID);
    HAL::WiFiHAL::init();
    HAL::WiFiHAL::setMode(HAL::WiFiHAL::Mode::Station);
    HAL::WiFiHAL::connect(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (!HAL::WiFiHAL::isConnected() && attempts < 40) {
        HAL::Platform::delayMs(500);
        attempts++;
    }

    if (HAL::WiFiHAL::isConnected()) {
        DLOG_I(LOG_APP, "WiFi connected! IP: %s", HAL::WiFiHAL::getLocalIP().c_str());
    } else {
        DLOG_E(LOG_APP, "WiFi connection failed!");
        while (1) HAL::Platform::delayMs(1000);
    }
    // Configure MQTT
    MQTTConfig mqttCfg;
    mqttCfg.broker = MQTT_BROKER;
    mqttCfg.port = MQTT_PORT;
    mqttCfg.username = MQTT_USER;
    mqttCfg.password = MQTT_PASSWORD;
    mqttCfg.clientId = "domotics-ha-" + String((uint32_t)HAL::Platform::getChipId(), HEX);
    mqttCfg.enableLWT = true;
    mqttCfg.lwtTopic = "homeassistant/esp32-demo/availability";
    mqttCfg.lwtMessage = "offline";
    mqttCfg.lwtQoS = 1;
    mqttCfg.lwtRetain = true;
    
    core.addComponent(std::make_unique<MQTTComponent>(mqttCfg));
    
    // Configure Home Assistant (communicates with MQTT via EventBus)
    HAConfig haCfg;
    HA::setField(haCfg.nodeId, "MyDeviceId", HA::MAX_NODE_ID);
    HA::setField(haCfg.deviceName, "MyDeviceName", HA::MAX_DEVICE_NAME);
    HA::setField(haCfg.manufacturer, "MyManufacturer", HA::MAX_MANUFACTURER);
    HA::setField(haCfg.model, "MyModel", HA::MAX_MODEL);
    HA::setField(haCfg.swVersion, "1.0.0", HA::MAX_SW_VERSION);
    HA::setField(haCfg.discoveryPrefix, "homeassistant", HA::MAX_DISCOVERY_PREFIX);
    {
        String configUrlStr = "http://" + HAL::WiFiHAL::getLocalIP();
        HA::setField(haCfg.configUrl, configUrlStr.c_str(), HA::MAX_CONFIG_URL);
    }
    HA::setField(haCfg.suggestedArea, "Office", HA::MAX_SUGGESTED_AREA);
    
    auto ha = std::make_unique<HomeAssistantComponent>(haCfg);
    haPtr = ha.get();
    
    // ========== Add Entities ==========
    
    // Temperature sensor
    haPtr->addSensor("temperature", "Temperature", "°C", "temperature", "mdi:thermometer");
    
    // Humidity sensor
    haPtr->addSensor("humidity", "Humidity", "%", "humidity", "mdi:water-percent");
    
    // Uptime sensor
    haPtr->addSensor("uptime", "Uptime", "s", "", "mdi:clock-outline");
    
    // WiFi signal sensor
    haPtr->addSensor("wifi_signal", "WiFi Signal", "dBm", "signal_strength", "mdi:wifi");
    
    // Free heap sensor
    haPtr->addSensor("free_heap", "Free Heap", "bytes", "", "mdi:memory");
    
    // Relay switch (controllable from HA)
    haPtr->addSwitch("relay", "Relay", [](bool state) {
        HAL::Platform::digitalWrite(LED_BUILTIN, state ? HAL::ledBuiltinOn() : HAL::ledBuiltinOff());
        DLOG_I(LOG_APP, "Relay set to: %s", state ? "ON" : "OFF");
        // NOTE: State is published separately in loop() to avoid recursion
    }, "mdi:electric-switch");
    
    // Restart button
    haPtr->addButton("restart", "Restart", []() {
        DLOG_I(LOG_APP, "Restart button pressed from Home Assistant");
        HAL::Platform::delayMs(1000);
        HAL::Platform::restart();
    }, "mdi:restart");

    // Alarm control panel (ArmHome + ArmAway + Trigger, no PIN code)
    haPtr->addAlarmControlPanel("alarm", "Demo Alarm",
        [](const String& command, const String& /* code */) {
            DLOG_I(LOG_APP, "Alarm command received: %s", command.c_str());

            if (command == AlarmPanelCommand::ARM_AWAY ||
                command == AlarmPanelCommand::ARM_HOME) {
                if (alarmState == AlarmDemoState::Disarmed) {
                    alarmState = AlarmDemoState::Arming;
                    alarmDelayStart = HAL::Platform::getMillis();
                    // Publish Arming IMMEDIATELY so HA UI updates before exit delay
                    haPtr->publishState("alarm", AlarmPanelState::Arming);
                    DLOG_I(LOG_APP, "Alarm arming — exit delay %lums", ALARM_EXIT_DELAY);
                }
            } else if (command == AlarmPanelCommand::DISARM) {
                alarmState = AlarmDemoState::Disarmed;
                haPtr->publishState("alarm", AlarmPanelState::Disarmed);
                DLOG_I(LOG_APP, "Alarm disarmed");
            } else if (command == AlarmPanelCommand::TRIGGER) {
                if (alarmState == AlarmDemoState::ArmedAway ||
                    alarmState == AlarmDemoState::Pending) {
                    alarmState = AlarmDemoState::Triggered;
                    alarmDelayStart = HAL::Platform::getMillis();
                    haPtr->publishState("alarm", AlarmPanelState::Triggered);
                    DLOG_I(LOG_APP, "Alarm TRIGGERED!");
                }
            }
        },
        "mdi:shield-home",
        AlarmFeature::ArmHome | AlarmFeature::ArmAway | AlarmFeature::Trigger);

    core.addComponent(std::move(ha));
    
    // Initialize core
    if (!core.begin()) {
        DLOG_E(LOG_APP, "Failed to initialize core!");
        while (1) HAL::Platform::delayMs(1000);
    }
    
    DLOG_I(LOG_APP, "========================================");
    DLOG_I(LOG_APP, "System ready!");
    DLOG_I(LOG_APP, "MQTT Broker: %s:%d", MQTT_BROKER, MQTT_PORT);
    DLOG_I(LOG_APP, "Node ID: %s", haCfg.nodeId);
    DLOG_I(LOG_APP, "Registered %d entities", haPtr->getStatistics().entityCount);
    DLOG_I(LOG_APP, "========================================");
}

// ========== Loop ==========

void loop() {
    // Run core loop (handles MQTT, component updates, etc.)
    core.loop();
    
    // Update sensor values periodically
    if (sensorTimer.isReady() && haPtr) {
        // Read and publish sensor values
        float temp = getTemperature();
        float humidity = getHumidity();
        uint32_t uptime = HAL::Platform::getMillis() / 1000;
        uint32_t freeHeap = HAL::Platform::getFreeHeap();

        int32_t rssi = HAL::WiFiHAL::getRSSI();
        
        haPtr->publishState("temperature", temp);
        haPtr->publishState("humidity", humidity);
        haPtr->publishState("uptime", (float)uptime);
        haPtr->publishState("wifi_signal", (float)rssi);
        haPtr->publishState("free_heap", (float)freeHeap);
        
        DLOG_I(LOG_APP, "Published sensors: Temp=%.1f°C, Humidity=%.1f%%, Uptime=%ds",
               temp, humidity, uptime);
    }
    
    // Publish initial state once HA is ready
    if (!initialStatePublished && haPtr && haPtr->isReady()) {
        bool currentRelayState = HAL::Platform::digitalRead(LED_BUILTIN) == HAL::ledBuiltinOn();
        haPtr->publishState("relay", currentRelayState);
        lastRelayState = currentRelayState;
        initialStatePublished = true;
        DLOG_I(LOG_APP, "Published initial relay state: %s", currentRelayState ? "ON" : "OFF");
    }
    
    // Publish relay state only when it changes (not on timer!)
    if (haPtr && haPtr->isMQTTConnected()) {
        bool currentRelayState = HAL::Platform::digitalRead(LED_BUILTIN) == HAL::ledBuiltinOn();
        
        // Publish only on state change
        if (currentRelayState != lastRelayState) {
            haPtr->publishState("relay", currentRelayState);
            DLOG_I(LOG_APP, "Relay state changed: %s", currentRelayState ? "ON" : "OFF");
            lastRelayState = currentRelayState;
        }
    }
    
    // ========== Alarm Panel State Machine ==========
    // Simulates exit delay, entry delay, and siren duration via millis()
    if (alarmState == AlarmDemoState::Arming) {
        if (HAL::Platform::getMillis() - alarmDelayStart >= ALARM_EXIT_DELAY) {
            alarmState = AlarmDemoState::ArmedAway;
            haPtr->publishState("alarm", AlarmPanelState::ArmedAway);
            DLOG_I(LOG_APP, "Alarm armed_away (exit delay complete)");

            // Simulate a sensor trigger after 3s for demo purposes
            alarmDelayStart = HAL::Platform::getMillis();
        }
    } else if (alarmState == AlarmDemoState::ArmedAway) {
        // Simulate sensor trigger 3s after arming completes
        if (HAL::Platform::getMillis() - alarmDelayStart >= 3000) {
            alarmState = AlarmDemoState::Pending;
            alarmDelayStart = HAL::Platform::getMillis();
            // Publish Pending IMMEDIATELY so HA UI updates before entry delay
            haPtr->publishState("alarm", AlarmPanelState::Pending);
            DLOG_I(LOG_APP, "Sensor triggered — pending, entry delay %lums", ALARM_ENTRY_DELAY);
        }
    } else if (alarmState == AlarmDemoState::Pending) {
        if (HAL::Platform::getMillis() - alarmDelayStart >= ALARM_ENTRY_DELAY) {
            alarmState = AlarmDemoState::Triggered;
            alarmDelayStart = HAL::Platform::getMillis();
            haPtr->publishState("alarm", AlarmPanelState::Triggered);
            DLOG_I(LOG_APP, "Alarm TRIGGERED (entry delay expired)");
        }
    } else if (alarmState == AlarmDemoState::Triggered) {
        // Auto-disarm after siren duration for demo
        if (HAL::Platform::getMillis() - alarmDelayStart >= ALARM_TRIGGER_DURATION) {
            alarmState = AlarmDemoState::Disarmed;
            haPtr->publishState("alarm", AlarmPanelState::Disarmed);
            DLOG_I(LOG_APP, "Alarm auto-disarmed (siren timeout)");
        }
    }

    // Heartbeat log every 5 seconds
    if (aliveTimer.isReady()) {
        DLOG_I(LOG_APP, "System alive, uptime: %lus, MQTT: %s",
               HAL::Platform::getMillis()/1000, haPtr && haPtr->isMQTTConnected() ? "connected" : "disconnected");
    }
}
