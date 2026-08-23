/**
 * @file test_system_config.cpp
 * @brief SystemConfig and the SystemState enum.
 *
 * The presets are the documented entry point — `SystemConfig::standard()` is
 * what the README tells a new user to write. Nothing checked that they enabled
 * what they claim, or that they build on each other the way the comments say.
 */

#include <DomoticsCore/SystemConfig.h>
#include <unity.h>

using namespace DomoticsCore;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// systemStateToString
// ============================================================================

void test_every_state_has_a_name(void) {
    TEST_ASSERT_EQUAL_STRING("BOOTING", systemStateToString(SystemState::BOOTING));
    TEST_ASSERT_EQUAL_STRING("WIFI_CONNECTING", systemStateToString(SystemState::WIFI_CONNECTING));
    TEST_ASSERT_EQUAL_STRING("WIFI_CONNECTED", systemStateToString(SystemState::WIFI_CONNECTED));
    TEST_ASSERT_EQUAL_STRING("SERVICES_STARTING", systemStateToString(SystemState::SERVICES_STARTING));
    TEST_ASSERT_EQUAL_STRING("READY", systemStateToString(SystemState::READY));
    TEST_ASSERT_EQUAL_STRING("ERROR", systemStateToString(SystemState::ERROR));
    TEST_ASSERT_EQUAL_STRING("OTA_UPDATE", systemStateToString(SystemState::OTA_UPDATE));
    TEST_ASSERT_EQUAL_STRING("SHUTDOWN", systemStateToString(SystemState::SHUTDOWN));
}

void test_an_out_of_range_state_is_named_rather_than_crashing(void) {
    // The default arm exists; this is what reaches it. A log line built from a
    // corrupted state must not dereference nothing.
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", systemStateToString(static_cast<SystemState>(99)));
}

// ============================================================================
// Defaults
// ============================================================================

void test_default_identity(void) {
    SystemConfig config;
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("DomoticsCore", config.manufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("1.0.0", config.firmwareVersion.c_str());
    // Empty on purpose: begin() fills it from the chip.
    TEST_ASSERT_TRUE(config.model.isEmpty());
}

void test_default_enables_only_led_and_console(void) {
    SystemConfig config;
    TEST_ASSERT_TRUE(config.enableLED);
    TEST_ASSERT_TRUE(config.enableConsole);
    TEST_ASSERT_TRUE(config.wifiAutoConfig);

    TEST_ASSERT_FALSE(config.enableWebUI);
    TEST_ASSERT_FALSE(config.enableNTP);
    TEST_ASSERT_FALSE(config.enableStorage);
    TEST_ASSERT_FALSE(config.enableMQTT);
    TEST_ASSERT_FALSE(config.enableHomeAssistant);
    TEST_ASSERT_FALSE(config.enableOTA);
    TEST_ASSERT_FALSE(config.enableSystemInfo);
}

void test_default_ports(void) {
    SystemConfig config;
    TEST_ASSERT_EQUAL_UINT16(23, config.consolePort);
    TEST_ASSERT_EQUAL_UINT16(80, config.webUIPort);
    TEST_ASSERT_EQUAL_UINT16(1883, config.mqttPort);
    TEST_ASSERT_EQUAL_UINT8(3, config.consoleMaxClients);
    TEST_ASSERT_EQUAL_UINT8(2, config.ledPin);
    TEST_ASSERT_TRUE(config.ledActiveHigh);
}

void test_default_storage_namespace_and_ntp_server(void) {
    SystemConfig config;
    TEST_ASSERT_EQUAL_STRING("domotics", config.storageNamespace.c_str());
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", config.ntpServer.c_str());
    TEST_ASSERT_EQUAL_STRING("UTC", config.ntpTimezone.c_str());
}

// ============================================================================
// Presets
// ============================================================================

void test_minimal_is_led_console_wifi(void) {
    SystemConfig config = SystemConfig::minimal();
    TEST_ASSERT_TRUE(config.enableLED);
    TEST_ASSERT_TRUE(config.enableConsole);
    TEST_ASSERT_TRUE(config.wifiAutoConfig);

    TEST_ASSERT_FALSE(config.enableWebUI);
    TEST_ASSERT_FALSE(config.enableNTP);
    TEST_ASSERT_FALSE(config.enableStorage);
    TEST_ASSERT_FALSE(config.enableMQTT);
    TEST_ASSERT_FALSE(config.enableHomeAssistant);
    TEST_ASSERT_FALSE(config.enableOTA);
    TEST_ASSERT_FALSE(config.enableSystemInfo);
}

void test_standard_adds_webui_ntp_storage_to_minimal(void) {
    SystemConfig config = SystemConfig::standard();
    TEST_ASSERT_TRUE(config.enableLED);
    TEST_ASSERT_TRUE(config.enableConsole);
    TEST_ASSERT_TRUE(config.enableWebUI);
    TEST_ASSERT_TRUE(config.enableNTP);
    TEST_ASSERT_TRUE(config.enableStorage);

    // Everything needing an external service stays off.
    TEST_ASSERT_FALSE(config.enableMQTT);
    TEST_ASSERT_FALSE(config.enableHomeAssistant);
    TEST_ASSERT_FALSE(config.enableOTA);
    TEST_ASSERT_FALSE(config.enableSystemInfo);
}

void test_fullstack_enables_everything(void) {
    SystemConfig config = SystemConfig::fullStack();
    TEST_ASSERT_TRUE(config.enableLED);
    TEST_ASSERT_TRUE(config.enableConsole);
    TEST_ASSERT_TRUE(config.enableWebUI);
    TEST_ASSERT_TRUE(config.enableNTP);
    TEST_ASSERT_TRUE(config.enableStorage);
    TEST_ASSERT_TRUE(config.enableMQTT);
    TEST_ASSERT_TRUE(config.enableHomeAssistant);
    TEST_ASSERT_TRUE(config.enableOTA);
    TEST_ASSERT_TRUE(config.enableSystemInfo);
}

void test_presets_are_independent_copies(void) {
    // They are built by value from one another; a mutation must not travel.
    SystemConfig a = SystemConfig::standard();
    SystemConfig b = SystemConfig::standard();
    a.deviceName = "Mutated";
    a.enableMQTT = true;

    TEST_ASSERT_EQUAL_STRING("DomoticsCore", b.deviceName.c_str());
    TEST_ASSERT_FALSE(b.enableMQTT);
}

void test_presets_leave_credentials_empty(void) {
    // No preset may invent a broker, an SSID or a password.
    SystemConfig config = SystemConfig::fullStack();
    TEST_ASSERT_TRUE(config.wifiSSID.isEmpty());
    TEST_ASSERT_TRUE(config.wifiPassword.isEmpty());
    TEST_ASSERT_TRUE(config.mqttBroker.isEmpty());
    TEST_ASSERT_TRUE(config.mqttUser.isEmpty());
    TEST_ASSERT_TRUE(config.mqttPassword.isEmpty());
    TEST_ASSERT_TRUE(config.mqttClientId.isEmpty());
}

void test_default_log_level(void) {
    TEST_ASSERT_EQUAL(LOG_LEVEL_INFO, SystemConfig().defaultLogLevel);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_every_state_has_a_name);
    RUN_TEST(test_an_out_of_range_state_is_named_rather_than_crashing);

    RUN_TEST(test_default_identity);
    RUN_TEST(test_default_enables_only_led_and_console);
    RUN_TEST(test_default_ports);
    RUN_TEST(test_default_storage_namespace_and_ntp_server);

    RUN_TEST(test_minimal_is_led_console_wifi);
    RUN_TEST(test_standard_adds_webui_ntp_storage_to_minimal);
    RUN_TEST(test_fullstack_enables_everything);
    RUN_TEST(test_presets_are_independent_copies);
    RUN_TEST(test_presets_leave_credentials_empty);
    RUN_TEST(test_default_log_level);

    return UNITY_END();
}
