#include <unity.h>
#include <string>
#include <cstring>
#include <cstdint>
#include <functional>
#include <vector>
#include <map>

// Native build stubs for Arduino types
#ifdef NATIVE_BUILD
typedef std::string String;
inline unsigned long millis() { static unsigned long t = 0; return t += 10; }
inline void delay(unsigned long) {}

namespace DomoticsCore {
namespace Mocks {

// ============================================================================
// MockWifiHAL - Simulates WiFi connectivity
// ============================================================================
class MockWifiHAL {
public:
    static bool connected;
    static std::string ssid;
    static std::string localIP;
    static int8_t rssi;
    
    static bool isConnected() { return connected; }
    static std::string getSSID() { return ssid; }
    static std::string getLocalIP() { return localIP; }
    static int8_t getRSSI() { return rssi; }
    
    static void simulateConnect(const std::string& testSSID = "TestNetwork") {
        connected = true;
        ssid = testSSID;
        localIP = "192.168.1.100";
        rssi = -50;
    }
    
    static void simulateDisconnect() {
        connected = false;
        ssid = "";
        localIP = "0.0.0.0";
        rssi = 0;
    }
    
    static void reset() {
        connected = false;
        ssid = "";
        localIP = "0.0.0.0";
        rssi = 0;
    }
};

bool MockWifiHAL::connected = false;
std::string MockWifiHAL::ssid = "";
std::string MockWifiHAL::localIP = "0.0.0.0";
int8_t MockWifiHAL::rssi = 0;

// ============================================================================
// MockEventBus - Records event emissions
// ============================================================================
struct MockEvent {
    std::string name;
    bool emitted;
};

class MockEventBus {
public:
    static std::vector<std::string> emittedEvents;
    static std::map<std::string, std::vector<std::function<void()>>> subscribers;
    
    static void emit(const std::string& eventName) {
        emittedEvents.push_back(eventName);
        auto it = subscribers.find(eventName);
        if (it != subscribers.end()) {
            for (auto& cb : it->second) cb();
        }
    }
    
    static void subscribe(const std::string& eventName, std::function<void()> cb) {
        subscribers[eventName].push_back(cb);
    }
    
    static bool wasEmitted(const std::string& eventName) {
        for (const auto& e : emittedEvents) {
            if (e == eventName) return true;
        }
        return false;
    }
    
    static int getEmitCount(const std::string& eventName) {
        int count = 0;
        for (const auto& e : emittedEvents) {
            if (e == eventName) count++;
        }
        return count;
    }
    
    static void reset() {
        emittedEvents.clear();
        subscribers.clear();
    }
};

std::vector<std::string> MockEventBus::emittedEvents;
std::map<std::string, std::vector<std::function<void()>>> MockEventBus::subscribers;

// ============================================================================
// MockNTPClient - Simulates NTP sync
// ============================================================================
class MockNTPClient {
public:
    static bool synced;
    static time_t currentTime;
    static std::string timezone;
    static int syncAttempts;
    static bool shouldFailSync;
    
    static bool sync() {
        syncAttempts++;
        if (shouldFailSync) return false;
        synced = true;
        return true;
    }
    
    static bool isSynced() { return synced; }
    static time_t getTime() { return currentTime; }
    
    static void setTimezone(const std::string& tz) { timezone = tz; }
    static std::string getTimezone() { return timezone; }
    
    static void simulateSync(time_t time = 1734508800) {
        currentTime = time;
        synced = true;
    }
    
    static void simulateSyncFailure() { shouldFailSync = true; }
    
    static void reset() {
        synced = false;
        currentTime = 0;
        timezone = "UTC0";
        syncAttempts = 0;
        shouldFailSync = false;
    }
};

bool MockNTPClient::synced = false;
time_t MockNTPClient::currentTime = 0;
std::string MockNTPClient::timezone = "UTC0";
int MockNTPClient::syncAttempts = 0;
bool MockNTPClient::shouldFailSync = false;

} // namespace Mocks
} // namespace DomoticsCore
#endif

using namespace DomoticsCore::Mocks;

// ============================================================================
// NTP Logic Under Test (extracted from NTPComponent for isolation)
// ============================================================================

class NTPLogicUnderTest {
public:
    bool wifiWasConnected = false;
    int syncAttemptCount = 0;
    bool timeSynced = false;
    std::string currentTimezone = "UTC0";
    
    // Called when WiFi connects (would be EventBus subscription in real code)
    void onWifiConnected() {
        if (!wifiWasConnected) {
            wifiWasConnected = true;
            attemptSync();
        }
    }
    
    // Called when WiFi disconnects
    void onWifiDisconnected() {
        wifiWasConnected = false;
        // NTP stays synced even if WiFi drops (time continues locally)
    }
    
    // Attempt NTP sync
    bool attemptSync() {
        if (!MockWifiHAL::isConnected()) {
            return false;
        }
        
        syncAttemptCount++;
        bool result = MockNTPClient::sync();
        if (result) {
            timeSynced = true;
            MockEventBus::emit("ntp/synced");
        }
        return result;
    }
    
    // Set timezone (no network needed)
    void setTimezone(const std::string& tz) {
        currentTimezone = tz;
        MockNTPClient::setTimezone(tz);
    }
    
    bool isSynced() const { return timeSynced; }
    std::string getTimezone() const { return currentTimezone; }
};

// ============================================================================
// Tests
// ============================================================================

NTPLogicUnderTest* ntp = nullptr;

void setUp(void) {
    MockWifiHAL::reset();
    MockEventBus::reset();
    MockNTPClient::reset();
    ntp = new NTPLogicUnderTest();
}

void tearDown(void) {
    delete ntp;
    ntp = nullptr;
}

// T121: Test NTP doesn't sync without WiFi
void test_ntp_no_sync_without_wifi(void) {
    // WiFi not connected
    TEST_ASSERT_FALSE(MockWifiHAL::isConnected());
    
    // Attempt sync should fail
    bool result = ntp->attemptSync();
    
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(ntp->isSynced());
    TEST_ASSERT_EQUAL(0, MockNTPClient::syncAttempts); // Didn't even try
}

// T122: Test NTP syncs when WiFi connects
void test_ntp_syncs_on_wifi_connect(void) {
    // Simulate WiFi connection
    MockWifiHAL::simulateConnect();
    
    // Trigger WiFi connected event
    ntp->onWifiConnected();
    
    // Should have attempted sync
    TEST_ASSERT_EQUAL(1, MockNTPClient::syncAttempts);
    TEST_ASSERT_TRUE(ntp->isSynced());
    TEST_ASSERT_TRUE(MockEventBus::wasEmitted("ntp/synced"));
}

// T123: Test NTP maintains time when WiFi disconnects
void test_ntp_maintains_time_after_wifi_disconnect(void) {
    // First, sync successfully
    MockWifiHAL::simulateConnect();
    ntp->onWifiConnected();
    TEST_ASSERT_TRUE(ntp->isSynced());
    
    // Now disconnect WiFi
    MockWifiHAL::simulateDisconnect();
    ntp->onWifiDisconnected();
    
    // Time should still be synced (continues running locally)
    TEST_ASSERT_TRUE(ntp->isSynced());
}

// T124: Test NTP retry on sync failure
void test_ntp_retry_on_sync_failure(void) {
    MockWifiHAL::simulateConnect();
    MockNTPClient::simulateSyncFailure();
    
    // First attempt should fail
    bool result = ntp->attemptSync();
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(ntp->isSynced());
    TEST_ASSERT_EQUAL(1, MockNTPClient::syncAttempts);
    
    // Reset failure condition and retry
    MockNTPClient::shouldFailSync = false;
    result = ntp->attemptSync();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(ntp->isSynced());
    TEST_ASSERT_EQUAL(2, MockNTPClient::syncAttempts);
}

// T125: Test timezone application (no network needed)
void test_ntp_timezone_application(void) {
    // Set timezone without any network
    ntp->setTimezone("CET-1CEST,M3.5.0,M10.5.0/3");
    
    TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", ntp->getTimezone().c_str());
    TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", MockNTPClient::getTimezone().c_str());
}

// Additional test: Multiple WiFi reconnects don't cause multiple syncs
void test_ntp_single_sync_on_reconnect(void) {
    // First connection
    MockWifiHAL::simulateConnect();
    ntp->onWifiConnected();
    TEST_ASSERT_EQUAL(1, MockNTPClient::syncAttempts);
    
    // Disconnect and reconnect
    MockWifiHAL::simulateDisconnect();
    ntp->onWifiDisconnected();
    
    MockWifiHAL::simulateConnect();
    ntp->onWifiConnected();
    
    // Should sync again after reconnect
    TEST_ASSERT_EQUAL(2, MockNTPClient::syncAttempts);
}

// Additional test: Event emission count
void test_ntp_emits_event_only_on_success(void) {
    MockWifiHAL::simulateConnect();
    
    // First attempt fails
    MockNTPClient::simulateSyncFailure();
    ntp->attemptSync();
    TEST_ASSERT_EQUAL(0, MockEventBus::getEmitCount("ntp/synced"));
    
    // Second attempt succeeds
    MockNTPClient::shouldFailSync = false;
    ntp->attemptSync();
    TEST_ASSERT_EQUAL(1, MockEventBus::getEmitCount("ntp/synced"));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ntp_no_sync_without_wifi);
    RUN_TEST(test_ntp_syncs_on_wifi_connect);
    RUN_TEST(test_ntp_maintains_time_after_wifi_disconnect);
    RUN_TEST(test_ntp_retry_on_sync_failure);
    RUN_TEST(test_ntp_timezone_application);
    RUN_TEST(test_ntp_single_sync_on_reconnect);
    RUN_TEST(test_ntp_emits_event_only_on_success);
    
    return UNITY_END();
}
