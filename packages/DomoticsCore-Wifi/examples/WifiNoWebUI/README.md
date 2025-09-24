# 05-CoreWithWifi Demo

A comprehensive demonstration of the DomoticsCore Wifi component showcasing the new simplified Wifi API that hides ESP32 mode complexity.

## Features Demonstrated

### Simplified Wifi API
- **User-Friendly Interface**: Simple `enableWifi()` and `enableAP()` methods
- **Automatic Mode Management**: Component automatically handles ESP32 Wifi modes (STA/AP/STA+AP)
- **No Mode Complexity**: Users don't need to understand ESP32's internal Wifi modes
- **Intuitive Design**: Request Wifi connectivity and/or AP functionality independently

### Core Wifi Functionality
- **Non-blocking connection**: Wifi connection attempts don't block the main loop
- **Automatic reconnection**: Smart reconnection logic with configurable intervals
- **Connection timeout**: Configurable timeout for connection attempts (15 seconds default)
- **Status monitoring**: Real-time connection status and signal quality reporting

### Advanced Features
- **Network scanning**: Periodic Wifi network discovery with error handling
- **Dual-mode support**: Simultaneous Wifi station and Access Point operation
- **Signal quality assessment**: RSSI-based connection quality evaluation (Excellent/Good/Fair/Poor)
- **Comprehensive status reporting**: Shows both Station and AP information in dual mode
- **Smart conflict prevention**: Automatically skips operations during mode transitions

### Demo Phases
1. **Connection Monitoring** (every 5 seconds)
   - Real-time connection status reporting
   - IP address and signal strength monitoring
   - Network quality assessment with descriptive labels

2. **Network Scanning** (every 15 seconds)
   - Scan for available Wifi networks
   - Display network names, signal strengths, and security status
   - Robust error handling for scan conflicts
   - Limited to top 10 networks for readability

3. **AP Mode Test** (30-45 seconds)
   - Demonstrates AP-only mode using `enableWifi(false)` + `enableAP()`
   - AP Name: WifiDemo_AP, Password: demo12345
   - AP IP: 192.168.4.1
   - Automatically returns to Wifi mode

4. **Wifi + AP Mode Test** (60-75 seconds)
   - Demonstrates simultaneous Wifi and AP using `enableWifi(true)` + `enableAP()`
   - AP Name: WifiDemo_Both, Password: demo12345
   - Shows both Station IP and AP IP (192.168.4.1)
   - Maintains Wifi connection while providing AP access

5. **Reconnection Testing** (every 2 minutes)
   - Demonstrates reconnection capability
   - Tests automatic recovery from disconnections
   - Smart logic that skips during AP mode tests

## Hardware Requirements

- ESP32 development board
- Wifi network access

## Configuration

**IMPORTANT**: Before running this demo, update the Wifi credentials in `src/main.cpp`:

```cpp
// Replace with your actual Wifi credentials
String ssid = "YourWifiSSID";
String password = "YourWifiPassword";
```

## Build and Upload

```bash
# Navigate to the 05-CoreWithWifi directory
cd examples/05-CoreWithWifi

# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

## Expected Output

The demo will log detailed Wifi status information including:

```
[INFO] === Wifi Status Report [Phase 1: Connection Monitoring] ===
[INFO] Status: Connected (Station mode)
[INFO] SSID: YourNetwork
[INFO] IP Address: 192.168.1.100
[INFO] Signal Strength: -45 dBm (Excellent)
[INFO] MAC Address: AA:BB:CC:DD:EE:FF
[INFO] Free heap: 257248 bytes
[INFO] Uptime: 15 seconds
```

Network scan results:
```
[INFO] === Phase 2: Network Scanning ===
[INFO] 🔍 Scanning for available networks...
[INFO] 📡 Found 10 networks:
[INFO]   1: HomeNetwork (-42 dBm) [Secured]
[INFO]   2: OfficeWifi (-55 dBm) [Secured]
[INFO]   3: GuestNetwork (-67 dBm) [Open]
```

AP mode test:
```
[INFO] === Phase 3: AP Mode Test ===
[INFO] 🔄 Testing AP-only mode...
[INFO] 📡 Enabling AP mode for 15 seconds
[INFO] 📶 AP Name: WifiDemo_AP
[INFO] 🔐 AP Password: demo12345
[INFO] 🌐 Connect to: http://192.168.4.1
[INFO] ✅ Successfully enabled AP-only mode
[INFO] Status: Connected (AP Only mode)
```

Wifi + AP simultaneous mode test:
```
[INFO] === Phase 4: Wifi + AP Mode Test ===
[INFO] 🔄 Testing Wifi + AP simultaneous mode...
[INFO] ✅ Successfully enabled Wifi + AP mode
[INFO] Status: Connected (STA+AP mode)
[INFO] Station SSID: YourNetwork
[INFO] Station IP: 192.168.1.100
[INFO] AP SSID: WifiDemo_Both
[INFO] AP IP: 192.168.4.1
[INFO] AP Clients: 0
```

## Wifi Component Features

### Configuration Parameters
- `ssid`: Wifi network name (required, max 32 chars)
- `password`: Wifi password (optional, max 64 chars)
- `reconnect_interval`: Reconnection attempt interval (1-60 seconds)
- `connection_timeout`: Connection timeout (5-60 seconds)
- `auto_reconnect`: Enable automatic reconnection (boolean)

### Status Monitoring
- Real-time connection status
- Signal strength (RSSI) measurement
- Connection quality assessment
- IP address and network information
- MAC address reporting

### Non-blocking Architecture
- All operations use non-blocking timers
- Connection attempts don't freeze the system
- Proper integration with DomoticsCore lifecycle
- Status updates via component status system

## Troubleshooting

### Connection Issues
1. Verify Wifi credentials are correct
2. Check network availability and signal strength
3. Ensure ESP32 is within range of Wifi router
4. Monitor serial output for detailed error messages

### Build Issues
1. Clean build cache: `pio run --target clean`
2. Update library dependencies
3. Verify PlatformIO configuration

## Memory Usage

Typical memory usage for this demo:
- Flash: ~25-30% (depending on ESP32 variant)
- RAM: ~6-7% (efficient memory management)

## New Simplified Wifi API

The Wifi component now provides an intuitive API that hides ESP32 mode complexity:

### Basic Usage
```cpp
#include <DomoticsCore/Components/Wifi.h>

// Create Wifi component
auto wifi = std::make_unique<WifiComponent>("YourWifiSSID", "YourPassword");

// Simple Wifi connectivity
wifi->enableWifi(true);   // Enable Wifi connection

// Simple Access Point
wifi->enableAP("MyDevice_AP", "password123");  // Enable AP

// Both simultaneously (automatic STA+AP mode)
wifi->enableWifi(true);
wifi->enableAP("MyDevice_AP", "password123");

// Disable features
wifi->enableWifi(false);  // Disable Wifi
wifi->disableAP();        // Disable AP
```

### Status Checking
```cpp
// Check what's enabled
bool wifiOn = wifi->isWifiEnabled();
bool apOn = wifi->isAPEnabled();

// Check connection status
if (wifi->isConnected()) {
    String ip = wifi->getLocalIP();      // Station IP (or AP IP in AP-only mode)
    String ssid = wifi->getSSID();       // Station SSID (or AP SSID in AP-only mode)
    int32_t rssi = wifi->getRSSI();      // Signal strength
}

// In dual mode, get specific information
if (wifi->isSTAAPMode()) {
    String stationIP = Wifi.localIP().toString();
    String apIP = Wifi.softAPIP().toString();
    int clients = Wifi.softAPgetStationNum();
}
```

### Integration Example
```cpp
// Add to DomoticsCore
core->addComponent(std::move(wifi));

// The component automatically handles:
// - ESP32 Wifi mode selection (WIFI_STA/WIFI_AP/WIFI_AP_STA)
// - Mode transitions with proper delays
// - Connection management and reconnection
// - Status reporting for all modes
```
