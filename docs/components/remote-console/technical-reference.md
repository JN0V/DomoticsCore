# DomoticsCore-RemoteConsole -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## Table of Contents

- [Namespace and Headers](#namespace-and-headers)
- [RemoteConsoleConfig](#remoteconsoleconfig)
- [LogEntry](#logentry)
- [CommandHandler](#commandhandler)
- [RemoteConsoleComponent](#remoteconsolecomponent)
  - [Constructor](#constructor)
  - [Lifecycle Methods](#lifecycle-methods)
  - [Public Methods](#public-methods)
- [Built-in Commands](#built-in-commands)
- [Custom Command Registration](#custom-command-registration)
- [Circular Buffer](#circular-buffer)
- [ANSI Color Codes](#ansi-color-codes)
- [Log Format](#log-format)
- [WebUI Integration](#webui-integration)
  - [RemoteConsoleWebUI](#remoteconsolewebui)
  - [WebUI API Routes](#webui-api-routes)
  - [WebUI Fields](#webui-fields)

---

## Namespace and Headers

```cpp
#include <DomoticsCore/RemoteConsole.h>      // Core component
#include <DomoticsCore/RemoteConsoleWebUI.h>  // Optional WebUI provider
```

All types reside in `DomoticsCore::Components`. The WebUI provider lives in `DomoticsCore::Components::WebUI`.

---

## RemoteConsoleConfig

Configuration structure passed to the component constructor. All fields have sensible defaults.

```cpp
struct RemoteConsoleConfig {
    bool enabled = true;
    uint16_t port = 23;
    bool requireAuth = false;
    String password = "";
    uint32_t bufferSize = DOMOTICS_LOG_BUFFER_SIZE;
    bool allowCommands = true;
    uint32_t authTimeoutMs = 10000;
    std::vector<HAL::IPAddress> allowedIPs;
    bool colorOutput = true;
    uint32_t maxClients = 3;
    LogLevel defaultLogLevel = LOG_LEVEL_INFO;
};
```

| Field            | Type                          | Default                      | Description                                                       |
|------------------|-------------------------------|------------------------------|-------------------------------------------------------------------|
| `enabled`        | `bool`                        | `true`                       | Enable or disable the Telnet server entirely.                     |
| `port`           | `uint16_t`                    | `23`                         | TCP port for the Telnet server.                                   |
| `requireAuth`    | `bool`                        | `false`                      | When `true`, new clients must authenticate with `auth <password>` before executing commands (except `help` and `quit`). Unauthenticated clients do not receive log output. |
| `password`       | `String`                      | `""`                         | The password required for authentication when `requireAuth` is `true`. |
| `bufferSize`     | `uint32_t`                    | `DOMOTICS_LOG_BUFFER_SIZE`   | Maximum number of log entries in the circular buffer. Platform-specific: ESP32 = 100, ESP8266 = 5. |
| `allowCommands`  | `bool`                        | `true`                       | When `false`, all commands except `help` and `quit` are blocked with a "Commands are disabled" message. Useful for log-only monitoring sessions. |
| `authTimeoutMs`  | `uint32_t`                    | `10000`                      | Time in milliseconds an unauthenticated client has to authenticate before being disconnected. Only applies when `requireAuth` is `true`. Set to `0` to disable the timeout. |
| `allowedIPs`     | `std::vector<HAL::IPAddress>` | `{}` (empty = all allowed)   | IP whitelist. An empty vector permits all IPs.                    |
| `colorOutput`    | `bool`                        | `true`                       | Emit ANSI escape codes for colored log output.                    |
| `maxClients`     | `uint32_t`                    | `3`                          | Maximum number of concurrent Telnet connections.                  |
| `defaultLogLevel`| `LogLevel`                    | `LOG_LEVEL_INFO`             | Initial log level for the console session.                        |

---

## LogEntry

Compact structure stored in the circular buffer.

```cpp
struct LogEntry {
    uint32_t timestamp;   // millis() at log time
    LogLevel level;       // Log severity
    String   tag;         // Component tag (e.g., "MQTT", "WIFI")
    String   message;     // Log message body

    LogEntry();
    LogEntry(uint32_t ts, LogLevel lvl, const char* t, const char* msg);
};
```

---

## CommandHandler

Type alias for command callback functions.

```cpp
typedef std::function<String(const String& args)> CommandHandler;
```

A handler receives the argument string (everything after the command name) and returns a `String` to display to the client. Returning `"QUIT"` is reserved for the disconnect signal.

---

## RemoteConsoleComponent

Inherits from `IComponent`. Metadata:

| Property      | Value                                           |
|---------------|-------------------------------------------------|
| `name`        | `"RemoteConsole"`                               |
| `version`     | `"1.4.1"`                                       |
| `author`      | `"DomoticsCore"`                                |
| `category`    | `"Debug"`                                       |
| `tags`        | `{"telnet", "console", "debug", "logging"}`     |

### Constructor

```cpp
RemoteConsoleComponent(const RemoteConsoleConfig& cfg = RemoteConsoleConfig());
```

Accepts an optional configuration. Registers all built-in commands during construction.

### Lifecycle Methods

These methods are called by the DomoticsCore `Core` engine and should not be invoked directly.

| Method                                                  | Description                                                                                  |
|---------------------------------------------------------|----------------------------------------------------------------------------------------------|
| `ComponentStatus begin()`                               | Registers the logger callback, creates the `WiFiServer`, and begins listening.               |
| `void onComponentsReady(const ComponentRegistry&)`      | Called after all components are initialized. Displays the connection info (IP + port) if WiFi is connected. |
| `void loop()`                                           | Processes pending reboot requests, accepts new clients (with IP whitelist and max-client checks), enforces authentication timeouts for unauthenticated clients, handles input from existing clients, and cleans up disconnected clients. |
| `ComponentStatus shutdown()`                            | Sends a shutdown message to all connected clients, stops the server, and releases resources.  |

### Public Methods

#### `uint16_t getPort() const`

Returns the currently configured Telnet port.

#### `HAL::WiFiServer* getServer() const`

Returns a pointer to the underlying `WiFiServer` instance, or `nullptr` if the server has not been started yet (i.e., before `begin()` is called or when the component is disabled).

#### `LogLevel getLogLevel() const`

Returns the current runtime log level.

#### `bool setPort(uint16_t port)`

Changes the Telnet port at runtime. Disconnects all clients, stops the old server, and restarts on the new port. Returns `false` if `port` is `0`.

#### `void log(LogLevel level, const char* tag, const char* message)`

Logs a message to the circular buffer and streams it to all connected clients. Messages above the current log level or not matching the active tag filter are silently discarded.

#### `void registerCommand(const String& cmd, CommandHandler handler)`

Registers a custom command. If a command with the same name already exists, it is overwritten.

#### `void setLogLevel(LogLevel level)`

Changes the runtime log level. Only messages at or below this level will be buffered and streamed.

#### `void setTagFilter(const std::vector<String>& tags)`

Sets a tag filter. Only log entries whose tag matches one of the provided strings will be shown. Pass an empty vector to clear the filter and show all tags.

#### `void clearBuffer()`

Clears the circular log buffer and releases the underlying memory back to the heap via `shrink_to_fit()`.

#### `std::vector<LogEntry> getRecentLogs(uint32_t count = 100)`

Returns the most recent `count` log entries from the circular buffer, ordered oldest to newest.

---

## Built-in Commands

All commands are case-insensitive. Arguments are separated from the command by a space.

| Command           | Arguments         | Description                                                              |
|-------------------|-------------------|--------------------------------------------------------------------------|
| `help`            | (none)            | Lists all available commands, including registered custom commands.       |
| `clear`           | (none)            | Clears the circular log buffer and releases memory.                      |
| `level`           | `<0-4>`           | Sets the runtime log level. Without arguments, displays the current level. Levels: 0 = NONE, 1 = ERROR, 2 = WARN, 3 = INFO, 4 = DEBUG. |
| `filter`          | `<tag>` or empty  | Filters logs to show only the specified tag. Without arguments, clears the filter (shows all). |
| `info`            | (none)            | Displays system information: uptime, free heap, chip model/revision, CPU frequency, WiFi SSID, IP, and RSSI. |
| `heap`            | (none)            | Displays the current free heap in bytes.                                 |
| `auth`            | `<password>`      | Authenticates the client session. If `requireAuth` is `false`, responds with "Authentication not required." If the password matches, the client becomes authenticated and receives log output. Otherwise responds with "Authentication failed." |
| `reboot`          | (none)            | Sends "Rebooting..." to all clients, waits 100 ms, then restarts the device via `HAL::restart()`. |
| `quit`            | (none)            | Sends "Goodbye!" and closes the client connection.                       |

Custom commands registered via `registerCommand()` also appear in the `help` output.

---

## Authentication Flow

When `requireAuth` is `true`, the following authentication flow applies:

1. **Connection**: A new client connects. The per-client auth state is set to `false`. The `clientConnectTime` is recorded.
2. **Welcome message**: The client sees "Authentication required. Use: auth <password>" instead of the standard welcome with recent logs.
3. **Command blocking**: All commands except `help`, `quit`, and `auth` are blocked with an "Authentication required" message.
4. **Log blocking**: Unauthenticated clients do not receive real-time log output.
5. **Authentication**: The client sends `auth <password>`. If the password matches `config.password`, the client's auth state is set to `true` and the client receives "Authentication successful!".
6. **Auth timeout**: On each `loop()` iteration, any unauthenticated client whose connection age exceeds `authTimeoutMs` is sent "Authentication timeout. Disconnecting." and disconnected. Set `authTimeoutMs = 0` to disable this timeout.

When `requireAuth` is `false` (default), all clients are automatically marked as authenticated on connect and receive the full welcome message including recent log history.

---

## Custom Command Registration

Register commands before or after `core.begin()`. The handler receives the argument portion of the user input and returns a `String` to display.

```cpp
auto console = std::make_unique<RemoteConsoleComponent>(config);
auto* ptr = console.get();

ptr->registerCommand("sensors", [](const String& args) {
    String result = "\nSensor Values:\n";
    result += "  Temperature: " + String(readTemp()) + " C\n";
    return result;
});

ptr->registerCommand("relay", [](const String& args) {
    if (args == "on") {
        digitalWrite(RELAY_PIN, HIGH);
        return String("Relay ON\n");
    }
    if (args == "off") {
        digitalWrite(RELAY_PIN, LOW);
        return String("Relay OFF\n");
    }
    return String("Usage: relay <on|off>\n");
});

core.addComponent(std::move(console));
```

---

## Circular Buffer

The log buffer uses a `std::vector<LogEntry>` that grows lazily up to `config.bufferSize`. This avoids a large upfront heap allocation that could cause an OOM crash on startup (especially on ESP8266).

Behavior:

1. While `logBufferCount < bufferSize`, entries are appended with `push_back()`.
2. Once the buffer reaches capacity, the oldest entry is overwritten in-place at `logBufferHead`.
3. `logBufferHead` always advances as `(logBufferHead + 1) % bufferSize`.
4. `clearBuffer()` calls `shrink_to_fit()` to return memory to the heap.

This design eliminates the memory leak that was previously observed with `std::deque`, where `pop_front()` did not reliably release memory on embedded platforms.

---

## ANSI Color Codes

When `config.colorOutput` is `true`, each log line is wrapped with ANSI escape sequences:

| Log Level   | Color  | Escape Code     |
|-------------|--------|-----------------|
| `ERROR`     | Red    | `\033[31m`      |
| `WARN`      | Yellow | `\033[33m`      |
| `INFO`      | Green  | `\033[32m`      |
| `DEBUG`     | Cyan   | `\033[36m`      |

All lines are terminated with the reset code `\033[0m`. Disable color output with `config.colorOutput = false` for clients that do not support ANSI escapes.

---

## Log Format

Each log line follows this structure:

```
[<timestamp>][<LEVEL>][<TAG>] <message>
```

Where `<LEVEL>` is a single-character abbreviation: `E`, `W`, `I`, `D`, or `NONE`.

Example:

```
[12345][I][MQTT] Connected to broker
[12890][W][WIFI] Signal weak: -72 dBm
[13001][E][APP] Sensor read failed
```

---

## WebUI Integration

### RemoteConsoleWebUI

The optional `RemoteConsoleWebUI` class (in `RemoteConsoleWebUI.h`) extends `CachingWebUIProvider` to expose the console status and configuration in the DomoticsCore web interface.

```cpp
#include <DomoticsCore/RemoteConsoleWebUI.h>

// After core.begin():
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* console = core.getComponent<RemoteConsoleComponent>("RemoteConsole");
if (webui && console) {
    auto* provider = new DomoticsCore::Components::WebUI::RemoteConsoleWebUI(console);
    webui->registerProviderWithComponent(provider, console);
    provider->init(webui);
}
```

### WebUI API Routes

| Route                      | Method | Description                                  |
|----------------------------|--------|----------------------------------------------|
| `/api/console/loglevels`   | GET    | Returns a JSON array of log level options: `[{"value":"0","label":"NONE"}, ..., {"value":"5","label":"VERBOSE"}]` |

### WebUI Fields

The `console_settings` context exposes the following fields with a 5-second real-time polling interval:

| Field ID    | Label     | Type      | Description                                         |
|-------------|-----------|-----------|-----------------------------------------------------|
| `status`    | Status    | Display   | Shows "Active" or "Inactive" (read-only).           |
| `connect`   | Connect   | Display   | Shows the `telnet <ip> <port>` connection string (read-only). |
| `port`      | Port      | Number    | Current Telnet port. Writable via POST (1-65535).   |
| `log_level` | Log level | Select    | Current log level. Writable via POST (0-5). Options loaded from `/api/console/loglevels`. |

### WebUI POST Handling

POST requests to `console_settings` accept `field` and `value` parameters:

- **`field=port`**: Calls `console->setPort()`. Returns `{"success":false}` for invalid or out-of-range values.
- **`field=log_level`**: Calls `console->setLogLevel()`. Returns `{"success":false}` for values outside 0-5.

The `hasDataChanged()` method tracks state via `LazyState<ConsoleUIState>` to avoid redundant JSON serialization when nothing has changed.

---

## Memory Considerations

| Resource                          | Approximate Size                        |
|-----------------------------------|-----------------------------------------|
| Flash (component + WiFiServer)    | ~60 KB                                  |
| RAM base                          | ~5 KB                                   |
| RAM per buffer entry              | ~100 bytes                              |
| Default ESP32 buffer (100 entries)| ~10 KB                                  |
| Default ESP8266 buffer (5 entries)| ~500 bytes                              |
