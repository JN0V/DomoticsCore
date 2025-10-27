# DomoticsCore-System

**Complete ready-to-use system with automatic component orchestration.**

This is the **"batteries included"** component that handles everything automatically:
- ✅ WiFi connection
- ✅ LED status visualization (automatic patterns)
- ✅ Remote console (telnet debugging)
- ✅ State management
- ✅ Component orchestration

**You only focus on YOUR application logic: sensors, relays, business logic.**

## Why Use This?

### Without System Component ❌

```cpp
// Manual setup - lots of boilerplate
auto led = std::make_unique<LEDComponent>();
led->setPin(2);
core.addComponent(std::move(led));

auto coordinator = std::make_unique<CoordinatorComponent>(config);
coordinator->onStateChange([led](SystemState old, SystemState new) {
    // Manually map states to LED patterns
    switch (new) {
        case SystemState::BOOTING: led->blink(200); break;
        case SystemState::WIFI_CONNECTING: led->blink(1000); break;
        // ... 8 more states ...
    }
});
core.addComponent(std::move(coordinator));

auto console = std::make_unique<RemoteConsoleComponent>(consoleConfig);
core.addComponent(std::move(console));

WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
    // Wait for connection...
}

core.begin();
```

### With System Component ✅

```cpp
// Simple setup - focus on YOUR code
SystemConfig config;
config.wifiSSID = "MyNetwork";
config.wifiPassword = "password";

System system(config);
system.begin();  // Everything handled automatically!
```

**That's it!** WiFi, LED, console, state management - all automatic.

## Quick Start

### 1. Install

```ini
lib_deps = 
    DomoticsCore-System
```

### 2. Configure

```cpp
#include <DomoticsCore/System.h>

SystemConfig config;
config.deviceName = "MyDevice";
config.firmwareVersion = "1.0.0";
config.wifiSSID = "YOUR_WIFI";
config.wifiPassword = "YOUR_PASSWORD";
config.enableLED = true;        // Automatic LED patterns
config.enableConsole = true;    // Telnet debugging

System system(config);
```

### 3. Initialize

```cpp
void setup() {
    Serial.begin(115200);
    
    if (!system.begin()) {
        Serial.println("Failed to start!");
        while (1) delay(1000);
    }
    
    // YOUR custom initialization here
}
```

### 4. Loop

```cpp
void loop() {
    system.loop();
    
    // YOUR application code here
}
```

## What You Get Automatically

### ✅ LED Status Visualization

The LED automatically shows system state:

| State | LED Pattern | When |
|-------|-------------|------|
| Fast Blink (200ms) | Booting | System starting |
| Slow Blink (1s) | WiFi connecting | Waiting for network |
| Heartbeat (2s) | WiFi connected | Network ready |
| Fade (1.5s) | Services starting | Loading components |
| Breathing (3s) | Ready | Normal operation |
| SOS (... --- ...) | Error | Something failed |
| Solid ON | OTA update | Firmware updating |

**You don't write any LED code!** It just works.

### ✅ Remote Console

Telnet debugging is automatically available:

```bash
telnet <device-ip> 23
```

**Built-in commands:**
- `help` - Show all commands
- `status` - System status (automatic)
- `wifi` - WiFi status (automatic)
- `level <0-4>` - Change log level
- `info` - System information
- `heap` - Memory usage
- `reboot` - Restart device

### ✅ WiFi Connection

WiFi connects automatically on `begin()`:
- Automatic connection with timeout
- Hostname configuration
- Status logging
- Error handling

## Adding Your Custom Commands

```cpp
// In setup(), after system.begin():

system.registerCommand("temp", [](const String& args) {
    float temp = readTemperature();
    return String("Temperature: ") + String(temp, 1) + "°C\n";
});

system.registerCommand("relay", [](const String& args) {
    if (args == "on") {
        digitalWrite(RELAY_PIN, HIGH);
        return "Relay ON\n";
    } else if (args == "off") {
        digitalWrite(RELAY_PIN, LOW);
        return "Relay OFF\n";
    }
    return "Usage: relay on|off\n";
});
```

Now via telnet:
```
> temp
Temperature: 22.5°C
> relay on
Relay ON
```

## Adding More Components

The System component handles the basics. You can still add more:

### MQTT

```cpp
#include <DomoticsCore/MQTT.h>

// After system.begin():
MQTTConfig mqttConfig;
mqttConfig.broker = "192.168.1.100";
auto mqtt = std::make_unique<MQTTComponent>(mqttConfig);
system.getCore().addComponent(std::move(mqtt));
```

### Web UI

```cpp
#include <DomoticsCore/WebUI.h>

WebUIConfig webConfig;
webConfig.port = 8080;
auto webui = std::make_unique<WebUIComponent>(webConfig);
system.getCore().addComponent(std::move(webui));
```

### NTP

```cpp
#include <DomoticsCore/NTP.h>

NTPConfig ntpConfig;
ntpConfig.server = "pool.ntp.org";
auto ntp = std::make_unique<NTPComponent>(ntpConfig);
system.getCore().addComponent(std::move(ntp));
```

## Configuration Reference

```cpp
struct SystemConfig {
    // Device identity
    String deviceName = "DomoticsCore";
    String firmwareVersion = "1.0.0";
    
    // WiFi (required for network features)
    String wifiSSID = "";
    String wifiPassword = "";
    uint32_t wifiTimeout = 30000;  // 30 seconds
    
    // LED (automatic status visualization)
    bool enableLED = true;
    uint8_t ledPin = 2;
    bool ledActiveHigh = true;
    
    // RemoteConsole (telnet debugging)
    bool enableConsole = true;
    uint16_t consolePort = 23;
    uint8_t consoleMaxClients = 3;
    
    // Logging
    LogLevel defaultLogLevel = LOG_LEVEL_INFO;
};
```

## Complete Example

See `examples/SimpleApp/` for a complete working example with:
- Temperature sensor (simulated)
- Relay control
- Custom console commands
- Automatic LED status
- Remote debugging

## Advanced Usage

### Manual State Control

```cpp
// Trigger OTA update state
system.setState(SystemState::OTA_UPDATE);

// LED automatically shows solid ON
// After update:
system.setState(SystemState::READY);
// LED automatically returns to breathing
```

### Access Core Directly

```cpp
// Add custom components
auto myComponent = std::make_unique<MyComponent>();
system.getCore().addComponent(std::move(myComponent));
```

### State Change Notifications

```cpp
system.getCoordinator()->onStateChange([](SystemState old, SystemState new) {
    // React to state changes
    if (new == SystemState::READY) {
        // Start your application logic
    }
});
```

## Benefits

1. **Minimal Code** - Focus on your application, not infrastructure
2. **Automatic LED** - Visual feedback without writing LED code
3. **Built-in Debugging** - Telnet console ready to use
4. **Production Ready** - Error handling, timeouts, recovery
5. **Extensible** - Add more components as needed

## Comparison

### Traditional Approach
```cpp
// 100+ lines of boilerplate
// Manual LED control
// Manual WiFi connection
// Manual error handling
// Manual state management
```

### DomoticsCore-System
```cpp
// 10 lines of configuration
SystemConfig config;
config.wifiSSID = "MyNetwork";
config.wifiPassword = "password";

System system(config);
system.begin();  // Done!
```

## Examples

### 1. SimpleApp (⭐ Start Here)

**`examples/SimpleApp/`** - Production-ready template

**Perfect for:**
- Simple IoT sensors
- Basic automation
- Learning DomoticsCore
- Quick prototyping

**Includes:**
- ✅ Automatic WiFi, LED, Console
- ✅ Temperature sensor (simulated)
- ✅ Relay control
- ✅ Custom console commands
- ✅ Only ~100 lines of code!

### 2. FullStack (Advanced)

**`examples/FullStack/`** - Complete component integration template

**Perfect for:**
- Complex applications
- Multiple components needed
- Production deployments
- Learning component integration

**Shows how to add:**
- WebUI (web interface)
- MQTT (message broker)
- Home Assistant (auto-discovery)
- NTP (time sync)
- OTA (remote updates)
- Storage (persistent config)

**Note:** This is a template with commented examples. Uncomment what you need!

### 3. Component-Specific Examples

For advanced/custom usage:
- `DomoticsCore-Coordinator/examples/BasicCoordinator/` - Manual state management
- `DomoticsCore-RemoteConsole/examples/BasicRemoteConsole/` - Console-only
- See individual component READMEs

## License

MIT
