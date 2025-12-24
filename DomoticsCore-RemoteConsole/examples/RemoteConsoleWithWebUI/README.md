# RemoteConsole with WebUI Example

This example demonstrates the RemoteConsole component integrated with WebUI for comprehensive system monitoring and debugging.

## Features

- **Telnet Remote Console** on port 23
  - Real-time log streaming
  - Built-in commands (help, info, logs, clear, level, filter, reboot, quit)
  - ANSI color output
  - Multiple concurrent client support (up to 3)
  - Circular log buffer (500 entries)

- **Web Interface**
  - System monitoring dashboard
  - Configuration management
  - Real-time metrics

- **WiFi Connectivity**
  - STA mode with configurable credentials
  - AP fallback mode if STA fails

## Hardware Requirements

- ESP32 or ESP8266 board
- USB cable for programming

## Usage

### 1. Configure WiFi

Edit `src/main.cpp` and update:

```cpp
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
```

### 2. Build and Upload

**For ESP32:**
```bash
pio run -e esp32dev -t upload
pio device monitor
```

**For ESP8266:**
```bash
pio run -e esp8266dev -t upload
pio device monitor
```

### 3. Connect via Telnet

Once the device is connected to WiFi, note the IP address from the serial monitor, then:

```bash
telnet <device-ip-address>
```

Or if using AP mode:
```bash
telnet 192.168.4.1
```

### 4. Available Commands

Once connected via telnet, type `help` to see available commands:

- `help` - Show available commands
- `info` - Display system information
- `logs` - Show recent log entries
- `clear` - Clear the log buffer
- `level <level>` - Set log level (0-5)
- `filter <tag>` - Filter logs by tag
- `reboot` - Restart the device
- `quit` - Disconnect from console

### 5. Web Interface

Open a browser and navigate to:
- `http://<device-ip-address>` (in STA mode)
- `http://192.168.4.1` (in AP mode)

## Example Session

```
$ telnet 192.168.1.100

========================================
  DomoticsCore Remote Console
========================================
Type 'help' for available commands

> help
Available commands:
  help     - Show this help message
  info     - Show system information
  logs     - Show recent logs
  clear    - Clear log buffer
  level    - Set log level (0-5)
  filter   - Filter logs by tag
  reboot   - Restart system
  quit     - Disconnect

> info

System Information:
  Uptime: 142s
  Free Heap: 234560 bytes
  Chip: ESP32 Rev1
  CPU Freq: 240 MHz
  WiFi: MyNetwork (192.168.1.100)

> logs
[INFO] [WIFI] Connected to MyNetwork
[INFO] [WEBUI] WebUI started on port 80
[INFO] [CONSOLE] RemoteConsole started on port 23
[INFO] [SETUP] System initialization complete

> level 4
Log level set to: DEBUG

> quit
Goodbye!
Connection closed by foreign host.
```

## Configuration Options

### RemoteConsoleConfig

```cpp
RemoteConsoleConfig config;
config.enabled = true;              // Enable/disable console
config.port = 23;                   // Telnet port
config.requireAuth = false;         // Password authentication
config.password = "";               // Auth password (if requireAuth=true)
config.bufferSize = 500;            // Circular buffer size
config.allowCommands = true;        // Enable built-in commands
config.colorOutput = true;          // ANSI color codes
config.maxClients = 3;              // Max concurrent connections
config.defaultLogLevel = LOG_LEVEL_INFO;  // Initial log level
```

## Troubleshooting

### Can't connect via telnet

1. Check device IP address in serial monitor
2. Ensure device and computer are on same network
3. Try AP mode: connect to "DomoticsCore-Console" WiFi, then `telnet 192.168.4.1`

### No logs appearing

1. Check log level: `level 4` (DEBUG)
2. Clear filter: `filter` (no arguments)
3. Generate logs by accessing the web interface

### Connection drops

- Check `maxClients` setting
- Ensure stable WiFi connection
- Check firewall settings

## Notes

- Telnet is unencrypted - use only on trusted networks
- For production, consider enabling `requireAuth` and setting a strong password
- Log buffer is circular - oldest entries are removed when buffer is full
- Color output may not work in all telnet clients (disable with `colorOutput = false`)
