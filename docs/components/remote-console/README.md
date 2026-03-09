# DomoticsCore-RemoteConsole

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## Overview

DomoticsCore-RemoteConsole is a Telnet-based remote debugging component for ESP32 and ESP8266 devices. It provides real-time log streaming, an interactive command processor, and a circular log buffer -- all accessible over the network without a physical serial connection.

## Key Features

- **Telnet Server** -- Standard telnet protocol on configurable port (default 23).
- **Real-time Log Streaming** -- All `DLOG_*` macro output is captured via the logger callback system and streamed to connected clients.
- **Circular Log Buffer** -- Configurable size (platform-dependent default); lazily allocated to avoid OOM on startup. Oldest entries are overwritten when full.
- **Built-in Commands** -- `help`, `clear`, `level`, `filter`, `info`, `heap`, `reboot`, `auth`, `quit`.
- **Custom Commands** -- Register application-specific commands with `registerCommand()`.
- **ANSI Color Output** -- Color-coded log levels: red (ERROR), yellow (WARN), green (INFO), cyan (DEBUG).
- **Tag Filtering** -- Show only logs matching a specific tag at runtime.
- **Password Authentication** -- Optional per-client authentication with configurable timeout (`requireAuth`, `password`, `authTimeoutMs`).
- **IP Whitelist** -- Restrict access to specific IP addresses.
- **Multi-client** -- Up to 3 concurrent Telnet connections (configurable).
- **WebUI Integration** -- Optional `RemoteConsoleWebUI` provider for browser-based monitoring.

## Quick Start

```ini
# platformio.ini
lib_deps =
    DomoticsCore-Core
    DomoticsCore-RemoteConsole
```

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/RemoteConsole.h>

using namespace DomoticsCore::Components;

Core core;

void setup() {
    // Connect WiFi first, then:
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

Connect from any machine on the same network:

```bash
telnet <device-ip> 23
```

## Supported Platforms

| Platform      | Default Buffer Size |
|---------------|---------------------|
| ESP32         | 100 entries         |
| ESP8266       | 5 entries           |

## Security Notice

Telnet transmits data in plain text. Use this component only on trusted networks.

### Authentication

When `requireAuth` is `true`, new clients must authenticate using the `auth <password>` command before they can execute any command other than `help` and `quit`. Unauthenticated clients do not receive log output. A configurable timeout (`authTimeoutMs`, default 10 seconds) automatically disconnects clients that fail to authenticate in time. Set `authTimeoutMs = 0` to disable the timeout.

When `requireAuth` is `false` (default), clients are automatically authenticated on connect and receive the full welcome message with recent logs.

### Command Gating

When `allowCommands` is `false`, all commands except `help` and `quit` are blocked with a "Commands are disabled" message. This allows log-only monitoring sessions.

### IP Whitelist

The `allowedIPs` field restricts connections to a set of IP addresses. An empty list (default) allows all IPs. For production deployments, combine `allowedIPs` with `requireAuth` and network-level firewalling.

## Further Reading

- [Technical Reference](technical-reference.md) -- Full API documentation, all commands, configuration options, and WebUI details.
- [Project Context](project-context.md) -- AI-oriented context file with file inventory, dependencies, and conventions.
- [BasicRemoteConsole Example](../../../DomoticsCore-RemoteConsole/examples/BasicRemoteConsole/src/main.cpp)
- [RemoteConsoleWithWebUI Example](../../../DomoticsCore-RemoteConsole/examples/RemoteConsoleWithWebUI/src/main.cpp)

## License

MIT
