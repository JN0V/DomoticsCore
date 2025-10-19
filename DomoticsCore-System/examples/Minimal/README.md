# Minimal Example

**The simplest way to get started with DomoticsCore!**

## What You Get

This example provides the **absolute minimum** for a functional IoT device:

- ✅ **WiFi** - Automatic connection with AP fallback
- ✅ **LED** - Automatic status visualization (8 states)
- ✅ **RemoteConsole** - Telnet debugging (port 23)

**Just ~50 lines of code for a complete IoT device!**

## Perfect For

- 🎓 Learning DomoticsCore
- 🚀 Quick prototyping
- 📡 Simple sensors
- 🔧 Basic automation
- 💡 Understanding the framework

## Quick Start

### 1. Configure WiFi

Edit `src/main.cpp`:

```cpp
const char* WIFI_SSID = "";  // Leave empty for AP mode
const char* WIFI_PASSWORD = "";
```

**Leave empty for automatic AP mode!**

### 2. Build and Upload

```bash
pio run -t upload -t monitor
```

### 3. First Boot

**If WiFi credentials empty:**
1. Device starts AP mode: `MinimalDevice-XXXX`
2. Connect to AP with phone/laptop
3. Open browser: `http://192.168.4.1/wifi`
4. Configure WiFi
5. Device reboots and connects

**If WiFi credentials provided:**
- Connects automatically
- Shows IP in serial monitor

### 4. Access

**Serial Monitor:**
```
DomoticsCore System
Device: MinimalDevice v1.0.0
WiFi connected!
  IP: 192.168.1.215
Telnet: telnet 192.168.1.215 23
System Ready!
```

**Telnet:**
```bash
telnet 192.168.1.215 23
```

**Commands:**
- `help` - Show all commands
- `status` - System status
- `wifi` - WiFi info
- `temp` - Read temperature (example)
- `relay on/off` - Control relay (example)

## LED Patterns (Automatic)

| State | LED Pattern | Meaning |
|-------|-------------|---------|
| Fast Blink (200ms) | Booting | System starting |
| Slow Blink (1s) | WiFi connecting | Waiting for network |
| Heartbeat (2s) | WiFi connected | Network ready |
| Breathing (3s) | Ready | Normal operation |
| Fast Blink (300ms) | Error | Something failed |

**You don't write any LED code!** It just works.

## Code Structure

```cpp
// 1. Configure
SystemConfig config = SystemConfig::minimal();
config.deviceName = "MinimalDevice";
config.wifiSSID = "";  // AP mode

// 2. Create
System domotics(config);

// 3. Add your commands
domotics.registerCommand("temp", [](const String& args) {
    return "Temperature: 22.5°C\n";
});

// 4. Initialize
domotics.begin();  // Everything automatic!

// 5. Loop
domotics.loop();  // In loop()
```

## What's Included

### WiFi Component
- Automatic AP mode if no credentials
- Auto-reconnect on disconnect
- Non-blocking connection
- Status reporting

### LED Component
- Automatic pattern mapping
- State-driven updates
- No manual code needed

### RemoteConsole Component
- Telnet server (port 23)
- Built-in commands
- Easy custom commands
- Real-time debugging

## Customization

### Add Your Sensors

```cpp
float readTemperature() {
    // Replace with real sensor
    return dht.readTemperature();
}
```

### Add Your Actuators

```cpp
void setRelay(bool state) {
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
}
```

### Add Custom Commands

```cpp
domotics.registerCommand("mycommand", [](const String& args) {
    // Your logic here
    return "Result\n";
});
```

## Next Steps

**Want more features?**

- **Standard Example** - Adds WebUI, NTP, Storage
- **FullStack Example** - Adds MQTT, Home Assistant, OTA

**See:** `../Standard/` and `../FullStack/`

## Build Results

```
RAM:   13.7% (45,040 bytes)
Flash: 65.1% (853,701 bytes)
```

Plenty of room for your application code!

## Troubleshooting

### WiFi Won't Connect

- Check SSID and password
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- LED will blink fast on error
- Check serial monitor for details

### Can't Connect via Telnet

- Get IP from serial monitor
- Try: `telnet <ip> 23`
- Ensure port 23 not blocked

### LED Not Working

- Check GPIO pin (default: 2)
- Some boards use inverted logic
- Set `config.ledActiveHigh = false` if needed

## License

MIT
