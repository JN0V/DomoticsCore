#ifndef DOMOTICS_CORE_SYSTEM_CONFIG_H
#define DOMOTICS_CORE_SYSTEM_CONFIG_H

/**
 * @file SystemConfig.h
 * @brief Configuration structures and presets for DomoticsCore System component.
 * 
 * @example DomoticsCore-System/examples/Minimal/src/main.cpp
 * @example DomoticsCore-System/examples/Standard/src/main.cpp
 * @example DomoticsCore-System/examples/FullStack/src/main.cpp
 */

#include <DomoticsCore/Logger.h>

namespace DomoticsCore {

/**
 * @brief System states for lifecycle tracking
 */
enum class SystemState {
    BOOTING,           // Initial boot
    WIFI_CONNECTING,   // Connecting to WiFi
    WIFI_CONNECTED,    // WiFi established
    SERVICES_STARTING, // Starting services
    READY,             // All services operational
    ERROR,             // Critical error
    OTA_UPDATE,        // Firmware update in progress
    SHUTDOWN           // Graceful shutdown
};

/**
 * @brief Convert SystemState to human-readable string
 */
inline const char* systemStateToString(SystemState state) {
    switch (state) {
        case SystemState::BOOTING: return "BOOTING";
        case SystemState::WIFI_CONNECTING: return "WIFI_CONNECTING";
        case SystemState::WIFI_CONNECTED: return "WIFI_CONNECTED";
        case SystemState::SERVICES_STARTING: return "SERVICES_STARTING";
        case SystemState::READY: return "READY";
        case SystemState::ERROR: return "ERROR";
        case SystemState::OTA_UPDATE: return "OTA_UPDATE";
        case SystemState::SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

/**
 * @brief System configuration - simple, high-level settings
 * 
 * This struct provides a unified way to configure all DomoticsCore components.
 * Use the static factory methods for common configurations.
 */
struct SystemConfig {
    // Device identity
    String deviceName = "DomoticsCore";
    String manufacturer = "DomoticsCore";
    String model = "";                  // Auto-detected from ESP.getChipModel() if empty
    String firmwareVersion = "1.0.0";
    
    // WiFi behavior
    bool wifiAutoConfig = true;     // Auto AP mode if no credentials
    String wifiSSID = "";           // Leave empty for auto-config
    String wifiPassword = "";
    String wifiAPSSID = "";         // Auto-generated if empty (DeviceName-XXXX)
    String wifiAPPassword = "";     // Empty = open AP

    // LED (optional)
    bool enableLED = true;
    uint8_t ledPin = 2;
    bool ledActiveHigh = true;
    
    // RemoteConsole (optional)
    bool enableConsole = true;
    uint16_t consolePort = 23;
    uint8_t consoleMaxClients = 3;
    
    // WebUI (optional)
    bool enableWebUI = false;
    uint16_t webUIPort = 80;
    
    // MQTT (optional)
    bool enableMQTT = false;
    String mqttBroker = "";
    uint16_t mqttPort = 1883;
    String mqttUser = "";
    String mqttPassword = "";
    String mqttClientId = "";  // Auto-generated if empty
    
    // Home Assistant (optional, requires MQTT)
    bool enableHomeAssistant = false;
    
    // NTP (optional)
    bool enableNTP = false;
    String ntpServer = "pool.ntp.org";
    String ntpTimezone = "UTC";
    
    // OTA (optional)
    bool enableOTA = false;

    // SystemInfo (optional)
    bool enableSystemInfo = false;
    
    // Storage (optional)
    bool enableStorage = false;
    String storageNamespace = "domotics";

    // Loop watchdog (OBS-7). In effect on ESP32 only: the Arduino core does not
    // put loopTask on the task watchdog, so a stuck loop() hangs forever instead
    // of rebooting — measured 2026-09-05, >40 s of silence against a 5 s TWDT.
    // System::loop() feeds it, so any sketch that calls it regularly is safe;
    // expiry is a panic and leaves a core dump with the hang's backtrace. It
    // reconfigures the task watchdog timeout globally. 0 disables. On ESP8266
    // the SDK's soft WDT already resets a stuck loop in about 3 s.
    //
    // Why 30 s — measured 2026-09-05 on a WROOM-32D running the FullStack
    // configuration: the longest System::loop() iteration was 0.37 s idle and
    // 0.80 s during an HTTP firmware upload. The image verification and commit
    // (Update.end) run on the async web server task, in OTAWebUI's final
    // chunk, never in loop(). A synchronous WiFi scan called from loop() cost
    // 8.1 s. Thirty seconds is 37x the longest loop-side figure and 3.7x the
    // blocking call a sketch is most likely to make.
    uint32_t loopWatchdogSeconds = 30;
    
    // Logging
    LogLevel defaultLogLevel = LOG_LEVEL_INFO;
    
    // ========================================================================
    // Preset Configurations
    // ========================================================================
    
    /**
     * @brief Minimal configuration (WiFi, LED, Console only)
     * Perfect for: Simple sensors, basic automation, learning
     */
    static SystemConfig minimal() {
        SystemConfig config;
        config.enableLED = true;
        config.enableConsole = true;
        config.wifiAutoConfig = true;
        return config;
    }
    
    /**
     * @brief Standard configuration (+ WebUI, NTP, Storage)
     * Perfect for: Most applications, no external services needed
     */
    static SystemConfig standard() {
        SystemConfig config = minimal();
        config.enableWebUI = true;
        config.enableNTP = true;
        config.enableStorage = true;
        return config;
    }
    
    /**
     * @brief Full stack configuration (everything enabled)
     * Perfect for: Production deployments, complete IoT solutions
     * Note: Requires MQTT broker and OTA password configuration
     */
    static SystemConfig fullStack() {
        SystemConfig config = standard();
        config.enableMQTT = true;
        config.enableHomeAssistant = true;
        config.enableOTA = true;
        config.enableSystemInfo = true;
        return config;
    }
};

} // namespace DomoticsCore

#endif // DOMOTICS_CORE_SYSTEM_CONFIG_H
