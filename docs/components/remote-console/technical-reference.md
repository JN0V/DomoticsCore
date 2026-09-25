# DomoticsCore-RemoteConsole -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## Table of Contents

- [Namespace and Headers](#namespace-and-headers)
- [RemoteConsoleConfig](#remoteconsoleconfig)
- [LogEntry](#logentry)
- [CommandHandler](#commandhandler)
- [RemoteConsoleComponent](#remoteconsolecomponent)
  - [Constructor and Destructor](#constructor-and-destructor)
  - [Lifecycle Methods](#lifecycle-methods)
  - [Public Methods](#public-methods)
- [Built-in Commands](#built-in-commands)
- [Custom Command Registration](#custom-command-registration)
- [Circular Buffer](#circular-buffer)
- [The lines the platform writes itself](#the-lines-the-platform-writes-itself)
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
| `password`       | `String`                      | `""`                         | The password required for authentication when `requireAuth` is `true`. Must be non-empty: `begin()` clears `requireAuth` and warns otherwise, and an empty `auth` line never authenticates. |
| `bufferSize`     | `uint32_t`                    | `DOMOTICS_LOG_BUFFER_SIZE`   | Maximum number of log entries in the circular buffer. Platform-specific: ESP32 = 64, ESP8266 = 20. |
| `allowCommands`  | `bool`                        | `true`                       | When `false`, all commands except `help` and `quit` are blocked with a "Commands are disabled" message. Useful for log-only monitoring sessions. |
| `authTimeoutMs`  | `uint32_t`                    | `10000`                      | Time in milliseconds an unauthenticated client has to authenticate before being disconnected. Only applies when `requireAuth` is `true`. Set to `0` to disable the timeout. |
| `allowedIPs`     | `std::vector<HAL::IPAddress>` | `{}` (empty = all allowed)   | IP whitelist. An empty vector permits all IPs.                    |
| `authDelayMaxMs` | `uint32_t`                    | `8000`                       | Cap of the wait a failed `auth` puts before the next attempt from the same address is read: 1 s after the first failure, doubling per consecutive failure up to this cap; one success or a minute without failures clears it. The attempt is held, never refused, and the client is never disconnected for failing. `0` disables the wait. |
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

### Constructor and Destructor

```cpp
RemoteConsoleComponent(const RemoteConsoleConfig& cfg = RemoteConsoleConfig());
~RemoteConsoleComponent();
```

The constructor accepts an optional configuration and registers all built-in commands. The destructor deletes the `WiFiServer` instance if it was allocated.

### Lifecycle Methods

These methods are called by the DomoticsCore `Core` engine and should not be invoked directly.

| Method                                                  | Description                                                                                  |
|---------------------------------------------------------|----------------------------------------------------------------------------------------------|
| `ComponentStatus begin()`                               | Registers the logger callback, installs the core-log capture, and opens the `WiFiServer` — or defers it when there is no IP stack yet (see below). |
| `void onComponentsReady(const ComponentRegistry&)`      | Called after all components are initialized. Displays the connection info (IP + port) if WiFi is connected. |
| `void loop()`                                           | Drains the core-log intake, opens a deferred server once an IP stack exists, processes pending reboot requests (non-blocking, 100 ms after flag set), accepts new clients (with IP whitelist and max-client checks), enforces authentication timeouts for unauthenticated clients, handles input from existing clients, and cleans up disconnected clients. Calls `clients.shrink_to_fit()` after any client disconnection to release memory. |
| `ComponentStatus shutdown()`                            | Sends a shutdown message to all connected clients, stops the server, and releases resources.  |

### Public Methods

#### `uint16_t getPort() const`

Returns the currently configured Telnet port.

#### `HAL::WiFiServer* getServer() const`

Returns a pointer to the underlying `WiFiServer` instance, or `nullptr` when the server has not been opened: before `begin()`, when the component is disabled, or while it is waiting for an IP stack.

#### Waiting for an IP stack

A listening socket needs lwIP. ESP8266 has it from boot; on ESP32 it appears with
the first network interface, whichever transport creates one. A console whose
`begin()` runs before that — registered without a WiFi component, or ahead of it —
opens nothing, says so once at INFO, and keeps `getServer()` at `nullptr`; `loop()`
opens the server on the first iteration where the platform reports a stack. The
port a `setPort()` chose meanwhile is the one it opens. Nothing else in the
component's behaviour changes: everything that does not need a socket — the log
buffer, the core-log capture, the commands — runs from `begin()` as before.

#### `LogLevel getLogLevel() const`

Returns the current runtime log level.

#### `bool setPort(uint16_t port)`

Changes the Telnet port at runtime. Returns `true` immediately without side effects if the new port matches the current port. Otherwise disconnects all clients (with a notification message), clears all per-client state, stops the old server, and restarts on the new port. Returns `false` if `port` is `0`. Also returns `true` without restarting the server if the component is disabled or the server has not been started yet.

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
| `level`           | `<0-5>`           | Sets the runtime log level. Without arguments, displays the current level. Levels: 0 = NONE, 1 = ERROR, 2 = WARN, 3 = INFO, 4 = DEBUG, 5 = VERBOSE. Digits only, the same range the WebUI field and `/api/console/loglevels` offer — `abc` is refused rather than read as 0, which would have silenced the log, and so is a trailing argument (`level 3 verbose`). |
| `filter`          | `<tag>` or empty  | Filters logs to show only the specified tag. Without arguments, clears the filter (shows all). |
| `info`            | (none)            | Displays system information: uptime, free heap, chip model/revision, CPU frequency, WiFi SSID, IP, and RSSI. |
| `heap`            | (none)            | Displays the current free heap in bytes.                                 |
| `auth`            | `<password>`      | Authenticates the client session. If `requireAuth` is `false`, responds with "Authentication not required." If the password matches, the client becomes authenticated and receives log output. Otherwise responds with "Authentication failed." |
| `reboot`          | (none)            | Sends "Rebooting..." to all clients, then sets a non-blocking reboot flag. The device restarts via `HAL::restart()` on the next `loop()` iteration after a 100 ms delay, allowing the message to flush. |
| `core`            | `on`, `off` or empty | Without arguments, reports the platform-line capture: how many intake slots it has and how many lines it has dropped, and whether the platform's SDK narrates. `on` and `off` switch that narration where the platform has a switch for it (ESP8266); elsewhere they answer that there is nothing to switch, because the platform's lines are captured either way. |
| `quit`            | (none)            | Sends "Goodbye!" and closes the client connection.                       |

Custom commands registered via `registerCommand()` also appear in the `help` output.

---

## Authentication Flow

When `requireAuth` is `true`, the following authentication flow applies:

1. **Connection**: A new client connects. The per-client auth state is set to `false`. The `clientConnectTime` is recorded.
2. **Welcome message**: The client sees "Authentication required. Use: auth <password>" instead of the standard welcome with recent logs.
3. **Command blocking**: All commands except `help`, `quit`, and `auth` are blocked with an "Authentication required" message.
4. **Log blocking**: Unauthenticated clients do not receive real-time log output.
5. **Authentication**: The client sends `auth <password>`. If the password matches `config.password`, the client's auth state is set to `true` and the client receives "Authentication successful!". Otherwise "Authentication failed.", and the address's wait starts: the next `auth` line from that address — on this connection or a new one — is held, unread, until 1 s has passed since the failure, 2 s after the second consecutive failure, 4, 8 … up to `authDelayMaxMs`; nothing else is read from a client while its attempt is held. A success, or a minute without failures, clears the address. The wait is remembered for at most four addresses at a time. The serial log names each failure and its client id.
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

The log buffer is a `std::vector<LogEntry>` whose slots are allocated once, at exactly `config.bufferSize`, when the first line arrives. The messages themselves are allocated as the lines come, so a device fills the buffer over its first hours and holds it steady from then on.

Behavior:

1. The first line reserves `bufferSize` slots — one allocation of `bufferSize × sizeof(LogEntry)` (40 bytes on ESP32), where growing by doubling would hold up to twice the slots and move the whole vector on the way. A large `bufferSize` is therefore one large block, taken at the first line.
2. While `logBufferCount < bufferSize`, entries are moved in.
3. Once the buffer is full, the oldest slot is destroyed and rebuilt from the new line, so it holds a block sized to that line. Assigning over it would not: an ESP32 `String` keeps its larger buffer through an assignment, even a move, and every slot would settle at the longest line it ever held.
4. `logBufferHead` advances as `(logBufferHead + 1) % bufferSize`.
5. `clearBuffer()` releases the slots with `shrink_to_fit()`; the next line reserves them again.

This design eliminates the memory leak that was previously observed with `std::deque`, where `pop_front()` did not reliably release memory on embedded platforms.

`bufferSize` defaults to `DOMOTICS_LOG_BUFFER_SIZE`: **64 lines on ESP32, 20 on
ESP8266**. `LogEntry` holds two `String`s, so a line longer than the small-string
threshold takes a heap block of its own. Measured through the component, a full
history of 120-character lines holds **11 796 bytes on a WROOM-32D** and **3 368 on
a nodemcuv2**, and the same history rewritten with 20-character lines holds 5 656
and 1 448. The messages are taken as lines arrive, so a device's free heap settles
over its first hours rather than at boot. The worst case — every line as long as
a `DLOG_*` line can be formatted — is held under 20 KB for the default, at compile
time. Platform lines share this history with the framework's own, and evict them
at the same rate. An application that wants more history, or less, sets
`bufferSize`.

---

## The lines the platform writes itself

The console carries what `DLOG_*` emits. The Arduino core, the vendor SDK and
ESP-IDF write their own lines straight to the UART, so a device reachable only
over the network could not show them — and that is where a failed OTA says
*which* error it hit. Those lines are now captured, drained in `loop()` and
shown like any other, under the tag `PLATFORM`.

There is no single sink to capture them from. Measured on a WROOM-32D and a
nodemcuv2:

| line | what writes it | where it is captured |
|---|---|---|
| `E (1591) gpio: io_num=99 can only be input` | a precompiled ESP-IDF library, through `esp_log_write` | `esp_log_set_vprintf`, chained to the handler it replaced |
| `[ 1638][E][Preferences.cpp:50] begin(): …` | the Arduino core, through `log_printf` and `ets_printf` | the character sink, `ets_install_putc1`, which writes each character on before counting it |
| `scandone` | the ESP8266 SDK | the same character sink, and only while SDK printing is on |

The two ESP32 sinks are disjoint: a line written through one is never seen by the
other, and an Arduino build remaps `ESP_LOGx` onto the core's path, so the
vprintf hook alone captures nothing the application itself writes. Both sinks
keep the serial port exactly as it was.

**On ESP8266 the SDK narrates nothing by default, and not by accident.**
`HardwareSerial::begin()` calls `end()`, which calls `uart_set_debug(UART_NO)`
and `system_set_os_print(0)`: a full failed join prints not one line. `core on`
switches it back on — for the console *and* the serial port — and costs about a
line a second while a join keeps failing, which is why the default is off and the
switch is a command rather than a setting. An application that calls
`Serial.setDebugOutput()` afterwards takes the character sink back; enabling
again reinstalls it.

### The intake

A captured line arrives in whatever context the platform's logger ran in — a
task, or an interrupt — so the sink neither allocates nor writes to a client. It
copies the line into a fixed ring (`DOMOTICS_CORE_LOG_SLOTS` × 128 bytes: 16
slots on ESP32, 4 on ESP8266), and `loop()` moves up to four lines per pass into
the console's own buffer. A line longer than a slot keeps its 127-character head,
where its level and subject are. A ring nobody has drained refuses the newest
line and counts it; the count is reported once a minute and by the `core`
command, which also reports the lines skipped because the framework was printing
its own.

The capture starts when the component does, so the bootloader's own output and
anything ESP-IDF writes before `begin()` are not in it. On ESP8266 a gdbstub
session owns the same character sink; this capture does not check for one.

The level comes from the shape of the line, so an error stays greppable as one:
`E (…)` from ESP-IDF, `[ …][E][…]` from the Arduino core, and anything else —
the SDK's own narration, which carries no level — is information.

This framework's own lines are excluded at the source. On ESP32 `DLOG_*` goes out
through the Arduino core's logger, so the character sink would see every one of
them and the console would carry each twice, once under its component's tag and
once as a platform line; `Logger` brackets its own output with a suppression the
sink honours, through a HAL macro that costs nothing on a platform whose logger
writes elsewhere. The suppression is one flag for the whole process: a line
another context writes inside that window is lost with ours — a whole one counted
apart, as "ours ignored", and one caught mid-assembly dropped and counted with
the rest, because its middle would be missing.

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
[20010][E][PLATFORM] E (19972) gpio: io_num=99 can only be input
```

The tag `PLATFORM` marks a line the platform wrote and this component captured,
rather than one the framework emitted; `filter PLATFORM` shows only those, and
`filter CORE` still means this framework's own core.

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

- **`field=port`**: Calls `console->setPort()`. Returns `{"success":false,"error":"Invalid port"}` for a malformed or out-of-range value. The value is digits only and inside 1..65535 — `"2424x"` is refused, not read as 2424. The same rule and range 0..5 apply to `field=log_level`.
- **`field=log_level`**: Calls `console->setLogLevel()`. Returns `{"success":false,"error":"Invalid log level"}` for values outside 0-5.

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
