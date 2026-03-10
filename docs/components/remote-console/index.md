# DomoticsCore-RemoteConsole -- Documentation Index

> Generated: 2026-03-10 | Component version: 1.4.1 | Project type: embedded (header-only library)

## Component Summary

DomoticsCore-RemoteConsole is a Telnet-based remote debugging component for ESP32 and ESP8266. It provides real-time log streaming, an interactive command processor with extensible custom commands, a circular log buffer, password authentication, IP whitelisting, and optional WebUI integration. The component is entirely header-only (no `.cpp` files).

## Quick Reference

| Property      | Value                                                  |
|---------------|--------------------------------------------------------|
| Namespace     | `DomoticsCore::Components`                             |
| Registration  | `"RemoteConsole"`                                      |
| Headers       | `RemoteConsole.h`, `RemoteConsoleWebUI.h`              |
| Dependencies  | `DomoticsCore-Core >=1.4.0`, `DomoticsCore-Wifi >=1.4.0` |
| Platforms     | ESP32, ESP8266                                         |
| Default Port  | 23 (Telnet)                                            |
| Test Framework| Unity (native platform)                                |
| Tests         | 30 tests across 9 categories                           |

## Documentation Map

| Document | Purpose |
|----------|---------|
| [README.md](README.md) | Overview, features, quick start, security guidance |
| [Technical Reference](technical-reference.md) | Full API docs, all commands, config fields, WebUI routes |
| [Project Context](project-context.md) | AI-oriented context: file inventory, classes, dependencies, architecture notes, testing, known issues |

## Source Files

```
DomoticsCore-RemoteConsole/
  include/DomoticsCore/
    RemoteConsole.h              # Core component (714 lines)
    RemoteConsoleWebUI.h         # Optional WebUI provider (168 lines)
  examples/
    BasicRemoteConsole/          # Minimal example with custom commands
    RemoteConsoleWithWebUI/      # Full example with WebUI + AP fallback
  test/
    test_remoteconsole_component/
      test_remoteconsole_component.cpp  # 30 Unity tests
  library.json                   # PlatformIO manifest
  platformio.ini                 # Native test config
  README.md                      # Component-level README
```

## Getting Started

1. Add `DomoticsCore-Core` and `DomoticsCore-RemoteConsole` to `lib_deps` in your `platformio.ini`.
2. Connect WiFi using the HAL abstraction layer.
3. Create a `RemoteConsoleConfig`, configure as needed, and add the component to `Core`.
4. Connect via `telnet <device-ip> 23`.
5. See the [Quick Start](README.md#quick-start) section for a complete code example.

## AI-Assisted Development

For AI agents working on this component:
- Start with [Project Context](project-context.md) for a structured overview of files, classes, and conventions.
- Use [Technical Reference](technical-reference.md) for precise API signatures and behavior contracts.
- The component is header-only; all changes go in `include/DomoticsCore/RemoteConsole.h` or `RemoteConsoleWebUI.h`.
- All platform calls must go through `HAL::` abstractions (see Constitution compliance in Project Context).
- Run tests with `pio test -e native` from the component directory.
