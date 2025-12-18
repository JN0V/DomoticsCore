#pragma once

/**
 * @file MockWifiHAL.h
 * @brief Mock WiFi HAL for isolated unit testing without real network
 * 
 * This mock replaces the real WiFi HAL to allow testing components like
 * NTP, MQTT, OTA without requiring actual network connectivity.
 */

#include <Arduino.h>
#include <functional>

namespace DomoticsCore {
namespace Mocks {

/**
 * @brief Mock WiFi HAL that simulates network connectivity
 */
class MockWifiHAL {
public:
    // Simulated connection state
    static bool connected;
    static String ssid;
    static String localIP;
    static int8_t rssi;
    
    // Callbacks for test verification
    static std::function<void()> onConnectCalled;
    static std::function<void()> onDisconnectCalled;
    
    // HAL interface implementation
    static bool isConnected() { return connected; }
    static String getSSID() { return ssid; }
    static String getLocalIP() { return localIP; }
    static int8_t getRSSI() { return rssi; }
    
    // Test control methods
    static void simulateConnect(const String& testSSID = "TestNetwork", const String& ip = "192.168.1.100") {
        connected = true;
        ssid = testSSID;
        localIP = ip;
        rssi = -50;
        if (onConnectCalled) onConnectCalled();
    }
    
    static void simulateDisconnect() {
        connected = false;
        ssid = "";
        localIP = "0.0.0.0";
        rssi = 0;
        if (onDisconnectCalled) onDisconnectCalled();
    }
    
    static void reset() {
        connected = false;
        ssid = "";
        localIP = "0.0.0.0";
        rssi = 0;
        onConnectCalled = nullptr;
        onDisconnectCalled = nullptr;
    }
};

// Static member initialization
bool MockWifiHAL::connected = false;
String MockWifiHAL::ssid = "";
String MockWifiHAL::localIP = "0.0.0.0";
int8_t MockWifiHAL::rssi = 0;
std::function<void()> MockWifiHAL::onConnectCalled = nullptr;
std::function<void()> MockWifiHAL::onDisconnectCalled = nullptr;

} // namespace Mocks
} // namespace DomoticsCore
