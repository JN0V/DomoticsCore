# WebUI Demo

A comprehensive demonstration of the DomoticsCore WebUI component featuring a responsive dark-themed web interface with real-time updates and modular component integration.

## Features

### Web Interface
- **Responsive Design**: Works on desktop, tablet, and mobile devices
- **Dark Theme**: Modern dark UI with blue accent colors
- **Real-time Updates**: WebSocket-powered live data updates
- **Modular Sections**: Dashboard, Devices, Settings with dynamic content
- **REST API**: Full REST API for all component interactions

### Integrated Components
- **LED Control**: Brightness, effects (solid, blink, fade, pulse, rainbow, breathing), color picker
- **WiFi Management**: Connection status, signal strength, network scanning
- **Storage Management**: Key-value preferences, namespace info, live statistics
- **WebUI Framework**: Component registration, API routing, WebSocket handling

### UI Sections
- **Header**: Device name, current time, status indicators
- **Sidebar**: Navigation menu with icons (Dashboard, Devices, Settings, System, Logs)
- **Footer**: Manufacturer info, version, copyright
- **Main Content**: Dynamic component sections with interactive controls

## Hardware Requirements

- ESP32 development board
- Built-in LED (pin 2) for LED demonstrations
- WiFi connectivity for web interface access

## Software Dependencies

- ESP32 Arduino Framework
- AsyncWebServer library
- AsyncTCP library
- ArduinoJson library
- DomoticsCore library (local)

## Configuration

### WiFi Setup
The demo supports two WiFi modes:

1. **Stored Credentials**: Uses previously saved WiFi credentials
2. **AP Mode**: Creates access point "WebUI Demo Device_Setup" for initial configuration

### Component Configuration
- **LED**: Built-in LED on pin 2 (inverted logic)
- **Storage**: Namespace "webui_demo" with 50 entry limit
- **WebUI**: Port 80, WebSocket updates every 2 seconds

## Build and Upload

```bash
# Navigate to the WebUIDemo directory
cd examples/WebUIDemo

# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

## Usage

### Initial Setup
1. Flash the firmware to your ESP32
2. Connect to serial monitor (115200 baud) to see startup logs
3. If no WiFi credentials stored, device creates AP mode
4. Connect to "WebUI Demo Device_Setup" network
5. Navigate to http://192.168.4.1 for initial WiFi configuration

### Normal Operation
1. Device connects to configured WiFi network
2. Check serial monitor for IP address
3. Navigate to http://[device-ip] in web browser
4. Explore the different sections:
   - **Dashboard**: Overview of all components
   - **Devices**: LED control with real-time updates
   - **Settings**: WiFi and Storage management

### Web Interface Features

#### Dashboard
- System overview with component status
- Real-time data from all registered components
- Quick access to key controls

#### Devices Section
- **LED Control Panel**:
  - Brightness slider (0-255)
  - Effect selection (solid, blink, fade, pulse, rainbow, breathing)
  - Color picker for RGB color selection
  - Real-time status updates

#### Settings Section
- **WiFi Management**:
  - Current connection status and signal strength
  - Network scanning functionality
  - Connection details (SSID, IP address)
  
- **Storage Management**:
  - Namespace information and usage statistics
  - Test key-value editing
  - Live entry count updates

### REST API Endpoints

#### LED Control
- `GET /api/led` - Get LED status and settings
- `POST /api/led` - Update LED settings
- `POST /api/led/brightness` - Set brightness
- `POST /api/led/effect` - Set effect
- `POST /api/led/color` - Set color

#### WiFi Management
- `GET /api/wifi` - Get WiFi status
- `POST /api/wifi/scan` - Scan for networks

#### Storage Management
- `GET /api/storage` - Get storage info and test values
- `POST /api/storage/test` - Update test values

### WebSocket Updates
- Endpoint: `ws://[device-ip]/ws`
- Real-time data updates every 2 seconds
- JSON format with component data
- Automatic UI refresh without page reload

## Expected Output

### Serial Monitor
```
=== DomoticsCore WebUI Demo Starting ===
[INFO] Starting core with 4 components...
[INFO] WiFiComponent: Connecting to WiFi...
[INFO] StorageComponent: Initialized namespace 'webui_demo'
[INFO] LEDComponent: Initialized 1 LED(s)
[INFO] WebUIComponent: Server started on port 80
[INFO] === WebUI Demo Ready ===
[INFO] Features:
[INFO] - Responsive dark theme web interface
[INFO] - Real-time WebSocket updates
[INFO] - LED control with effects and colors
[INFO] - WiFi management and network scanning
[INFO] - Storage preferences management
[INFO] - Mobile and desktop responsive design
[INFO] WebUI available at: http://192.168.1.100
[INFO] WebSocket endpoint: ws://192.168.1.100/ws
[INFO] REST API: http://192.168.1.100/api/
```

### Web Interface
- Dark theme with blue accents
- Responsive layout adapting to screen size
- Smooth animations and transitions
- Real-time data updates without page refresh
- Interactive controls with immediate feedback

## Customization

### Adding New Components
1. Implement `IWebUIProvider` interface in your component
2. Define `getWebUISection()` with fields and API endpoints
3. Implement `handleWebUIRequest()` for REST API handling
4. Implement `getWebUIData()` for real-time updates
5. Register with WebUI using `registerProvider()`

### Modifying UI Theme
- Edit CSS in `WebUIContent.h`
- Customize colors, fonts, spacing
- Add new icons or layouts
- Modify responsive breakpoints

### Extending API
- Add new endpoints in component's `handleWebUIRequest()`
- Define new field types in `WebUIField`
- Implement custom validation and error handling

## Troubleshooting

### Common Issues

**WebUI not accessible**
- Check WiFi connection status in serial monitor
- Verify IP address in logs
- Ensure port 80 is not blocked
- Try accessing via AP mode if WiFi fails

**LED not responding**
- Verify pin 2 connection (built-in LED)
- Check LED configuration in serial logs
- Test with different effects and brightness levels

**WebSocket connection failed**
- Check browser developer console for errors
- Verify WebSocket endpoint URL
- Ensure AsyncWebSocket library is properly installed

**Storage errors**
- Check namespace configuration (max 15 characters)
- Verify NVS partition availability
- Monitor entry count limits

**Build errors**
- Ensure all dependencies are installed
- Check PlatformIO library versions
- Verify DomoticsCore library path
- Clean build cache: `pio run --target clean`

### Debug Information
- Enable detailed logging with `CORE_DEBUG_LEVEL=3`
- Monitor serial output for component status
- Use browser developer tools for web interface debugging
- Check REST API responses with network tab

## Architecture Notes

### Component Integration
- Each component implements `IWebUIProvider` for UI integration
- WebUI component manages registration and routing
- Pull model: UI requests data from components via REST API
- Push model: Components send real-time updates via WebSocket

### Data Flow
1. **UI Request**: Browser sends HTTP request to REST endpoint
2. **Component Handler**: WebUI routes to appropriate component
3. **Data Processing**: Component processes request and returns JSON
4. **UI Update**: Browser updates interface with response data
5. **Real-time Updates**: WebSocket pushes live data to connected clients

### Security Considerations
- Currently open access (no authentication)
- CORS headers configured for development
- Input validation in component handlers
- Future: Add authentication and authorization layers

This demo showcases the complete WebUI framework capabilities and serves as a template for building sophisticated web interfaces for ESP32-based IoT devices.
