# DomoticsCore-RemoteConsole -- Project Context

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides structured context for AI assistants and developers working on the RemoteConsole component.

---

## Identity

| Property     | Value                                                                   |
|--------------|-------------------------------------------------------------------------|
| Name         | `DomoticsCore-RemoteConsole`                                            |
| Version      | `1.4.1`                                                                 |
| License      | MIT                                                                     |
| Author       | JN0V                                                                    |
| Repository   | `https://github.com/JN0V/DomoticsCore.git`                             |
| Frameworks   | Arduino                                                                 |
| Platforms    | `espressif32`, `espressif8266`                                          |
| Category     | Debug                                                                   |
| Keywords     | `esp32`, `domotics`, `telnet`, `console`, `debug`, `logging`            |

---

## File Inventory

```
DomoticsCore-RemoteConsole/
  include/DomoticsCore/
    RemoteConsole.h              # Main component: RemoteConsoleComponent, RemoteConsoleConfig, LogEntry
    RemoteConsoleWebUI.h         # Optional WebUI provider: RemoteConsoleWebUI
  examples/
    BasicRemoteConsole/
      src/main.cpp               # Minimal example with custom commands
      platformio.ini
    RemoteConsoleWithWebUI/
      src/main.cpp               # Full example with WebUI integration
      platformio.ini
      README.md                  # Example-specific documentation
  test/
    test_remoteconsole_component/
      test_remoteconsole_component.cpp  # Unity tests: creation, config, lifecycle, memory leak
  library.json                   # PlatformIO library manifest
  platformio.ini                 # Native test environment config
  README.md                      # Component README
```

Total source files: 2 headers, 0 implementation files (header-only component).

---

## Key Classes and Types

### `DomoticsCore::Components::RemoteConsoleComponent`
- **Inherits**: `IComponent`
- **Role**: Telnet server that streams logs and processes commands.
- **Lifecycle**: `begin()` -> `onComponentsReady()` -> `loop()` (repeated) -> `shutdown()`
- **Header**: `include/DomoticsCore/RemoteConsole.h`

### `DomoticsCore::Components::RemoteConsoleConfig`
- **Role**: Configuration struct with defaults for all console settings.
- **Header**: `include/DomoticsCore/RemoteConsole.h`
- **Caveat**: The fields `requireAuth`, `password`, and `allowCommands` are declared in the struct but **NOT enforced** at runtime. Authentication is not implemented and command gating is not checked. Only `allowedIPs` is enforced for access control.

### `DomoticsCore::Components::LogEntry`
- **Role**: Single log entry stored in the circular buffer (timestamp, level, tag, message).
- **Header**: `include/DomoticsCore/RemoteConsole.h`

### `DomoticsCore::Components::CommandHandler`
- **Role**: `std::function<String(const String& args)>` typedef for command callbacks.
- **Header**: `include/DomoticsCore/RemoteConsole.h`

### `DomoticsCore::Components::WebUI::RemoteConsoleWebUI`
- **Inherits**: `CachingWebUIProvider`
- **Role**: Optional WebUI provider that exposes console status and configuration to the browser.
- **Header**: `include/DomoticsCore/RemoteConsoleWebUI.h`

---

## Dependencies

Declared in `library.json`:

| Dependency            | Version     | Purpose                                     |
|-----------------------|-------------|---------------------------------------------|
| `DomoticsCore-Core`   | `>=1.4.0`  | `IComponent`, `Core`, `Logger`, `Platform_HAL` |
| `DomoticsCore-Wifi`   | `>=1.4.0`  | `Wifi_HAL`, `WiFiServer_HAL`, `WiFiClient`  |

Optional (for WebUI integration):

| Dependency            | Purpose                                     |
|-----------------------|---------------------------------------------|
| `DomoticsCore-WebUI`  | `IWebUIProvider`, `CachingWebUIProvider`, `WebUIComponent` |
| `ArduinoJson`         | JSON serialization in WebUI data methods    |

---

## Architecture Notes

- **Header-only**: The component has no `.cpp` files. All logic resides in `RemoteConsole.h` and `RemoteConsoleWebUI.h`.
- **Lazy buffer allocation**: The circular buffer (`std::vector<LogEntry>`) grows on demand via `push_back()` up to `bufferSize`, then overwrites in-place. This prevents OOM on startup, particularly on ESP8266 with its limited heap.
- **Logger integration**: Uses `LoggerCallbacks::addCallback()` to tap into the `DLOG_*` macro system without redefining any macros.
- **Client management**: Each client has a per-IP command buffer (`std::map<uint32_t, String>`). Telnet negotiation bytes and non-printable characters are silently discarded.
- **WebUI state caching**: `RemoteConsoleWebUI` uses `LazyState<ConsoleUIState>` to avoid reserializing JSON when the underlying data has not changed.

---

## Conventions

- **Component registration name**: `"RemoteConsole"` (set in constructor via `metadata.name`).
- **Log tag**: `LOG_CONSOLE` is used for all internal logging.
- **Command names**: Lowercase, single-word. Arguments separated by space.
- **Return protocol**: Command handlers return a `String`. The special return value `"QUIT"` triggers client disconnection.
- **Port 0 rejected**: `setPort(0)` returns `false` without side effects.

---

## Testing

Tests reside in `test/test_remoteconsole_component/test_remoteconsole_component.cpp` and use the Unity test framework. The test suite runs on the `native` platform.

Test categories:
- **Creation** -- Default and custom config construction.
- **Config defaults** -- Verifies all `RemoteConsoleConfig` default values.
- **Lifecycle** -- `begin()`, `loop()`, `shutdown()` sequences.
- **Dependencies** -- Confirms no hard dependencies (WiFi is optional at init time).
- **Edge cases** -- Zero buffer size, zero max clients, empty password, disabled color/commands.
- **Memory leak** -- Rotates 5000 log entries through a 100-entry buffer and asserts less than 1 byte leaked per entry. Also tests rapid fill/clear cycles (100 iterations).

---

## Constitution Compliance

This component adheres to the DomoticsCore Constitution:

- **SOLID**: Single responsibility (Telnet server + log buffer). Open for extension via `registerCommand()`. Depends on `IComponent` abstraction.
- **HAL abstraction**: All platform calls go through `HAL::Platform`, `HAL::WiFiHAL`, `HAL::WiFiServer`, and `HAL::WiFiClient`. No direct Arduino or ESP-IDF calls.
- **Memory safety**: Circular buffer with lazy growth prevents OOM. `clearBuffer()` uses `shrink_to_fit()` to release memory. Memory leak tests validate long-running stability.
- **Component lifecycle**: Implements `begin()`, `loop()`, `shutdown()`, and `onComponentsReady()` as required by `IComponent`.
- **Logging**: Uses `DLOG_*` macros with the `LOG_CONSOLE` tag throughout.
