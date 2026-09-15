# 06-CoreWithWebUIOnly Demo

This demonstration showcases the DomoticsCore framework's **WebUI Component architecture** without requiring network connectivity. The demo demonstrates how components can "publish/indicate/categorize" their data through the WebUI system, creating automatic user interfaces.

## Architecture Demonstrated

### 🏗️ WebUI Component System
- Uses the built-in `WebUIComponent` from DomoticsCore library
- Demonstrates the `IWebUIProvider` interface pattern
- Shows automatic component registration and UI generation
- Real-time WebSocket updates with configurable intervals

### 📋 Component Registration Pattern
- Components inherit from both `IComponent` and `IWebUIProvider`
- Automatic discovery and registration of WebUI providers
- Components define their own UI sections and field types
- Categorization system (hardware, system, settings, etc.)

### 🎛️ Field Type System
- **Boolean fields**: Toggle switches for on/off controls
- **Slider fields**: Range controls with min/max values
- **Display fields**: Read-only information display
- **Real-time updates**: Live data streaming via WebSocket

## Components Demonstrated

### 💡 LED Demo Component
- **Category**: Hardware
- **Fields**: LED State (Boolean), Brightness (Slider), GPIO Pin (Display)
- **API**: `/api/led_demo` with GET/POST methods
- **Real-time**: 2-second update interval
- **Features**: Direct GPIO control, visual feedback

### 📊 System Monitor Component  
- **Category**: System
- **Fields**: Uptime, Free Heap, Chip Model, CPU Frequency (all Display)
- **API**: `/api/system` with system information
- **Real-time**: 3-second update interval
- **Features**: Live system metrics, hardware information using ESPAsyncWebServer

## Key Questions Answered

### ❓ Why Direct WiFi Instead of DomoticsCore WiFi Component?
This demo intentionally uses **direct ESP32 WiFi** (`#include <WiFi.h>`) instead of the DomoticsCore WiFi component to:
- **Focus on WebUI architecture**: Keep the demo simple and focused on WebUI provider patterns
- **Minimize dependencies**: Avoid WiFi component complexity for a WebUI-only demonstration  
- **Local AP only**: No need for full WiFi management features (scanning, credentials, etc.)
- **Educational clarity**: Show the difference between using raw ESP32 APIs vs DomoticsCore components

For production applications, you would typically use the DomoticsCore WiFi component, as demonstrated in other examples.

### ❓ Do We Need the PlatformIO Changes?
- **PlatformIO changes**: ✅ **YES** - The `symlink://../../` dependency and modern ESPAsync libraries are necessary; a `symlink://` dependency is built where it lives, so a header change is picked up on the next build without any refresh step
- **Build flags**: ✅ **YES** - C++14 support and logging levels are required for DomoticsCore

## Features Demonstrated

### 🏗️ WebUI Component Architecture
- **Component Registration**: Automatic discovery of `IWebUIProvider` implementations
- **Field Type System**: Boolean, Slider, Display field types with automatic UI generation
- **Categorization**: Components organize themselves into categories (hardware, system, etc.)
- **Real-time Updates**: WebSocket-based live data streaming
- **API Routing**: Automatic per-component API endpoint handling

### Technical Features
- **Access Point Mode**: Creates local WiFi network for device access
- **Async Web Server**: Non-blocking web server using ESPAsyncWebServer
- **RESTful API**: Clean JSON API endpoints for device control
- **Component Integration**: Demonstrates LED component integration with WebUI
- **Resource Monitoring**: Real-time heap, CPU, and connection monitoring

## WebUI Components
1. **System Status Dashboard**
   - Uptime tracking
   - Free heap memory display
   - CPU frequency monitoring
   - Connected clients count

2. **LED Control Panel**
   - Turn LED on/off buttons
   - Toggle functionality
   - Visual status indicator
   - Real-time state updates

3. **Information Display**
   - Demo feature overview
   - System specifications
   - Usage instructions

## Hardware Requirements

- ESP32 development board
- Built-in LED (typically on pin 2)
- No external WiFi network required

## Access Instructions

1. **Upload the demo** to your ESP32
2. **Look for WiFi network** named `DomoticsCore-XXXXXX` (where XXXXXX is derived from MAC address)
3. **Connect to the network** (open network, no password required)
4. **Open web browser** and navigate to `http://192.168.4.1`
5. **Interact with the interface** - control LED, monitor system status

## Build and Upload

```bash
# Navigate to the 06-CoreWithWebUIOnly directory
cd examples/06-CoreWithWebUIOnly

# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

## Expected Output

Serial monitor will show:
```
[INFO] === DomoticsCore Local WebUI Demo Ready ===
[INFO] 🌐 Connect to WiFi: Look for 'DomoticsCore-XXXXXX' network
[INFO] 🔗 Open browser: http://192.168.4.1
[INFO] 🚀 Features available:
[INFO] 📱 - Responsive web interface
[INFO] 💡 - LED control via web UI
[INFO] 📊 - Real-time system monitoring
[INFO] 🔄 - Auto-updating status display
[INFO] 📶 - No internet connection required
```

Status updates:
```
[INFO] [LocalWebUI] === Local WebUI Status ===
[INFO] [LocalWebUI] Web Server: Running
[INFO] [LocalWebUI] AP Clients: 1
[INFO] [LocalWebUI] LED State: OFF
[INFO] [LocalWebUI] Free Heap: 245832 bytes
[INFO] [LocalWebUI] Access URL: http://192.168.4.1
```

## API Endpoints

The demo provides a RESTful API for programmatic access:

### GET /api/status
Returns system status information:
```json
{
  "uptime": 120,
  "free_heap": 245832,
  "chip_model": "ESP32-D0WD-V3",
  "chip_cores": 2,
  "cpu_freq": 240,
  "ap_clients": 1,
  "led_state": false
}
```

### POST /api/led
Control LED state:
```bash
# Turn LED on
curl -X POST -d "state=on" http://192.168.4.1/api/led

# Turn LED off
curl -X POST -d "state=off" http://192.168.4.1/api/led
```

### POST /api/led/toggle
Toggle LED state:
```bash
curl -X POST http://192.168.4.1/api/led/toggle
```

## WebUI Features

### Responsive Design
- **Mobile-friendly**: Optimized for smartphones and tablets
- **Grid layout**: Adaptive status grid that adjusts to screen size
- **Modern styling**: Clean, professional appearance with gradients and shadows

### Real-time Updates
- **Auto-refresh**: Status updates every 2 seconds without page reload
- **Live indicators**: Visual LED status with colored indicator
- **Dynamic content**: All values update automatically

### User Experience
- **Intuitive controls**: Clear, labeled buttons for all actions
- **Visual feedback**: Immediate response to user interactions
- **Status indicators**: Clear visual representation of system state

## Memory Usage

Typical memory usage for this demo:
- Flash: ~30-35% (including web server and HTML content)
- RAM: ~8-10% (web server overhead included)

## Integration Example

To add local WebUI to your own projects:

```cpp
#include <ESPAsyncWebServer.h>
#include <DomoticsCore/Components/LED.h>

AsyncWebServer server(80);

// Setup AP mode
WiFi.mode(WIFI_AP);
WiFi.softAP("MyDevice-AP");

// Add API endpoints
server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
});

// Start server
server.begin();
```

## Troubleshooting

### Cannot Connect to AP
1. Ensure ESP32 is powered and running
2. Look for network name starting with "DomoticsCore-"
3. Try forgetting and reconnecting to the network
4. Check serial monitor for AP startup messages

### Web Page Not Loading
1. Verify you're connected to the ESP32's AP network
2. Try `http://192.168.4.1` in browser address bar
3. Clear browser cache and cookies
4. Try a different browser or incognito mode

### LED Not Responding
1. Check if built-in LED is on correct pin (usually pin 2)
2. Verify LED component initialization in serial monitor
3. Try manual toggle via API: `curl -X POST http://192.168.4.1/api/led/toggle`

## Use Cases

This demo is perfect for:
- **Device Configuration**: Local setup without network dependency
- **Field Deployment**: Configuration in environments without WiFi
- **Embedded Systems**: Standalone device control interfaces
- **IoT Prototyping**: Quick web interface development
- **Educational Projects**: Learning web server and AP mode concepts

## Next Steps

This local WebUI foundation can be extended with:
- Additional device controls (sensors, actuators)
- Configuration management (WiFi credentials, device settings)
- Data logging and visualization
- File upload/download capabilities
- WebSocket integration for real-time communication
