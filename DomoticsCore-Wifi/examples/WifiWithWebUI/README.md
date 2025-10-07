# WifiWithWebUI Example

This example demonstrates the WiFi component integrated with the WebUI system, showcasing:

## Features

- **AP Mode for Initial Setup**: Device starts in Access Point mode for easy first-time configuration
- **Live WiFi Scanning**: Discover and connect to available networks through the web interface
- **Real-time Status Badges**: Visual indicators for WiFi STA and AP connection status
- **Settings Panel**: Configure WiFi credentials and AP settings through the web interface
- **Instant Network Switching**: Change WiFi networks without rebooting

## Getting Started

1. **Upload the firmware** to your ESP32
2. **Connect to the AP**: The device creates an open WiFi network named `DomoticsCore-XXXXXX`
3. **Open the WebUI**: Navigate to `http://192.168.4.1:8080` in your browser
4. **Configure WiFi**: 
   - Go to **Settings** tab
   - Expand **WiFi (STA)** section
   - Click **Scan Networks** to discover available networks
   - Select your network and enter the password
   - Click **Connect**

5. **Access from your network**: Once connected, the device will be accessible at its assigned IP address

## Web Interface

### Status Badges (Top Right)
- **WiFi**: Shows `ON` when connected to a WiFi network (STA mode)
- **AP**: Shows `ON` when Access Point is active

### Components Tab
- **WiFi Card**: Displays current connection status, SSID, and IP address

### Settings Tab
- **WiFi (STA)**: Configure station mode (connect to WiFi network)
  - Enable/disable WiFi
  - SSID and password configuration
  - Network scanning
  - Connect button
  
- **Access Point (AP)**: Configure AP mode settings
  - Enable/disable AP
  - Custom AP SSID

## API Reference

### WifiWebUI Provider

The `WifiWebUI` class provides:
- **Status badges**: Real-time WiFi STA and AP status indicators
- **Component card**: Detailed connection information
- **Settings panels**: WiFi configuration interface
- **State tracking**: Efficient change detection using `LazyState<T>` to minimize WebSocket traffic

## Architecture Notes

### State Tracking Strategy

The example demonstrates **lazy state initialization** to handle component initialization order:

- State is initialized on **first access**, not in constructor
- Prevents timing-dependent issues with component initialization
- Each context has independent state tracking
- Change detection only occurs after initialization

### WebSocket Optimization

- **Update interval**: 5 seconds (WiFi state changes infrequently)
- **Change detection**: Only send updates when state actually changes
- **Queue-aware**: Checks if client can receive before sending

## Troubleshooting

**Can't access WebUI:**
- Verify you're connected to the device's AP network
- Check that your browser isn't forcing HTTPS (use `http://` explicitly)
- Try `http://192.168.4.1:8080` directly

**WiFi connection fails:**
- Verify the password is correct
- Check that the network is 2.4GHz (ESP32 doesn't support 5GHz)
- Look for error messages in the serial monitor

## Related Components

- **DomoticsCore-Core**: Core component system
- **DomoticsCore-Wifi**: WiFi connectivity component
- **DomoticsCore-WebUI**: Web interface framework

## License

See main repository LICENSE file.
