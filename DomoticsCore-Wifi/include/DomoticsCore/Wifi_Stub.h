#ifndef DOMOTICS_CORE_WIFI_STUB_H
#define DOMOTICS_CORE_WIFI_STUB_H

/**
 * @file Wifi_Stub.h
 * @brief Stub WiFi implementation for unsupported platforms.
 */

#if !DOMOTICS_PLATFORM_ESP32 && !DOMOTICS_PLATFORM_ESP8266

#include <vector>
#include <utility>

namespace DomoticsCore {
namespace HAL {
namespace WiFiImpl {

inline bool stubbedConnected = false;
inline void setConnectedForTest(bool connected) { stubbedConnected = connected; }

// ---------------------------------------------------------------------------
// Scriptable scan results.
//
// `scanNetworks()` returned a hard 0 here until 2026-08-29, and that meant the
// two scan-summary loops in Wifi.h — the ones MEM-2 rewrote — had **never
// executed on any platform CI can run**. Neither loop body is entered when the
// scan finds nothing, so the required native job would have stayed green
// through a rewrite that produced the wrong text, the wrong separator or a
// truncated entry. The device suite that measures them needs a radio and a
// neighbourhood; this needs neither, and it is what pins the format.
//
// The table is empty by default, so a suite that does not script it sees
// exactly the behaviour this file had before — zero networks, empty SSIDs.
// ---------------------------------------------------------------------------
struct StubNetwork {
    String ssid;
    int32_t rssi;
};

inline std::vector<StubNetwork>& stubbedNetworks() {
    static std::vector<StubNetwork> networks;
    return networks;
}

// 0 or above: the table's size is the result. Negative: returned as-is, which
// is how the failure branches are reached (-1 for the synchronous scan guard,
// -2 for WIFI_SCAN_FAILED on the asynchronous one).
inline int16_t stubbedScanResult = 0;

inline void setScannedNetworksForTest(std::vector<StubNetwork> networks) {
    stubbedNetworks() = std::move(networks);
    stubbedScanResult = 0;
}

inline void setScanFailedForTest(int16_t code) {
    stubbedNetworks().clear();
    stubbedScanResult = code;
}

inline void resetScanForTest() {
    stubbedNetworks().clear();
    stubbedNetworks().shrink_to_fit();
    stubbedScanResult = 0;
}

inline void init() {}
inline void setMode(WiFiHAL::Mode) {}
inline void connect(const char*, const char*) {}
inline void disconnect() {}
inline bool startAP(const char*, const char*) { return false; }
inline void stopAP() {}
inline WiFiHAL::Status getStatus() { return WiFiHAL::Status::NotSupported; }
inline bool isConnected() { return stubbedConnected; }
inline String getLocalIP() { return "0.0.0.0"; }
inline String getAPIP() { return "0.0.0.0"; }
inline String getSSID() { return ""; }
inline int32_t getRSSI() { return 0; }
inline String getMacAddress() { return "00:00:00:00:00:00"; }
inline void setHostname(const char*) {}
inline void setAutoReconnect(bool) {}
inline int16_t scanNetworks(bool) {
    if (stubbedScanResult < 0) return stubbedScanResult;
    return static_cast<int16_t>(stubbedNetworks().size());
}
inline String getScannedSSID(uint8_t index) {
    const auto& networks = stubbedNetworks();
    return index < networks.size() ? networks[index].ssid : String("");
}
inline int32_t getScannedRSSI(uint8_t index) {
    const auto& networks = stubbedNetworks();
    return index < networks.size() ? networks[index].rssi : 0;
}
inline WiFiHAL::Mode getMode() { return WiFiHAL::Mode::Off; }
inline String getAPSSID() { return ""; }
inline uint8_t getAPStationCount() { return 0; }
inline int16_t scanComplete() {
    if (stubbedScanResult < 0) return stubbedScanResult;
    return static_cast<int16_t>(stubbedNetworks().size());
}
// No-op: the scripted table is owned by whichever test set it, not by the
// component under test. On a real SDK this frees the scan-result list.
inline void scanDelete() {}
inline void disconnectAndOff() {}
inline uint8_t getRawStatus() { return 0; }

} // namespace WiFiImpl

class NetworkClient {};
class SecureNetworkClient {};

} // namespace HAL
} // namespace DomoticsCore

#endif // Stub

#endif // DOMOTICS_CORE_WIFI_STUB_H
