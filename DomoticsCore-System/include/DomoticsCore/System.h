#ifndef DOMOTICS_CORE_SYSTEM_H
#define DOMOTICS_CORE_SYSTEM_H

#include <DomoticsCore/Core.h>
#include <DomoticsCore/LED.h>
#include <DomoticsCore/RemoteConsole.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/Logger.h>

// Optional components - include if available (will fail gracefully if not installed)
#if __has_include(<DomoticsCore/WebUI.h>)
#include <DomoticsCore/WebUI.h>
#endif

#if __has_include(<DomoticsCore/NTP.h>)
#include <DomoticsCore/NTP.h>
#endif

// WebUI providers - only include if WebUI is available
#if __has_include(<DomoticsCore/WebUI.h>)
  #if __has_include(<DomoticsCore/NTPWebUI.h>)
  #include <DomoticsCore/NTPWebUI.h>
  #endif
  #if __has_include(<DomoticsCore/WifiWebUI.h>)
  #include <DomoticsCore/WifiWebUI.h>
  #endif
  #if __has_include(<DomoticsCore/MQTTWebUI.h>)
  #include <DomoticsCore/MQTTWebUI.h>
  #endif
  #if __has_include(<DomoticsCore/OTAWebUI.h>)
  #include <DomoticsCore/OTAWebUI.h>
  #endif
  #if __has_include(<DomoticsCore/SystemInfoWebUI.h>)
  #include <DomoticsCore/SystemInfoWebUI.h>
  #endif
  #if __has_include(<DomoticsCore/RemoteConsoleWebUI.h>)
  #include <DomoticsCore/RemoteConsoleWebUI.h>
  #endif
#endif

#if __has_include(<DomoticsCore/Storage.h>)
#include <DomoticsCore/Storage.h>
#endif

#if __has_include(<DomoticsCore/MQTT.h>)
#include <DomoticsCore/MQTT.h>
#endif

#if __has_include(<DomoticsCore/OTA.h>)
#include <DomoticsCore/OTA.h>
#endif

#if __has_include(<DomoticsCore/SystemInfo.h>)
#include <DomoticsCore/SystemInfo.h>
#endif

#if __has_include(<DomoticsCore/HomeAssistant.h>)
#include <DomoticsCore/HomeAssistant.h>
#endif

#include <vector>
#include <functional>

#define LOG_SYSTEM "SYSTEM"

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
 * @brief System configuration - simple, high-level settings
 */
struct SystemConfig {
    // Device identity
    String deviceName = "DomoticsCore";
    String firmwareVersion = "1.0.0";
    
    // WiFi behavior
    bool wifiAutoConfig = true;     // Auto AP mode if no credentials
    String wifiSSID = "";           // Leave empty for auto-config
    String wifiPassword = "";
    String wifiAPSSID = "";         // Auto-generated if empty (DeviceName-XXXX)
    String wifiAPPassword = "";     // Empty = open AP
    uint32_t wifiTimeout = 30000;   // 30 seconds
    
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
    uint16_t webUIPort = 80;  // Standard HTTP port
    bool webUIEnableAPI = true;
    
    // MQTT (optional)
    bool enableMQTT = false;
    String mqttBroker = "";
    uint16_t mqttPort = 1883;
    String mqttUser = "";
    String mqttPassword = "";
    String mqttClientId = "";  // Auto-generated if empty
    
    // Home Assistant (optional, requires MQTT)
    bool enableHomeAssistant = false;
    String haDiscoveryPrefix = "homeassistant";
    
    // NTP (optional)
    bool enableNTP = false;
    String ntpServer = "pool.ntp.org";
    String ntpTimezone = "UTC";
    
    // OTA (optional)
    bool enableOTA = false;
    String otaPassword = "";  // Empty = no password
    
    // SystemInfo (optional)
    bool enableSystemInfo = false;
    
    // Storage (optional)
    bool enableStorage = false;
    String storageNamespace = "domotics";
    
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
        // All other components disabled
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

/**
 * @brief Complete ready-to-use system
 * 
 * This is the "batteries included" component that provides:
 * - Automatic WiFi connection
 * - LED status visualization (automatic)
 * - Remote console (telnet)
 * - State management (automatic)
 * - Component orchestration (automatic)
 * 
 * The developer only needs to:
 * 1. Configure WiFi credentials
 * 2. Add their custom sensors/actuators
 * 3. Call begin() and loop()
 * 
 * Everything else is handled automatically!
 */
class System {
private:
    SystemConfig config;
    Core core;
    
    // Component references (owned by core)
    Components::LEDComponent* led = nullptr;
    Components::RemoteConsoleComponent* console = nullptr;
    Components::WifiComponent* wifi = nullptr;
    
    // WebUI provider pointers (owned by System, need cleanup)
    #if __has_include(<DomoticsCore/WebUI.h>)
      #if __has_include(<DomoticsCore/WifiWebUI.h>)
      Components::WebUI::WifiWebUI* wifiWebUIProvider = nullptr;
      #endif
      #if __has_include(<DomoticsCore/NTPWebUI.h>)
      Components::WebUI::NTPWebUI* ntpWebUIProvider = nullptr;
      #endif
      #if __has_include(<DomoticsCore/MQTTWebUI.h>)
      Components::WebUI::MQTTWebUI* mqttWebUIProvider = nullptr;
      #endif
      #if __has_include(<DomoticsCore/OTAWebUI.h>)
      Components::WebUI::OTAWebUI* otaWebUIProvider = nullptr;
      #endif
      #if __has_include(<DomoticsCore/SystemInfoWebUI.h>)
      Components::WebUI::SystemInfoWebUI* systemInfoWebUIProvider = nullptr;
      #endif
      #if __has_include(<DomoticsCore/RemoteConsoleWebUI.h>)
      Components::WebUI::RemoteConsoleWebUI* consoleWebUIProvider = nullptr;
      #endif
    #endif
    
    // System state
    SystemState state = SystemState::BOOTING;
    std::vector<std::function<void(SystemState, SystemState)>> stateCallbacks;
    
    bool initialized = false;
    
public:
    System(const SystemConfig& cfg = SystemConfig()) : config(cfg) {}
    
    ~System() {
        // Core manages all components including LED
        
        // Clean up WebUI providers
        #if __has_include(<DomoticsCore/WebUI.h>)
          #if __has_include(<DomoticsCore/WifiWebUI.h>)
          delete wifiWebUIProvider;
          #endif
          #if __has_include(<DomoticsCore/NTPWebUI.h>)
          delete ntpWebUIProvider;
          #endif
          #if __has_include(<DomoticsCore/MQTTWebUI.h>)
          delete mqttWebUIProvider;
          #endif
          #if __has_include(<DomoticsCore/OTAWebUI.h>)
          delete otaWebUIProvider;
          #endif
          #if __has_include(<DomoticsCore/SystemInfoWebUI.h>)
          delete systemInfoWebUIProvider;
          #endif
          #if __has_include(<DomoticsCore/RemoteConsoleWebUI.h>)
          delete consoleWebUIProvider;
          #endif
        #endif
    }
    
    /**
     * @brief Initialize the system
     * 
     * This automatically:
     * - Sets up LED with automatic state visualization
     * - Connects to WiFi
     * - Starts remote console
     * - Configures coordinator
     * 
     * @return true if initialization successful
     */
    bool begin() {
        if (initialized) {
            DLOG_W(LOG_SYSTEM, "System already initialized");
            return true;
        }
        
        DLOG_I(LOG_SYSTEM, "========================================");
        DLOG_I(LOG_SYSTEM, "DomoticsCore System");
        DLOG_I(LOG_SYSTEM, "Device: %s v%s", config.deviceName.c_str(), config.firmwareVersion.c_str());
        DLOG_I(LOG_SYSTEM, "========================================");
        
        // Initialize state
        state = SystemState::BOOTING;
        
        // ====================================================================
        // 1. LED Component (if enabled) - Early init for error visualization
        // ====================================================================
        if (config.enableLED) {
            auto ledPtr = std::make_unique<Components::LEDComponent>();
            led = ledPtr.get();
            led->addSingleLED(config.ledPin, "status", 255, !config.ledActiveHigh);
            core.addComponent(std::move(ledPtr));
            
            // Initialize early so it can show error states during boot
            if (led->begin() == Components::ComponentStatus::Success) {
                led->setActive(true);  // Mark as initialized to skip double-init
                DLOG_I(LOG_SYSTEM, "✓ LED component initialized (early)");
            } else {
                DLOG_E(LOG_SYSTEM, "✗ LED initialization failed");
            }
        }
        
        // ====================================================================
        // 2. Storage Component (if enabled) - Early so others can load config
        // ====================================================================
        #if __has_include(<DomoticsCore/Storage.h>)
        Components::StorageComponent* storage = nullptr;
        if (config.enableStorage) {
            auto storagePtr = std::make_unique<Components::StorageComponent>();
            storage = storagePtr.get();
            core.addComponent(std::move(storagePtr));
            
            // Initialize storage early so we can load credentials
            if (storage->begin() == Components::ComponentStatus::Success) {
                storage->setActive(true);  // Mark as initialized to skip double-init
                DLOG_I(LOG_SYSTEM, "✓ Storage component initialized (early)");
                
                // Load WiFi credentials from storage if not provided in config
                if (config.wifiSSID.isEmpty()) {
                    config.wifiSSID = storage->getString("wifi_ssid", "");
                    config.wifiPassword = storage->getString("wifi_password", "");
                    if (!config.wifiSSID.isEmpty()) {
                        DLOG_I(LOG_SYSTEM, "Loaded WiFi credentials from storage: %s", config.wifiSSID.c_str());
                    }
                }
            } else {
                DLOG_E(LOG_SYSTEM, "✗ Storage initialization failed");
            }
        }
        #else
        if (config.enableStorage) {
            DLOG_W(LOG_SYSTEM, "⚠️  Storage requested but library not installed");
            DLOG_I(LOG_SYSTEM, "📦 To enable: Add 'symlink://../../../DomoticsCore-Storage' to lib_deps");
        }
        #endif
        
        // ====================================================================
        // 3. WiFi Component (AUTOMATIC with AP fallback)
        // ====================================================================
        auto wifiPtr = std::make_unique<Components::WifiComponent>(config.wifiSSID, config.wifiPassword);
        wifi = wifiPtr.get();
        
        // Check if AP should be enabled (from storage or config)
        bool enableAP = false;
        String apSSID = config.wifiAPSSID;
        
        #if __has_include(<DomoticsCore/Storage.h>)
        auto* storageComp = core.getComponent<Components::StorageComponent>("Storage");
        if (storageComp) {
            // Load AP state from storage
            enableAP = storageComp->getBool("wifi_ap_enabled", false);
            String savedAPSSID = storageComp->getString("wifi_ap_ssid", "");
            if (savedAPSSID.length() > 0) {
                apSSID = savedAPSSID;
            }
            if (enableAP) {
                DLOG_I(LOG_SYSTEM, "Loaded AP state from storage: enabled=%d, SSID=%s", enableAP, apSSID.c_str());
            }
        }
        #endif
        
        // Enable AP mode if stored state says so, or if no credentials and auto-config enabled
        if (enableAP || (config.wifiSSID.isEmpty() && config.wifiAutoConfig)) {
            // Generate AP SSID if not provided
            if (apSSID.isEmpty()) {
                uint64_t chipid = ESP.getEfuseMac();
                apSSID = config.deviceName + "-" + String((uint32_t)(chipid >> 32), HEX);
            }
            wifi->enableAP(apSSID, config.wifiAPPassword);
            DLOG_I(LOG_SYSTEM, "✓ WiFi AP mode enabled: %s", apSSID.c_str());
        }
        
        core.addComponent(std::move(wifiPtr));
        DLOG_I(LOG_SYSTEM, "✓ WiFi component configured");
        
        // ====================================================================
        // 4. RemoteConsole (if enabled) - AFTER WiFi
        // ====================================================================
        if (config.enableConsole) {
            Components::RemoteConsoleConfig consoleConfig;
            consoleConfig.port = config.consolePort;
            consoleConfig.maxClients = config.consoleMaxClients;
            consoleConfig.defaultLogLevel = config.defaultLogLevel;
            
            auto consolePtr = std::make_unique<Components::RemoteConsoleComponent>(consoleConfig);
            console = consolePtr.get();
            
            // Register default system commands
            console->registerCommand("status", [this](const String& args) {
                return getSystemStatus();
            });
            
            console->registerCommand("wifi", [this](const String& args) {
                return getWiFiStatus();
            });
            
            core.addComponent(std::move(consolePtr));
            DLOG_I(LOG_SYSTEM, "✓ RemoteConsole enabled (port %d)", config.consolePort);
        }
        
        // ====================================================================
        // 5. Optional Components - Auto-add if library available
        // ====================================================================
        // IMPORTANT: Add components BEFORE core.begin() so they get initialized!
        
        #if __has_include(<DomoticsCore/WebUI.h>)
        if (config.enableWebUI) {
            Components::WebUIConfig webuiConfig;
            webuiConfig.port = config.webUIPort;
            webuiConfig.deviceName = config.deviceName;
            
            // Load WebUI configuration from storage if available
            #if __has_include(<DomoticsCore/Storage.h>)
            auto* storageComp = core.getComponent<Components::StorageComponent>("Storage");
            if (storageComp) {
                webuiConfig.theme = storageComp->getString("webui_theme", "dark");
                String savedDeviceName = storageComp->getString("device_name", "");
                if (savedDeviceName.length() > 0) {
                    webuiConfig.deviceName = savedDeviceName;
                }
                DLOG_I(LOG_SYSTEM, "Loaded WebUI config from storage: theme=%s, deviceName=%s", 
                       webuiConfig.theme.c_str(), webuiConfig.deviceName.c_str());
            }
            #endif
            
            auto webuiPtr = std::make_unique<Components::WebUIComponent>(webuiConfig);
            core.addComponent(std::move(webuiPtr));
            DLOG_I(LOG_SYSTEM, "✓ WebUI component added (port %d)", config.webUIPort);
        }
        #else
        if (config.enableWebUI) {
            DLOG_W(LOG_SYSTEM, "⚠️  WebUI requested but library not installed");
            DLOG_I(LOG_SYSTEM, "📦 To enable: Add 'symlink://../../../DomoticsCore-WebUI' to lib_deps in platformio.ini");
        }
        #endif
        
        #if __has_include(<DomoticsCore/NTP.h>)
        if (config.enableNTP) {
            Components::NTPConfig ntpConfig;
            
            // Load NTP configuration from storage if available
            #if __has_include(<DomoticsCore/Storage.h>)
            auto* storageComp = core.getComponent<Components::StorageComponent>("Storage");
            if (storageComp) {
                ntpConfig.enabled = storageComp->getBool("ntp_enabled", true);
                ntpConfig.timezone = storageComp->getString("ntp_timezone", "UTC0");
                ntpConfig.syncInterval = (uint32_t)storageComp->getInt("ntp_interval", 3600);
                
                // Load servers from comma-separated string
                String serversStr = storageComp->getString("ntp_servers", "");
                if (serversStr.length() > 0) {
                    ntpConfig.servers.clear();
                    int start = 0;
                    int commaPos;
                    while ((commaPos = serversStr.indexOf(',', start)) != -1) {
                        String server = serversStr.substring(start, commaPos);
                        server.trim();
                        if (server.length() > 0) {
                            ntpConfig.servers.push_back(server);
                        }
                        start = commaPos + 1;
                    }
                    // Last server
                    String server = serversStr.substring(start);
                    server.trim();
                    if (server.length() > 0) {
                        ntpConfig.servers.push_back(server);
                    }
                }
                DLOG_I(LOG_SYSTEM, "Loaded NTP config from storage: timezone=%s", ntpConfig.timezone.c_str());
            }
            #endif
            
            auto ntpPtr = std::make_unique<Components::NTPComponent>(ntpConfig);
            core.addComponent(std::move(ntpPtr));
            DLOG_I(LOG_SYSTEM, "✓ NTP component added");
        }
        #else
        if (config.enableNTP) {
            DLOG_W(LOG_SYSTEM, "⚠️  NTP requested but library not installed");
            DLOG_I(LOG_SYSTEM, "📦 To enable: Add 'symlink://../../../DomoticsCore-NTP' to lib_deps");
        }
        #endif
        
        #if __has_include(<DomoticsCore/MQTT.h>)
        if (config.enableMQTT) {
            Components::MQTTConfig mqttConfig;
            mqttConfig.broker = config.mqttBroker;
            mqttConfig.port = config.mqttPort;
            mqttConfig.username = config.mqttUser;
            mqttConfig.password = config.mqttPassword;
            mqttConfig.clientId = config.mqttClientId;
            mqttConfig.enabled = true;
            
            auto mqttPtr = std::make_unique<Components::MQTTComponent>(mqttConfig);
            core.addComponent(std::move(mqttPtr));
            DLOG_I(LOG_SYSTEM, "✓ MQTT component added");
        }
        #else
        if (config.enableMQTT) {
            DLOG_W(LOG_SYSTEM, "⚠️  MQTT requested but library not installed");
            DLOG_I(LOG_SYSTEM, "📦 To enable: Add 'symlink://../../../DomoticsCore-MQTT' to lib_deps");
        }
        #endif
        
        #if __has_include(<DomoticsCore/OTA.h>)
        if (config.enableOTA) {
            auto otaPtr = std::make_unique<Components::OTAComponent>();
            core.addComponent(std::move(otaPtr));
            DLOG_I(LOG_SYSTEM, "✓ OTA component added");
        }
        #else
        if (config.enableOTA) {
            DLOG_W(LOG_SYSTEM, "⚠️  OTA requested but library not installed");
            DLOG_I(LOG_SYSTEM, "📦 To enable: Add 'symlink://../../../DomoticsCore-OTA' to lib_deps");
        }
        #endif
        
        #if __has_include(<DomoticsCore/SystemInfo.h>)
        if (config.enableSystemInfo) {
            auto sysInfoPtr = std::make_unique<Components::SystemInfoComponent>();
            core.addComponent(std::move(sysInfoPtr));
            DLOG_I(LOG_SYSTEM, "✓ SystemInfo component added");
        }
        #else
        if (config.enableSystemInfo) {
            DLOG_W(LOG_SYSTEM, "⚠️  SystemInfo requested but library not installed");
            DLOG_I(LOG_SYSTEM, "📦 To enable: Add 'symlink://../../../DomoticsCore-SystemInfo' to lib_deps");
        }
        #endif
        
        // Note: HomeAssistant is added AFTER core.begin() because it needs MQTT reference
        
        // ====================================================================
        // 6. Initialize Core (with all registered components)
        // ====================================================================
        if (!core.begin()) {
            DLOG_E(LOG_SYSTEM, "Core initialization failed!");
            setState(SystemState::ERROR);
            return false;
        }
        
        // ====================================================================
        // 6.5. Add HomeAssistant (requires MQTT component reference)
        // ====================================================================
        #if __has_include(<DomoticsCore/HomeAssistant.h>)
        if (config.enableHomeAssistant) {
            auto* mqttComponent = core.getComponent<Components::MQTTComponent>("MQTT");
            if (mqttComponent) {
                auto haPtr = std::make_unique<Components::HomeAssistant::HomeAssistantComponent>(mqttComponent);
                if (core.addComponent(std::move(haPtr))) {
                    DLOG_I(LOG_SYSTEM, "✓ Home Assistant component added and initialized");
                } else {
                    DLOG_E(LOG_SYSTEM, "✗ Home Assistant component failed to initialize");
                }
            } else {
                DLOG_W(LOG_SYSTEM, "⚠️  Home Assistant requires MQTT - MQTT not available");
            }
        }
        #else
        if (config.enableHomeAssistant) {
            DLOG_W(LOG_SYSTEM, "⚠️  Home Assistant requested but library not installed");
            DLOG_I(LOG_SYSTEM, "📦 To enable: Add 'symlink://../../../DomoticsCore-HomeAssistant' to lib_deps");
        }
        #endif
        
        // ====================================================================
        // 6.6. Register WebUI Providers (after components are initialized)
        // ====================================================================
        #if __has_include(<DomoticsCore/WebUI.h>)
        DLOG_I(LOG_SYSTEM, "Registering WebUI providers...");
        auto* webuiComponent = core.getComponent<Components::WebUIComponent>("WebUI");
        if (webuiComponent) {
            DLOG_I(LOG_SYSTEM, "WebUI component found, registering providers...");
            // Register WiFi WebUI provider
            #if __has_include(<DomoticsCore/WifiWebUI.h>)
            if (wifi) {
                wifiWebUIProvider = new Components::WebUI::WifiWebUI(wifi);
                
                // Set WebUI component reference for network change notifications
                wifiWebUIProvider->setWebUIComponent(webuiComponent);
                
                // Set up credential persistence callback if Storage available
                #if __has_include(<DomoticsCore/Storage.h>)
                auto* storageComp = core.getComponent<Components::StorageComponent>("Storage");
                if (storageComp) {
                    wifiWebUIProvider->setCredentialsSaveCallback(
                        [storageComp](const String& ssid, const String& password) {
                            DLOG_I(LOG_SYSTEM, "Saving WiFi credentials: SSID='%s', Password=***", ssid.c_str());
                            bool ssidOk = storageComp->putString("wifi_ssid", ssid);
                            bool passOk = storageComp->putString("wifi_password", password);
                            DLOG_I(LOG_SYSTEM, "Storage result: SSID=%s, Password=%s", ssidOk ? "OK" : "FAIL", passOk ? "OK" : "FAIL");
                        }
                    );
                    
                    wifiWebUIProvider->setAPStateCallback(
                        [storageComp](bool enabled, const String& ssid) {
                            DLOG_I(LOG_SYSTEM, "Saving WiFi AP state: enabled=%d, SSID='%s'", enabled, ssid.c_str());
                            storageComp->putBool("wifi_ap_enabled", enabled);
                            if (enabled && ssid.length() > 0) {
                                storageComp->putString("wifi_ap_ssid", ssid);
                            }
                        }
                    );
                    
                    DLOG_I(LOG_SYSTEM, "✓ WiFi WebUI provider registered (with storage persistence)");
                } else {
                    DLOG_I(LOG_SYSTEM, "✓ WiFi WebUI provider registered (no persistence)");
                }
                #else
                DLOG_I(LOG_SYSTEM, "✓ WiFi WebUI provider registered (no persistence)");
                #endif
                
                webuiComponent->registerProviderWithComponent(wifiWebUIProvider, wifi);
            }
            #endif
            
            // Register NTP WebUI provider
            #if __has_include(<DomoticsCore/NTPWebUI.h>)
            auto* ntpComponent = core.getComponent<Components::NTPComponent>("NTP");
            if (ntpComponent) {
                ntpWebUIProvider = new Components::WebUI::NTPWebUI(ntpComponent);
                
                // Set up NTP configuration persistence callback if Storage available
                #if __has_include(<DomoticsCore/Storage.h>)
                auto* storageComp = core.getComponent<Components::StorageComponent>("Storage");
                if (storageComp) {
                    ntpWebUIProvider->setConfigSaveCallback(
                        [storageComp](const Components::NTPConfig& cfg) {
                            DLOG_I(LOG_SYSTEM, "Saving NTP config: timezone='%s', interval=%lu", 
                                   cfg.timezone.c_str(), cfg.syncInterval);
                            storageComp->putBool("ntp_enabled", cfg.enabled);
                            storageComp->putString("ntp_timezone", cfg.timezone);
                            storageComp->putInt("ntp_interval", (int)cfg.syncInterval);
                            // Save servers as comma-separated string
                            String serversStr;
                            for (size_t i = 0; i < cfg.servers.size(); i++) {
                                if (i > 0) serversStr += ",";
                                serversStr += cfg.servers[i];
                            }
                            storageComp->putString("ntp_servers", serversStr);
                        }
                    );
                    DLOG_I(LOG_SYSTEM, "✓ NTP WebUI provider registered (with storage persistence)");
                } else {
                    DLOG_I(LOG_SYSTEM, "✓ NTP WebUI provider registered (no persistence)");
                }
                #else
                DLOG_I(LOG_SYSTEM, "✓ NTP WebUI provider registered (no persistence)");
                #endif
                
                webuiComponent->registerProviderWithComponent(ntpWebUIProvider, ntpComponent);
            }
            #endif
            
            // Register MQTT WebUI provider
            #if __has_include(<DomoticsCore/MQTTWebUI.h>)
            auto* mqttComponent = core.getComponent<Components::MQTTComponent>("MQTT");
            if (mqttComponent) {
                mqttWebUIProvider = new Components::WebUI::MQTTWebUI(mqttComponent);
                webuiComponent->registerProviderWithComponent(mqttWebUIProvider, mqttComponent);
                DLOG_I(LOG_SYSTEM, "✓ MQTT WebUI provider registered");
            }
            #endif
            
            // Register OTA WebUI provider
            #if __has_include(<DomoticsCore/OTAWebUI.h>)
            auto* otaComponent = core.getComponent<Components::OTAComponent>("OTA");
            if (otaComponent) {
                otaWebUIProvider = new Components::WebUI::OTAWebUI(otaComponent);
                webuiComponent->registerProviderWithComponent(otaWebUIProvider, otaComponent);
                // Initialize OTA routes (required for custom upload endpoints)
                otaWebUIProvider->init(webuiComponent);
                DLOG_I(LOG_SYSTEM, "✓ OTA WebUI provider registered");
            }
            #endif
            
            // Register SystemInfo WebUI provider
            #if __has_include(<DomoticsCore/SystemInfoWebUI.h>)
            auto* sysInfoComponent = core.getComponent<Components::SystemInfoComponent>("SystemInfo");
            if (sysInfoComponent) {
                systemInfoWebUIProvider = new Components::WebUI::SystemInfoWebUI(sysInfoComponent);
                webuiComponent->registerProviderWithComponent(systemInfoWebUIProvider, sysInfoComponent);
                DLOG_I(LOG_SYSTEM, "✓ SystemInfo WebUI provider registered");
            }
            #endif
            
            // Register RemoteConsole WebUI provider
            #if __has_include(<DomoticsCore/RemoteConsoleWebUI.h>)
            if (console) {
                consoleWebUIProvider = new Components::WebUI::RemoteConsoleWebUI(console);
                webuiComponent->registerProviderWithComponent(consoleWebUIProvider, console);
                DLOG_I(LOG_SYSTEM, "✓ RemoteConsole WebUI provider registered");
            }
            #endif
            
            // Set up WebUI configuration persistence callback if Storage available
            #if __has_include(<DomoticsCore/Storage.h>)
            auto* storageComp = core.getComponent<Components::StorageComponent>("Storage");
            if (storageComp) {
                webuiComponent->setConfigCallback(
                    [storageComp](const Components::WebUIConfig& cfg) {
                        DLOG_I(LOG_SYSTEM, "Saving WebUI config: theme='%s', deviceName='%s'", 
                               cfg.theme.c_str(), cfg.deviceName.c_str());
                        storageComp->putString("webui_theme", cfg.theme);
                        storageComp->putString("device_name", cfg.deviceName);
                    }
                );
                DLOG_I(LOG_SYSTEM, "✓ WebUI config persistence enabled");
            }
            #endif
        } else {
            DLOG_E(LOG_SYSTEM, "WebUI component NOT found! Providers not registered.");
        }
        #endif
        
        // ====================================================================
        // 7. System Ready!
        // ====================================================================
        setState(SystemState::READY);
        
        DLOG_I(LOG_SYSTEM, "========================================");
        DLOG_I(LOG_SYSTEM, "System Ready!");
        if (wifi) {
            String ip = wifi->getLocalIP();  // getLocalIP() handles both STA and AP modes
            
            if (wifi->isSTAConnected()) {
                DLOG_I(LOG_SYSTEM, "WiFi: %s", wifi->getSSID().c_str());
                DLOG_I(LOG_SYSTEM, "IP: %s", ip.c_str());
            } else if (wifi->isAPEnabled()) {
                DLOG_I(LOG_SYSTEM, "WiFi: AP Mode - %s", wifi->getAPSSID().c_str());
                DLOG_I(LOG_SYSTEM, "IP: %s", ip.c_str());
            }
            
            if (config.enableConsole) {
                DLOG_I(LOG_SYSTEM, "Telnet: telnet %s %d", ip.c_str(), config.consolePort);
            }
            if (config.enableWebUI) {
                DLOG_I(LOG_SYSTEM, "WebUI: http://%s:%d", ip.c_str(), config.webUIPort);
            }
        }
        DLOG_I(LOG_SYSTEM, "========================================");
        
        initialized = true;
        return true;
    }
    
    /**
     * @brief Main loop - call this in Arduino loop()
     */
    void loop() {
        if (!initialized) return;
        core.loop();
    }
    
    /**
     * @brief Get the Core instance (for adding custom components)
     */
    Core& getCore() {
        return core;
    }
    
    /**
     * @brief Get current system state
     */
    SystemState getState() const {
        return state;
    }
    
    /**
     * @brief Register state change callback
     */
    void onStateChange(std::function<void(SystemState, SystemState)> callback) {
        stateCallbacks.push_back(callback);
    }
    
    /**
     * @brief Get the RemoteConsole (for adding custom commands)
     */
    Components::RemoteConsoleComponent* getConsole() {
        return console;
    }
    
    /**
     * @brief Get the WiFi component (for manual control if needed)
     */
    Components::WifiComponent* getWiFi() {
        return wifi;
    }
    
    /**
     * @brief Register a custom console command
     */
    void registerCommand(const String& name, std::function<String(const String&)> handler) {
        if (console) {
            console->registerCommand(name, handler);
        }
    }
    
private:
    
    /**
     * @brief Get system status (for console command)
     */
    String getSystemStatus() {
        String status = "System Status:\n";
        status += "  Device: " + config.deviceName + " v" + config.firmwareVersion + "\n";
        status += "  Uptime: " + String(millis() / 1000) + "s\n";
        status += "  Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
        status += "  State: " + String(stateToString(state)) + "\n";
        return status;
    }
    
    /**
     * @brief Get WiFi status (for console command)
     */
    String getWiFiStatus() {
        if (wifi) {
            return wifi->getDetailedStatus();
        }
        return "WiFi Status: Not initialized\n";
    }
    
    /**
     * @brief Set system state and update LED
     */
    void setState(SystemState newState) {
        if (newState == state) return;
        
        SystemState oldState = state;
        state = newState;
        
        DLOG_I(LOG_SYSTEM, "State: %s → %s", 
               stateToString(oldState), stateToString(newState));
        
        // Update LED pattern
        updateLEDPattern(newState);
        
        // Notify callbacks
        for (auto& callback : stateCallbacks) {
            callback(oldState, newState);
        }
    }
    
    /**
     * @brief Update LED pattern based on system state
     */
    void updateLEDPattern(SystemState state) {
        if (!led) return;
        
        switch (state) {
            case SystemState::BOOTING:
                led->setLEDEffect("status", Components::LEDEffect::Blink, 200);  // Fast blink
                break;
            case SystemState::WIFI_CONNECTING:
                led->setLEDEffect("status", Components::LEDEffect::Blink, 1000);  // Slow blink
                break;
            case SystemState::WIFI_CONNECTED:
                led->setLEDEffect("status", Components::LEDEffect::Pulse, 2000);  // Heartbeat
                break;
            case SystemState::SERVICES_STARTING:
                led->setLEDEffect("status", Components::LEDEffect::Fade, 1500);  // Fade
                break;
            case SystemState::READY:
                led->setLEDEffect("status", Components::LEDEffect::Breathing, 3000);  // Slow breathing
                break;
            case SystemState::ERROR:
                led->setLEDEffect("status", Components::LEDEffect::Blink, 300);  // Fast blink
                break;
            case SystemState::OTA_UPDATE:
                led->setLED("status", Components::LEDColor::White(), 255);  // Solid on
                break;
            case SystemState::SHUTDOWN:
                led->setLED("status", Components::LEDColor::Off(), 0);  // Off
                break;
        }
    }
    
    /**
     * @brief Convert state to string
     */
    const char* stateToString(SystemState state) const {
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
};

} // namespace DomoticsCore

#endif // DOMOTICS_CORE_SYSTEM_H
