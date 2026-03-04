# DomoticsCore - Project Context

> AI-agent context document. Provides the essential knowledge needed to work effectively on this codebase.

## Project Identity

- **Name**: DomoticsCore
- **Version**: 1.7.0
- **Type**: Embedded IoT library (PlatformIO / Arduino framework)
- **Platforms**: ESP32, ESP32-C3, ESP8266
- **Language**: C++ (header-only design)
- **License**: MIT
- **Repository**: https://github.com/JN0V/DomoticsCore

## What It Is

DomoticsCore is a modular, production-ready domotics framework for ESP microcontrollers. It provides a component-based architecture where each feature (WiFi, MQTT, OTA, Web UI, etc.) is an independent, self-contained module that plugs into a central runtime (`Core`). The framework emphasizes:

- **Zero-config operation**: Components auto-register, auto-resolve dependencies, and auto-initialize in the correct order.
- **Visual debugging**: LED patterns indicate system state even when software crashes.
- **Event-driven communication**: Components communicate via an `EventBus` without direct coupling.
- **Hardware abstraction**: A HAL layer enables the same code to run on ESP32, ESP8266, and native (testing) platforms.

## Repository Structure

```
DomoticsCore/
├── DomoticsCore-Core/           # Core runtime (registry, lifecycle, EventBus, logging, timers)
├── DomoticsCore-LED/            # LED effects (blink, fade, pulse, rainbow, breathing)
├── DomoticsCore-Wifi/           # WiFi STA/AP with async scanning
├── DomoticsCore-WebUI/          # Web interface + WebSocket + REST API
├── DomoticsCore-MQTT/           # MQTT client with QoS, queuing, reconnection
├── DomoticsCore-HomeAssistant/  # Home Assistant MQTT Discovery integration
├── DomoticsCore-NTP/            # Network time synchronization
├── DomoticsCore-OTA/            # Over-the-air firmware updates
├── DomoticsCore-Storage/        # Persistent key-value storage (NVS)
├── DomoticsCore-System/         # Meta-orchestrator (assembles all components)
├── DomoticsCore-SystemInfo/     # System metrics (heap, uptime, CPU)
├── DomoticsCore-RemoteConsole/  # Telnet debug console
├── docs/                        # Documentation (architecture, guides, reference)
├── examples/                    # Cross-component examples
├── tests/                       # Unit tests and mocks
├── library.json                 # PlatformIO library metadata
└── library.properties           # Arduino library metadata
```

## Dependency Graph

```
Core (foundation - no dependencies)
  ├── LED
  ├── Storage
  ├── SystemInfo
  ├── RemoteConsole
  ├── NTP
  ├── OTA
  ├── Wifi
  │   └── MQTT
  │       └── HomeAssistant
  └── WebUI
      └── *WebUI providers (one per component)

System (meta-orchestrator - depends on all)
```

## Key Architectural Patterns

### Header-Only Design
Almost all components are header-only (`.h` files). Exceptions: `Core` and `OTA` have `.cpp` files. Some components use an `_impl.h` pattern to separate declaration from implementation while staying header-only.

### Component Model
Every component extends `IComponent` and implements:
- `begin()` — initialization (called in dependency order)
- `loop()` — periodic updates
- `shutdown()` — cleanup
- `getDependencies()` — declares required components
- `getMetadata()` — name, version, description

### HAL (Hardware Abstraction Layer)
Platform-specific code is isolated in `*_HAL.h` files with compile-time routing:
- `*_ESP32.h` — ESP32/ESP32-C3 implementation
- `*_ESP8266.h` — ESP8266 implementation
- `*_Stub.h` — Native platform stub for testing

### EventBus
Instance-based, queue-based publish/subscribe messaging. Components publish events without knowing who listens. Events are queued on `publish()` and dispatched during `poll()` (called automatically by `Core::loop()`). Supports sticky events for late subscribers, wildcard subscriptions (`*` prefix matching), and backpressure (queue capped at 32 events). Topics use slash separators (e.g., `wifi/sta/connected`, `mqtt/message`).

### WebUI Provider Pattern
Each component optionally provides a `*WebUI` class implementing `IWebUIProvider`. Providers register with the `WebUIComponent` and contribute to 6 UI locations: `Dashboard`, `ComponentDetail`, `HeaderStatus`, `QuickControls`, `Settings`, `HeaderInfo`.

## External Dependencies

| Library | Version | Used By |
|---------|---------|---------|
| ArduinoJson | ^7.0.0 | MQTT, HomeAssistant, WebUI |
| ESPAsyncWebServer | ^3.8.0 | WebUI |
| AsyncTCP | ^3.4.8 | WebUI |
| PubSubClient | ^2.8 | MQTT |

## Build System

- **PlatformIO** is the primary build system
- `library.json` at root defines the library metadata and build flags
- Each component has its own `library.json` for standalone use
- Build script: `build_all_examples.sh`
- Test runner: `run_all_tests.sh`
- CI: GitHub Actions (`.github/workflows/`)

## Testing Strategy

- Native platform tests (no hardware required)
- Mocks in `tests/mocks/` for WiFi, MQTT, Storage, NTP, WebServer, EventBus
- `HeapTracker` for memory leak detection
- Component-level tests in each `DomoticsCore-*/test/` directory
- Framework-level tests in `tests/unit/`

## Constitution (NON-NEGOTIABLE)

**All development on DomoticsCore MUST comply with the project constitution** located at `.specify/memory/constitution.md`. The constitution supersedes all other development practices. Key non-negotiable principles:

- **TDD Mandatory**: Red-Green-Refactor. 100% coverage gate per phase. No untested code.
- **Memory Leak Prevention (ABSOLUTE PRIORITY)**: Heap stability verification, no String concatenation in loops, `shrink_to_fit()` after container operations, PROGMEM for constants.
- **HAL Isolation**: `#ifdef` platform directives FORBIDDEN everywhere except HAL files.
- **Non-Blocking**: `delay()` is FORBIDDEN (except in boot sequences). `loop()` must complete in < 10ms.
- **SOLID, KISS, YAGNI**: Applied rigorously to all code.
- **File Size Limit**: 800 lines maximum per file (excluding blanks/comments).
- **EventBus for Communication**: No direct component-to-component references.
- **Centralized Storage**: No direct Preferences/SPIFFS access.
- **Semantic Versioning**: Use `tools/bump_version.py`, never manual edits.
- **All documentation in English**.

Before making ANY code change, read the full constitution: `.specify/memory/constitution.md`

## Important Conventions

1. **Component registration**: Always use `core.addComponent()` — never instantiate components manually after Core is running.
2. **Naming**: Components are registered by their class name without "Component" suffix (e.g., `MQTTComponent` registers as `"MQTT"`).
3. **Configuration**: Each component has a `*Config` struct. Configuration is set before `begin()` is called.
4. **Logging**: Use `DLOG_I()`, `DLOG_W()`, `DLOG_E()`, `DLOG_D()`, `DLOG_V()` macros with a tag. Never use `Serial.println()` directly. Logging is macro-based and compile-time controlled via `CORE_DEBUG_LEVEL`.
5. **Memory**: ESP8266 has ~40KB free heap. Use `MemoryManager` profiles to adapt behavior per platform.
6. **Events**: Prefer EventBus over direct component references for cross-component communication.
7. **WebUI**: Never hold references to WebUI context objects — they are transient.

## Common Pitfalls

- **Do not mutate config after `begin()`** — some components snapshot config at initialization.
- **MQTT message ordering** — QoS 0 messages can be lost; use QoS 1+ for critical data.
- **ESP8266 AP+STA** — single radio means channel must match; use heap-aware mode.
- **Storage namespace length** — NVS keys are limited to 15 characters on ESP32.
- **WebSocket payload size** — large payloads must use `StreamingContextSerializer` for chunked sending.
- **OTA on ESP8266** — requires enough free flash for dual-partition scheme.

## Component Quick Reference

| Component | Lines | Header-Only | Has WebUI | Key Class |
|-----------|-------|-------------|-----------|-----------|
| Core | ~4,900 | No (has .cpp) | No | `Core`, `IComponent`, `EventBus` |
| WebUI | ~4,900 | Yes | N/A | `WebUIComponent`, `IWebUIProvider` |
| Wifi | ~1,900 | Yes | Yes | `WifiComponent` |
| MQTT | ~1,800 | Yes | Yes | `MQTTComponent` |
| System | ~1,500 | Yes | No | `System` |
| OTA | ~1,500 | No (has .cpp) | Yes | `OTAComponent` |
| Storage | ~1,300 | Yes | Yes | `StorageComponent` |
| HomeAssistant | ~1,200 | Yes | Yes | `HomeAssistantComponent` |
| NTP | ~1,100 | Yes | Yes | `NTPComponent` |
| RemoteConsole | ~800 | Yes | Yes | `RemoteConsoleComponent` |
| LED | ~700 | Yes | Yes | `LEDComponent` |
| SystemInfo | ~400 | Yes | Yes | `SystemInfoComponent` |

## Per-Component Context

Detailed project contexts for each component are available in:
`docs/components/{component-name}/project-context.md`

## Related Documentation

- [Architecture Guide](architecture.md) — Framework design patterns
- [Getting Started](getting-started.md) — Tutorial from zero to first project
- [HAL Architecture](architecture/hal-architecture.md) — Platform abstraction details
- [EventBus Patterns](architecture/eventbus-patterns.md) — Event system usage
- [Component Lifecycle](architecture/component-lifecycle.md) — Lifecycle states
