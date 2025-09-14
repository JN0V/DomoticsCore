# WiFi Demo

A comprehensive demonstration of the DomoticsCore WiFi component showcasing advanced connectivity management features.

## Features Demonstrated

### Core WiFi Functionality
- **Non-blocking connection**: WiFi connection attempts don't block the main loop
- **Automatic reconnection**: Smart reconnection logic with configurable intervals
- **Connection timeout**: Configurable timeout for connection attempts (15 seconds default)
- **Status monitoring**: Real-time connection status and signal quality reporting

### Advanced Features
- **Network scanning**: Periodic WiFi network discovery and listing
- **Signal quality assessment**: RSSI-based connection quality evaluation
- **Detailed status reporting**: Comprehensive connection information
- **Connection state tracking**: Monitor ongoing connection attempts

### Demo Phases
1. **Connection Monitoring** (every 5 seconds)
   - Connection status reporting
   - IP address and signal strength
   - Network quality assessment

2. **Network Scanning** (every 30 seconds)
   - Scan for available WiFi networks
   - Display network names and signal strengths
   - Limited to top 10 networks for readability

3. **Reconnection Testing** (every 2-3 minutes)
   - Demonstrates reconnection capability
   - Tests automatic recovery from disconnections

## Hardware Requirements

- ESP32 development board
- WiFi network access

## Configuration

**IMPORTANT**: Before running this demo, update the WiFi credentials in `src/main.cpp`:

```cpp
// Replace with your actual WiFi credentials
String ssid = "YourWiFiSSID";
String password = "YourWiFiPassword";
```

## Build and Upload

```bash
# Navigate to the WiFiDemo directory
cd examples/WiFiDemo

# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

## Expected Output

The demo will log detailed WiFi status information including:

```
[INFO] WiFi Status: Connected
[INFO] SSID: YourNetwork
[INFO] IP Address: 192.168.1.100
[INFO] Signal Strength: -45 dBm (Excellent)
[INFO] MAC Address: AA:BB:CC:DD:EE:FF
```

Network scan results:
```
[INFO] Found 8 networks:
[INFO]   1. HomeNetwork (-42 dBm)
[INFO]   2. OfficeWiFi (-55 dBm)
[INFO]   3. GuestNetwork (-67 dBm)
```

## WiFi Component Features

### Configuration Parameters
- `ssid`: WiFi network name (required, max 32 chars)
- `password`: WiFi password (optional, max 64 chars)
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
1. Verify WiFi credentials are correct
2. Check network availability and signal strength
3. Ensure ESP32 is within range of WiFi router
4. Monitor serial output for detailed error messages

### Build Issues
1. Clean build cache: `pio run --target clean`
2. Update library dependencies
3. Verify PlatformIO configuration

## Memory Usage

Typical memory usage for this demo:
- Flash: ~25-30% (depending on ESP32 variant)
- RAM: ~6-7% (efficient memory management)

## Integration Example

To use the WiFi component in your own projects:

```cpp
#include <DomoticsCore/Components/WiFi.h>

// Create WiFi component
auto wifiComponent = std::make_unique<WiFiComponent>("YourWifiSSID", "YourPassword");

// Add to core
core->addComponent(std::move(wifiComponent));

// Access WiFi status
if (wifiComponent->isConnected()) {
    String ip = wifiComponent->getLocalIP();
    int32_t rssi = wifiComponent->getRSSI();
}
```
