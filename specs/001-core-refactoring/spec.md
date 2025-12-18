# Feature Specification: DomoticsCore Library Refactoring

**Feature Branch**: `001-core-refactoring`  
**Created**: 2025-12-17  
**Status**: Draft  
**Input**: Component-by-component library refactoring to improve documentation, architecture, testing, and reliability

## Overview

This specification defines the systematic refactoring of the DomoticsCore library to achieve rock-solid reliability, comprehensive test coverage, and full compliance with the DomoticsCore Constitution v1.2.0.

**Scope**: 12 components to refactor in dependency order, starting with Core (the foundation).

**Components** (in refactoring order):
1. DomoticsCore-Core (foundation - EventBus, IComponent, Timer)
2. DomoticsCore-Storage (persistence layer)
3. DomoticsCore-LED (visual feedback)
4. DomoticsCore-Wifi (network connectivity)
5. DomoticsCore-NTP (time synchronization)
6. DomoticsCore-MQTT (message broker)
7. DomoticsCore-OTA (firmware updates)
8. DomoticsCore-RemoteConsole (debugging)
9. DomoticsCore-WebUI (web interface)
10. DomoticsCore-SystemInfo (monitoring)
11. DomoticsCore-HomeAssistant (HA integration)
12. DomoticsCore-System (orchestration layer)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Component Lifecycle Orchestration (Priority: P1)

As a library developer, I need the Core component to orchestrate a clean, predictable initialization cycle so that all components start in the correct order and are notified at the right moment.

**Why this priority**: The initialization lifecycle is the foundation of system reliability. Components have dependencies on each other, and starting them in the wrong order or triggering actions before dependencies are ready causes race conditions and hard-to-debug failures.

**Independent Test**: Run lifecycle unit tests verifying initialization order, event notifications, and post-init callbacks.

**Acceptance Scenarios**:

1. **Given** ComponentRegistry with 5 components having dependencies, **When** begin() is called, **Then** components initialize in topological dependency order
2. **Given** a component depending on Storage, **When** Storage completes begin(), **Then** dependent component receives STORAGE_READY event before its own begin() completes
3. **Given** all components registered, **When** system startup completes, **Then** SYSTEM_READY event is published and all components can safely trigger actions
4. **Given** a component with async initialization, **When** async init completes, **Then** component publishes COMPONENT_READY event with its name
5. **Given** WiFi component, **When** network becomes available, **Then** NETWORK_READY event is published before any network-dependent component attempts connection
6. **Given** circular dependency detected, **When** begin() is called, **Then** system logs error and fails gracefully with clear diagnostic

---

### User Story 2 - Core Component Reliability (Priority: P1)

As a library developer, I need the Core component to be rock-solid with 100% test coverage so that all other components can depend on it with confidence.

**Why this priority**: Core is the foundation. EventBus, IComponent interface, Timer utilities, and ComponentRegistry are used by ALL other components. Any bug here propagates everywhere.

**Independent Test**: Run `pio test -e native` on DomoticsCore-Core and verify 100% pass rate with coverage report.

**Acceptance Scenarios**:

1. **Given** the Core component, **When** I run all unit tests, **Then** 100% of tests pass with 100% code coverage
2. **Given** a component using EventBus, **When** I publish an event, **Then** all subscribers receive it within 1ms
3. **Given** NonBlockingDelay timer, **When** the delay expires, **Then** callback executes without blocking main loop
4. **Given** ComponentRegistry, **When** component fails during begin(), **Then** error is logged and dependent components are notified

---

### User Story 3 - Storage Component Reliability (Priority: P2)

As a library developer, I need the Storage component to reliably persist data so that device configuration survives reboots.

**Why this priority**: Storage is the second dependency layer. WiFi, MQTT, and other components need to store configuration.

**Independent Test**: Run storage unit tests verifying put/get operations, namespace isolation, and edge cases.

**Acceptance Scenarios**:

1. **Given** StorageComponent, **When** I store a value, **Then** it persists across simulated reboots
2. **Given** two components with different namespaces, **When** both store key "config", **Then** values remain isolated
3. **Given** storage at 95% capacity, **When** I attempt to store, **Then** appropriate error is returned

---

### User Story 4 - Network Components Reliability (Priority: P3)

As a library developer, I need WiFi, NTP, and MQTT components to handle network failures gracefully.

**Why this priority**: Network components are critical for IoT functionality but depend on Core and Storage being solid first.

**Independent Test**: Run integration tests simulating network failures and verifying recovery behavior.

**Acceptance Scenarios**:

1. **Given** WiFi connected, **When** connection drops, **Then** automatic reconnection within 30 seconds
2. **Given** MQTT connected, **When** broker unavailable, **Then** messages queued and sent on reconnect
3. **Given** NTP sync failed, **When** network restored, **Then** time synchronized within 60 seconds

---

### User Story 5 - WebUI and System Reliability (Priority: P4)

As an end user, I need the WebUI to remain responsive and the System orchestration to handle errors gracefully.

**Why this priority**: These are the highest-level components that depend on all others being stable.

**Independent Test**: Run WebUI under load and verify response times; test System with component failures.

**Acceptance Scenarios**:

1. **Given** WebUI serving requests, **When** 10 concurrent connections, **Then** response time < 200ms
2. **Given** System with failed component, **When** error occurs, **Then** LED shows error state and console accessible
3. **Given** OTA update in progress, **When** update fails, **Then** device remains functional with previous firmware

---

### Edge Cases

**Lifecycle Edge Cases:**
- What happens when a component's begin() takes longer than expected?
- How does system handle circular dependencies between components?
- What happens when a component fails during initialization?
- How are late-registered components handled after SYSTEM_READY?
- What happens when a dependency is missing at runtime?

**Communication Edge Cases:**
- What happens when EventBus has 100+ subscribers to same topic?
- How does system handle event storms (rapid fire events)?
- What happens when event handler throws an exception?

**Storage Edge Cases:**
- How does system handle Storage corruption?
- What happens when storage is full?

**Network Edge Cases:**
- What happens when WiFi credentials are invalid?
- How does MQTT handle message overflow when disconnected?
- What happens if network comes up before WiFi component is ready?

**System Edge Cases:**
- What happens during OTA if power is lost mid-update?
- How does WebUI handle malformed JSON requests?
- What happens when loop() is called before begin() completes?

## Requirements *(mandatory)*

### Functional Requirements

**Constitution Compliance:**
- **FR-001**: Each component MUST have 100% unit test coverage for non-HAL code; HAL code is tested via integration tests
- **FR-002**: All code files MUST be under 800 lines (target 200-500 lines)
- **FR-003**: All inter-component communication MUST use EventBus exclusively
- **FR-004**: All hardware-specific code MUST be in `*_HAL` suffixed files
- **FR-005**: No component MUST use `delay()` - only NonBlockingDelay from Timer.h
- **FR-006**: All persistent storage MUST go through StorageComponent
- **FR-007**: Library MUST be publishable to both PlatformIO and Arduino registries

**Testing Requirements:**
- **FR-008**: Each component MUST have unit tests runnable on native platform (not requiring ESP32)
- **FR-009**: Each component MUST have integration tests verifiable on real hardware
- **FR-010**: Test coverage reports MUST be generated for each component
- **FR-011**: All tests MUST pass before any commit

**Documentation Requirements:**
- **FR-012**: Each component MUST have a README.md with API documentation
- **FR-013**: Each public function MUST have doc comments
- **FR-014**: Architecture decisions MUST be documented in docs/

**Performance Requirements:**
- **FR-015**: Core component binary size MUST be < 300KB
- **FR-016**: Full stack binary size MUST be < 1MB
- **FR-017**: Main loop iteration MUST complete in < 10ms
- **FR-018**: No heap growth over 24h continuous operation

**Lifecycle Requirements:**
- **FR-019**: ComponentRegistry MUST resolve dependencies and initialize components in topological order
- **FR-020**: Each component MUST publish a COMPONENT_READY event upon successful initialization
- **FR-021**: Core MUST publish SYSTEM_READY event only after all registered components complete begin()
- **FR-022**: Components MUST NOT trigger network-dependent actions before NETWORK_READY event
- **FR-023**: Components MUST NOT access storage before STORAGE_READY event
- **FR-024**: Circular dependencies MUST be detected at registration time with clear error message
- **FR-025**: Component failures during begin() MUST be logged and MUST NOT crash the system
- **FR-026**: Late-registered components (after SYSTEM_READY) MUST receive appropriate ready events immediately
- **FR-027**: The shutdown sequence MUST proceed in reverse dependency order

### Key Entities

- **IComponent**: Base interface for all components (begin, loop, shutdown, getDependencies)
- **EventBus**: Central pub/sub messaging system with sticky events
- **ComponentRegistry**: Manages component lifecycle with dependency resolution
- **StorageComponent**: Centralized key-value persistence
- **NonBlockingDelay**: Timer utility for async operations

### Lifecycle Events

| Event | Publisher | When | Subscribers Should |
|-------|-----------|------|-------------------|
| COMPONENT_READY | Each component | After successful begin() | Know dependency is available |
| STORAGE_READY | StorageComponent | After storage initialized | Start loading configuration |
| NETWORK_READY | WiFiComponent | After network connected | Start network operations |
| SYSTEM_READY | Core | After all components ready | Trigger startup actions |
| COMPONENT_ERROR | Any component | On unrecoverable error | Handle graceful degradation |
| SHUTDOWN_START | Core | Before shutdown begins | Save state, close connections |

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% unit test coverage for all 12 components
- **SC-002**: All 12 components pass constitution compliance checklist
- **SC-003**: Zero compiler warnings across all components
- **SC-004**: All files under 800 lines (verified by automated check)
- **SC-005**: Binary sizes within targets (Core < 300KB, Full < 1MB)
- **SC-006**: All examples compile and run successfully
- **SC-007**: Library published to PlatformIO registry
- **SC-008**: Library published to Arduino Library Manager
- **SC-009**: No blocking calls (delay) outside boot sequences
- **SC-010**: 24h stability test passes with no memory leaks
- **SC-011**: Component initialization order is deterministic and documented
- **SC-012**: All lifecycle events are published at correct times (verified by integration tests)
- **SC-013**: System recovers gracefully from component initialization failures

## Directory Structure Audit

### Current Structure

```
DomoticsCore/
├── DomoticsCore-Core/          # Foundation (EventBus, IComponent, Timer)
├── DomoticsCore-Storage/       # Persistence layer
├── DomoticsCore-LED/           # Visual feedback
├── DomoticsCore-Wifi/          # Network connectivity
├── DomoticsCore-NTP/           # Time synchronization
├── DomoticsCore-MQTT/          # Message broker
├── DomoticsCore-OTA/           # Firmware updates
├── DomoticsCore-RemoteConsole/ # Debugging
├── DomoticsCore-WebUI/         # Web interface
├── DomoticsCore-SystemInfo/    # Monitoring
├── DomoticsCore-HomeAssistant/ # HA integration
├── DomoticsCore-System/        # Orchestration layer
├── tests/unit/                 # 6 unit tests
├── examples/                   # Root examples (empty, README only)
└── docs/                       # Documentation
```

### Current Unit Tests (6 tests)

| Test | Purpose | Status |
|------|---------|--------|
| 01-optional-dependencies | Tests optional dependency support | ✅ Keep |
| 02-lifecycle-callback | Tests afterAllComponentsReady() lifecycle | ✅ Keep |
| 03-bug2-early-init | Bug reproduction: Early-init scenario | ❌ **DELETE** - Anti-pattern |
| 04-system-scenario | Bug reproduction: System.begin() scenario | ❌ **DELETE** - Anti-pattern |
| 05-storage-namespace | Tests storage namespace isolation | ✅ Keep |
| 06-webui-refactor | Tests WebUI refactoring | ✅ Keep |

### Early-Init Analysis

**Finding**: Early-init was identified and **eliminated as an anti-pattern** in v1.1.1 (see CHANGELOG).

**Evidence from CHANGELOG v1.1.1:**
> "Eliminated Early-Init Requirement - Major Improvement: Refactored WifiComponent and System to eliminate Storage early-init pattern."
> "Storage early-init eliminated. Only LED early-init remains (justified for boot error visualization)."

**Why Early-Init is an Anti-Pattern:**
1. Bypasses ComponentRegistry's dependency resolution
2. Creates hidden initialization order dependencies
3. Makes testing harder (cannot mock properly)
4. Violates the Dependency Inversion principle
5. Was a workaround, not a proper architectural solution

**Exception**: LED early-init is acceptable for boot error visualization only.

**Recommendation**:
- **DELETE** tests 03 and 04 - they test an anti-pattern that was eliminated
- Keep tests 01, 02, 05, 06 - they test valid patterns
- Add new tests for proper lifecycle (afterAllComponentsReady pattern)

### Current Examples (28 total)

| Component | Examples | Recommendation |
|-----------|----------|----------------|
| **Core** | 5 (CoreOnly, DummyComponent, EventBusBasics, EventBusCoordinators, EventBusTests) | ⚠️ Consolidate: Keep CoreOnly, merge EventBus examples into 1 |
| **Storage** | 3 (BasicStorage, NamespaceDemo, StorageWithWebUI) | ✅ Keep all - each demonstrates different use case |
| **LED** | 2 (BasicLED, LEDWithWebUI) | ✅ Keep - Basic + WebUI pattern |
| **Wifi** | 2 (BasicWifi, WifiWithWebUI) | ✅ Keep - Basic + WebUI pattern |
| **NTP** | 2 (BasicNTP, NTPWithWebUI) | ✅ Keep - Basic + WebUI pattern |
| **MQTT** | 3 (BasicMQTT, MQTTWithWebUI, MQTTWifiWithWebUI) | ⚠️ Review: MQTTWifiWithWebUI may be redundant |
| **OTA** | 1 (OTAWithWebUI) | ⚠️ Add BasicOTA example |
| **RemoteConsole** | 1 (BasicRemoteConsole) | ⚠️ Add RemoteConsoleWithWebUI example |
| **WebUI** | 2 (HeadlessAPI, WebUIOnly) | ✅ Keep - demonstrates different modes |
| **SystemInfo** | 2 (BasicSystemInfo, SystemInfoWithWebUI) | ✅ Keep - Basic + WebUI pattern |
| **HomeAssistant** | 2 (BasicHA, HAWithWebUI) | ✅ Keep - Basic + WebUI pattern |
| **System** | 3 (FullStack, Minimal, Standard) | ✅ Keep - demonstrates complexity levels |

### Target Structure

```
DomoticsCore/
├── DomoticsCore-*/
│   ├── include/                # Public headers
│   ├── src/                    # Implementation
│   ├── examples/               # Component-specific examples
│   │   ├── Basic*/             # Minimal usage (1 per component)
│   │   └── *WithWebUI/         # WebUI integration (optional)
│   └── test/                   # Component-specific unit tests (NEW)
├── tests/
│   ├── unit/                   # Cross-component unit tests
│   └── integration/            # Integration tests (NEW)
├── examples/                   # Multi-component examples (move FullStack here)
└── docs/
    ├── architecture/           # Architecture decisions
    ├── api/                    # API documentation
    └── guides/                 # User guides
```

### Structure Requirements

- **FR-028**: Each component MUST have at least one "Basic" example demonstrating minimal usage
- **FR-029**: Each component with WebUI support MUST have a "WithWebUI" example
- **FR-030**: Unit tests MUST be runnable on native platform without hardware
- **FR-031**: Integration tests MUST be separated from unit tests
- **FR-032**: Examples MUST NOT duplicate functionality covered by unit tests
- **FR-033**: Each component directory MUST contain a README.md with API documentation
- **FR-034**: Root examples/ directory MUST contain multi-component integration examples

### Example Consolidation Plan

| Action | Examples | Rationale |
|--------|----------|-----------|
| **MERGE** | EventBusBasics + EventBusCoordinators + EventBusTests → EventBusDemo | 3 examples test same feature |
| **REMOVE** | MQTTWifiWithWebUI | Redundant with MQTTWithWebUI |
| **ADD** | BasicOTA | Missing basic example |
| **ADD** | RemoteConsoleWithWebUI | Missing WebUI example |
| **MOVE** | FullStack → examples/FullStack | Multi-component belongs in root |

### Test Migration Plan

| Current Location | Target Location | Rationale |
|------------------|-----------------|-----------|
| tests/unit/01-optional-dependencies | DomoticsCore-Core/test/ | Tests Core feature |
| tests/unit/02-lifecycle-callback | DomoticsCore-Core/test/ | Tests Core lifecycle |
| tests/unit/05-storage-namespace | DomoticsCore-Storage/test/ | Tests Storage feature |
| tests/unit/06-webui-refactor | DomoticsCore-WebUI/test/ | Tests WebUI feature |
| tests/unit/03-bug2-early-init | **DELETE** | Anti-pattern eliminated in v1.1.1 |
| tests/unit/04-system-scenario | **DELETE** | Anti-pattern eliminated in v1.1.1 |

## Clarifications

### Session 2025-12-17

- Q: How should we handle test coverage for HAL files? → A: HAL files tested via integration tests only; 100% coverage applies to non-HAL code
- Q: What criteria must be met to consider a component's refactoring complete? → A: Tests pass + coverage + constitution checklist + documentation updated
- Q: Is CI/CD part of refactoring scope? → A: Create simple local CI script (pre-commit checks) as part of Core phase; GitHub Actions minimal

## Assumptions

- Existing component APIs will be preserved where possible (deprecate before removing)
- Refactoring proceeds one component at a time (Progressive Refactoring principle)
- Tests are written BEFORE refactoring each component (TDD principle)
- Each component refactoring is a separate phase with its own quality gate
- Examples serve as integration tests and user documentation
- Bug reproduction tests are archived once bugs are confirmed fixed

## Dependencies

- PlatformIO for build and test
- Native platform tests (no hardware required for unit tests)
- ESP32 hardware for integration tests

### Local CI Script (to be created in Core phase)

A simple local pre-commit validation script that runs:
1. `python tools/check_versions.py --verbose` - Version consistency
2. `pio test -e native` - Unit tests on native platform
3. File size check (all files < 800 lines)
4. Compiler warnings check

**Usage**: Run before committing to ensure quality gates pass locally.

**GitHub Actions**: Minimal - only run on PR/push to main (build verification only).

## Out of Scope

- Adding new features (YAGNI principle)
- Changing public APIs without deprecation period
- Supporting additional platforms beyond ESP32/ESP8266
- Rewriting multiple components simultaneously
