/**
 * @file test_settings_round_trip.cpp
 * @brief What a settings card stores, and what the next boot makes of it.
 *
 * Each case writes through the real provider wiring (setupWebUIProviders) or
 * straight into Storage, then runs the loaders a boot runs, on a fresh set of
 * components sharing the same stored namespace.
 */

#include <unity.h>
#include <DomoticsCore/System.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::SystemHelpers;

namespace {

std::map<String, String> fieldValue(const char* field, const char* value) {
    std::map<String, String> params;
    params[String("field")] = String(field);
    params[String("value")] = String(value);
    return params;
}

bool succeeded(const String& response) {
    return strstr(response.c_str(), "\"success\":true") != nullptr;
}

/// One boot: every component, the storage keys, the loaders, the providers.
struct Boot {
    Core core;
    SystemConfig config;
    WebUIProviders providers;
    WifiComponent* wifi = nullptr;
    StorageComponent* storage = nullptr;
    SystemInfoComponent* sysInfo = nullptr;
    WebUIComponent* webui = nullptr;
    MQTTComponent* mqtt = nullptr;
    NTPComponent* ntp = nullptr;
    OTAComponent* ota = nullptr;
    HomeAssistant::HomeAssistantComponent* ha = nullptr;

    explicit Boot(const MQTTConfig& mcfg = compiledMqtt()) {
        config.deviceName = "Compiled";
        config.enableStorage = config.enableWebUI = config.enableMQTT = true;
        config.enableNTP = config.enableOTA = config.enableHomeAssistant = true;

        auto w = std::make_unique<WifiComponent>(String(""), String(""));
        wifi = w.get(); core.addComponent(std::move(w));
        StorageConfig sc; sc.namespace_name = "roundtrip";
        auto s = std::make_unique<StorageComponent>(sc);
        storage = s.get(); core.addComponent(std::move(s));
        SystemInfoConfig si; si.deviceName = config.deviceName;
        auto i = std::make_unique<SystemInfoComponent>(si);
        sysInfo = i.get(); core.addComponent(std::move(i));
        auto u = std::make_unique<WebUIComponent>();
        webui = u.get(); core.addComponent(std::move(u));
        auto m = std::make_unique<MQTTComponent>(mcfg);
        mqtt = m.get(); core.addComponent(std::move(m));
        auto n = std::make_unique<NTPComponent>();
        ntp = n.get(); core.addComponent(std::move(n));
        auto o = std::make_unique<OTAComponent>();
        ota = o.get(); core.addComponent(std::move(o));
        HomeAssistant::HAConfig hcfg;
        HomeAssistant::HA::setField(hcfg.nodeId, "compiled_node", sizeof(hcfg.nodeId));
        auto h = std::make_unique<HomeAssistant::HomeAssistantComponent>(hcfg);
        ha = h.get(); core.addComponent(std::move(h));

        core.begin();
        loadAllConfigs(core, config, wifi);
        setupWebUIProviders(core, config, providers, wifi, nullptr);
    }

    static MQTTConfig compiledMqtt() {
        MQTTConfig m;
        m.broker = "compiled.broker";
        m.username = "compiled-user";
        m.password = "compiled-pass";
        m.clientId = "compiled-id";
        return m;
    }

    String post(IWebUIProvider* provider, const char* context, const char* field, const char* value) {
        return provider->handleWebUIRequest(String(context), String("/api/ui/action"),
                                            String("POST"), fieldValue(field, value));
    }

    ~Boot() { core.shutdown(); }
};

}  // namespace

// Flash outlives a reboot; the stub's contents outlive an instance only when asked.
void setUp() {
    HAL::RAMOnlyStorage::forgetPersistedForTest();
    HAL::RAMOnlyStorage::persistAcrossInstancesForTest = true;
}
void tearDown() {
    HAL::RAMOnlyStorage::persistAcrossInstancesForTest = false;
    HAL::RAMOnlyStorage::forgetPersistedForTest();
    HAL::WiFiImpl::resetWifiStateForTest();
}

// The fixture's own premise: what one boot stores, the next one reads.
void test_the_fixture_persists_across_boots() {
    { Boot b; b.storage->putString("mqtt_broker", "stored.broker"); }
    Boot next;
    TEST_ASSERT_EQUAL_STRING("stored.broker", next.mqtt->getConfig().broker.c_str());
}

// --- a stored empty value does not replace what the firmware was built with ----

void test_a_stored_empty_client_id_keeps_the_compiled_one() {
    { Boot b; b.storage->putString("mqtt_clientid", ""); }
    Boot next;
    TEST_ASSERT_EQUAL_STRING("compiled-id", next.mqtt->getConfig().clientId.c_str());
}

// An empty broker or username is a choice: MQTT off, an anonymous broker.
void test_a_cleared_username_stays_cleared() {
    {
        Boot b;
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.mqtt, "mqtt_settings", "username", "")));
    }
    Boot next;
    TEST_ASSERT_EQUAL_STRING("", next.mqtt->getConfig().username.c_str());
}

void test_a_cleared_manufacturer_stays_cleared() {
    {
        Boot b;
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.ha, "ha_settings", "manufacturer", "")));
    }
    Boot next;
    TEST_ASSERT_EQUAL_STRING("", next.ha->getConfig().manufacturer);
}

// The card stores what the component applied, so the default will topic that
// followed each rename comes back on the last one.
void test_the_will_follows_two_renames_across_a_reboot() {
    {
        Boot b;
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.mqtt, "mqtt_settings", "client_id", "first")));
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.mqtt, "mqtt_settings", "client_id", "second")));
        TEST_ASSERT_EQUAL_STRING("second/status", b.mqtt->getConfig().lwtTopic.c_str());
    }
    Boot next;
    TEST_ASSERT_EQUAL_STRING("second", next.mqtt->getConfig().clientId.c_str());
    TEST_ASSERT_EQUAL_STRING("second/status", next.mqtt->getConfig().lwtTopic.c_str());
}

void test_stored_empty_identity_values_keep_the_compiled_ones() {
    {
        Boot b;
        b.storage->putString("ha_nodeid", "");
        b.storage->putString("ha_disc_prefix", "");
        b.storage->putString("ha_device_name", "");
        b.storage->putString("webui_theme", "");
        b.storage->putString("webui_color", "");
        b.storage->putString("webui_user", "");
        b.storage->putString("ntp_timezone", "");
    }
    Boot next;
    TEST_ASSERT_EQUAL_STRING("compiled_node", next.ha->getConfig().nodeId);
    TEST_ASSERT_EQUAL_STRING("homeassistant", next.ha->getConfig().discoveryPrefix);
    TEST_ASSERT_TRUE(strlen(next.ha->getConfig().deviceName) > 0);
    TEST_ASSERT_TRUE(strlen(next.webui->getConfig().theme) > 0);
    TEST_ASSERT_TRUE(strlen(next.webui->getConfig().primaryColor) > 0);
    TEST_ASSERT_TRUE(strlen(next.webui->getConfig().username) > 0);
    TEST_ASSERT_TRUE(next.ntp->getConfig().timezone.length() > 0);
}

// --- a rename from the SystemInfo card survives a WebUI settings save ----------

void test_a_rename_survives_a_webui_settings_save() {
    {
        Boot b;
        TEST_ASSERT_NOT_NULL(b.providers.sysInfo);
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.sysInfo, "system_settings", "device_name", "Cellar")));
        TEST_ASSERT_EQUAL_STRING_MESSAGE("Cellar", b.webui->getConfig().deviceName,
            "the page header still shows the old name");
        TEST_ASSERT_TRUE(succeeded(b.post(b.webui, "webui_settings", "theme", "dark")));
    }
    Boot next;
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Cellar", next.config.deviceName.c_str(),
        "a WebUI settings save wrote the old name back");
}

// --- SystemConfig's NTP fields reach the component ------------------------------

void test_the_system_ntp_fields_reach_the_component() {
    SystemConfig cfg;
    cfg.enableNTP = true;
    cfg.ntpServer = "ntp.example.test";
    cfg.ntpTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";
    const NTPConfig n = systemNtpConfig(cfg);
    TEST_ASSERT_EQUAL_STRING("ntp.example.test", n.servers.at(0).c_str());
    TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", n.timezone.c_str());
}

void test_the_system_ntp_defaults_are_the_component_defaults() {
    const NTPConfig n = systemNtpConfig(SystemConfig());
    const NTPConfig d;
    TEST_ASSERT_EQUAL_STRING(d.timezone.c_str(), n.timezone.c_str());
    TEST_ASSERT_EQUAL(d.servers.size(), n.servers.size());
}

// --- a card edit comes back after a reboot --------------------------------------

void test_mqtt_card_edits_survive_a_reboot() {
    {
        Boot b;
        TEST_ASSERT_NOT_NULL(b.providers.mqtt);
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.mqtt, "mqtt_settings", "use_tls", "true")));
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.mqtt, "mqtt_settings", "lwt_topic", "house/panel/alive")));
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.mqtt, "mqtt_settings", "lwt_message", "gone")));
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.mqtt, "mqtt_settings", "lwt_enabled", "false")));
    }
    Boot next;
    const MQTTConfig m = next.mqtt->getConfig();
    TEST_ASSERT_TRUE(m.useTLS);
    TEST_ASSERT_FALSE(m.enableLWT);
    TEST_ASSERT_EQUAL_STRING("house/panel/alive", m.lwtTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("gone", m.lwtMessage.c_str());
}

void test_ha_card_edits_survive_a_reboot() {
    {
        Boot b;
        TEST_ASSERT_NOT_NULL(b.providers.ha);
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.ha, "ha_settings", "manufacturer", "Acme")));
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.ha, "ha_settings", "model", "AC-1")));
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.ha, "ha_settings", "suggested_area", "Cellar")));
    }
    Boot next;
    TEST_ASSERT_EQUAL_STRING("Acme", next.ha->getConfig().manufacturer);
    TEST_ASSERT_EQUAL_STRING("AC-1", next.ha->getConfig().model);
    TEST_ASSERT_EQUAL_STRING("Cellar", next.ha->getConfig().suggestedArea);
}

void test_ota_card_edits_survive_a_reboot() {
    {
        Boot b;
        TEST_ASSERT_NOT_NULL(b.providers.ota);
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.ota, "ota_unified", "update_url", "http://fw.example.test/fw.bin")));
        TEST_ASSERT_TRUE(succeeded(b.post(b.providers.ota, "ota_unified", "auto_reboot", "false")));
    }
    Boot next;
    TEST_ASSERT_EQUAL_STRING("http://fw.example.test/fw.bin", next.ota->getConfig().updateUrl.c_str());
    TEST_ASSERT_FALSE(next.ota->getConfig().autoReboot);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_fixture_persists_across_boots);
    RUN_TEST(test_a_stored_empty_client_id_keeps_the_compiled_one);
    RUN_TEST(test_a_cleared_username_stays_cleared);
    RUN_TEST(test_a_cleared_manufacturer_stays_cleared);
    RUN_TEST(test_the_will_follows_two_renames_across_a_reboot);
    RUN_TEST(test_stored_empty_identity_values_keep_the_compiled_ones);
    RUN_TEST(test_a_rename_survives_a_webui_settings_save);
    RUN_TEST(test_the_system_ntp_fields_reach_the_component);
    RUN_TEST(test_the_system_ntp_defaults_are_the_component_defaults);
    RUN_TEST(test_mqtt_card_edits_survive_a_reboot);
    RUN_TEST(test_ha_card_edits_survive_a_reboot);
    RUN_TEST(test_ota_card_edits_survive_a_reboot);
    return UNITY_END();
}
