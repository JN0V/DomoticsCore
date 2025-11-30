#ifndef DOMOTICS_CORE_SYSTEM_PERSISTENCE_H
#define DOMOTICS_CORE_SYSTEM_PERSISTENCE_H

/**
 * @file SystemPersistence.h
 * @brief Handles loading and saving configuration from/to Storage component.
 * 
 * This module provides persistence for all component configurations using
 * the StorageComponent (ESP32 NVS). It ensures that user settings are 
 * preserved across reboots.
 */

#include <DomoticsCore/Core.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Platform_HAL.h>  // For HAL::getChipId()
#include "SystemConfig.h"

// Storage component (optional)
#if __has_include(<DomoticsCore/Storage.h>)
#include <DomoticsCore/Storage.h>
#define SYSTEM_HAS_STORAGE 1
#else
#define SYSTEM_HAS_STORAGE 0
#endif

// Component configs needed for loading
#if __has_include(<DomoticsCore/Wifi.h>)
#include <DomoticsCore/Wifi.h>
#endif

#if __has_include(<DomoticsCore/WebUI.h>)
#include <DomoticsCore/WebUI.h>
#endif

#if __has_include(<DomoticsCore/NTP.h>)
#include <DomoticsCore/NTP.h>
#endif

#if __has_include(<DomoticsCore/MQTT.h>)
#include <DomoticsCore/MQTT.h>
#endif

#if __has_include(<DomoticsCore/HomeAssistant.h>)
#include <DomoticsCore/HomeAssistant.h>
#endif

#if __has_include(<DomoticsCore/SystemInfo.h>)
#include <DomoticsCore/SystemInfo.h>
#endif

#define LOG_PERSISTENCE "PERSIST"

namespace DomoticsCore {
namespace SystemHelpers {

/**
 * @brief Load device name from Storage
 */
inline void loadDeviceName(Core& core, SystemConfig& config) {
#if SYSTEM_HAS_STORAGE
    if (!config.enableStorage) return;
    
    auto* storage = core.getComponent<Components::StorageComponent>("Storage");
    if (!storage) return;
    
    String savedName = storage->getString("device_name", "");
    if (!savedName.isEmpty()) {
        config.deviceName = savedName;
        DLOG_I(LOG_PERSISTENCE, "Loaded device name: %s", config.deviceName.c_str());
        
        // Update SystemInfo if present
        #if __has_include(<DomoticsCore/SystemInfo.h>)
        auto* sysInfo = core.getComponent<Components::SystemInfoComponent>("System Info");
        if (sysInfo) {
            Components::SystemInfoConfig siCfg = sysInfo->getConfig();
            siCfg.deviceName = savedName;
            sysInfo->setConfig(siCfg);
        }
        #endif
    }
#endif
}

/**
 * @brief Load WiFi configuration from Storage
 */
inline void loadWifiConfig(Core& core, const SystemConfig& config, Components::WifiComponent* wifi) {
#if SYSTEM_HAS_STORAGE
    if (!config.enableStorage || config.wifiSSID.length() > 0 || !wifi) return;
    
    auto* storage = core.getComponent<Components::StorageComponent>("Storage");
    if (!storage) return;
    
    Components::WifiConfig wifiConfig = wifi->getConfig();
    
    wifiConfig.ssid = storage->getString("wifi_ssid", wifiConfig.ssid);
    wifiConfig.password = storage->getString("wifi_pass", wifiConfig.password);
    wifiConfig.autoConnect = storage->getBool("wifi_autocon", wifiConfig.autoConnect);
    wifiConfig.enableAP = storage->getBool("wifi_ap_en", wifiConfig.enableAP);
    wifiConfig.apSSID = storage->getString("wifi_ap_ssid", wifiConfig.apSSID);
    wifiConfig.apPassword = storage->getString("wifi_ap_pass", wifiConfig.apPassword);
    
    // Auto-generate AP SSID if empty
    if (wifiConfig.enableAP && wifiConfig.apSSID.isEmpty()) {
        uint64_t chipid = HAL::getChipId();
        wifiConfig.apSSID = config.deviceName + "-" + String((uint32_t)(chipid >> 32), HEX);
    }
    
    if (!wifiConfig.ssid.isEmpty()) {
        wifi->setConfig(wifiConfig);
        wifi->updateWifiMode();
        DLOG_I(LOG_PERSISTENCE, "Loaded WiFi config: SSID=%s, AP=%d", 
               wifiConfig.ssid.c_str(), wifiConfig.enableAP);
    }
#endif
}

/**
 * @brief Load WebUI configuration from Storage
 */
inline void loadWebUIConfig(Core& core, const SystemConfig& config) {
#if SYSTEM_HAS_STORAGE && __has_include(<DomoticsCore/WebUI.h>)
    if (!config.enableWebUI || !config.enableStorage) return;
    
    auto* storage = core.getComponent<Components::StorageComponent>("Storage");
    auto* webui = core.getComponent<Components::WebUIComponent>("WebUI");
    if (!storage || !webui) return;
    
    Components::WebUIConfig webuiConfig = webui->getConfig();
    
    webuiConfig.theme = storage->getString("webui_theme", webuiConfig.theme);
    webuiConfig.deviceName = storage->getString("device_name", webuiConfig.deviceName);
    webuiConfig.primaryColor = storage->getString("webui_color", webuiConfig.primaryColor);
    webuiConfig.enableAuth = storage->getBool("webui_auth", webuiConfig.enableAuth);
    webuiConfig.username = storage->getString("webui_user", webuiConfig.username);
    webuiConfig.password = storage->getString("webui_pass", webuiConfig.password);
    
    webui->setConfig(webuiConfig);
    DLOG_I(LOG_PERSISTENCE, "Loaded WebUI config: theme=%s", webuiConfig.theme.c_str());
#endif
}

/**
 * @brief Load NTP configuration from Storage
 */
inline void loadNTPConfig(Core& core, const SystemConfig& config) {
#if SYSTEM_HAS_STORAGE && __has_include(<DomoticsCore/NTP.h>)
    if (!config.enableNTP || !config.enableStorage) return;
    
    auto* storage = core.getComponent<Components::StorageComponent>("Storage");
    auto* ntp = core.getComponent<Components::NTPComponent>("NTP");
    if (!storage || !ntp) return;
    
    Components::NTPConfig ntpConfig = ntp->getConfig();
    
    ntpConfig.enabled = storage->getBool("ntp_enabled", ntpConfig.enabled);
    ntpConfig.timezone = storage->getString("ntp_timezone", ntpConfig.timezone);
    ntpConfig.syncInterval = (uint32_t)storage->getInt("ntp_interval", ntpConfig.syncInterval);
    
    // Load servers from comma-separated string
    String serversStr = storage->getString("ntp_servers", "");
    if (serversStr.length() > 0) {
        ntpConfig.servers.clear();
        int start = 0;
        int commaPos;
        while ((commaPos = serversStr.indexOf(',', start)) != -1) {
            String server = serversStr.substring(start, commaPos);
            server.trim();
            if (server.length() > 0) ntpConfig.servers.push_back(server);
            start = commaPos + 1;
        }
        String lastServer = serversStr.substring(start);
        lastServer.trim();
        if (lastServer.length() > 0) ntpConfig.servers.push_back(lastServer);
    }
    
    ntp->setConfig(ntpConfig);
    DLOG_I(LOG_PERSISTENCE, "Loaded NTP config: timezone=%s", ntpConfig.timezone.c_str());
#endif
}

/**
 * @brief Load MQTT configuration from Storage
 */
inline void loadMQTTConfig(Core& core, const SystemConfig& config) {
#if SYSTEM_HAS_STORAGE && __has_include(<DomoticsCore/MQTT.h>)
    if (!config.enableMQTT || !config.enableStorage) return;
    
    auto* storage = core.getComponent<Components::StorageComponent>("Storage");
    auto* mqtt = core.getComponent<Components::MQTTComponent>("MQTT");
    if (!storage || !mqtt) return;
    
    Components::MQTTConfig mqttConfig = mqtt->getConfig();
    
    mqttConfig.broker = storage->getString("mqtt_broker", mqttConfig.broker);
    mqttConfig.port = (uint16_t)storage->getInt("mqtt_port", mqttConfig.port);
    mqttConfig.username = storage->getString("mqtt_user", mqttConfig.username);
    mqttConfig.password = storage->getString("mqtt_pass", mqttConfig.password);
    mqttConfig.clientId = storage->getString("mqtt_clientid", mqttConfig.clientId);
    
    mqtt->setConfig(mqttConfig);
    DLOG_I(LOG_PERSISTENCE, "Loaded MQTT config: broker=%s:%d", 
           mqttConfig.broker.c_str(), mqttConfig.port);
#endif
}

/**
 * @brief Load HomeAssistant configuration from Storage
 */
inline void loadHomeAssistantConfig(Core& core, const SystemConfig& config) {
#if SYSTEM_HAS_STORAGE && __has_include(<DomoticsCore/HomeAssistant.h>)
    if (!config.enableHomeAssistant || !config.enableStorage) return;
    
    auto* storage = core.getComponent<Components::StorageComponent>("Storage");
    auto* ha = core.getComponent<Components::HomeAssistant::HomeAssistantComponent>("HomeAssistant");
    if (!storage || !ha) return;
    
    Components::HomeAssistant::HAConfig haConfig = ha->getConfig();
    
    haConfig.nodeId = storage->getString("ha_nodeid", haConfig.nodeId);
    haConfig.deviceName = storage->getString("ha_device_name", haConfig.deviceName);
    haConfig.manufacturer = storage->getString("ha_mfg", haConfig.manufacturer);
    haConfig.model = storage->getString("ha_model", haConfig.model);
    haConfig.swVersion = storage->getString("ha_sw_ver", haConfig.swVersion);
    haConfig.discoveryPrefix = storage->getString("ha_disc_prefix", haConfig.discoveryPrefix);
    
    ha->setConfig(haConfig);
    DLOG_I(LOG_PERSISTENCE, "Loaded HomeAssistant config: nodeId=%s", haConfig.nodeId.c_str());
#endif
}

/**
 * @brief Load all configurations from Storage
 */
inline void loadAllConfigs(Core& core, SystemConfig& config, Components::WifiComponent* wifi) {
    loadDeviceName(core, config);
    loadWifiConfig(core, config, wifi);
    loadWebUIConfig(core, config);
    loadNTPConfig(core, config);
    loadMQTTConfig(core, config);
    loadHomeAssistantConfig(core, config);
}

} // namespace SystemHelpers
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_SYSTEM_PERSISTENCE_H
