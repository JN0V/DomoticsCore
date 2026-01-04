# Feature Specification: ESP8266 Full Support

**Feature Branch**: `002-esp8266-full-support`  
**Created**: 2025-12-18  
**Status**: Draft  
**Input**: Extend ESP8266 support by validating each component individually, from simplest to most complex, ensuring minimal RAM usage and full WebUI integration.

## Overview

This specification defines a **component-by-component** approach to ESP8266 support in DomoticsCore. Each component is validated individually in dependency order, ensuring minimal RAM usage before proceeding to the next.

**Key Principles:**
- **Incremental validation**: Test each component in isolation before combining
- **RAM-first**: Every component must prove minimal RAM footprint
- **WebUI is mandatory**: Not optional - must work even if adaptation is needed
- **Physical validation**: Each phase validated on real ESP8266 hardware

**Current State:**
- Platform_HAL.h already defines ESP8266 macros (DOMOTICS_PLATFORM_ESP8266, DOMOTICS_HAS_WIFI, etc.)
- Several HALs have ESP8266 implementations (Wifi_HAL, Storage_HAL, NTP_HAL, SystemInfo_HAL)
- WebUI uses ESPAsyncWebServer which requires adaptation for ESP8266

**ESP8266 Constraints:**
- RAM: ~80KB (vs 320KB on ESP32) - strict budget management required
- No native FreeRTOS
- Different async libraries (ESPAsyncWebServer-esphome for ESP8266)
- No Preferences.h (uses LittleFS instead)

## Component Validation Order

Components are validated in dependency order, from simplest to most complex:

| Phase | Component | Dependencies | RAM Budget | Validation |
|-------|-----------|--------------|------------|------------|
| 1 | Core | None | Baseline | EventBus, Timer, IComponent |
| 2 | LED | Core | +1KB | Patterns, status indication |
| 3 | Storage | Core | +2KB | LittleFS read/write |
| 4 | WiFi | Core, Storage | +5KB | Connect, reconnect, IP |
| 5 | NTP | Core, WiFi | +1KB | Time sync via configTime() |
| 6 | SystemInfo | Core | +1KB | Heap, CPU, flash metrics |
| 7 | RemoteConsole | Core, WiFi | +3KB | Telnet, log streaming |
| 8 | MQTT | Core, WiFi, Storage | +5KB | Pub/sub, reconnect |
| 9 | OTA | Core, WiFi | +2KB | Firmware update |
| 10 | HomeAssistant | Core, MQTT | +3KB | Discovery, entities |
| 11 | WebUI | Core, WiFi, Storage | +15KB | HTTP, WebSocket, dashboard |
| 12 | System | All | Final | Full orchestration |

**RAM Target**: Full stack must leave ≥20KB free heap after initialization.

**Validation Strategy**: Phases can proceed in parallel if their dependencies are met. Blocked phases don't prevent independent work.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Phase 1-3: Core Foundation (Priority: P1)

As a developer, I want Core, LED, and Storage components validated on ESP8266 as the foundation for all other components.

**Why this priority**: These are the base dependencies. Nothing else works without them.

**Independent Test**: Create minimal ESP8266 sketch using only Core + LED + Storage. Measure heap before/after.

**Acceptance Scenarios**:

1. **Given** Core only on ESP8266, **When** compiled and run, **Then** EventBus and Timer work, heap baseline established
2. **Given** LED added to Core, **When** I trigger patterns, **Then** LED blinks correctly, heap increase ≤1KB
3. **Given** Storage added, **When** I store/retrieve values, **Then** LittleFS persistence works, heap increase ≤2KB
4. **Given** Phase 1-3 complete, **When** running combined, **Then** total heap usage documented as baseline

---

### User Story 2 - Phase 4-6: Network Foundation (Priority: P1)

As a developer, I want WiFi, NTP, and SystemInfo validated on ESP8266 to enable network-dependent features.

**Why this priority**: Network is required for WebUI, MQTT, OTA, and RemoteConsole.

**Independent Test**: Create ESP8266 sketch with WiFi + NTP + SystemInfo on top of Phase 1-3. Verify connection and metrics.

**Acceptance Scenarios**:

1. **Given** WiFi added, **When** connecting to network, **Then** IP obtained, reconnection works, heap increase ≤5KB
2. **Given** NTP added, **When** WiFi connected, **Then** time syncs via configTime(), heap increase ≤1KB
3. **Given** SystemInfo added, **When** querying metrics, **Then** heap/CPU/flash values returned correctly
4. **Given** Phase 4-6 complete, **When** running 10 minutes, **Then** no memory leaks, heap stable

---

### User Story 3 - Phase 7-10: Network Services (Priority: P2)

As a developer, I want RemoteConsole, MQTT, OTA, and HomeAssistant validated on ESP8266.

**Why this priority**: These services depend on network foundation and are required before WebUI integration.

**Independent Test**: Add each component incrementally, measure heap impact, verify functionality.

**Acceptance Scenarios**:

1. **Given** RemoteConsole added, **When** Telnet connects, **Then** logs stream in real-time, heap increase ≤3KB
2. **Given** MQTT added, **When** broker available, **Then** pub/sub works, reconnection works, heap increase ≤5KB
3. **Given** OTA added, **When** update triggered, **Then** firmware flashes successfully
4. **Given** HomeAssistant added, **When** MQTT connected, **Then** discovery messages sent, entities work

---

### User Story 4 - Phase 11: WebUI Integration (Priority: P1)

As a user, I want full WebUI functionality on ESP8266 to configure and monitor my device through a web interface.

**Why this priority**: WebUI is mandatory for user interaction. Must work even if library adaptation is needed.

**Independent Test**: Compile WebUI on ESP8266, access dashboard, test all UI features.

**Acceptance Scenarios**:

1. **Given** WebUI with ESPAsyncWebServer-esphome, **When** compiled, **Then** compilation succeeds without errors
2. **Given** WebUI running, **When** accessing device IP, **Then** dashboard loads completely
3. **Given** WebSocket enabled, **When** client connects, **Then** real-time updates work
4. **Given** all previous phases + WebUI, **When** running, **Then** heap remains ≥20KB free
5. **Given** HTTP request, **When** sent to ESP8266, **Then** response time <500ms

---

### User Story 5 - Phase 12: Full System Validation (Priority: P2)

As a user, I want all 12 components running together on a physical ESP8266 with stable operation.

**Why this priority**: Final validation proves the complete stack works in real-world conditions.

**Independent Test**: Flash full System example on physical ESP8266, monitor for 24 hours.

**Acceptance Scenarios**:

1. **Given** System with all components, **When** booted, **Then** all components initialize successfully
2. **Given** 24-hour runtime, **When** monitoring heap, **Then** no memory leaks detected
3. **Given** physical device, **When** LED/Serial/WebUI observed, **Then** all feedback mechanisms work
4. **Given** network interruption, **When** connectivity restored, **Then** all services reconnect automatically

---

### Edge Cases

**Memory:**
- Low heap tiered response: <15KB warning+reject, <10KB suspend WebUI, <5KB reboot
- JSON parsing uses JsonDocument (ArduinoJson 7.x) with 2KB max usage; allocation failure returns error, does not crash
- Recovery on allocation failure: log error, return false/empty, caller handles gracefully

**Per-Component Validation:**
- Exact heap impact documented in heap-measurements.md during implementation
- Component combinations tested incrementally; budget exceeded triggers optimization per FR-003b

**Network:**
- ESP8266 WiFi reconnection uses WiFi.setAutoReconnect(true) + EventBus notification
- RemoteConsole Telnet uses WiFiServer (compatible with ESP8266)
- WebSocket drop triggers automatic reconnection attempt via ESPAsyncWebServer-esphome

**Storage:**
- LittleFSStorage detects corrupted JSON files and returns empty/default values (no crash)
- JSON document size limited to 2KB maximum on ESP8266

## Requirements *(mandatory)*

### Functional Requirements

**Phase 1-3: Core Foundation**
- **FR-001**: Core (EventBus, IComponent, Timer, ComponentRegistry) MUST work on ESP8266 with heap baseline documented
- **FR-002**: LEDComponent MUST work on ESP8266 with heap increase ≤1KB
- **FR-003**: StorageComponent MUST use LittleFSStorage on ESP8266 with heap increase ≤2KB
- **FR-003b**: If a component exceeds its RAM budget, it MUST be optimized/refactored to fit before proceeding
- **FR-003c**: JSON documents on ESP8266 MUST be limited to 2KB maximum
- **FR-003d**: LittleFSStorage MUST handle corrupted files gracefully (return default, no crash)

**Phase 4-6: Network Foundation**
- **FR-004**: WifiComponent MUST use ESP8266WiFi.h with heap increase ≤5KB
- **FR-005**: NTPComponent MUST use configTime() on ESP8266 via NTP_HAL with heap increase ≤1KB
- **FR-006**: SystemInfoComponent MUST return ESP8266 metrics via SystemInfo_HAL with heap increase ≤1KB

**Phase 7-10: Network Services**
- **FR-007**: RemoteConsoleComponent MUST work via WiFiServer on ESP8266 with heap increase ≤3KB
- **FR-008**: MQTTComponent MUST work via PubSubClient with heap increase ≤5KB
- **FR-009**: OTAComponent MUST support OTA updates on ESP8266 with heap increase ≤2KB
- **FR-010**: HomeAssistantComponent MUST work when MQTT is available with heap increase ≤3KB

**Phase 11: WebUI (Mandatory)**
- **FR-011**: WebUIComponent MUST compile and work on ESP8266
- **FR-012**: WebUI MUST use ESPAsyncWebServer-esphome or equivalent compatible library
- **FR-013**: WebUI MUST support HTTP requests with response time <500ms
- **FR-014**: WebUI MUST support WebSocket for real-time updates
- **FR-015**: WebUI heap increase MUST be ≤15KB

**Phase 12: Full System**
- **FR-016**: SystemComponent MUST orchestrate all ESP8266-compatible components
- **FR-017**: Full stack MUST leave ≥20KB free heap after initialization
- **FR-018**: System MUST run stable for 24 hours without memory leaks
- **FR-018b**: System MUST implement tiered low-heap response: <15KB log+reject connections, <10KB suspend WebUI, <5KB reboot

**Platform and Compilation**
- **FR-019**: The library MUST compile without errors on PlatformIO `esp8266dev`
- **FR-020**: Platform_HAL.h MUST correctly define DOMOTICS_PLATFORM_ESP8266 and associated macros

**Documentation**
- **FR-021**: Each phase MUST have a dedicated ESP8266 example
- **FR-022**: Heap measurements MUST be documented for each component

### Key Entities

- **Platform_HAL**: Abstraction layer defining platform macros and capabilities
- **Storage_HAL**: Storage abstraction (Preferences on ESP32, LittleFS on ESP8266)
- **Wifi_HAL**: WiFi abstraction unifying ESP32 and ESP8266
- **NTP_HAL**: NTP abstraction (esp_sntp on ESP32, configTime on ESP8266)
- **SystemInfo_HAL**: System metrics abstraction

### HAL Architecture Rule (Constitution IX)

**`#ifdef` platform directives are FORBIDDEN everywhere except HAL files.**

- **Allowed**: `{Component}_HAL.h`, `{Component}_ESP32.h`, `{Component}_ESP8266.h`, `{Component}_Stub.h`
- **Forbidden**: Business logic, components, examples, applications, Logger.h, utility files
- **Platform Utilities**: If ESP32 provides utilities (e.g., `log_i()` macro), Platform_ESP8266.h MUST define equivalent so consumers see unified API
- **Validation**: Any `#ifdef DOMOTICS_PLATFORM_*` or `#if defined(ESP32/ESP8266)` outside HAL files is a constitution violation

### Component Validation Checklist

Each component must pass validation before proceeding to the next phase:

| Phase | Component | Compiles | Functions | Heap Budget | Validated |
|-------|-----------|----------|-----------|-------------|-----------|
| 1 | Core | ☐ | ☐ | Baseline | ☐ |
| 2 | LED | ☐ | ☐ | ≤1KB | ☐ |
| 3 | Storage | ☐ | ☐ | ≤2KB | ☐ |
| 4 | WiFi | ☐ | ☐ | ≤5KB | ☐ |
| 5 | NTP | ☐ | ☐ | ≤1KB | ☐ |
| 6 | SystemInfo | ☐ | ☐ | ≤1KB | ☐ |
| 7 | RemoteConsole | ☐ | ☐ | ≤3KB | ☐ |
| 8 | MQTT | ☐ | ☐ | ≤5KB | ☐ |
| 9 | OTA | ☐ | ☐ | ≤2KB | ☐ |
| 10 | HomeAssistant | ☐ | ☐ | ≤3KB | ☐ |
| 11 | WebUI | ☐ | ☐ | ≤15KB | ☐ |
| 12 | System | ☐ | ☐ | ≥20KB free | ☐ |

**Total RAM Budget**: ~40KB for all components, leaving ≥20KB free from 80KB total.

## Success Criteria *(mandatory)*

### Measurable Outcomes

**Per-Component Validation:**
- **SC-001**: Each component compiles successfully on ESP8266 in isolation
- **SC-002**: Each component meets its heap budget when added incrementally
- **SC-003**: No component introduces memory leaks over 10-minute test

**Integration Validation:**
- **SC-004**: Phases 1-3 (Core Foundation) pass all acceptance scenarios
- **SC-005**: Phases 4-6 (Network Foundation) pass all acceptance scenarios
- **SC-006**: Phases 7-10 (Network Services) pass all acceptance scenarios
- **SC-007**: Phase 11 (WebUI) fully functional with dashboard and WebSocket

**Final System Validation:**
- **SC-008**: Full stack boots in under 5 seconds
- **SC-009**: Full stack leaves ≥20KB free heap after initialization
- **SC-010**: Full stack runs stable for 24 hours on physical device
- **SC-011**: WebUI responds to HTTP requests in under 500ms

**Documentation:**
- **SC-012**: Each phase has a working ESP8266 example
- **SC-013**: Heap measurements documented for each component

## Assumptions

- **Primary target board**: Wemos D1 Mini (4MB flash)
- User has a compatible ESP8266 device (Wemos D1 Mini recommended, NodeMCU compatible)
- PlatformIO is used as the build environment
- Physical ESP8266 available for validation at each phase
- ESPAsyncWebServer-esphome or compatible library available for WebUI
- Target WiFi network accessible for network component testing

## Dependencies

- PlatformIO with ESP8266 support
- ESP8266 Arduino Core >= 3.0.0
- ESPAsyncWebServer-esphome (for WebUI on ESP8266)
- PubSubClient (ESP8266 compatible)
- ArduinoJson (ESP8266 compatible)
- LittleFS (included in ESP8266 Arduino Core)

## Clarifications

### Session 2025-12-18

- Q: What is the strategy when a component exceeds its allocated RAM budget? → A: Optimize - Refactor/optimize the component to fit within budget
- Q: What should the system do when free heap drops below a critical threshold at runtime? → A: Tiered progressive strategy - Warning → Reject connections → Suspend WebUI → Reboot
- Q: What is the primary ESP8266 board variant for validation? → A: Wemos D1 Mini (4MB flash, compact, popular for IoT)
- Q: What is the maximum JSON document size allowed for ESP8266? → A: 2KB maximum (balanced for config/state documents)
- Q: Can phases with no dependencies on a blocked phase proceed in parallel? → A: Yes, parallel when independent (phases can proceed if their dependencies are met)

## Out of Scope

- Support for platforms other than ESP32/ESP8266
- Advanced memory optimizations beyond documented budgets
- Full TLS/SSL support on ESP8266 (hardware limitations)
- New features not present on ESP32
- Performance optimizations beyond basic functionality
