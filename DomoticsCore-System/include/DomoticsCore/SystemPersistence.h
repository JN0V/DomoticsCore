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
#include <DomoticsCore/FlightRecorder.h>  // OBS-3: the death the bootdiag blob records
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
 * @brief Register all known storage keys with the Storage component
 * Each component's keys are registered so Storage can enumerate them
 */
inline void registerStorageKeys(Core& core, const SystemConfig& config) {
#if SYSTEM_HAS_STORAGE
    if (!config.enableStorage) return;
    
    auto* storage = core.getComponent<Components::StorageComponent>("Storage");
    if (!storage) return;
    
    using KeyDef = Components::StorageKeyDef;
    
    // Device keys
    storage->registerKeys("System", {
        KeyDef("device_name", 's', "Device name")
    });
    
    // WiFi keys
    storage->registerKeys("WiFi", {
        KeyDef("wifi_ssid", 's', "WiFi SSID"),
        KeyDef("wifi_pass", 's', "WiFi password"),
        KeyDef("wifi_autocon", 'b', "Auto-connect"),
        KeyDef("wifi_ap_en", 'b', "AP mode enabled"),
        KeyDef("wifi_ap_ssid", 's', "AP SSID"),
        KeyDef("wifi_ap_pass", 's', "AP password")
    });
    
    // WebUI keys
#if __has_include(<DomoticsCore/WebUI.h>)
    if (config.enableWebUI) {
        storage->registerKeys("WebUI", {
            KeyDef("webui_theme", 's', "Theme"),
            KeyDef("webui_color", 's', "Primary color"),
            KeyDef("webui_auth", 'b', "Auth enabled"),
            KeyDef("webui_user", 's', "Username"),
            KeyDef("webui_pass", 's', "Password")
        });
    }
#endif
    
    // NTP keys
#if __has_include(<DomoticsCore/NTP.h>)
    if (config.enableNTP) {
        storage->registerKeys("NTP", {
            KeyDef("ntp_enabled", 'b', "NTP enabled"),
            KeyDef("ntp_timezone", 's', "Timezone"),
            KeyDef("ntp_interval", 'i', "Sync interval"),
            KeyDef("ntp_servers", 's', "NTP servers")
        });
    }
#endif
    
    // MQTT keys
#if __has_include(<DomoticsCore/MQTT.h>)
    if (config.enableMQTT) {
        storage->registerKeys("MQTT", {
            KeyDef("mqtt_enabled", 'b', "MQTT enabled"),
            KeyDef("mqtt_broker", 's', "Broker address"),
            KeyDef("mqtt_port", 'i', "Broker port"),
            KeyDef("mqtt_user", 's', "Username"),
            KeyDef("mqtt_pass", 's', "Password"),
            KeyDef("mqtt_clientid", 's', "Client ID")
        });
    }
#endif
    
    // HomeAssistant keys
#if __has_include(<DomoticsCore/HomeAssistant.h>)
    if (config.enableHomeAssistant) {
        storage->registerKeys("HomeAssistant", {
            KeyDef("ha_nodeid", 's', "Node ID"),
            KeyDef("ha_device_name", 's', "Device name"),
            KeyDef("ha_disc_prefix", 's', "Discovery prefix"),
            KeyDef("ha_mfg", 's', "Manufacturer"),
            KeyDef("ha_model", 's', "Model"),
            KeyDef("ha_sw_ver", 's', "Software version")
        });
    }
#endif
    
    DLOG_I(LOG_PERSISTENCE, "Storage keys registered");
#endif
}

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
        // NOTE: Do NOT call updateWifiMode() here! This runs during System::begin()
        // when heap is low and configSaveCallback_ is not yet set. The mode update
        // is deferred to afterAllComponentsReady() which has heap guards and fallback logic.
        DLOG_I(LOG_PERSISTENCE, "Loaded WiFi config: SSID=%s, AP=%d (mode update deferred)", 
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
    
    webuiConfig.setTheme(storage->getString("webui_theme", webuiConfig.getTheme()).c_str());
    webuiConfig.setDeviceName(storage->getString("device_name", webuiConfig.getDeviceName()).c_str());
    webuiConfig.setPrimaryColor(storage->getString("webui_color", webuiConfig.getPrimaryColor()).c_str());
    webuiConfig.enableAuth = storage->getBool("webui_auth", webuiConfig.enableAuth);
    webuiConfig.setUsername(storage->getString("webui_user", webuiConfig.getUsername()).c_str());
    webuiConfig.setPassword(storage->getString("webui_pass", webuiConfig.getPassword()).c_str());
    
    webui->setConfig(webuiConfig);
    DLOG_I(LOG_PERSISTENCE, "Loaded WebUI config: theme=%s", webuiConfig.theme);
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
    
    mqttConfig.enabled = storage->getBool("mqtt_enabled", mqttConfig.enabled);
    mqttConfig.broker = storage->getString("mqtt_broker", mqttConfig.broker);
    mqttConfig.port = (uint16_t)storage->getInt("mqtt_port", mqttConfig.port);
    mqttConfig.username = storage->getString("mqtt_user", mqttConfig.username);
    mqttConfig.password = storage->getString("mqtt_pass", mqttConfig.password);
    mqttConfig.clientId = storage->getString("mqtt_clientid", mqttConfig.clientId);
    
    mqtt->setConfig(mqttConfig);
    DLOG_I(LOG_PERSISTENCE, "Loaded MQTT config: enabled=%d, broker=%s:%d", 
           mqttConfig.enabled, mqttConfig.broker.c_str(), mqttConfig.port);
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
    
    using namespace Components::HomeAssistant;
    HAConfig haConfig = ha->getConfig();

    HA::setField(haConfig.nodeId, storage->getString("ha_nodeid", haConfig.nodeId).c_str(), HA::MAX_NODE_ID);
    HA::setField(haConfig.deviceName, storage->getString("ha_device_name", haConfig.deviceName).c_str(), HA::MAX_DEVICE_NAME);
    HA::setField(haConfig.manufacturer, storage->getString("ha_mfg", haConfig.manufacturer).c_str(), HA::MAX_MANUFACTURER);
    HA::setField(haConfig.model, storage->getString("ha_model", haConfig.model).c_str(), HA::MAX_MODEL);
    HA::setField(haConfig.swVersion, storage->getString("ha_sw_ver", haConfig.swVersion).c_str(), HA::MAX_SW_VERSION);
    HA::setField(haConfig.discoveryPrefix, storage->getString("ha_disc_prefix", haConfig.discoveryPrefix).c_str(), HA::MAX_DISCOVERY_PREFIX);

    ha->setConfig(haConfig);
    DLOG_I(LOG_PERSISTENCE, "Loaded HomeAssistant config: nodeId=%s", haConfig.nodeId);
#endif
}

#if __has_include(<DomoticsCore/Storage.h>) && __has_include(<DomoticsCore/SystemInfo.h>)
/**
 * @brief The `bootdiag` blob: this boot's figures and the last recorded death, in one Storage write.
 *
 * Replaces `last_reset`, `boot_heap` and `boot_minheap` (OBS-6's keys) so a
 * boot costs two writes — `boot_count` and this — on a backend that rewrites
 * its whole file per changed key (OBS-3, residual 9). The death fields carry
 * forward across clean boots; an identical death (same dedup key) bumps
 * `sameCount` and keeps the first occurrence's fields.
 */
struct BootDiagRecord {
    uint32_t version = 1;
    int32_t  lastReset = -1;        // HAL::Platform::ResetReason of this boot
    uint32_t bootHeap = 0;          // free heap when this run started
    uint32_t bootMinHeap = 0;       // minimum so far, where the platform tracks one
    uint32_t minHeapTracked = 0;
    uint32_t promotion = 0;         // FlightRecorder::Promotion of the last death, 0 = none on record
    uint32_t phase = 0;
    uint32_t buildId = 0;
    uint32_t uptimeMs = 0;
    uint32_t reason = 0;            // the crash callback's reason (ESP8266), or 0
    uint32_t epc1 = 0;
    uint32_t failSize = 0;
    uint32_t failCaller = 0;
    uint32_t minFree = 0;
    uint32_t dedupKey = 0;
    uint32_t sameCount = 0;
};

inline bool readBootDiagRecord(Components::StorageComponent& storage, BootDiagRecord& out) {
    BootDiagRecord r;
    size_t n = storage.getBlob("bootdiag", reinterpret_cast<uint8_t*>(&r), sizeof(r));
    if (n != sizeof(r) || r.version != 1) return false;
    out = r;
    return true;
}

/**
 * @brief Increment and persist the boot counter, and persist this boot's diagnostics.
 *
 * Returns the new boot count, already pushed into SystemInfo. Two writes:
 * `boot_count` and the `bootdiag` blob. The keys the blob replaces are
 * removed once on the first boot of this build so they do not sit in
 * NVS/LittleFS forever, as OBS-6 did for `last_heap`/`last_minheap`.
 */
inline uint32_t persistBootDiagnostics(Components::StorageComponent& storage,
                                       Components::SystemInfoComponent& sysInfo) {
    uint32_t bootCount = storage.getInt("boot_count", 0) + 1;
    storage.putInt("boot_count", static_cast<int32_t>(bootCount));

    const auto& diag = sysInfo.getBootDiagnostics();
    sysInfo.setBootCount(bootCount);

    BootDiagRecord prev;
    const bool hadPrev = readBootDiagRecord(storage, prev);

    BootDiagRecord rec;
    rec.lastReset = static_cast<int32_t>(diag.resetReason);
    rec.bootHeap = diag.bootHeap;
    rec.minHeapTracked = diag.bootMinHeapTracked ? 1 : 0;
    rec.bootMinHeap = diag.bootMinHeapTracked ? diag.bootMinHeap : 0;

    const FlightRecorder& fr = FlightRecorder::instance();
    if (fr.hasPromotedRecord()) {
        const FlightRecord& d = fr.promoted();
        const uint32_t key = d.dedupKey();
        if (hadPrev && prev.promotion != 0 && prev.dedupKey == key) {
            // the same death again: keep the first occurrence, count this one
            rec.promotion = prev.promotion; rec.phase = prev.phase; rec.buildId = prev.buildId;
            rec.uptimeMs = prev.uptimeMs; rec.reason = prev.reason; rec.epc1 = prev.epc1;
            rec.failSize = prev.failSize; rec.failCaller = prev.failCaller; rec.minFree = prev.minFree;
            rec.dedupKey = key; rec.sameCount = prev.sameCount + 1;
        } else {
            rec.promotion = static_cast<uint32_t>(fr.promotion());
            rec.phase = d.phase(); rec.buildId = d.w[FlightRecord::W_BUILD];
            rec.uptimeMs = d.lastUptimeMs(); rec.reason = d.cbReason(); rec.epc1 = d.epc1();
            rec.failSize = d.failSize(); rec.failCaller = d.failCaller(); rec.minFree = d.minFreeBytes();
            rec.dedupKey = key; rec.sameCount = 1;
        }
    } else if (hadPrev) {
        // a clean boot keeps the last death on record
        rec.promotion = prev.promotion; rec.phase = prev.phase; rec.buildId = prev.buildId;
        rec.uptimeMs = prev.uptimeMs; rec.reason = prev.reason; rec.epc1 = prev.epc1;
        rec.failSize = prev.failSize; rec.failCaller = prev.failCaller; rec.minFree = prev.minFree;
        rec.dedupKey = prev.dedupKey; rec.sameCount = prev.sameCount;
    }
    storage.putBlob("bootdiag", reinterpret_cast<const uint8_t*>(&rec), sizeof(rec));

    static const char* const retired[] = { "last_reset", "boot_heap", "boot_minheap", "last_heap", "last_minheap" };
    for (const char* k : retired) {
        if (storage.exists(k)) storage.remove(k);
    }
    return bootCount;
}

/** @brief The persisted block of `bootdiag`, allocation-free. Returns characters written. */
inline size_t formatPersistedBootDiagnostics(Components::StorageComponent& storage, char* buf, size_t len) {
    BootDiagRecord r;
    const bool have = readBootDiagRecord(storage, r);
    int n = snprintf(buf, len,
             "\nPersisted Data:\n"
             "  boot_count: %d\n"
             "  last_reset: %ld\n"
             "  boot_heap: %lu\n"
             "  boot_minheap: %s\n",
             storage.getInt("boot_count", 0),
             have ? static_cast<long>(r.lastReset) : -1L,
             have ? static_cast<unsigned long>(r.bootHeap) : 0UL,
             have && r.minHeapTracked ? String(r.bootMinHeap).c_str() : "n/a");
    if (n < 0) return 0;
    size_t used = static_cast<size_t>(n) < len ? static_cast<size_t>(n) : len - 1;
    if (have && r.promotion != 0) {
        int m = snprintf(buf + used, len - used,
                 "  last death: %s x%lu | phase %lu | uptime %lu s | reason %lu epc1 0x%08lx | fail %lu B from 0x%08lx | min free %lu B | build %08lx\n",
                 FlightRecorder::promotionName(static_cast<FlightRecorder::Promotion>(r.promotion)),
                 static_cast<unsigned long>(r.sameCount), static_cast<unsigned long>(r.phase),
                 static_cast<unsigned long>(r.uptimeMs / 1000u), static_cast<unsigned long>(r.reason),
                 static_cast<unsigned long>(r.epc1), static_cast<unsigned long>(r.failSize),
                 static_cast<unsigned long>(r.failCaller), static_cast<unsigned long>(r.minFree),
                 static_cast<unsigned long>(r.buildId));
        if (m > 0) used += static_cast<size_t>(m) < len - used ? static_cast<size_t>(m) : len - used - 1;
    }
    return used;
}
#endif

/**
 * @brief Load all configurations from Storage
 */
inline void loadAllConfigs(Core& core, SystemConfig& config, Components::WifiComponent* wifi) {
    // First register all storage keys so they can be enumerated
    registerStorageKeys(core, config);
    
    // Then load configurations
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
