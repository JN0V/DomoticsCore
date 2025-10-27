# DomoticsCore-RemoteConsole

Telnet-based remote console for real-time log streaming and command execution on ESP32 devices.

## Features

- ✅ **Telnet Server** - Standard telnet protocol (port 23)
- ✅ **Real-time Log Streaming** - See logs as they happen
- ✅ **Circular Buffer** - Configurable log history (default 500 entries)
- ✅ **Runtime Log Level** - Change log verbosity without reboot
- ✅ **Tag Filtering** - Filter logs by component tag
- ✅ **Command Processor** - Built-in and custom commands
- ✅ **ANSI Colors** - Color-coded log levels
- ✅ **Logger Callback System** - No macro redefinition needed
- ✅ **IP Whitelist** - Optional IP-based access control
- ✅ **Password Auth** - Optional password protection
- ✅ **Multi-client** - Support multiple concurrent connections

## Installation

```ini
lib_deps = 
    DomoticsCore-Core
    DomoticsCore-RemoteConsole
```

## Basic Usage

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/RemoteConsole.h>

using namespace DomoticsCore::Components;

Core core;

void setup() {
    // Connect to WiFi first
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    // Configure RemoteConsole
    RemoteConsoleConfig config;
    config.port = 23;
    config.bufferSize = 500;
    config.colorOutput = true;
    
    auto console = std::make_unique<RemoteConsoleComponent>(config);
    core.addComponent(std::move(console));
    
    core.begin();
}

void loop() {
    core.loop();
}
```

## Configuration

```cpp
struct RemoteConsoleConfig {
    bool enabled = true;                    // Enable/disable
    uint16_t port = 23;                     // Telnet port
    bool requireAuth = false;               // Password authentication
    String password = "";                   // Auth password
    uint32_t bufferSize = 500;              // Log buffer size
    bool allowCommands = true;              // Enable commands
    std::vector<IPAddress> allowedIPs;      // IP whitelist (empty = all)
    bool colorOutput = true;                // ANSI colors
    uint32_t maxClients = 3;                // Max connections
    LogLevel defaultLogLevel = LOG_LEVEL_INFO;  // Initial log level
};
```

## Built-in Commands

Connect via telnet and use these commands:

```bash
telnet 192.168.1.100 23
```

### Available Commands:

- **`help`** - Show all available commands
- **`clear`** - Clear log buffer
- **`level <0-4>`** - Set log level (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)
- **`filter <tag>`** - Filter logs by tag (empty = show all)
- **`info`** - System information (uptime, heap, WiFi, etc.)
- **`heap`** - Memory usage
- **`reboot`** - Restart device
- **`quit`** - Disconnect

### Example Session:

```
$ telnet 192.168.1.100 23
Trying 192.168.1.100...
Connected to 192.168.1.100.

========================================
  DomoticsCore Remote Console
========================================
Type 'help' for available commands

Recent logs:
[1234][I][APP] System ready!
[5678][I][MQTT] Connected to broker

> help
Available commands:
  help              - Show this help
  clear             - Clear log buffer
  level <level>     - Set log level (0-4)
  filter <tag>      - Filter logs by tag
  info              - System information
  heap              - Memory usage
  reboot            - Restart device
  quit              - Disconnect

> info
System Information:
  Uptime: 123s
  Free Heap: 234567 bytes
  Chip: ESP32 Rev1
  CPU Freq: 240 MHz
  WiFi: MyNetwork (192.168.1.100)
  RSSI: -45 dBm

> level 4
Log level set to: 4

> filter MQTT
Filtering logs by tag: MQTT

[9012][D][MQTT] Publishing state
[9345][I][MQTT] Message received

> quit
Goodbye!
Connection closed by foreign host.
```

## Custom Commands

Register your own commands:

```cpp
auto console = std::make_unique<RemoteConsoleComponent>(config);
auto* consolePtr = console.get();

// Add custom command
consolePtr->registerCommand("sensors", [](const String& args) {
    String result = "\nSensor Values:\n";
    result += "  Temperature: " + String(getTemperature()) + "°C\n";
    result += "  Humidity: " + String(getHumidity()) + "%\n";
    return result;
});

consolePtr->registerCommand("relay", [](const String& args) {
    if (args == "on") {
        digitalWrite(RELAY_PIN, HIGH);
        return String("Relay turned ON\n");
    } else if (args == "off") {
        digitalWrite(RELAY_PIN, LOW);
        return String("Relay turned OFF\n");
    }
    return String("Usage: relay <on|off>\n");
});

core.addComponent(std::move(console));
```

## Logger Integration

The RemoteConsole automatically captures all `DLOG_*` macro output via the callback system:

```cpp
// Your application code
DLOG_I(LOG_APP, "Application started");
DLOG_W(LOG_MQTT, "Connection timeout");
DLOG_E(LOG_WIFI, "Failed to connect");

// All these logs are:
// 1. Printed to Serial (as usual)
// 2. Stored in circular buffer
// 3. Streamed to connected telnet clients
```

**No macro redefinition needed!** The logger callback system is clean and non-invasive.

## Security

### Password Authentication:

```cpp
RemoteConsoleConfig config;
config.requireAuth = true;
config.password = "mysecretpass";
```

### IP Whitelist:

```cpp
RemoteConsoleConfig config;
config.allowedIPs = {
    IPAddress(192, 168, 1, 100),
    IPAddress(192, 168, 1, 101)
};
```

## Log Levels

```cpp
enum LogLevel {
    LOG_LEVEL_NONE = 0,     // No logs
    LOG_LEVEL_ERROR = 1,    // Errors only
    LOG_LEVEL_WARN = 2,     // Errors + Warnings
    LOG_LEVEL_INFO = 3,     // Errors + Warnings + Info (default)
    LOG_LEVEL_DEBUG = 4,    // All except Verbose
    LOG_LEVEL_VERBOSE = 5   // Everything
};
```

## ANSI Color Codes

When `colorOutput = true`:
- **Red** - Errors
- **Yellow** - Warnings
- **Green** - Info
- **Cyan** - Debug
- **White** - Verbose

## Memory Usage

- **Flash:** ~60KB (component + WiFiServer)
- **RAM:** ~5KB base + (bufferSize * ~100 bytes per entry)
- **Default buffer (500 entries):** ~50KB RAM

## Use Cases

- ✅ **Remote Debugging** - Debug devices without USB connection
- ✅ **Production Monitoring** - Monitor deployed devices
- ✅ **Log Analysis** - Filter and analyze logs in real-time
- ✅ **Remote Control** - Execute commands remotely
- ✅ **Troubleshooting** - Diagnose issues without physical access

## Examples

- **BasicRemoteConsole** - Simple telnet console with custom commands
- **RemoteConsoleWithAuth** - Password-protected console (coming soon)
- **RemoteConsoleFiltering** - Advanced log filtering (coming soon)

## Limitations

- Telnet is unencrypted (use on trusted networks only)
- Buffer size limited by available RAM
- Max 3 concurrent clients by default (configurable)

## Future Enhancements

- SSH support for encrypted connections
- Web-based console UI
- Log export to file/SD card
- Command history and tab completion
- Regex-based log filtering

## License

MIT
