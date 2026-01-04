# Implementation Plan: ESP8266 Full Support

**Branch**: `002-esp8266-full-support` | **Date**: 2025-12-18 | **Spec**: [spec.md](spec.md)  
**Input**: Feature specification from `/specs/002-esp8266-full-support/spec.md`

## Summary

Extend ESP8266 support in DomoticsCore by validating each of the 12 components individually, from simplest to most complex. Each component must meet its RAM budget before proceeding. WebUI is mandatory and must work even if library adaptation is needed. Final validation on physical Wemos D1 Mini hardware with 24-hour stability test.

## Technical Context

**Language/Version**: C++14 (gnu++14), Arduino Framework, ESP8266 Arduino Core >= 3.0.0  
**Primary Dependencies**: ESP8266WiFi, ESPAsyncWebServer-esphome, PubSubClient ^2.8 (existing), ArduinoJson ^7.0.0, LittleFS  
**Storage**: LittleFS (JSON files, 2KB max document size)  
**Testing**: PlatformIO native tests, physical device validation per phase  
**Target Platform**: ESP8266 (Wemos D1 Mini, 4MB flash, 80KB RAM)  
**Project Type**: Embedded IoT library (multi-component architecture)  
**Performance Goals**: Boot <5s, loop <10ms, HTTP response <500ms, WebSocket <100ms  
**Constraints**: ≥20KB free heap after init, tiered low-heap response (<15KB warn, <10KB suspend WebUI, <5KB reboot)  
**Scale/Scope**: 12 components, ~40KB total RAM budget, 12 validation phases
**HAL Architecture**: Separate implementation files per platform (ESP32, ESP8266, ESP32-C3, ESP32-H2, etc.)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Checkpoint | Status |
|-----------|------------|--------|
| **I. SOLID** | SRP: Each file has single responsibility? | ✅ Existing HAL structure maintained |
| **I. SOLID** | Dependency Inversion: Uses IComponent/interfaces? | ✅ All components use IComponent |
| **II. TDD** | Test plan defined for each phase? | ✅ Per-phase validation in spec |
| **II. TDD** | 100% coverage gate documented? | ✅ Each component validated before next |
| **III. KISS** | Simplest solution chosen? No over-engineering? | ✅ Minimal changes to existing code |
| **IV. YAGNI** | No speculative features? Only what's needed now? | ✅ Only ESP8266 compatibility, no new features |
| **V. Performance** | Memory budget considered? Binary size estimated? | ✅ Per-component RAM budgets defined |
| **VI. EventBus** | Inter-component communication via EventBus only? | ✅ Existing EventBus architecture preserved |
| **VII. File Size** | All files < 800 lines? Target 200-500? | ✅ HAL files are small, focused |
| **VIII. Progressive** | One component at a time? Compatibility preserved? | ✅ 12-phase incremental validation |
| **IX. HAL** | Hardware-specific code in `*_HAL` files only? | ✅ Separate files per platform (*_ESP32.h, *_ESP8266.h) |
| **X. NonBlockingTimer** | No delay()? Uses Timer::NonBlockingDelay? | ✅ Existing Timer pattern maintained |
| **XI. Storage** | No direct Preferences access? Uses StorageComponent? | ✅ LittleFSStorage for ESP8266 |
| **XII. Multi-Registry** | Compatible with PlatformIO AND Arduino registries? | ✅ ESP8266 builds on both |
| **XIII. Anti-Pattern** | No early-init, singletons, god objects, circular deps? | ✅ ComponentRegistry manages lifecycle |
| **XIV. Versioning** | Using bump_version.py? Version consistency checked? | ✅ Will bump after validation complete |

**Embedded Constraints Check:**
- [x] Flash target: Core < 300KB, Full < 1MB (ESP8266 has 4MB)
- [x] RAM target: Core < 20KB, Full < 60KB (≥20KB free from 80KB = 60KB used max)
- [x] Loop latency < 10ms (existing architecture)
- [x] No blocking operations in main loop (NonBlockingTimer pattern)

## Project Structure

### Documentation (this feature)

```text
specs/002-esp8266-full-support/
├── plan.md              # This file
├── research.md          # Phase 0: Technology research
├── data-model.md        # Phase 1: Component/HAL structure
├── quickstart.md        # Phase 1: Getting started guide
├── contracts/           # Phase 1: API contracts (N/A for this feature)
├── checklists/          # Validation checklists
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
DomoticsCore/
├── DomoticsCore-Core/
│   └── include/DomoticsCore/
│       ├── Platform_HAL.h      # Platform detection (routing)
│       ├── Platform_ESP32.h    # ESP32 specifics
│       ├── Platform_ESP8266.h  # ESP8266 specifics (NEW)
│       ├── IComponent.h        # Component interface
│       ├── EventBus.h          # Event system
│       └── Utils/Timer.h       # NonBlockingTimer
├── DomoticsCore-Storage/
│   └── include/DomoticsCore/
│       ├── Storage_HAL.h       # Storage routing header
│       ├── Storage_ESP32.h     # Preferences-based (ESP32)
│       └── Storage_ESP8266.h   # LittleFS-based (NEW)
├── DomoticsCore-LED/           # Phase 2
├── DomoticsCore-Wifi/
│   └── include/DomoticsCore/
│       ├── Wifi_HAL.h          # WiFi routing header
│       ├── Wifi_ESP32.h        # ESP32WiFi
│       └── Wifi_ESP8266.h      # ESP8266WiFi (NEW)
├── DomoticsCore-NTP/
│   └── include/DomoticsCore/
│       ├── NTP_HAL.h           # NTP routing header
│       ├── NTP_ESP32.h         # esp_sntp
│       └── NTP_ESP8266.h       # configTime() (NEW)
├── DomoticsCore-SystemInfo/
│   └── include/DomoticsCore/
│       ├── SystemInfo_HAL.h    # SystemInfo routing header
│       ├── SystemInfo_ESP32.h  # ESP32 metrics
│       └── SystemInfo_ESP8266.h # ESP8266 metrics (NEW)
├── DomoticsCore-RemoteConsole/ # Phase 7
├── DomoticsCore-MQTT/          # Phase 8 (PubSubClient ^2.8, no changes)
├── DomoticsCore-OTA/           # Phase 9 (existing Update API, no changes)
├── DomoticsCore-HomeAssistant/ # Phase 10
├── DomoticsCore-WebUI/         # Phase 11 (ESPAsyncWebServer-esphome for ESP8266)
└── examples/
    └── ESP8266/                # New examples for ESP8266 validation
        ├── Phase1_Core/
        ├── Phase2_LED/
        ├── Phase3_Storage/
        └── ...
```

**Structure Decision**: Existing multi-component library structure with **new HAL architecture**:
- Each `*_HAL.h` becomes a **routing header** that includes platform-specific implementation
- Platform implementations in `*_ESP32.h`, `*_ESP8266.h` (and future `*_ESP32C3.h`, `*_ESP32H2.h`)
- Existing code refactored: ESP32 code moves to `*_ESP32.h`, ESP8266 code to `*_ESP8266.h`
- New ESP8266-specific examples under `examples/ESP8266/`

## Complexity Tracking

> No Constitution violations. All principles satisfied.
