# DomoticsCore Logging System

Unified, decentralized logging with component tags and multiple log levels built on ESP32 Arduino Core.

## Features

- **Component tagging** for filtering and debugging
- **5 log levels** (Error, Warning, Info, Debug, Verbose)  
- **Decentralized** - no central enum to modify
- **Extensible** for custom application components
- **Auto timestamps** (millis before NTP, real time after)
- **Callback system** - Stream logs to RemoteConsole, files, etc.
- **Compile-time filtering** - Remove debug code from production builds
- **Runtime filtering** - Dynamic log level control via RemoteConsole

## Quick Start

```cpp
#include <DomoticsCore/Logger.h>

// Use predefined library tags
DLOG_I(LOG_CORE, "System initialized");
DLOG_W(LOG_WIFI, "Connection unstable, RSSI: %d", WiFi.RSSI());

// Define custom application tags
#define LOG_SENSOR "SENSOR"
#define LOG_PUMP   "PUMP"

DLOG_I(LOG_SENSOR, "Temperature: %.2f°C", temperature);
DLOG_E(LOG_PUMP, "Motor failure detected");
```

## Log Levels

The logging system supports five log levels:

| Macro | Level | Description | Use Case |
|-------|-------|-------------|----------|
| `DLOG_E` | Error | Critical errors that prevent normal operation | System failures, connection errors |
| `DLOG_W` | Warning | Issues that don't stop operation but need attention | Retries, fallbacks, configuration issues |
| `DLOG_I` | Info | General information about system state | Startup messages, connection status |
| `DLOG_D` | Debug | Detailed information for debugging | Function entry/exit, variable values |
| `DLOG_V` | Verbose | Very detailed tracing information | Loop iterations, detailed state changes |

## Log Level Control

Control which log levels are compiled and displayed using the `CORE_DEBUG_LEVEL` build flag:

```ini
# In platformio.ini
build_flags = 
    -DCORE_DEBUG_LEVEL=3
```

| Level | Value | Output |
|-------|-------|--------|
| None | 0 | No logging output |
| Error only | 1 | Only error messages |
| Error + Warning | 2 | Errors and warnings |
| Error + Warning + Info | 3 | Errors, warnings, and info (default) |
| Error + Warning + Info + Debug | 4 | All except verbose |
| All levels | 5 | All log messages including verbose |

## Predefined Component Tags

The library provides these predefined component tags:

| Tag | Component | Description |
|-----|-----------|-------------|
| `LOG_CORE` | DomoticsCore | Main framework initialization and control |
| `LOG_WIFI` | WiFi | WiFi connection and management |
| `LOG_MQTT` | MQTT Client | MQTT broker communication |
| `LOG_HTTP` | Web Server | HTTP requests and responses |
| `LOG_HA` | Home Assistant | Home Assistant integration |
| `LOG_OTA` | OTA Manager | Over-the-air updates |
| `LOG_LED` | LED Manager | Status LED control |
| `LOG_SECURITY` | Security | Authentication and security events |
| `LOG_WEB` | Web Config | Web configuration interface |
| `LOG_SYSTEM` | System Utils | System utilities and NTP |
| `LOG_STORAGE` | Storage | Preferences and data storage |
| `LOG_NTP` | NTP | Network time protocol synchronization |
| `LOG_CONSOLE` | RemoteConsole | Telnet-based remote console |

## Custom Component Tags

Applications can easily define their own component tags:

```cpp
// Define custom tags for your application
#define LOG_SENSOR   "SENSOR"
#define LOG_ACTUATOR "ACTUATOR"
#define LOG_DATABASE "DB"
#define LOG_MODBUS   "MODBUS"
#define LOG_DISPLAY  "DISPLAY"

// Use them in your code
DLOG_I(LOG_SENSOR, "DHT22 initialized on pin %d", DHT_PIN);
DLOG_W(LOG_ACTUATOR, "Servo position timeout");
DLOG_E(LOG_DATABASE, "Failed to connect to InfluxDB");
```

## Usage Examples

### Basic Logging
```cpp
#include <DomoticsCore/Logger.h>

void setup() {
    DLOG_I(LOG_CORE, "Application starting...");
    
    // Initialize sensors
    if (initSensors()) {
        DLOG_I(LOG_SENSOR, "All sensors initialized successfully");
    } else {
        DLOG_E(LOG_SENSOR, "Sensor initialization failed");
    }
}

void loop() {
    float temp = readTemperature();
    DLOG_D(LOG_SENSOR, "Temperature reading: %.2f°C", temp);
    
    if (temp > 35.0) {
        DLOG_W(LOG_SENSOR, "High temperature detected: %.2f°C", temp);
    }
}
```

### Error Handling
```cpp
bool connectToWiFi() {
    DLOG_I(LOG_WIFI, "Attempting WiFi connection...");
    
    if (WiFi.begin(ssid, password) != WL_CONNECTED) {
        DLOG_E(LOG_WIFI, "Failed to connect to %s", ssid);
        return false;
    }
    
    DLOG_I(LOG_WIFI, "Connected to %s, IP: %s", ssid, WiFi.localIP().toString().c_str());
    return true;
}
```

### Component-Specific Logging
```cpp
class TemperatureSensor {
private:
    static constexpr const char* LOG_TAG = "TEMP_SENSOR";
    
public:
    bool begin() {
        DLOG_I(LOG_TAG, "Initializing temperature sensor");
        
        if (!sensor.begin()) {
            DLOG_E(LOG_TAG, "Sensor initialization failed");
            return false;
        }
        
        DLOG_I(LOG_TAG, "Sensor ready, resolution: %d bits", sensor.getResolution());
        return true;
    }
    
    float readTemperature() {
        float temp = sensor.readTemperature();
        DLOG_V(LOG_TAG, "Raw temperature reading: %.3f", temp);
        
        if (temp < -40 || temp > 85) {
            DLOG_W(LOG_TAG, "Temperature out of range: %.2f°C", temp);
        }
        
        return temp;
    }
};
```

### Inline Custom Tags
```cpp
// For one-off or temporary logging
DLOG_D("CALIBRATION", "Starting sensor calibration sequence");
DLOG_I("FACTORY_RESET", "Clearing all stored preferences");
DLOG_W("MEMORY", "Low heap warning: %d bytes free", ESP.getFreeHeap());
```

## Log Output Format

The logging system produces output in this format:
```
[TIMESTAMP] [LEVEL] [COMPONENT] Message content
```

Examples:
```
[12345] I [CORE] System initialized
[12678] W [WIFI] Connection unstable, RSSI: -78
[12890] E [MQTT] Connection failed
[13001] D [HA] Command for switch: ON
[13500] I [STORAGE] Initialized successfully
[14000] I [LED] All LEDs configured
```

## Best Practices

### 1. Use Appropriate Log Levels
- **Error**: Only for actual errors that prevent operation
- **Warning**: For issues that don't stop operation but need attention
- **Info**: For important state changes and milestones
- **Debug**: For detailed debugging information
- **Verbose**: For very detailed tracing (use sparingly)

### 2. Create Meaningful Component Tags
```cpp
// Good - specific and clear
#define LOG_TEMP_SENSOR "TEMP"
#define LOG_HUMIDITY_SENSOR "HUMID"
#define LOG_PRESSURE_SENSOR "PRESS"

// Avoid - too generic
#define LOG_STUFF "STUFF"
#define LOG_THING "THING"
```

### 3. Include Context in Messages
```cpp
// Good - includes relevant context
DLOG_E(LOG_MQTT, "Failed to publish to topic '%s', error: %d", topic, error);

// Less helpful - missing context
DLOG_E(LOG_MQTT, "Publish failed");
```

### 4. Use Format Strings Efficiently
```cpp
// Good - uses format string
DLOG_I(LOG_SENSOR, "Temperature: %.2f°C, Humidity: %.1f%%", temp, humidity);

// Avoid - string concatenation
String msg = "Temperature: " + String(temp) + "°C";
DLOG_I(LOG_SENSOR, "%s", msg.c_str());
```

### 5. Conditional Verbose Logging
```cpp
// For expensive operations in verbose logging
#if CORE_DEBUG_LEVEL >= 5
    String debugInfo = generateDetailedDebugInfo();
    DLOG_V(LOG_SYSTEM, "Debug info: %s", debugInfo.c_str());
#endif
```

## Integration with External Tools

### Serial Monitor Filtering
Most serial monitors support filtering. Use component tags to filter logs:
```bash
# Show only MQTT-related logs
pio device monitor --filter="[MQTT]"

# Show only errors and warnings
pio device monitor --filter="[E]|[W]"
```

### Log Parsing
The consistent format makes it easy to parse logs programmatically:
```python
import re

log_pattern = r'\[(\d+)\] ([EWDIV]) \[([^\]]+)\] (.+)'

with open('serial_log.txt', 'r') as f:
    for line in f:
        match = re.match(log_pattern, line.strip())
        if match:
            timestamp, level, component, message = match.groups()
            print(f"Component: {component}, Level: {level}, Message: {message}")
```

## Migration from Serial.print

If you're migrating from `Serial.print*` calls:

```cpp
// Old way
Serial.println("System started");
Serial.printf("Temperature: %.2f\n", temp);

// New way
DLOG_I(LOG_CORE, "System started");
DLOG_I(LOG_SENSOR, "Temperature: %.2f", temp);
```

## Logger Callback System

The logging system includes a callback mechanism that allows components to capture log messages in real-time.

### How It Works

Every log message is:
1. Printed to Serial (as usual)
2. Broadcast to all registered callbacks

```cpp
// Simplified flow
DLOG_I(LOG_APP, "message")
  ├─► Serial output (log_i)
  └─► LoggerCallbacks::broadcast()
      └─► RemoteConsole, File, etc.
```

### Registering Callbacks

```cpp
#include <DomoticsCore/Logger.h>

// Register a callback
LoggerCallbacks::addCallback([](LogLevel level, const char* tag, const char* msg) {
    // Do something with the log
    if (level == LOG_LEVEL_ERROR) {
        sendAlertEmail(tag, msg);
    }
});
```

### Use Cases

- **RemoteConsole**: Stream logs to telnet clients
- **File Logging**: Write logs to SD card
- **Syslog**: Send logs to remote syslog server
- **Alerts**: Trigger notifications on errors
- **Display**: Show logs on OLED/LCD screen

### RemoteConsole Integration

The `DomoticsCore-RemoteConsole` component uses callbacks to provide real-time log streaming via Telnet:

```cpp
#include <DomoticsCore/RemoteConsole.h>

// Configure RemoteConsole
RemoteConsoleConfig config;
config.port = 23;
config.bufferSize = 500;

auto console = std::make_unique<RemoteConsoleComponent>(config);
core.addComponent(std::move(console));
core.begin();

// Now connect via telnet to see logs in real-time
// $ telnet 192.168.1.100 23
```

**Features:**
- Real-time log streaming
- Runtime log level control (`level 4`)
- Tag filtering (`filter MQTT`)
- Command execution
- ANSI color output

See `DomoticsCore-RemoteConsole` documentation for details.

## Compile-Time vs Runtime Filtering

The logging system provides two levels of control:

### Compile-Time Filtering (`CORE_DEBUG_LEVEL`)

**Purpose:** Remove debug code from production builds to save flash memory.

```ini
# platformio.ini
build_flags = 
    -DCORE_DEBUG_LEVEL=2  # Errors + Warnings only
```

**Effect:** Debug and verbose log calls are completely removed from the binary.

### Runtime Filtering (RemoteConsole)

**Purpose:** Dynamically change log verbosity without recompiling.

```bash
# Connect via telnet
$ telnet 192.168.1.100 23
> level 4  # Show all logs including debug
> level 1  # Show only errors
```

**Effect:** Filters which logs are displayed, but code is still in the binary.

### Best Practice: Use Both!

```ini
# Development build - include all logs
build_flags = -DCORE_DEBUG_LEVEL=5

# Production build - only errors and warnings
build_flags = -DCORE_DEBUG_LEVEL=2
```

Then use RemoteConsole's runtime filtering when debugging production devices.

## Performance Considerations

- Log macros are compiled out when `CORE_DEBUG_LEVEL` is set below their level
- Format strings are more efficient than string concatenation
- Verbose logging can impact performance in tight loops
- Consider using conditional compilation for expensive debug operations
- Callbacks add minimal overhead (~1-2µs per log call)

## Troubleshooting

### No Log Output
1. Check `CORE_DEBUG_LEVEL` in build flags
2. Verify serial monitor baud rate (115200)
3. Ensure `Serial.begin()` is called before logging

### Missing Component Tags
1. Verify `#include <DomoticsCore/Logger.h>`
2. Check that custom tags are defined before use
3. Ensure tag strings don't contain spaces or special characters

### Performance Issues
1. Reduce log level in production builds
2. Avoid logging in high-frequency loops
3. Use conditional compilation for expensive debug operations
