# DomoticsCore Documentation

## Overview

Welcome to the DomoticsCore documentation. DomoticsCore is a modular, production-ready IoT framework for ESP32, ESP32-C3, and ESP8266 microcontrollers. It provides a component-based architecture where each feature (WiFi, MQTT, OTA, Web UI, etc.) is an independent, self-contained module.

> **All development MUST comply with the [DomoticsCore Constitution](../.specify/memory/constitution.md).** Read it before making any code change.

---

## Quick Navigation

| What you need | Where to go |
|---------------|-------------|
| First time? | [Getting Started Guide](getting-started.md) |
| Understand the design | [Architecture Guide](architecture.md) |
| AI agent context | [Project Context](project-context.md) |
| Full technical reference | [Technical Reference](technical-reference.md) |
| Specific component | [Component Documentation](#component-documentation) below |
| Create a component | [Custom Components Guide](guides/custom-components.md) |
| Add a web interface | [WebUI Developer Guide](guides/webui-developer.md) |

---

## Documentation Index

### Global Documentation

| Document | Description |
|----------|-------------|
| [Project Context](project-context.md) | AI-agent context for the whole project |
| [Technical Reference](technical-reference.md) | Cross-cutting technical reference |
| [Getting Started](getting-started.md) | Tutorial from installation to first project |
| [Architecture Guide](architecture.md) | Framework design, patterns, and best practices |

### Architecture Guides

- [HAL Architecture](architecture/hal-architecture.md) — Hardware Abstraction Layer routing pattern
- [Component Lifecycle](architecture/component-lifecycle.md) — Lifecycle states and methods
- [EventBus Patterns](architecture/eventbus-patterns.md) — Publish/subscribe and sticky events
- [Component Configuration](architecture/component-configuration-pattern.md) — Configuration patterns

### Developer Guides

- [Custom Components](guides/custom-components.md) — Creating your own components
- [WebUI Development](guides/webui-developer.md) — Adding web interfaces to components
- [WebUI State Tracking](guides/webui-state-tracking.md) — Efficient state management with LazyState

### Reference

- [EventBus Architecture](reference/eventbus-architecture.md) — Complete EventBus API and patterns

### Reliability

- [Storage & Boot Diagnostics](reliability/storage-verbosity-and-boot-diagnostics.md) — Persistent boot diagnostics

---

## Component Documentation

Each component has 3 documentation files in `docs/components/{name}/`:
- **README.md** — Lightweight overview, quick start, what it does
- **technical-reference.md** — Full API reference, configuration, internals
- **project-context.md** — AI-agent context for working on this component

### Core (`DomoticsCore-Core/`)
> Component registry, lifecycle management, EventBus, logging, timers, memory management

- [Overview](components/core/README.md) | [Technical Reference](components/core/technical-reference.md) | [Project Context](components/core/project-context.md)
- Source docs: [README](../DomoticsCore-Core/README.md), [Logging Guide](../DomoticsCore-Core/LOGGING.md)

### WiFi (`DomoticsCore-Wifi/`)
> Non-blocking WiFi STA/AP management with async scanning

- [Overview](components/wifi/README.md) | [Technical Reference](components/wifi/technical-reference.md) | [Project Context](components/wifi/project-context.md)

### WebUI (`DomoticsCore-WebUI/`)
> Web interface with WebSocket real-time updates, REST API, provider pattern

- [Overview](components/webui/README.md) | [Technical Reference](components/webui/technical-reference.md) | [Project Context](components/webui/project-context.md)

### MQTT (`DomoticsCore-MQTT/`)
> MQTT client with QoS, auto-reconnection, message queuing, wildcards

- [Overview](components/mqtt/README.md) | [Technical Reference](components/mqtt/technical-reference.md) | [Project Context](components/mqtt/project-context.md)
- Source docs: [Specifications](../DomoticsCore-MQTT/SPECIFICATIONS.md), [State Machine](../DomoticsCore-MQTT/STATE_MACHINE.md)

### Home Assistant (`DomoticsCore-HomeAssistant/`)
> MQTT Discovery integration with automatic entity registration

- [Overview](components/home-assistant/README.md) | [Technical Reference](components/home-assistant/technical-reference.md) | [Project Context](components/home-assistant/project-context.md)
- Source docs: [Specifications](../DomoticsCore-HomeAssistant/SPECIFICATIONS.md)

### NTP (`DomoticsCore-NTP/`)
> Network time synchronization with timezone and DST support

- [Overview](components/ntp/README.md) | [Technical Reference](components/ntp/technical-reference.md) | [Project Context](components/ntp/project-context.md)
- Source docs: [Specifications](../DomoticsCore-NTP/SPECIFICATIONS.md)

### OTA (`DomoticsCore-OTA/`)
> Over-the-air firmware updates with HTTPS, manifest, and progress tracking

- [Overview](components/ota/README.md) | [Technical Reference](components/ota/technical-reference.md) | [Project Context](components/ota/project-context.md)

### Storage (`DomoticsCore-Storage/`)
> Persistent key-value storage with namespace isolation (NVS / LittleFS)

- [Overview](components/storage/README.md) | [Technical Reference](components/storage/technical-reference.md) | [Project Context](components/storage/project-context.md)

### LED (`DomoticsCore-LED/`)
> Visual status indicators with 6 effects (Blink, Fade, Pulse, Rainbow, Breathing, Solid)

- [Overview](components/led/README.md) | [Technical Reference](components/led/technical-reference.md) | [Project Context](components/led/project-context.md)

### RemoteConsole (`DomoticsCore-RemoteConsole/`)
> Telnet debugging console with log streaming, ANSI colors, custom commands

- [Overview](components/remote-console/README.md) | [Technical Reference](components/remote-console/technical-reference.md) | [Project Context](components/remote-console/project-context.md)

### SystemInfo (`DomoticsCore-SystemInfo/`)
> System metrics: heap, uptime, CPU frequency, chip model, boot diagnostics

- [Overview](components/system-info/README.md) | [Technical Reference](components/system-info/technical-reference.md) | [Project Context](components/system-info/project-context.md)

### System (`DomoticsCore-System/`)
> Meta-orchestrator that assembles all components into a ready-to-use system

- [Overview](components/system/README.md) | [Technical Reference](components/system/technical-reference.md) | [Project Context](components/system/project-context.md)

---

## Architecture Overview

```
DomoticsCore Framework
├── Core                      # Component registry, lifecycle, MemoryManager
├── Components
│   ├── WiFi                 # Network connectivity
│   ├── WebUI                # Web interface (WebSocket + REST API)
│   ├── NTP                  # Time synchronization
│   ├── MQTT                 # MQTT client
│   ├── HomeAssistant        # HA integration
│   ├── OTA                  # Over-the-air updates
│   ├── LED                  # Status LED control
│   ├── Storage              # Persistent storage (NVS / LittleFS)
│   ├── RemoteConsole        # Telnet console with WebUI
│   ├── SystemInfo           # System information + boot diagnostics
│   └── System               # Complete system integration
│
├── HAL                      # Hardware Abstraction Layer
│   ├── Platform_HAL.h       # Platform detection and system functions
│   ├── Wifi_HAL.h           # Unified WiFi interface
│   ├── Storage_HAL.h        # Key-value storage abstraction
│   ├── NTP_HAL.h            # Time synchronization
│   └── Update_HAL.h         # OTA update abstraction
│
├── Testing                  # Test infrastructure
│   └── HeapTracker          # Memory leak detection (native + hardware)
│
└── Utils                    # Utilities (Timer, Logger, EventBus, etc.)
```

### Dependency Graph

```
Core (foundation - no dependencies)
  ├── LED, Storage, SystemInfo, RemoteConsole, NTP, OTA
  ├── Wifi
  │   └── MQTT
  │       └── HomeAssistant
  └── WebUI (*WebUI providers for each component)

System (meta-orchestrator - depends on all)
```

---

## Documentation Structure

```
docs/
├── README.md                              # This file - documentation index
├── project-context.md                     # Global AI-agent context
├── technical-reference.md                 # Global technical reference
├── getting-started.md                     # Complete tutorial
├── architecture.md                        # Framework design
├── architecture/
│   ├── hal-architecture.md                # HAL routing pattern
│   ├── component-lifecycle.md             # Component lifecycle states
│   ├── eventbus-patterns.md               # EventBus patterns
│   └── component-configuration-pattern.md # Config patterns
├── guides/
│   ├── custom-components.md               # Component development
│   ├── webui-developer.md                 # WebUI integration
│   └── webui-state-tracking.md            # State management
├── reference/
│   └── eventbus-architecture.md           # EventBus API reference
├── reliability/
│   └── storage-verbosity-and-boot-diagnostics.md
└── components/                            # Per-component documentation
    ├── core/
    │   ├── README.md                      # Overview
    │   ├── technical-reference.md         # Full API
    │   └── project-context.md             # AI context
    ├── wifi/
    ├── webui/
    ├── mqtt/
    ├── home-assistant/
    ├── ntp/
    ├── ota/
    ├── storage/
    ├── led/
    ├── remote-console/
    ├── system-info/
    └── system/
```

---

## Quick Links

- **Main Repository**: [GitHub](https://github.com/JN0V/DomoticsCore)
- **Issues**: [GitHub Issues](https://github.com/JN0V/DomoticsCore/issues)
- **PlatformIO Registry**: [jn0v/DomoticsCore](https://registry.platformio.org/libraries/jn0v/DomoticsCore)
- **Changelog**: [CHANGELOG.md](../CHANGELOG.md)
- **Constitution**: [.specify/memory/constitution.md](../.specify/memory/constitution.md)
- **Code Remediation Roadmap**: [CODE-ROADMAP.md](CODE-ROADMAP.md)

---

**DomoticsCore Version:** 1.7.0 | **Documentation Last Updated:** 2026-03-04

**License:** See main repository [LICENSE](../LICENSE) file.
