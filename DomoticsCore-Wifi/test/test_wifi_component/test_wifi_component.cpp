// Unit tests for WifiComponent
// Tests: events, config, modes, lifecycle, INetworkProvider interface, edge cases
// Includes HeapTracker integration for memory leak detection

#include <unity.h>
#include <DomoticsCore/Core.h>
#include <DomoticsCore/Wifi.h>
#include <DomoticsCore/WifiEvents.h>
#include <DomoticsCore/Testing/HeapTracker.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;
using namespace DomoticsCore::Testing;

// Test state
static Core* testCore = nullptr;
static bool staConnectedReceived = false;
static bool staConnectedValue = false;
static bool apEnabledReceived = false;
static bool apEnabledValue = false;

void setUp(void) {
    testCore = new Core();
    staConnectedReceived = false;
    staConnectedValue = false;
    apEnabledReceived = false;
    apEnabledValue = false;
}

void tearDown(void) {
    if (testCore) {
        testCore->shutdown();
        delete testCore;
        testCore = nullptr;
    }
    // The scripted scan table is process-global. Leaving one test's networks
    // behind would make the next one pass or fail for the previous test's data.
    HAL::WiFiImpl::resetScanForTest();
}

// ============================================================================
// WifiEvents Constants Tests
// ============================================================================

void test_wifi_events_constants_defined(void) {
    // Verify event constants are properly defined
    TEST_ASSERT_NOT_NULL(WifiEvents::EVENT_STA_CONNECTED);
    TEST_ASSERT_NOT_NULL(WifiEvents::EVENT_AP_ENABLED);
    TEST_ASSERT_NOT_NULL(WifiEvents::EVENT_NETWORK_READY);

    // Verify they have expected values
    TEST_ASSERT_EQUAL_STRING("wifi/sta/connected", WifiEvents::EVENT_STA_CONNECTED);
    TEST_ASSERT_EQUAL_STRING("wifi/ap/enabled", WifiEvents::EVENT_AP_ENABLED);
    TEST_ASSERT_EQUAL_STRING("network/ready", WifiEvents::EVENT_NETWORK_READY);
}

// ============================================================================
// WifiComponent Creation Tests
// ============================================================================

void test_wifi_component_creation_default(void) {
    auto wifi = std::make_unique<WifiComponent>();

    TEST_ASSERT_EQUAL_STRING("Wifi", wifi->metadata.name);
    // The exact version is enforced by tools/check_versions.py, which compares
    // library.json against every metadata.version in the component's sources.
    // Repeating the literal here only makes the test stale at the next release.
    TEST_ASSERT_NOT_NULL(wifi->metadata.version);
    TEST_ASSERT_NOT_EQUAL('\0', wifi->metadata.version[0]);
}

void test_wifi_component_creation_with_credentials(void) {
    auto wifi = std::make_unique<WifiComponent>("TestSSID", "TestPassword");

    TEST_ASSERT_EQUAL_STRING("Wifi", wifi->metadata.name);
    TEST_ASSERT_EQUAL_STRING("TestSSID", wifi->getConfiguredSSID().c_str());
}

// ============================================================================
// WifiConfig Tests
// ============================================================================

void test_wifi_config_defaults(void) {
    WifiConfig config;

    TEST_ASSERT_TRUE(config.ssid.isEmpty());
    TEST_ASSERT_TRUE(config.password.isEmpty());
    TEST_ASSERT_TRUE(config.autoConnect);
    TEST_ASSERT_FALSE(config.enableAP);
    TEST_ASSERT_TRUE(config.apSSID.isEmpty());
    TEST_ASSERT_TRUE(config.apPassword.isEmpty());
    TEST_ASSERT_EQUAL_UINT32(5000, config.reconnectInterval);
    TEST_ASSERT_EQUAL_UINT32(15000, config.connectionTimeout);
}

void test_wifi_config_get_set(void) {
    auto wifi = std::make_unique<WifiComponent>();

    WifiConfig config;
    config.ssid = "MyNetwork";
    config.password = "MyPassword";
    config.autoConnect = true;
    config.enableAP = true;
    config.apSSID = "MyAP";
    config.apPassword = "APPassword";

    wifi->setConfig(config);

    WifiConfig retrieved = wifi->getConfig();
    TEST_ASSERT_EQUAL_STRING("MyNetwork", retrieved.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("MyPassword", retrieved.password.c_str());
    TEST_ASSERT_TRUE(retrieved.autoConnect);
    TEST_ASSERT_TRUE(retrieved.enableAP);
    TEST_ASSERT_EQUAL_STRING("MyAP", retrieved.apSSID.c_str());
    TEST_ASSERT_EQUAL_STRING("APPassword", retrieved.apPassword.c_str());
}

// ============================================================================
// WiFi Component State Tests
// ============================================================================

void test_wifi_component_initial_state(void) {
    auto wifi = std::make_unique<WifiComponent>();

    // On stub platform, WiFi is never connected
    TEST_ASSERT_FALSE(wifi->isSTAConnected());
    TEST_ASSERT_FALSE(wifi->isConnectionInProgress());
}

void test_wifi_component_ap_only_mode(void) {
    // Empty SSID = AP-only mode
    auto wifi = std::make_unique<WifiComponent>("", "");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // On stub, AP operations return false but state flags should be set
    // The component should initialize successfully in AP mode
    TEST_ASSERT_EQUAL(ComponentStatus::Success, wifiPtr->getLastStatus());
}

// ============================================================================
// WiFi Event Emission Tests
// ============================================================================

void test_wifi_ap_enabled_event_on_enable(void) {
    // Subscribe to AP enabled event
    testCore->getEventBus().subscribe(WifiEvents::EVENT_AP_ENABLED, [](const void* payload) {
        apEnabledReceived = true;
        if (payload) {
            apEnabledValue = *static_cast<const bool*>(payload);
        }
    });

    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();
    testCore->loop();

    // Enable AP mode - should emit EVENT_AP_ENABLED
    wifiPtr->enableAP("TestAP", "password123", true);
    testCore->loop();

    // On stub platform, startAP returns false, so event may not be emitted
    // But the component should not crash
    TEST_ASSERT_TRUE(wifiPtr->isAPEnabled()); // Internal flag should be set
}

void test_wifi_credentials_update(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // Update credentials
    wifiPtr->setCredentials("NewSSID", "NewPassword", false);

    TEST_ASSERT_EQUAL_STRING("NewSSID", wifiPtr->getConfiguredSSID().c_str());
}

void test_wifi_network_info_json(void) {
    auto wifi = std::make_unique<WifiComponent>("TestNet", "TestPass");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    String info = wifiPtr->getNetworkInfo();

    // Should return valid JSON
    TEST_ASSERT_TRUE(info.length() > 0);
    TEST_ASSERT_TRUE(info.indexOf("type") >= 0);
    TEST_ASSERT_TRUE(info.indexOf("Wifi") >= 0);
}

void test_wifi_network_type(void) {
    auto wifi = std::make_unique<WifiComponent>();

    TEST_ASSERT_EQUAL_STRING("Wifi", wifi->getNetworkType().c_str());
}

void test_wifi_disconnect_reconnect(void) {
    auto wifi = std::make_unique<WifiComponent>("TestSSID", "TestPass");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // Disconnect should not crash
    wifiPtr->disconnect();

    // Reconnect should not crash
    wifiPtr->reconnect();

    // Component should still be valid
    TEST_ASSERT_EQUAL_STRING("Wifi", wifiPtr->metadata.name);
}

void test_wifi_scan_async(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // Start async scan - should not crash on stub
    wifiPtr->startScanAsync();

    // Get scan summary - on stub it will show "Scanning..."
    String summary = wifiPtr->getLastScanSummary();
    TEST_ASSERT_TRUE(summary.length() > 0);
}

// ============================================================================
// INetworkProvider Interface Tests
// ============================================================================

void test_wifi_inetworkprovider_isconnected(void) {
    auto wifi = std::make_unique<WifiComponent>();

    // On stub, isConnected should return false (no STA) but may return true if AP enabled
    TEST_ASSERT_FALSE(wifi->isConnected()); // Initially no connectivity
}

void test_wifi_inetworkprovider_getlocalip(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    String ip = wifiPtr->getLocalIP();
    // On stub, returns "0.0.0.0"
    TEST_ASSERT_TRUE(ip.length() > 0);
}

void test_wifi_inetworkprovider_getconnectionstatus(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    String status = wifiPtr->getConnectionStatus();
    TEST_ASSERT_TRUE(status.length() > 0);
}

// ============================================================================
// Mode Detection Tests
// ============================================================================

void test_wifi_mode_detection_initial(void) {
    auto wifi = std::make_unique<WifiComponent>("TestSSID", "TestPass");

    // Before begin, should not be in AP mode
    TEST_ASSERT_FALSE(wifi->isAPMode());
    TEST_ASSERT_FALSE(wifi->isSTAAPMode());
}

void test_wifi_has_connectivity(void) {
    auto wifi = std::make_unique<WifiComponent>();

    // On stub, no connectivity initially
    TEST_ASSERT_FALSE(wifi->hasConnectivity());
}

void test_wifi_is_wifi_enabled(void) {
    auto wifi = std::make_unique<WifiComponent>();

    // WiFi should be enabled by default
    TEST_ASSERT_TRUE(wifi->isWifiEnabled());
}

void test_wifi_is_ap_enabled_initial(void) {
    auto wifi = std::make_unique<WifiComponent>();

    // AP should be disabled by default
    TEST_ASSERT_FALSE(wifi->isAPEnabled());
}

// ============================================================================
// Mode Switching Tests
// ============================================================================

void test_wifi_enable_disable_wifi(void) {
    auto wifi = std::make_unique<WifiComponent>("TestSSID", "TestPass");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // Disable WiFi
    wifiPtr->enableWifi(false);
    TEST_ASSERT_FALSE(wifiPtr->isWifiEnabled());

    // Re-enable WiFi
    wifiPtr->enableWifi(true);
    TEST_ASSERT_TRUE(wifiPtr->isWifiEnabled());
}

void test_wifi_enable_ap_with_ssid(void) {
    auto wifi = std::make_unique<WifiComponent>("TestSSID", "TestPass");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // Enable AP
    wifiPtr->enableAP("MyAccessPoint", "appassword", true);

    TEST_ASSERT_TRUE(wifiPtr->isAPEnabled());
    TEST_ASSERT_EQUAL_STRING("MyAccessPoint", wifiPtr->getAPSSID().c_str());
}

void test_wifi_disable_ap(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // Enable then disable AP
    wifiPtr->enableAP("TestAP", "", true);
    TEST_ASSERT_TRUE(wifiPtr->isAPEnabled());

    wifiPtr->disableAP();
    TEST_ASSERT_FALSE(wifiPtr->isAPEnabled());
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

void test_wifi_full_lifecycle(void) {
    auto wifi = std::make_unique<WifiComponent>("TestSSID", "TestPass");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));

    // Begin
    testCore->begin();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, wifiPtr->getLastStatus());

    // Multiple loops should not crash
    for (int i = 0; i < 10; i++) {
        testCore->loop();
    }

    // Shutdown
    testCore->shutdown();
}

void test_wifi_shutdown_returns_success(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    ComponentStatus status = wifiPtr->shutdown();
    TEST_ASSERT_EQUAL(ComponentStatus::Success, status);
}

void test_wifi_no_dependencies(void) {
    auto wifi = std::make_unique<WifiComponent>();

    auto deps = wifi->getDependencies();
    TEST_ASSERT_EQUAL(0, deps.size());
}

// ============================================================================
// Status Methods Tests
// ============================================================================

void test_wifi_get_detailed_status(void) {
    auto wifi = std::make_unique<WifiComponent>("TestSSID", "TestPass");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    String status = wifiPtr->getDetailedStatus();
    TEST_ASSERT_TRUE(status.length() > 0);
    TEST_ASSERT_TRUE(status.indexOf("Wifi Status") >= 0);
}

void test_wifi_get_ap_info_json(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    String info = wifiPtr->getAPInfo();
    TEST_ASSERT_TRUE(info.length() > 0);
    TEST_ASSERT_TRUE(info.indexOf("active") >= 0);
}

void test_wifi_get_mac_address(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    String mac = wifiPtr->getMacAddress();
    // On stub, returns "00:00:00:00:00:00"
    TEST_ASSERT_TRUE(mac.length() > 0);
    TEST_ASSERT_TRUE(mac.indexOf(":") >= 0);
}

void test_wifi_get_rssi(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    int32_t rssi = wifiPtr->getRSSI();
    // On stub, returns 0
    TEST_ASSERT_EQUAL_INT32(0, rssi);
}

void test_wifi_get_ssid_configured(void) {
    auto wifi = std::make_unique<WifiComponent>("ConfiguredSSID", "pass");

    TEST_ASSERT_EQUAL_STRING("ConfiguredSSID", wifi->getConfiguredSSID().c_str());
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

void test_wifi_empty_ssid_starts_ap_mode(void) {
    auto wifi = std::make_unique<WifiComponent>("", "");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // With empty SSID, component should initialize successfully in AP mode
    TEST_ASSERT_EQUAL(ComponentStatus::Success, wifiPtr->getLastStatus());
}

void test_wifi_config_multiple_updates(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // Multiple config updates should not crash
    for (int i = 0; i < 5; i++) {
        WifiConfig config;
        config.ssid = "Network" + String(i);
        config.password = "Pass" + String(i);
        wifiPtr->setConfig(config);
    }

    WifiConfig final = wifiPtr->getConfig();
    TEST_ASSERT_EQUAL_STRING("Network4", final.ssid.c_str());
}

void test_wifi_credentials_with_reconnect(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    // setCredentials with reconnectNow=true should trigger connection attempt
    wifiPtr->setCredentials("NewNetwork", "NewPass", true);

    // On stub, connection will fail but component should be stable
    TEST_ASSERT_EQUAL_STRING("NewNetwork", wifiPtr->getConfiguredSSID().c_str());
    TEST_ASSERT_TRUE(wifiPtr->isConnectionInProgress());
}

void test_wifi_scan_networks_sync(void) {
    auto wifi = std::make_unique<WifiComponent>();
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    std::vector<String> networks;
    bool result = wifiPtr->scanNetworks(networks);

    // Nothing scripted the scan table, so the stub reports no networks — the
    // behaviour this stub had unconditionally before the table existed.
    TEST_ASSERT_TRUE(result); // Should not fail
    TEST_ASSERT_EQUAL(0, networks.size());
}

// ============================================================================
// Scan summary formatting (MEM-2)
//
// These exist because the two loops that build the scan text had never run on
// any platform CI can execute: the stub reported zero networks, and neither
// loop body is entered when the scan finds nothing. A rewrite could change the
// separator, truncate an entry or drop the ten-entry cap and every required
// check would still have been green. The device suite that measures the same
// loops needs a radio in range; this needs nothing.
//
// The text asserted here is what the concatenation these loops replaced
// produced: "<ssid> (<rssi> dBm)", joined by ", ".
// ============================================================================

// A component whose loop() reaches the async scan poll and does nothing else.
//
// The SSID must be non-empty: WifiComponent::loop() returns early when it is
// (`if (ssid.isEmpty()) return;`), forty lines before the poll — so a fixture
// with the default empty config never reaches the loop under test at all.
// autoConnect=false keeps shouldConnect false, so no connection is attempted
// and no timer branch fires.
static void makeIdle(WifiComponent& wifi) {
    WifiConfig cfg;
    cfg.ssid = "scan-fixture";  // non-empty, never connected to
    cfg.autoConnect = false;
    cfg.enableAP = false;
    wifi.setConfig(cfg);
}

void test_wifi_scan_entry_format(void) {
    HAL::WiFiImpl::setScannedNetworksForTest({
        {String("HomeNet"), -42},
        {String("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"), -100},  // 32 chars, the SSID maximum
        {String(""), -70},                                    // a hidden AP reports no SSID
    });

    WifiComponent wifi;
    std::vector<String> networks;

    TEST_ASSERT_TRUE(wifi.scanNetworks(networks));
    TEST_ASSERT_EQUAL(3, networks.size());

    TEST_ASSERT_EQUAL_STRING("HomeNet (-42 dBm)", networks[0].c_str());
    // The whole 32-character SSID survives the stack buffer the rewrite formats
    // into. Shrink that buffer and this is the assertion that goes red.
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345 (-100 dBm)", networks[1].c_str());
    TEST_ASSERT_EQUAL_STRING(" (-70 dBm)", networks[2].c_str());
}

void test_wifi_scan_failure_returns_false(void) {
    // WIFI_SCAN_FAILED is -2, not -1. The guard used to test `n == -1` only and
    // then reserve(static_cast<size_t>(n)) — 4 GB on a 40 KB heap.
    HAL::WiFiImpl::setScanFailedForTest(-2);

    WifiComponent wifi;
    std::vector<String> networks;

    TEST_ASSERT_FALSE(wifi.scanNetworks(networks));
    TEST_ASSERT_EQUAL(0, networks.size());
}

void test_wifi_scan_failure_minus_one_returns_false(void) {
    HAL::WiFiImpl::setScanFailedForTest(-1);

    WifiComponent wifi;
    std::vector<String> networks;

    TEST_ASSERT_FALSE(wifi.scanNetworks(networks));
}

void test_wifi_async_summary_format(void) {
    HAL::WiFiImpl::setScannedNetworksForTest({
        {String("HomeNet"), -42},
        {String("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"), -100},
        {String(""), -70},
    });

    WifiComponent wifi;
    makeIdle(wifi);

    wifi.startScanAsync();
    TEST_ASSERT_EQUAL_STRING("Scanning...", wifi.getLastScanSummary().c_str());

    wifi.loop();

    TEST_ASSERT_EQUAL_STRING(
        "HomeNet (-42 dBm), ABCDEFGHIJKLMNOPQRSTUVWXYZ012345 (-100 dBm),  (-70 dBm)",
        wifi.getLastScanSummary().c_str());
}

void test_wifi_async_summary_is_the_join_of_the_sync_entries(void) {
    // The two loops are separate code building the same text, and only one of
    // them can be measured on a board — the synchronous one, which logs each
    // entry and so can be sampled from inside its last iteration. The device
    // suite's evidence therefore only carries over to the async loop for as
    // long as the two produce identical entries. This is what says they do.
    HAL::WiFiImpl::setScannedNetworksForTest({
        {String("HomeNet"), -42},
        {String("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"), -100},
        {String(""), -70},
    });

    WifiComponent syncWifi;
    std::vector<String> networks;
    TEST_ASSERT_TRUE(syncWifi.scanNetworks(networks));

    String expected;
    for (size_t i = 0; i < networks.size(); ++i) {
        if (i) expected += ", ";
        expected += networks[i];
    }

    WifiComponent asyncWifi;
    makeIdle(asyncWifi);
    asyncWifi.startScanAsync();
    asyncWifi.loop();

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), asyncWifi.getLastScanSummary().c_str());
}

void test_wifi_async_summary_caps_at_ten(void) {
    std::vector<HAL::WiFiImpl::StubNetwork> many;
    for (int i = 0; i < 12; ++i) {
        many.push_back({String("net") + String(i), -50 - i});
    }
    HAL::WiFiImpl::setScannedNetworksForTest(many);

    WifiComponent wifi;
    makeIdle(wifi);

    wifi.startScanAsync();
    wifi.loop();

    String summary = wifi.getLastScanSummary();

    // Ten entries, nine separators.
    int separators = 0;
    for (int at = summary.indexOf(", "); at >= 0; at = summary.indexOf(", ", at + 2)) {
        separators++;
    }
    TEST_ASSERT_EQUAL(9, separators);

    TEST_ASSERT_TRUE(summary.startsWith("net0 (-50 dBm), "));
    TEST_ASSERT_TRUE(summary.endsWith("net9 (-59 dBm)"));
    // The eleventh and twelfth must not appear at all.
    TEST_ASSERT_TRUE(summary.indexOf("net10") < 0);
    TEST_ASSERT_TRUE(summary.indexOf("net11") < 0);
}

void test_wifi_async_summary_empty_scan(void) {
    // Zero networks: the loop is not entered and the summary is empty. This is
    // also the state a CI runner is always in, which is why the on-device suite
    // refuses to pass on it.
    WifiComponent wifi;
    makeIdle(wifi);

    wifi.startScanAsync();
    wifi.loop();

    TEST_ASSERT_EQUAL_STRING("", wifi.getLastScanSummary().c_str());
}

void test_wifi_async_summary_scan_failed(void) {
    HAL::WiFiImpl::setScanFailedForTest(-2);  // WIFI_SCAN_FAILED

    WifiComponent wifi;
    makeIdle(wifi);

    wifi.startScanAsync();
    wifi.loop();

    TEST_ASSERT_EQUAL_STRING("Scan failed", wifi.getLastScanSummary().c_str());
}

void test_wifi_network_info_contains_all_fields(void) {
    auto wifi = std::make_unique<WifiComponent>("TestNet", "TestPass");
    WifiComponent* wifiPtr = wifi.get();

    testCore->addComponent(std::move(wifi));
    testCore->begin();

    String info = wifiPtr->getNetworkInfo();

    // Verify all expected fields are present
    TEST_ASSERT_TRUE(info.indexOf("\"type\"") >= 0);
    TEST_ASSERT_TRUE(info.indexOf("\"sta_connected\"") >= 0);
    TEST_ASSERT_TRUE(info.indexOf("\"ap_enabled\"") >= 0);
    TEST_ASSERT_TRUE(info.indexOf("\"ap_mode\"") >= 0);
}

// ============================================================================
// Memory Leak Detection Tests (HeapTracker)
// ============================================================================

void test_wifi_memory_stability_lifecycle() {
    HeapTracker tracker;
    
    tracker.checkpoint("before");
    
    // Create and destroy WiFi components multiple times
    for (int i = 0; i < 5; i++) {
        WifiComponent wifi;
        WifiConfig config;
        config.ssid = "TestNetwork";
        config.password = "TestPassword";
        wifi.setConfig(config);
        // Component destroyed at end of scope
    }
    
    tracker.checkpoint("after");
    
    MemoryTestResult result = tracker.assertStable("before", "after", 512);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

void test_wifi_memory_stability_config_changes() {
    HeapTracker tracker;
    WifiComponent wifi;
    
    tracker.checkpoint("baseline");
    
    // Perform many config changes
    for (int i = 0; i < 20; i++) {
        WifiConfig config;
        config.ssid = "Network" + String(i);
        config.password = "Pass" + String(i);
        wifi.setConfig(config);
    }
    
    MemoryTestResult result = tracker.assertNoGrowth("baseline", 256);
    TEST_ASSERT_TRUE_MESSAGE(result.passed, result.message.c_str());
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Event constants tests
    RUN_TEST(test_wifi_events_constants_defined);

    // Component creation tests
    RUN_TEST(test_wifi_component_creation_default);
    RUN_TEST(test_wifi_component_creation_with_credentials);

    // Config tests
    RUN_TEST(test_wifi_config_defaults);
    RUN_TEST(test_wifi_config_get_set);

    // State tests
    RUN_TEST(test_wifi_component_initial_state);
    RUN_TEST(test_wifi_component_ap_only_mode);

    // Event and behavior tests
    RUN_TEST(test_wifi_ap_enabled_event_on_enable);
    RUN_TEST(test_wifi_credentials_update);
    RUN_TEST(test_wifi_network_info_json);
    RUN_TEST(test_wifi_network_type);
    RUN_TEST(test_wifi_disconnect_reconnect);
    RUN_TEST(test_wifi_scan_async);

    // INetworkProvider interface tests
    RUN_TEST(test_wifi_inetworkprovider_isconnected);
    RUN_TEST(test_wifi_inetworkprovider_getlocalip);
    RUN_TEST(test_wifi_inetworkprovider_getconnectionstatus);

    // Mode detection tests
    RUN_TEST(test_wifi_mode_detection_initial);
    RUN_TEST(test_wifi_has_connectivity);
    RUN_TEST(test_wifi_is_wifi_enabled);
    RUN_TEST(test_wifi_is_ap_enabled_initial);

    // Mode switching tests
    RUN_TEST(test_wifi_enable_disable_wifi);
    RUN_TEST(test_wifi_enable_ap_with_ssid);
    RUN_TEST(test_wifi_disable_ap);

    // Lifecycle tests
    RUN_TEST(test_wifi_full_lifecycle);
    RUN_TEST(test_wifi_shutdown_returns_success);
    RUN_TEST(test_wifi_no_dependencies);

    // Status methods tests
    RUN_TEST(test_wifi_get_detailed_status);
    RUN_TEST(test_wifi_get_ap_info_json);
    RUN_TEST(test_wifi_get_mac_address);
    RUN_TEST(test_wifi_get_rssi);
    RUN_TEST(test_wifi_get_ssid_configured);

    // Edge cases tests
    RUN_TEST(test_wifi_empty_ssid_starts_ap_mode);
    RUN_TEST(test_wifi_config_multiple_updates);
    RUN_TEST(test_wifi_credentials_with_reconnect);
    RUN_TEST(test_wifi_scan_networks_sync);
    RUN_TEST(test_wifi_network_info_contains_all_fields);

    // Scan summary formatting (MEM-2) — the loops the stub could not reach
    RUN_TEST(test_wifi_scan_entry_format);
    RUN_TEST(test_wifi_scan_failure_returns_false);
    RUN_TEST(test_wifi_scan_failure_minus_one_returns_false);
    RUN_TEST(test_wifi_async_summary_format);
    RUN_TEST(test_wifi_async_summary_is_the_join_of_the_sync_entries);
    RUN_TEST(test_wifi_async_summary_caps_at_ten);
    RUN_TEST(test_wifi_async_summary_empty_scan);
    RUN_TEST(test_wifi_async_summary_scan_failed);

    // Memory leak detection tests (HeapTracker)
    RUN_TEST(test_wifi_memory_stability_lifecycle);
    RUN_TEST(test_wifi_memory_stability_config_changes);

    return UNITY_END();
}
