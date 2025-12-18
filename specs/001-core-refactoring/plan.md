# Implementation Plan: DomoticsCore Library Refactoring

**Branch**: `001-core-refactoring` | **Date**: 2025-12-17 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-core-refactoring/spec.md`

## Summary

Systematic refactoring of 12 DomoticsCore components to achieve:
- 100% unit test coverage (non-HAL code)
- Full Constitution v1.4.0 compliance
- Rock-solid reliability and lifecycle orchestration
- Proper documentation and examples

**Approach**: Component-by-component refactoring in dependency order, starting with Core, using TDD (tests first).

## Technical Context

**Language/Version**: C++14 (Arduino/ESP-IDF compatible)
**Primary Dependencies**: PlatformIO, ESP32Async/ESPAsyncWebServer ^3.8.0, ESP32Async/AsyncTCP ^3.4.8, bblanchon/ArduinoJson ^7.0.0
**Storage**: ESP32 Preferences (NVS), abstracted via StorageComponent
**Testing**: PlatformIO Unity framework (`pio test -e native` for unit tests)
**Target Platform**: ESP32 (primary), ESP8266 (secondary)
**Project Type**: Embedded library (multi-component monorepo)
**Performance Goals**: Loop < 10ms, Boot < 5s, WebSocket < 100ms
**Constraints**: Core < 300KB flash / < 20KB RAM, Full < 1MB flash / < 60KB RAM
**Scale/Scope**: 12 components, ~28 examples, targeting PlatformIO + Arduino registries

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Checkpoint | Status |
|-----------|------------|--------|
| **I. SOLID** | SRP: Each file has single responsibility? | ☐ |
| **I. SOLID** | Dependency Inversion: Uses IComponent/interfaces? | ☐ |
| **II. TDD** | Test plan defined for each phase? | ☐ |
| **II. TDD** | 100% coverage gate documented? | ☐ |
| **III. KISS** | Simplest solution chosen? No over-engineering? | ☐ |
| **IV. YAGNI** | No speculative features? Only what's needed now? | ☐ |
| **V. Performance** | Memory budget considered? Binary size estimated? | ☐ |
| **VI. EventBus** | Inter-component communication via EventBus only? | ☐ |
| **VII. File Size** | All files < 800 lines? Target 200-500? | ☐ |
| **VIII. Progressive** | One component at a time? Compatibility preserved? | ☐ |
| **IX. HAL** | Hardware-specific code in `*_HAL` files only? | ☐ |
| **X. NonBlockingTimer** | No delay()? Uses Timer::NonBlockingDelay? | ☐ |
| **XI. Storage** | No direct Preferences access? Uses StorageComponent? | ☐ |
| **XII. Multi-Registry** | Compatible with PlatformIO AND Arduino registries? | ☐ |
| **XIII. Anti-Pattern** | No early-init, singletons, god objects, circular deps? | ☐ |
| **XIV. Versioning** | Using bump_version.py? Version consistency checked? | ☐ |

**Embedded Constraints Check:**
- [ ] Flash target: Core < 300KB, Full < 1MB
- [ ] RAM target: Core < 20KB, Full < 60KB
- [ ] Loop latency < 10ms
- [ ] No blocking operations in main loop

## Project Structure

### Documentation (this feature)

```text
specs/001-core-refactoring/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output (component entities)
├── quickstart.md        # Phase 1 output (refactoring guide)
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
DomoticsCore/
├── DomoticsCore-Core/           # Phase 1: Foundation
│   ├── include/DomoticsCore/
│   │   ├── Core.h
│   │   ├── IComponent.h
│   │   ├── EventBus.h
│   │   └── Utils/Timer.h
│   ├── src/
│   ├── examples/
│   │   ├── 01-CoreOnly/
│   │   └── 02-EventBusDemo/     # Merged from 3 examples
│   └── test/                    # NEW: Component-specific tests
├── DomoticsCore-Storage/        # Phase 2
├── DomoticsCore-LED/            # Phase 3
├── DomoticsCore-Wifi/           # Phase 4
├── DomoticsCore-NTP/            # Phase 5
├── DomoticsCore-MQTT/           # Phase 6
├── DomoticsCore-OTA/            # Phase 7
├── DomoticsCore-RemoteConsole/  # Phase 8
├── DomoticsCore-WebUI/          # Phase 9
├── DomoticsCore-SystemInfo/     # Phase 10
├── DomoticsCore-HomeAssistant/  # Phase 11
├── DomoticsCore-System/         # Phase 12
├── tests/
│   ├── unit/                    # Cross-component tests (keep 4, delete 2)
│   └── integration/             # NEW: Hardware integration tests
└── tools/
    ├── check_versions.py
    ├── bump_version.py
    └── local_ci.sh              # NEW: Pre-commit validation
```

**Structure Decision**: Embedded library monorepo with per-component structure. Each component gets its own `test/` directory for unit tests. Cross-component and integration tests remain in root `tests/`.

## Refactoring Phases

| Phase | Component | Priority | Dependencies |
|-------|-----------|----------|--------------|
| 1 | Core | P1 | None (foundation) |
| 2 | Storage | P2 | Core |
| 3 | LED | P3 | Core |
| 4 | Wifi | P3 | Core, Storage |
| 5 | NTP | P3 | Core, Wifi |
| 6 | MQTT | P3 | Core, Storage, Wifi |
| 7 | OTA | P4 | Core, Wifi, WebUI |
| 8 | RemoteConsole | P4 | Core, WebUI |
| 9 | WebUI | P4 | Core, Storage |
| 10 | SystemInfo | P4 | Core, WebUI |
| 11 | HomeAssistant | P4 | Core, MQTT |
| 12 | System | P4 | All above |

## Complexity Tracking

> No constitution violations identified. All principles can be followed.
