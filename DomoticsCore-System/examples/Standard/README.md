# Standard Example

**Production-ready configuration with web interface!**

## What You Get

This example provides a **complete standalone IoT device**:

- ✅ **WiFi** - Automatic connection with AP fallback
- ✅ **LED** - Automatic status visualization  
- ✅ **RemoteConsole** - Telnet debugging (port 23)
- 📦 **WebUI** - Auto-added if DomoticsCore-WebUI library installed (port 80)
- 📦 **NTP** - Auto-added if DomoticsCore-NTP library installed
- 📦 **Storage** - Auto-added if DomoticsCore-Storage library installed

**How it works:** System automatically detects and adds optional components if their libraries are available. Just add the library to `platformio.ini` and it will be auto-configured!

## Enabling Optional Components

To enable WebUI, NTP, or Storage, add the library to your `platformio.ini`:

```ini
lib_deps =
    file://../../DomoticsCore-System
    file://../../DomoticsCore-WebUI      ; Adds web interface
    file://../../DomoticsCore-NTP        ; Adds time sync
    file://../../DomoticsCore-Storage    ; Adds persistent config
```

The System will automatically detect and configure them!

## Perfect For

- 🏭 Production deployments
- 📊 Most applications
- 🌐 Web-based monitoring
- ⏰ Time-based automation
- 💾 Persistent settings
- 🔒 Complete standalone devices

## Quick Start

### 1. Configure

Edit `src/main.cpp`:

```cpp
const char* WIFI_SSID = "";  // Leave empty for AP mode
const char* WIFI_PASSWORD = "";
```

### 2. Build and Upload

```bash
pio run -t upload -t monitor
```

### 3. Access

**Web Interface:**
```
http://<device-ip>:8080
```

**Telnet:**
```bash
telnet <device-ip> 23
```

**Serial Monitor:**
```
DomoticsCore System
Device: StandardDevice v1.0.0
WiFi connected!
  IP: 192.168.1.215
Telnet: telnet 192.168.1.215 23
WebUI: http://192.168.1.215:8080
System Ready!
```

## What's Included

### Everything from Minimal

- WiFi (with AP fallback)
- LED (automatic patterns)
- RemoteConsole (telnet)

### Plus Standard Features

#### WebUI (Port 8080)

**Features:**
- Web-based configuration
- Real-time monitoring
- WiFi network scanner
- System information
- API endpoints

**Access:** `http://<device-ip>:8080`

**Endpoints:**
- `/` - Main interface
- `/wifi` - WiFi configuration
- `/api/status` - System status (JSON)
- `/api/sensor` - Sensor data (JSON)

#### NTP (Time Sync)

**Features:**
- Automatic time synchronization
- Timezone support
- DST handling

**Usage:**
```cpp
time_t now = time(nullptr);
struct tm* timeinfo = localtime(&now);
Serial.printf("Time: %02d:%02d:%02d\n", 
    timeinfo->tm_hour, 
    timeinfo->tm_min, 
    timeinfo->tm_sec);
```

#### Storage (Persistent Config)

**Features:**
- Key-value storage
- Survives reboots
- Type-safe access

**Usage:**
```cpp
// Save
storage->putString("wifi_ssid", "MyNetwork");
storage->putInt("threshold", 25);

// Load
String ssid = storage->getString("wifi_ssid", "default");
int threshold = storage->getInt("threshold", 20);
```

## Code Structure

```cpp
// 1. Configure
SystemConfig config = SystemConfig::standard();
config.deviceName = "StandardDevice";

// 2. Create
System domotics(config);

// 3. Initialize
domotics.begin();  // Everything automatic!

// 4. Use components
auto storage = domotics.getCore().getComponent<StorageComponent>();
storage->putString("key", "value");
```

## Web Interface Features

### WiFi Configuration

- Live network scanning
- Signal strength display
- One-click network selection
- Instant connection (no reboot!)

### System Monitoring

- Uptime
- Memory usage
- WiFi status
- Component status

### API Access

**Get system status:**
```bash
curl http://192.168.1.215:8080/api/status
```

**Response:**
```json
{
  "device": "StandardDevice",
  "version": "1.0.0",
  "uptime": 12345,
  "free_heap": 234567,
  "wifi": {
    "ssid": "MyNetwork",
    "ip": "192.168.1.215",
    "rssi": -45
  }
}
```

## Time-Based Automation

```cpp
void loop() {
    domotics.loop();
    
    // Time-based control
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    // Turn on at 6 AM
    if (timeinfo->tm_hour == 6 && timeinfo->tm_min == 0) {
        digitalWrite(RELAY_PIN, HIGH);
    }
    
    // Turn off at 10 PM
    if (timeinfo->tm_hour == 22 && timeinfo->tm_min == 0) {
        digitalWrite(RELAY_PIN, LOW);
    }
}
```

## Persistent Settings

```cpp
void setup() {
    // ... system setup ...
    
    auto storage = domotics.getCore().getComponent<StorageComponent>();
    
    // Load saved threshold
    int threshold = storage->getInt("temp_threshold", 25);
    
    // Use in application
    if (temperature > threshold) {
        // Take action
    }
}

// Update via web interface or console
void updateThreshold(int newValue) {
    auto storage = domotics.getCore().getComponent<StorageComponent>();
    storage->putInt("temp_threshold", newValue);
}
```

## Build Results

```
RAM:   ~20-25% (depending on usage)
Flash: ~70-75% (with all features)
```

## Comparison

### vs Minimal

| Feature | Minimal | Standard |
|---------|---------|----------|
| WiFi | ✅ | ✅ |
| LED | ✅ | ✅ |
| Console | ✅ | ✅ |
| WebUI | ❌ | ✅ |
| NTP | ❌ | ✅ |
| Storage | ❌ | ✅ |
| **Use Case** | Learning | Production |

### vs FullStack

| Feature | Standard | FullStack |
|---------|----------|-----------|
| All Standard | ✅ | ✅ |
| MQTT | ❌ | ✅ |
| Home Assistant | ❌ | ✅ |
| OTA | ❌ | ✅ |
| **External Services** | None | MQTT broker |

## Next Steps

**Want MQTT integration?**

See: `../FullStack/` example

**Want simpler?**

See: `../Minimal/` example

## Troubleshooting

### Can't Access Web Interface

- Check IP address in serial monitor
- Try: `http://<ip>:8080`
- Ensure port 8080 not blocked

### Time Not Syncing

- Check internet connection
- Verify NTP server accessible
- Check timezone configuration

### Storage Not Persisting

- Ensure Storage component initialized
- Check available flash space
- Verify namespace configuration

## License

MIT
