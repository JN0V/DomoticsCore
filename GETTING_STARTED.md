# Getting Started with DomoticsCore

**Quick guide to start building ESP32 IoT applications**

---

## 🚀 Quick Start

### Installation from GitHub (Development/Testing)

**Step 1: Add to platformio.ini**
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps = 
    https://github.com/JN0V/DomoticsCore.git#v1.0.0

build_flags = 
    -std=c++14
```

**Step 2: Write your code**
```cpp
#include <DomoticsCore/System.h>

using namespace DomoticsCore;
System* domotics = nullptr;

void setup() {
    SystemConfig config = SystemConfig::fullStack();
    config.deviceName = "MyDevice";
    config.wifiSSID = "";  // Empty = AP mode
    
    domotics = new System(config);
    domotics->begin();
}

void loop() {
    domotics->loop();
}
```

**Step 3: Build**
```bash
pio run -t upload -t monitor
```

That's it! Everything is configured automatically.

---

### Installation from PlatformIO Registry (Recommended for Production)

### 1. Install DomoticsCore-System

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps = 
    DomoticsCore-System
```

### 2. Write Your Application

```cpp
#include <DomoticsCore/System.h>

using namespace DomoticsCore;

System* domotics = nullptr;

// YOUR sensor code
float readTemperature() {
    return 22.5;  // Replace with real sensor
}

void setup() {
    Serial.begin(115200);
    
    // Simple configuration
    SystemConfig config;
    config.deviceName = "MyDevice";
    config.wifiSSID = "YOUR_WIFI";
    config.wifiPassword = "YOUR_PASSWORD";
    
    domotics = new System(config);
    
    // Add custom commands
    domotics->registerCommand("temp", [](const String& args) {
        return String("Temp: ") + String(readTemperature(), 1) + "°C\n";
    });
    
    // Initialize (automatic: WiFi, LED, Console)
    if (!domotics->begin()) {
        while (1) delay(1000);
    }
}

void loop() {
    domotics->loop();
    
    // YOUR application code here
}
```

### 3. Build and Upload

```bash
pio run -t upload -t monitor
```

### 4. Access Your Device

Once running:
- **Telnet**: `telnet <device-ip> 23`
- **LED**: Watch for system status (automatic)
- **Serial**: Monitor for logs

**That's it!** WiFi, LED patterns, telnet console - all automatic.

---

## 📚 What You Get Automatically

### ✅ WiFi Connection
- Connects on `begin()`
- Handles timeouts
- Logs connection status

### ✅ LED Status (Automatic Patterns)
- **Fast Blink (200ms)** - Booting
- **Slow Blink (1s)** - WiFi connecting
- **Heartbeat (2s)** - WiFi connected
- **Breathing (3s)** - System ready ← Normal operation
- **Fast Blink (300ms)** - Error

### ✅ Remote Console (Telnet)
Built-in commands:
- `status` - System status
- `wifi` - WiFi info
- `help` - Show all commands
- `level <0-4>` - Change log level
- `heap` - Memory usage
- `reboot` - Restart

Plus YOUR custom commands!

---

## 📖 Examples

### Start Here

**`DomoticsCore-System/examples/SimpleApp/`**

Complete production-ready template with:
- Temperature sensor (simulated)
- Relay control
- Custom console commands
- Only ~100 lines of code!

### Advanced Examples

**Component-specific:**
- `DomoticsCore-RemoteConsole/examples/BasicRemoteConsole/`
- `DomoticsCore-Coordinator/examples/BasicCoordinator/`
- See individual component directories

---

## 🎯 Common Use Cases

### Temperature Monitoring

```cpp
#include <DHT.h>

DHT dht(4, DHT22);

void setup() {
    // ... system setup ...
    dht.begin();
    
    domotics->registerCommand("temp", [](const String& args) {
        float temp = dht.readTemperature();
        return String("Temperature: ") + String(temp, 1) + "°C\n";
    });
}
```

### Relay Control

```cpp
#define RELAY_PIN 5

void setup() {
    // ... system setup ...
    pinMode(RELAY_PIN, OUTPUT);
    
    domotics->registerCommand("relay", [](const String& args) {
        if (args == "on") {
            digitalWrite(RELAY_PIN, HIGH);
            return "Relay ON\n";
        } else if (args == "off") {
            digitalWrite(RELAY_PIN, LOW);
            return "Relay OFF\n";
        }
        return "Usage: relay on|off\n";
    });
}
```

### Periodic Sensor Reading

```cpp
void loop() {
    domotics->loop();
    
    static unsigned long lastRead = 0;
    if (millis() - lastRead > 10000) {  // Every 10 seconds
        float temp = readTemperature();
        Serial.printf("Temperature: %.1f°C\n", temp);
        
        // Control based on sensor
        if (temp > 25.0) {
            digitalWrite(RELAY_PIN, HIGH);  // Turn on cooling
        }
        
        lastRead = millis();
    }
}
```

---

## 🔧 Adding More Components

The System handles the basics. Add more as needed:

### MQTT

```cpp
#include <DomoticsCore/MQTT.h>

// After system.begin():
MQTTConfig mqttConfig;
mqttConfig.broker = "192.168.1.100";
auto mqtt = std::make_unique<MQTTComponent>(mqttConfig);
domotics->getCore().addComponent(std::move(mqtt));
```

### Web UI

```cpp
#include <DomoticsCore/WebUI.h>

WebUIConfig webConfig;
webConfig.port = 8080;
auto webui = std::make_unique<WebUIComponent>(webConfig);
domotics->getCore().addComponent(std::move(webui));
```

### NTP Time Sync

```cpp
#include <DomoticsCore/NTP.h>

NTPConfig ntpConfig;
ntpConfig.server = "pool.ntp.org";
auto ntp = std::make_unique<NTPComponent>(ntpConfig);
domotics->getCore().addComponent(std::move(ntp));
```

---

## 🐛 Troubleshooting

### WiFi Won't Connect

- Check SSID and password
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- LED will blink fast on error
- Check serial monitor for details

### Can't Connect via Telnet

- Get IP from serial monitor
- Try: `telnet <ip> 23` (space between IP and port)
- Ensure port 23 not blocked by firewall

### LED Not Working

- Check GPIO pin (default: 2)
- Some boards use inverted logic
- Set `config.ledActiveHigh = false` if needed

---

## 📚 Documentation

- **Main README**: `README.md`
- **System Component**: `SYSTEM_COMPONENT.md`
- **Architecture**: `ARCHITECTURE_DECISIONS.md`
- **Changelog**: `CHANGELOG.md`
- **Component READMEs**: See each component directory

---

## 🎓 Learning Path

1. **Start**: `DomoticsCore-System/examples/SimpleApp/`
2. **Understand**: Read `SYSTEM_COMPONENT.md`
3. **Customize**: Add your sensors/actuators
4. **Extend**: Add MQTT, WebUI, etc.
5. **Advanced**: Explore individual components

---

## 💡 Tips

### Development
- Use `CORE_DEBUG_LEVEL=4` for development
- Use `CORE_DEBUG_LEVEL=2` for production
- Connect via telnet for live debugging
- Use `level` command to change log level at runtime

### Production
- Set strong WiFi password
- Change default device name
- Enable OTA for remote updates
- Add watchdog timer
- Test error recovery

### Best Practices
- Keep `loop()` non-blocking
- Use timers for periodic tasks
- Handle sensor errors gracefully
- Log important events
- Document custom commands

---

## 🆘 Support

- **Examples**: See `examples/` directories
- **Documentation**: Component READMEs
- **Issues**: GitHub issues
- **Community**: [Add your community link]

---

## ✅ Next Steps

1. ✅ Copy `SimpleApp` example
2. ✅ Configure WiFi credentials
3. ✅ Add your sensors/actuators
4. ✅ Test via telnet console
5. ✅ Add MQTT/WebUI as needed
6. ✅ Deploy to production

**Happy building!** 🚀
