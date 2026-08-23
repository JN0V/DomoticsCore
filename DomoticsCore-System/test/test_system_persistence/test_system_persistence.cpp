/**
 * @file test_system_persistence.cpp
 * @brief SystemHelpers — loading component configuration back out of Storage.
 *
 * These are free functions taking a Core by reference, so they can be driven
 * directly: put values in Storage, call the loader, look at what the component
 * ended up with. Every one of them is a stack of early returns — no Storage,
 * feature disabled, component absent, stored value empty — and it is the early
 * returns, not the happy path, that decide whether a user's saved settings come
 * back after a reboot.
 */

#include <unity.h>
#include <DomoticsCore/System.h>

using namespace DomoticsCore;
using namespace DomoticsCore::SystemHelpers;

void setUp(void) {}
void tearDown(void) {}

// A Core with Storage and WiFi up, ready to be written to and read back.
struct Fixture {
    Core core;
    Components::WifiComponent* wifi = nullptr;
    Components::StorageComponent* storage = nullptr;
    Components::SystemInfoComponent* sysInfo = nullptr;

    explicit Fixture(bool withStorage = true, bool withSystemInfo = false) {
        auto wifiPtr = std::make_unique<Components::WifiComponent>(String(""), String(""));
        wifi = wifiPtr.get();
        core.addComponent(std::move(wifiPtr));

        if (withStorage) {
            Components::StorageConfig sc;
            sc.namespace_name = "testns";
            auto sPtr = std::make_unique<Components::StorageComponent>(sc);
            storage = sPtr.get();
            core.addComponent(std::move(sPtr));
        }

        if (withSystemInfo) {
            Components::SystemInfoConfig si;
            si.deviceName = "Before";
            auto siPtr = std::make_unique<Components::SystemInfoComponent>(si);
            sysInfo = siPtr.get();
            core.addComponent(std::move(siPtr));
        }

        core.begin();
    }
};

static SystemConfig storageEnabled() {
    SystemConfig config;
    config.enableStorage = true;
    return config;
}

// ============================================================================
// loadDeviceName
// ============================================================================

void test_device_name_is_restored(void) {
    Fixture f;
    f.storage->putString("device_name", "Cellar");

    SystemConfig config = storageEnabled();
    loadDeviceName(f.core, config);

    TEST_ASSERT_EQUAL_STRING("Cellar", config.deviceName.c_str());
}

void test_an_absent_device_name_leaves_the_default(void) {
    Fixture f;

    SystemConfig config = storageEnabled();
    loadDeviceName(f.core, config);

    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.deviceName.c_str());
}

void test_an_empty_stored_device_name_is_ignored(void) {
    // An empty string is a value Storage will return happily; adopting it would
    // leave the device nameless.
    Fixture f;
    f.storage->putString("device_name", "");

    SystemConfig config = storageEnabled();
    loadDeviceName(f.core, config);

    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.deviceName.c_str());
}

void test_device_name_is_not_loaded_when_storage_is_disabled(void) {
    Fixture f;
    f.storage->putString("device_name", "Cellar");

    SystemConfig config;               // enableStorage stays false
    loadDeviceName(f.core, config);

    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.deviceName.c_str());
}

void test_device_name_without_a_storage_component(void) {
    Fixture f(false);

    SystemConfig config = storageEnabled();   // asked for, but not registered
    loadDeviceName(f.core, config);

    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.deviceName.c_str());
}

void test_device_name_reaches_systeminfo(void) {
    Fixture f(true, true);
    f.storage->putString("device_name", "Cellar");

    SystemConfig config = storageEnabled();
    loadDeviceName(f.core, config);

    // SystemInfo publishes the name over the WebUI and MQTT; leaving it stale
    // would show one name in the console and another in Home Assistant.
    TEST_ASSERT_EQUAL_STRING("Cellar", f.sysInfo->getConfig().deviceName.c_str());
}

// ============================================================================
// loadWifiConfig
// ============================================================================

void test_wifi_credentials_are_restored(void) {
    Fixture f;
    f.storage->putString("wifi_ssid", "HomeNet");
    f.storage->putString("wifi_pass", "hunter2");

    SystemConfig config = storageEnabled();
    loadWifiConfig(f.core, config, f.wifi);

    TEST_ASSERT_EQUAL_STRING("HomeNet", f.wifi->getConfig().ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("hunter2", f.wifi->getConfig().password.c_str());
}

void test_ap_settings_are_restored(void) {
    Fixture f;
    f.storage->putString("wifi_ssid", "HomeNet");
    f.storage->putBool("wifi_ap_en", true);
    f.storage->putString("wifi_ap_ssid", "Fallback");
    f.storage->putString("wifi_ap_pass", "letmein");

    SystemConfig config = storageEnabled();
    loadWifiConfig(f.core, config, f.wifi);

    TEST_ASSERT_TRUE(f.wifi->getConfig().enableAP);
    TEST_ASSERT_EQUAL_STRING("Fallback", f.wifi->getConfig().apSSID.c_str());
    TEST_ASSERT_EQUAL_STRING("letmein", f.wifi->getConfig().apPassword.c_str());
}

void test_an_absent_ap_ssid_keeps_the_one_the_component_already_has(void) {
    // loadWifiConfig() carries a branch that builds an AP SSID from the device
    // name when the stored one is empty. It does not fire here, and as far as
    // this suite can tell it never fires: WifiComponent::begin() has already
    // fallen back to AP mode and named itself "DomoticsCore-<MAC>", so the SSID
    // the loader inspects is never empty. See PERSIST-1 in CODE-ROADMAP.
    //
    // The behaviour is pinned as it is rather than as the branch intends, so
    // whoever settles PERSIST-1 sees this test change with it.
    Fixture f;
    f.storage->putString("wifi_ssid", "HomeNet");
    f.storage->putBool("wifi_ap_en", true);
    // wifi_ap_ssid deliberately absent

    SystemConfig config = storageEnabled();
    config.deviceName = "Kitchen";
    loadWifiConfig(f.core, config, f.wifi);

    String apSSID = f.wifi->getConfig().apSSID;
    TEST_ASSERT_FALSE(apSSID.isEmpty());
    TEST_ASSERT_FALSE(apSSID.startsWith(String("Kitchen-")));
    TEST_ASSERT_TRUE(apSSID.startsWith(String("DomoticsCore-")));
}

void test_an_explicit_ssid_in_code_beats_the_stored_one(void) {
    // config.wifiSSID set means the sketch hard-coded credentials. Storage must
    // not override what the developer wrote.
    Fixture f;
    f.storage->putString("wifi_ssid", "HomeNet");

    SystemConfig config = storageEnabled();
    config.wifiSSID = "Hardcoded";
    loadWifiConfig(f.core, config, f.wifi);

    TEST_ASSERT_FALSE(f.wifi->getConfig().ssid == String("HomeNet"));
}

void test_an_empty_stored_ssid_leaves_the_component_alone(void) {
    Fixture f;
    Components::WifiConfig before = f.wifi->getConfig();
    f.storage->putString("wifi_pass", "orphaned");   // password without an SSID

    SystemConfig config = storageEnabled();
    loadWifiConfig(f.core, config, f.wifi);

    // Nothing is applied without an SSID — otherwise a stray password would be
    // written over the live configuration.
    TEST_ASSERT_TRUE(f.wifi->getConfig().ssid == before.ssid);
    TEST_ASSERT_TRUE(f.wifi->getConfig().password == before.password);
}

void test_a_null_wifi_component_is_survived(void) {
    Fixture f;
    f.storage->putString("wifi_ssid", "HomeNet");

    SystemConfig config = storageEnabled();
    loadWifiConfig(f.core, config, nullptr);   // must return, not dereference

    TEST_ASSERT_TRUE(true);
}

void test_wifi_is_not_loaded_when_storage_is_disabled(void) {
    Fixture f;
    f.storage->putString("wifi_ssid", "HomeNet");

    SystemConfig config;                        // enableStorage false
    loadWifiConfig(f.core, config, f.wifi);

    TEST_ASSERT_FALSE(f.wifi->getConfig().ssid == String("HomeNet"));
}

// ============================================================================
// registerStorageKeys and loadAllConfigs
// ============================================================================

void test_registering_keys_makes_stored_values_dumpable(void) {
    Fixture f;
    f.storage->putString("device_name", "Cellar");

    SystemConfig config = storageEnabled();
    registerStorageKeys(f.core, config);

    // dumpContents() walks the registered keys and prints the ones that exist —
    // the `storage` console command shows nothing at all if registration was
    // skipped, and shows only what has actually been written.
    String dump = f.storage->dumpContents();
    TEST_ASSERT_NOT_NULL(strstr(dump.c_str(), "device_name = \"Cellar\""));
    TEST_ASSERT_NULL(strstr(dump.c_str(), "wifi_ssid"));   // registered, never written

    f.storage->putString("wifi_ssid", "HomeNet");
    TEST_ASSERT_NOT_NULL(strstr(f.storage->dumpContents().c_str(), "wifi_ssid = \"HomeNet\""));
}

void test_passwords_are_masked_in_the_dump(void) {
    Fixture f;
    f.storage->putString("wifi_pass", "hunter2");

    SystemConfig config = storageEnabled();
    registerStorageKeys(f.core, config);

    // The dump goes out over telnet on the `storage` command.
    String dump = f.storage->dumpContents();
    TEST_ASSERT_NULL(strstr(dump.c_str(), "hunter2"));
    TEST_ASSERT_NOT_NULL(strstr(dump.c_str(), "wifi_pass = ****"));
}

void test_registering_keys_is_skipped_when_storage_is_disabled(void) {
    Fixture f;
    SystemConfig config;                        // enableStorage false
    registerStorageKeys(f.core, config);

    TEST_ASSERT_NULL(strstr(f.storage->dumpContents().c_str(), "wifi_ssid"));
}

void test_load_all_configs_restores_name_and_wifi_together(void) {
    Fixture f;
    f.storage->putString("device_name", "Cellar");
    f.storage->putString("wifi_ssid", "HomeNet");

    SystemConfig config = storageEnabled();
    loadAllConfigs(f.core, config, f.wifi);

    TEST_ASSERT_EQUAL_STRING("Cellar", config.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("HomeNet", f.wifi->getConfig().ssid.c_str());
}

void test_load_all_configs_without_a_storage_component(void) {
    // The path a user installing System alone takes. Every loader must fall
    // through rather than fault.
    Fixture f(false);

    SystemConfig config = storageEnabled();
    loadAllConfigs(f.core, config, f.wifi);

    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.deviceName.c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_device_name_is_restored);
    RUN_TEST(test_an_absent_device_name_leaves_the_default);
    RUN_TEST(test_an_empty_stored_device_name_is_ignored);
    RUN_TEST(test_device_name_is_not_loaded_when_storage_is_disabled);
    RUN_TEST(test_device_name_without_a_storage_component);
    RUN_TEST(test_device_name_reaches_systeminfo);

    RUN_TEST(test_wifi_credentials_are_restored);
    RUN_TEST(test_ap_settings_are_restored);
    RUN_TEST(test_an_absent_ap_ssid_keeps_the_one_the_component_already_has);
    RUN_TEST(test_an_explicit_ssid_in_code_beats_the_stored_one);
    RUN_TEST(test_an_empty_stored_ssid_leaves_the_component_alone);
    RUN_TEST(test_a_null_wifi_component_is_survived);
    RUN_TEST(test_wifi_is_not_loaded_when_storage_is_disabled);

    RUN_TEST(test_registering_keys_makes_stored_values_dumpable);
    RUN_TEST(test_passwords_are_masked_in_the_dump);
    RUN_TEST(test_registering_keys_is_skipped_when_storage_is_disabled);
    RUN_TEST(test_load_all_configs_restores_name_and_wifi_together);
    RUN_TEST(test_load_all_configs_without_a_storage_component);

    return UNITY_END();
}
