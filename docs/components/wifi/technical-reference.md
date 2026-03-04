# DomoticsCore-Wifi -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [WifiConfig](#wificonfig)
- [WifiComponent](#wificomponent)
  - [Constructor](#constructor)
  - [Lifecycle Methods](#lifecycle-methods)
  - [Connection Management](#connection-management)
  - [AP Management](#ap-management)
  - [Scanning](#scanning)
  - [Status and Information](#status-and-information)
  - [Configuration](#configuration)
  - [Deferred Operations](#deferred-operations)
- [INetworkProvider Interface](#inetworkprovider-interface)
- [Connection Modes](#connection-modes)
  - [STA Mode (Station)](#sta-mode-station)
  - [AP Mode (Access Point)](#ap-mode-access-point)
  - [STA+AP Mode (Dual)](#staap-mode-dual)
- [Reconnection Logic](#reconnection-logic)
- [Async Scanning](#async-scanning)
- [Credential Management](#credential-management)
- [AP Auto-SSID Generation](#ap-auto-ssid-generation)
- [WifiEvents](#wifievents)
- [WifiWebUI Provider](#wifiwebui-provider)
- [WiFi HAL](#wifi-hal)
  - [HAL Enums](#hal-enums)
  - [HAL Functions](#hal-functions)
  - [Platform Implementations](#platform-implementations)
- [Platform Differences](#platform-differences)
  - [ESP32](#esp32)
  - [ESP8266](#esp8266)
  - [Stub (Native Tests)](#stub-native-tests)
- [WiFiServer HAL](#wifiserver-hal)

---

## Architecture Overview

The WiFi component follows a layered architecture:

```
+-------------------+     +-------------------+
| WifiWebUI         |     | Other Components  |
| (WebUI Provider)  |     | (MQTT, System)    |
+--------+----------+     +--------+----------+
         |                          |
         v                          v
+-------------------------------------------+
| WifiComponent : IComponent,               |
|                 INetworkProvider           |
+-------------------------------------------+
         |
         v
+-------------------------------------------+
| WiFiHAL (Hardware Abstraction Layer)      |
+----+---------------+---------------------+
     |               |                     |
     v               v                     v
Wifi_ESP32.h   Wifi_ESP8266.h       Wifi_Stub.h
```

`WifiComponent` extends both `IComponent` (for lifecycle management) and `INetworkProvider` (for generic network access). The HAL layer isolates all platform-specific WiFi SDK calls behind a unified API, complying with Constitution principle IX (HAL #ifdef Isolation).

---

## WifiConfig

Configuration structure for the WiFi component. Defined in `Wifi.h`.

```cpp
struct WifiConfig {
    String ssid = "";                   // WiFi network SSID (empty = AP-only mode)
    String password = "";               // WiFi network password
    bool autoConnect = true;            // Auto-connect on boot if SSID set
    bool enableAP = false;              // Enable AP alongside STA
    String apSSID = "";                 // AP SSID (auto-generated if empty)
    String apPassword = "";             // AP password (open if empty)
    uint32_t reconnectInterval = 5000;  // Reconnection interval in ms
    uint32_t connectionTimeout = 15000; // Connection timeout in ms
};
```

**Behavior rules:**
- If `ssid` is empty, the device starts in AP-only mode regardless of `enableAP`.
- If `ssid` is set, STA mode is used, with AP enabled only when `enableAP = true`.

---

## WifiComponent

**Namespace:** `DomoticsCore::Components`
**Inherits:** `IComponent`, `INetworkProvider`
**Header:** `DomoticsCore/Wifi.h`

### Constructor

```cpp
WifiComponent(const String& ssid = "", const String& password = "")
```

- `ssid`: Target WiFi network name. If empty, the component starts in AP-only mode.
- `password`: WiFi network password.

**Internal defaults:**
- `reconnectTimer`: 5000 ms
- `statusTimer`: 30000 ms
- `connectionTimer`: 100 ms polling interval
- `staFallbackTimer_`: 30000 ms (disabled at start)
- `rebootTimer_`: 1500 ms (disabled at start)

Sets `metadata.name = "Wifi"`, `metadata.version = "1.4.1"`.

### Lifecycle Methods

#### `begin() -> ComponentStatus`

Initializes the WiFi subsystem:
1. **No STA credentials with pre-configured AP**: Starts AP-only mode with the configured AP SSID, emits `EVENT_AP_ENABLED` and `EVENT_NETWORK_READY`.
2. **No STA credentials (no pre-configured AP)**: Calls `connectToWifi()` which auto-generates an AP SSID from the MAC address (e.g., `DomoticsCore-A1B2C3`).
3. **STA credentials provided**: Sets Station mode, disables auto-reconnect, and begins a non-blocking connection attempt.

#### `afterAllComponentsReady()`

Checks for pending mode updates (`pendingModeUpdate_`). Mode changes are deferred to `loop()` to ensure all component setup (callbacks, providers) is complete before executing WiFi SDK calls.

#### `loop()`

The main event processing method, called every iteration. Handles (in order):
1. **Pending reboot**: Executes deferred reboot for STA mode switch on low-heap devices.
2. **Deferred mode update**: Executes `updateWifiMode()` after HTTP response is sent and TCP buffers freed.
3. **Deferred config save**: Writes NVS configuration deferred from HTTP handlers to avoid OOM.
4. **STA fallback timer**: Monitors STA connection attempts during AP-to-STA transitions. On success, restarts AP if heap permits. On timeout, restores AP-only mode and saves config with `autoConnect=false`.
5. **Connection polling**: Checks connection status for ongoing attempts; handles timeout (15s default).
6. **Reconnection**: Triggers new connection attempt when disconnected and `shouldConnect` is true.
7. **Status logging**: Periodic debug logging of connection state (every 30s).
8. **Async scan polling**: Checks for completed scan results without blocking.

#### `shutdown() -> ComponentStatus`

Stops connection attempts, calls `HAL::WiFiHAL::disconnectAndOff()`, and returns `Success`.

### Connection Management

| Method | Signature | Description |
|--------|-----------|-------------|
| `isSTAConnected()` | `bool isSTAConnected() const` | True if connected to a WiFi network in STA mode |
| `isAPConnected()` | `bool isAPConnected() const` | Alias for `isAPEnabled()` |
| `hasConnectivity()` | `bool hasConnectivity() const` | True if STA connected OR AP enabled |
| `isConnected()` | `bool isConnected() const override` | INetworkProvider impl -- returns `hasConnectivity()` |
| `disconnect()` | `void disconnect()` | Manually disconnect from WiFi |
| `reconnect()` | `void reconnect()` | Request reconnection (non-blocking) |
| `isConnectionInProgress()` | `bool isConnectionInProgress() const` | True if a connection attempt is active |
| `enableWifi()` | `bool enableWifi(bool enable = true)` | Enable or disable STA mode, calls `updateWifiMode()` |

### AP Management

| Method | Signature | Description |
|--------|-----------|-------------|
| `enableAP()` | `bool enableAP(const String& apSSID, const String& apPassword = "", bool enable = true)` | Enable AP with given SSID/password |
| `disableAP()` | `bool disableAP()` | Disable AP mode |
| `isAPEnabled()` | `bool isAPEnabled() const` | True if AP mode is enabled (internal flag) |
| `isAPMode()` | `bool isAPMode() const` | True if HAL reports AP or STA+AP mode |
| `isSTAAPMode()` | `bool isSTAAPMode() const` | True if HAL reports STA+AP mode |
| `getAPSSID()` | `String getAPSSID() const` | Returns configured AP SSID |
| `getAPInfo()` | `String getAPInfo() const` | Returns JSON with AP details (active, ssid, ip, clients) |

### Scanning

| Method | Signature | Description |
|--------|-----------|-------------|
| `scanNetworks()` | `bool scanNetworks(std::vector<String>& networks)` | Synchronous scan, fills vector with "SSID (RSSI dBm)" strings |
| `startScanAsync()` | `void startScanAsync()` | Start non-blocking scan (returns immediately) |
| `getLastScanSummary()` | `String getLastScanSummary() const` | Returns last scan results as comma-separated string |

### Status and Information

| Method | Signature | Description |
|--------|-----------|-------------|
| `getLocalIP()` | `String getLocalIP() const override` | Returns STA IP in STA/STA+AP mode, AP IP in AP-only mode |
| `getSSID()` | `String getSSID() const` | Returns connected SSID (STA) or AP SSID (AP mode) |
| `getConfiguredSSID()` | `String getConfiguredSSID() const` | Returns the configured target SSID (not necessarily connected) |
| `getRSSI()` | `int32_t getRSSI() const` | Signal strength in dBm |
| `getMacAddress()` | `String getMacAddress() const` | MAC address |
| `getNetworkType()` | `String getNetworkType() const override` | Returns `"Wifi"` |
| `getConnectionStatus()` | `String getConnectionStatus() const override` | Human-readable status string |
| `getNetworkInfo()` | `String getNetworkInfo() const override` | JSON with full network details |
| `getDetailedStatus()` | `String getDetailedStatus() const` | Multi-line status string with IP, SSID, RSSI, MAC |
| `isWifiEnabled()` | `bool isWifiEnabled() const` | True if STA mode is enabled |

### Configuration

| Method | Signature | Description |
|--------|-----------|-------------|
| `getConfig()` | `WifiConfig getConfig() const` | Returns current config constructed from internal state |
| `setConfig()` | `void setConfig(const WifiConfig& cfg)` | Applies new config, schedules deferred mode update |
| `setCredentials()` | `void setCredentials(const String& ssid, const String& password, bool reconnectNow = true)` | Update STA credentials and optionally reconnect |
| `setSTACredentials()` | `void setSTACredentials(const String& ssid, const String& password, bool enable)` | Lightweight credential setter avoiding WifiConfig allocation |
| `setConfigSaveCallback()` | `void setConfigSaveCallback(std::function<void(const WifiConfig&)> callback)` | Set persistence callback (used by SystemWebUISetup) |

### Deferred Operations

These methods schedule work for the next `loop()` iteration, after HTTP responses are sent and TCP buffers freed:

| Method | Description |
|--------|-------------|
| `scheduleUpdateWifiMode()` | Schedules `updateWifiMode()` for next loop |
| `scheduleConfigSave()` | Schedules NVS config write for next loop |

**Rationale:** On ESP8266, executing WiFi SDK calls or NVS writes during HTTP handlers can cause OOM crashes due to combined heap usage of TCP buffers + WiFi stack + NVS. Deferring to `loop()` ensures these buffers are freed first.

---

## INetworkProvider Interface

**Namespace:** `DomoticsCore::Components`
**Header:** `DomoticsCore/INetworkProvider.h`

Abstract interface allowing components (MQTT, WebUI, etc.) to depend on network connectivity without tight coupling to WiFi specifically.

| Method | Type | Description |
|--------|------|-------------|
| `isConnected()` | pure virtual | True if network is available |
| `getLocalIP()` | pure virtual | IP address as string |
| `getNetworkType()` | pure virtual | Type identifier (e.g., "WiFi", "Ethernet") |
| `getConnectionStatus()` | pure virtual | Human-readable status |
| `getNetworkInfo()` | pure virtual | JSON string with network details |
| `setConnectionCallback()` | virtual (default no-op) | Callback for connection state changes |
| `getSignalStrength()` | virtual (default 0) | Signal strength in dBm |
| `getMacAddress()` | virtual (default "") | MAC address |

`WifiComponent` implements all pure virtual methods and provides actual signal strength and MAC address.

---

## Connection Modes

### STA Mode (Station)

Standard client mode. The device connects to an existing WiFi network.

- Initiated when `wifiEnabled = true` and `apEnabled = false`.
- `updateWifiMode()` stops any running AP, sets HAL mode to `Station`, and begins connection attempts.
- Reconnection is handled automatically by the `reconnectTimer` (5s interval).

### AP Mode (Access Point)

The device creates its own WiFi network.

- Initiated when `wifiEnabled = false` and `apEnabled = true`.
- Skips AP restart if already running with the correct SSID (avoids brief dropout).
- Emits `EVENT_AP_ENABLED` and `EVENT_NETWORK_READY` on success.

### STA+AP Mode (Dual)

Both station and access point run simultaneously.

- Initiated when `wifiEnabled = true` and `apEnabled = true`.
- The behavior depends on available heap and platform:

**High heap (>= 10KB):**
Direct `StationAndAP` mode. AP starts first, then STA begins connecting.

**Constrained heap (3.5-10KB):**
Channel synchronization required (primarily ESP8266). The AP is stopped, STA connects to the router's channel, then the AP is restarted locked to that channel. A 30-second fallback timer monitors the STA attempt. On failure, AP-only mode is restored.

**Low heap (< 3.5KB):**
If AP clients are connected (browser via AP), the component saves config and schedules a reboot. On restart, heap is higher and STA can connect. If no AP clients, the STA-only-with-fallback path is used.

**Critical heap (< 2KB):**
STA is disabled entirely to prevent WiFi SDK crashes. Config is saved with `autoConnect = false`.

---

## Reconnection Logic

The reconnection system is entirely non-blocking, complying with Constitution principle X (Non-Blocking Timer Pattern):

1. **Connection attempt**: `startConnection()` calls `HAL::WiFiHAL::connect()` and sets `isConnecting = true`.
2. **Polling**: `loop()` polls `HAL::WiFiHAL::isConnected()` every 100 ms via `connectionTimer`.
3. **Timeout**: If no connection after `CONNECTION_TIMEOUT` (15s), status is set to `TimeoutError`.
4. **Retry**: When disconnected and `shouldConnect = true`, `reconnectTimer` (5s) triggers a new attempt.
5. **Heap guard**: `startConnection()` defers if free heap < 2500 bytes (WiFi.begin needs ~1.5-2KB).
6. **STA fallback**: During AP-to-STA transitions, a 30-second `staFallbackTimer_` monitors the attempt. On timeout, AP is restored and config saved with `autoConnect = false` to prevent boot loops.

---

## Async Scanning

Non-blocking network scanning avoids watchdog resets on ESP8266:

1. Call `startScanAsync()` -- issues `HAL::WiFiHAL::scanNetworks(true)` and sets `scanInProgress = true`.
2. `loop()` polls `HAL::WiFiHAL::scanComplete()`:
   - Returns `-2` on failure.
   - Returns `-1` while in progress.
   - Returns `>= 0` with network count when done.
3. Results (up to 10 networks) are formatted as comma-separated "SSID (RSSI dBm)" and stored in `lastScanSummary_`.
4. Retrieve results with `getLastScanSummary()`.

A synchronous `scanNetworks(vector)` method is also available but should be avoided on ESP8266 due to blocking concerns.

---

## Credential Management

Credentials can be changed at runtime through multiple methods:

- **`setCredentials(ssid, password, reconnectNow)`**: Updates credentials and optionally triggers immediate reconnection. Resets the reconnect timer.
- **`setSTACredentials(ssid, password, enable)`**: Lightweight alternative that avoids constructing a `WifiConfig` object (which allocates 6+ Strings). Preferred in low-heap paths such as HTTP handlers.
- **`setConfig(cfg)`**: Full configuration update. Schedules a deferred mode update via `pendingModeUpdate_`.

All methods update internal state; actual mode changes are applied via `updateWifiMode()`.

---

## AP Auto-SSID Generation

When no SSID is configured and no AP SSID has been pre-set:

1. The MAC address is read via `HAL::WiFiHAL::getMacAddress()`.
2. Colons are stripped.
3. The last 6 hex characters are appended to `"DomoticsCore-"`.
4. Result: `DomoticsCore-A1B2C3` (unique per device).

When the System component pre-configures the AP (e.g., device name-based SSID), that value is used instead.

---

## WifiEvents

**Namespace:** `DomoticsCore::WifiEvents`
**Header:** `DomoticsCore/WifiEvents.h`

| Event | Topic String | Payload | Description |
|-------|-------------|---------|-------------|
| `EVENT_STA_CONNECTED` | `"wifi/sta/connected"` | `bool` | Emitted when STA connects (`true`) or fails (`false`) |
| `EVENT_AP_ENABLED` | `"wifi/ap/enabled"` | `bool` | Emitted when AP is enabled or disabled |
| `EVENT_NETWORK_READY` | `"network/ready"` | `String` (IP) | Emitted when any network becomes available (STA or AP) |

These events are published via the IComponent `emit()` method, following Constitution principle VI (Modular EventBus Architecture).

---

## WifiWebUI Provider

**Namespace:** `DomoticsCore::Components::WebUI`
**Header:** `DomoticsCore/WifiWebUI.h`
**Inherits:** `CachingWebUIProvider`

Optional WebUI integration providing browser-based WiFi configuration.

### WebUI Contexts

| Context ID | Location | Description |
|-----------|----------|-------------|
| `wifi_status` | Status Badge (Header) | Network status badge with icon and tooltip, polls every 2s |
| `wifi_component` | Component Detail | Mode, connected network, and IP address card, polls every 2s |
| `wifi_settings` | Settings | Unified STA/AP configuration card with toggle, SSID, password, polls every 5s |

### Settings Fields

| Field | Type | Description |
|-------|------|-------------|
| `wifi_enabled` | Boolean | Enable/disable STA connection |
| `ssid` | Text | Target network SSID |
| `sta_password` | Password | Network password (never echoed back) |
| `ap_enabled` | Boolean | Enable/disable access point |
| `ap_ssid` | Text | Access point name |

### Change Tracking

`WifiWebUI` uses `LazyState<T>` helpers for efficient delta detection. Only changed contexts are pushed to the browser, minimizing bandwidth on constrained devices. Tracked states:

- `wifiStatusState` (bool): STA or AP active
- `staComponentState` (struct): connected, ssid, ip
- `staSettingsState` (struct): enabled, ssid
- `apSettingsState` (struct): enabled, ssid

### POST Handling

All settings changes go through `handleWebUIRequest()` with `contextId = "wifi_settings"` and `method = "POST"`. Key behaviors:

- **wifi_enabled = true**: Validates SSID is not empty, uses lightweight `setSTACredentials()` path, schedules deferred mode update and config save.
- **wifi_enabled = false**: Disables STA via `setSTACredentials()`.
- **ap_enabled toggle**: Uses full Get/Override/Set pattern on `WifiConfig`.
- **ap_ssid change**: Applied immediately if AP is running; stored as pending if AP is disabled.
- **scan_networks**: Triggers `startScanAsync()`.

---

## WiFi HAL

**Namespace:** `DomoticsCore::HAL::WiFiHAL`
**Header:** `DomoticsCore/Wifi_HAL.h`

The HAL provides a unified WiFi interface. The routing header includes the correct platform implementation at compile time.

### HAL Enums

```cpp
enum class Status { Disconnected, Connecting, Connected, ConnectionFailed, NotSupported };
enum class Mode   { Off, Station, AccessPoint, StationAndAP };
```

### HAL Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `init()` | `void init()` | Platform-specific initialization |
| `isSupported()` | `bool isSupported()` | True if platform has WiFi hardware |
| `setMode()` | `void setMode(Mode mode)` | Set WiFi operating mode |
| `connect()` | `void connect(const char* ssid, const char* password)` | Begin STA connection |
| `disconnect()` | `void disconnect()` | Disconnect STA |
| `startAP()` | `bool startAP(const char* ssid, const char* password)` | Start access point |
| `stopAP()` | `void stopAP()` | Stop access point |
| `getStatus()` | `Status getStatus()` | Get WiFi status enum |
| `isConnected()` | `bool isConnected()` | True if STA connected |
| `getLocalIP()` | `String getLocalIP()` | STA IP address |
| `getAPIP()` | `String getAPIP()` | AP IP address |
| `getSSID()` | `String getSSID()` | Connected network SSID |
| `getRSSI()` | `int32_t getRSSI()` | Signal strength in dBm |
| `getMacAddress()` | `String getMacAddress()` | Device MAC address |
| `setHostname()` | `void setHostname(const char* hostname)` | Set network hostname |
| `setAutoReconnect()` | `void setAutoReconnect(bool enabled)` | Enable/disable SDK auto-reconnect |
| `scanNetworks()` | `int16_t scanNetworks(bool async)` | Scan for networks (sync or async) |
| `scanComplete()` | `int16_t scanComplete()` | Poll async scan result (-1 = in progress, -2 = failed, >= 0 = count) |
| `scanDelete()` | `void scanDelete()` | Free scan results memory |
| `getScannedSSID()` | `String getScannedSSID(uint8_t index)` | SSID at scan index |
| `getScannedRSSI()` | `int32_t getScannedRSSI(uint8_t index)` | RSSI at scan index |
| `getMode()` | `Mode getMode()` | Current operating mode |
| `getAPSSID()` | `String getAPSSID()` | Running AP SSID |
| `getAPStationCount()` | `uint8_t getAPStationCount()` | Number of connected AP clients |
| `disconnectAndOff()` | `void disconnectAndOff()` | Disconnect and turn off WiFi radio |
| `getRawStatus()` | `uint8_t getRawStatus()` | Raw SDK status code |

### Platform Implementations

| File | Platform | Notes |
|------|----------|-------|
| `Wifi_ESP32.h` | ESP32 | Uses `WiFi.h` (ESP-IDF). Exposes `NetworkClient` and `SecureNetworkClient` (WiFiClientSecure). |
| `Wifi_ESP8266.h` | ESP8266 | Uses `ESP8266WiFi.h`. Calls `WiFi.persistent(false)` and `WiFi.setAutoConnect(false)` in `init()` to prevent SDK auto-restore (saves ~4KB heap). Exposes `SecureNetworkClient` as `BearSSL::WiFiClientSecure`. |
| `Wifi_Stub.h` | Native/Tests | No-op implementations. `isConnected()` returns a controllable `stubbedConnected` flag. `startAP()` returns false. |

---

## Platform Differences

### ESP32

- **Dual radio**: AP and STA operate on separate radios and can use different channels simultaneously.
- `updateWifiMode()` directly sets `StationAndAP` mode without needing to stop/restart the AP.
- Higher heap availability (320KB RAM) means the constrained-heap paths are rarely triggered.
- `WiFi.setHostname()` is the hostname API.

### ESP8266

- **Single radio**: AP and STA share one radio and MUST use the same channel.
- **Channel sync caveat**: If the target router is on a different channel than the running AP, the STA connection fails silently with `WL_IDLE_STATUS`. The component works around this by:
  1. Stopping the AP.
  2. Connecting STA (which tunes the radio to the router's channel).
  3. Restarting the AP locked to that channel.
- **Heap pressure**: AP+STA consumes ~2.5KB additional heap. With ~80KB total RAM (often ~3KB free in FullStack firmware), the component may skip AP restart if heap drops below 6.5KB.
- **SDK auto-connect disabled**: `WiFi.persistent(false)`, `WiFi.setAutoConnect(false)`, and `WiFi.setAutoReconnect(false)` are called in `init()` to prevent the SDK from auto-restoring stale STA state (saves ~4KB heap).
- **Config save deferral**: NVS writes from HTTP handlers are deferred to `loop()` via `scheduleConfigSave()`.
- **Reboot-to-STA**: When heap is too low for live STA switching (browser connected via AP), the component saves config and reboots after 1.5s delay.
- `WiFi.hostname()` is the hostname API (differs from ESP32).

### Stub (Native Tests)

- All functions are no-ops.
- `isConnected()` returns the `stubbedConnected` flag, controllable via `setConnectedForTest(bool)`.
- `startAP()` always returns `false`.
- Used for unit testing the component logic without hardware.

---

## WiFiServer HAL

**Header:** `DomoticsCore/WiFiServer_HAL.h`

Provides platform-independent `WiFiServer`, `WiFiClient`, and `IPAddress` types for TCP server operations:

| File | Platform | Implementation |
|------|----------|---------------|
| `WiFiServer_ESP32.h` | ESP32 | Type aliases to `::WiFiServer`, `::WiFiClient`, `::IPAddress` |
| `WiFiServer_ESP8266.h` | ESP8266 | Type aliases to `::WiFiServer`, `::WiFiClient`, `::IPAddress` |
| `WiFiServer_Stub.h` | Native | Full stub implementations with test helpers (`simulateClient()`, `simulateIncomingData()`) |
| `IPAddress_Stub.h` | Native | Stub IP address class for native tests |
