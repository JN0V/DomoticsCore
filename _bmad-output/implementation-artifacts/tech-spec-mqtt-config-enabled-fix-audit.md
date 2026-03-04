---
title: 'MQTT config.enabled Bug — Critical Fix Evaluation, Audit & Test Coverage'
slug: 'mqtt-config-enabled-fix-audit'
created: '2026-03-04'
status: 'completed'
stepsCompleted: [1, 2, 3, 4]
tech_stack: ['C++17', 'PlatformIO native', 'Unity Test Framework', 'PubSubClient ^2.8', 'ArduinoJson ^7.0', 'ESP32/ESP8266/Native']
files_to_modify:
  - 'DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h'
  - 'DomoticsCore-MQTT/include/DomoticsCore/MQTT.h'
  - 'DomoticsCore-MQTT/include/DomoticsCore/MQTT_HAL.h'
  - 'DomoticsCore-MQTT/include/DomoticsCore/MQTT_Stub.h'
  - 'DomoticsCore-Wifi/include/DomoticsCore/Wifi_Stub.h'
  - 'DomoticsCore-Wifi/include/DomoticsCore/Wifi_HAL.h'
  - 'DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp'
code_patterns: ['header-only with *_impl.h', 'EventBus pub/sub', 'HAL platform routing via DOMOTICS_PLATFORM_*', 'WiFiImpl->WiFiHAL delegation', 'debug* methods on MQTTComponent delegate to HAL']
test_patterns: ['Unity TEST_ASSERT*', 'setUp/tearDown cleanup', 'native platform env', 'autoReconnect=false for controlled connect', 'shutdown() in every test calling begin()']
---

# Tech-Spec: MQTT config.enabled Bug — Critical Fix Evaluation, Audit & Test Coverage

**Created:** 2026-03-04

## Overview

### Problem Statement

`MQTTComponent::loop()` silently drops all incoming MQTT messages when `config.enabled` becomes `false` after a successful connection. The connection appears fully healthy (`isConnected()` returns true, `publish()` succeeds), but `mqttClient->loop()` is never called, so PubSubClient never reads the TCP socket and no callbacks fire.

Two of three proposed fixes have been applied (loop() decoupling, setConfig() preservation), but the root cause fix (begin() mutating config.enabled) is not yet implemented. No test coverage exists for config.enabled state transitions.

**Critical evaluation result:** The three proposed fixes are NOT workarounds — they form a defense-in-depth strategy validated by multi-agent architectural review.

### Solution

Apply the missing root cause fix (#3), add test observability infrastructure (WiFi stub, MQTT stub loop counter), then add comprehensive test coverage validating all three fixes and lifecycle state transitions.

### Scope

**In Scope:**
- Fix #3: `begin()` should not mutate `config.enabled` when broker is empty
- WiFi stub enhancement for native lifecycle testing (both `Wifi_Stub.h` AND `Wifi_HAL.h`)
- MQTT stub enhancement: add `loopCallCount` to verify `mqttClient->loop()` invocation
- MQTTComponent: add `debugLoopCount()` (follows existing `debugTcpAvailable()` pattern)
- 8 unit tests (5 P0, 3 P1) covering bug regression and lifecycle scenarios
- Enhancement of existing test `test_mqtt_begin_without_broker`

**Out of Scope:**
- HomeAssistant component changes (separate spec 005)
- SystemPersistence modifications
- Real broker integration tests
- Version bumping (deferred until fixes validated)
- Edge case `setBroker("")` on active connection (separate ticket)
- Examples modification (all set `enabled=true` explicitly — bug only via persistence)

## Context for Development

### Codebase Patterns

- **Header-only architecture**: All MQTT implementation in `MQTT_impl.h`, included at end of `MQTT.h`
- **EventBus communication**: Components decoupled via `emit()`/`on<>()`. MQTT emits `mqtt/connected`, `mqtt/disconnected`, `mqtt/message`
- **HAL abstraction**: Two-namespace delegation: `WiFiImpl` (in Stub/ESP32) -> `WiFiHAL` (in Wifi_HAL.h wrappers). Same for MQTT.
- **Config pattern**: Public `MQTTConfig` struct, replaced via `setConfig()`, persisted externally by SystemPersistence
- **Lifecycle**: `begin()` -> `loop()` -> `shutdown()`; connection triggered by WiFi events or `autoReconnect`
- **Debug method pattern**: `debugTcpAvailable()` on MQTTComponent delegates to `mqttClient->tcpAvailable()` (virtual in HAL, overridden in stubs)
- **Platform detection**: `DOMOTICS_PLATFORM_ESP32` / `DOMOTICS_PLATFORM_ESP8266` / `DOMOTICS_PLATFORM_UNKNOWN`

### Files to Reference

| File | Purpose |
| ---- | ------- |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h` | All implementations: `loop()`, `begin()` (broker.isEmpty check), `setConfig()` (preserveEnabled), `connect()` |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h` | Class def: `isConnected()`, state (private), `debugTcpAvailable()` pattern |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT_HAL.h` | Abstract MQTTClient: `tcpAvailable()` virtual (default -1) |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT_Stub.h` | Mock: `simulateMessage()`, counters, `loop()` returns isConnected (no side effects currently) |
| `DomoticsCore-Wifi/include/DomoticsCore/Wifi_Stub.h` | WiFi stub: `WiFiImpl::isConnected()` hardcoded false |
| `DomoticsCore-Wifi/include/DomoticsCore/Wifi_HAL.h` | WiFi HAL: `WiFiHAL::isConnected()` delegates to `WiFiImpl::isConnected()` |
| `DomoticsCore-System/include/DomoticsCore/SystemPersistence.h` | Loads `mqtt_enabled` from NVS, calls `setConfig()` |
| `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp` | 25 existing tests, 0 on enabled logic |

### Technical Decisions

- **WiFi stub: two-file change required**. `setConnectedForTest()` must be added to BOTH `Wifi_Stub.h` (implementation) AND `Wifi_HAL.h` (delegation wrapper). Tests call `HAL::WiFiHAL::setConnectedForTest()`.
- **C++17 `inline` variable** for `stubbedConnected` (not `static` which gives internal linkage at namespace scope, broken across multiple TUs).
- **MQTT stub `loopCallCount`**: Counter + `getLoopCallCount()` to verify `mqttClient->loop()` was called. Without this, stub `loop()` is a no-op with no observable side effect.
- **`debugLoopCount()`** on MQTTComponent: follows `debugTcpAvailable()` pattern.
- **Test pattern: `autoReconnect=false`**: Prevents `begin()` from calling `connect()` implicitly.
- **Test pattern: `shutdown()`**: Every test calling `begin()` must call `shutdown()` before destruction.
- **Testing Fix #1**: Start with `config.enabled=false` + broker -> `begin()` -> manual `connect()` -> Connected + enabled=false. This bypasses Fix #2 (which only triggers via `setConfig()` on Connected state).

### config.enabled Audit Map (15 usages)

**WRITE operations:**
| Location | File | Anchor | Critical |
|----------|------|--------|----------|
| begin() auto-disable | MQTT_impl.h | `if (config.broker.isEmpty()) { config.enabled = false;` | **ROOT CAUSE** (Fix #3) |
| setConfig() preserve | MQTT_impl.h | `bool preserveEnabled = (state == MQTTState::Connected` | Fix #2 (applied) |
| WebUI toggle | MQTTWebUI.h | `cfg.enabled = newEnabled;` | Safe |
| WebUI persist | MQTTWebUI.h | `onConfigSaved(cfg)` | Safe |
| SystemPersistence load | SystemPersistence.h | `storage->getBool("mqtt_enabled"` | **Propagation vector** |
| Examples (x3) | examples/*/main.cpp | `mqttConfig.enabled = true;` | Safe |

**READ operations:**
| Location | File | Anchor |
|----------|------|--------|
| loop() reconnect guard | MQTT_impl.h | `} else if (config.enabled && config.autoReconnect) {` |
| begin() init guard | MQTT_impl.h | `if (!config.enabled) {` |
| WebUI display | MQTTWebUI.h | `doc["enabled"] = cfg.enabled;` |
| WebUI compare | MQTTWebUI.h | `bool wasEnabled = cfg.enabled;` |
| SystemPersistence log | SystemPersistence.h | `DLOG_I(LOG_PERSISTENCE, "Loaded MQTT config: enabled=%d` |

## Critical Evaluation (Party Mode)

### Fix Assessment

| Fix | Target | Verdict | Rationale |
|-----|--------|---------|-----------|
| #1 `loop()` decoupled | Message processing | Correct fix (applied) | connected -> process, enabled -> reconnect only |
| #2 `setConfig()` preserve | Defense-in-depth | Keep (applied) | Catches stale config from flash |
| #3 `begin()` no mutation | Root cause | **CRITICAL, NOT YET APPLIED** | Prevents bad state generation |

Defense-in-depth layers:
- Fix #3 prevents `enabled=false` from being created (root cause)
- Fix #2 catches it if it arrives via external config (flash corruption)
- Fix #1 ensures an active connection always processes messages

### Edge Cases (Documented, Out-of-Scope)

1. `setBroker("")` on active connection: `loop()` early-returns. Separate ticket.
2. `setConfig()` during `Connecting` state: Fix #2 only protects Connected. Acceptable.
3. `connect()` bypasses `config.enabled`: Intentional, aligned with Fix #1.

## Implementation Plan

### Implementation Order

1. Test infrastructure: WiFi stub + MQTT stub enhancements (prerequisites)
2. Fix #3: root cause (single line removal + log level change)
3. Tests: P0 first, then P1
4. Run full suite

### Tasks

- [x] **Task 1: Enhance WiFi Stub -- Implementation**
  - File: `DomoticsCore-Wifi/include/DomoticsCore/Wifi_Stub.h`
  - Action: In the `WiFiImpl` namespace, add:
    `inline bool stubbedConnected = false;`
    `inline void setConnectedForTest(bool connected) { stubbedConnected = connected; }`
    Modify `isConnected()` to `return stubbedConnected;` instead of `return false;`.
  - Notes: Use `inline` (C++17) not `static` for correct cross-TU linkage.

- [x] **Task 2: Enhance WiFi Stub -- HAL Delegation Wrapper**
  - File: `DomoticsCore-Wifi/include/DomoticsCore/Wifi_HAL.h`
  - Action: In the `WiFiHAL` namespace block (the non-ESP conditional), add:
    `inline void setConnectedForTest(bool connected) { WiFiImpl::setConnectedForTest(connected); }`
  - Notes: Without this, `HAL::WiFiHAL::setConnectedForTest()` calls produce a compile error.

- [x] **Task 3: Enhance MQTT Stub -- Add loopCallCount**
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_Stub.h`
  - Action: Add `uint32_t loopCallCount = 0;` member. Modify `loop()` to increment it: `loopCallCount++; return isConnected;`. Add `uint32_t getLoopCallCount() const { return loopCallCount; }` and `void resetLoopCount() { loopCallCount = 0; }`.
  - Notes: Essential for verifying Fix #1. Without this, stub loop() has no observable side effect.

- [x] **Task 4: Add getLoopCallCount() Virtual to MQTT HAL**
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_HAL.h`
  - Action: Add to `MQTTClient` base class (alongside `tcpAvailable()`):
    `virtual uint32_t getLoopCallCount() const { return 0; }`
  - Notes: Default 0, no overhead for production. Stub overrides it.

- [x] **Task 5: Add debugLoopCount() to MQTTComponent**
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h`
  - Action: Add after `debugTcpAvailable()`:
    `uint32_t debugLoopCount() const { return mqttClient ? mqttClient->getLoopCallCount() : 0; }`
  - Notes: Follows existing `debugTcpAvailable()` pattern.

- [x] **Task 6: Apply Fix #3 -- Remove config.enabled Mutation in begin()**
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h`
  - Action: In `begin()`, in the `if (config.broker.isEmpty())` block, remove `config.enabled = false;`. Change `DLOG_W` to `DLOG_I`. Update message to `"No broker configured - MQTT inactive until configured"`.
  - Anchor: `if (config.broker.isEmpty()) {` inside `MQTTComponent::begin()`
  - Notes: Root cause fix. Log level W->I because "no broker at boot" is normal operational state.

- [x] **Task 7: Enhance Existing Test -- Regression for Fix #3**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: In `test_mqtt_begin_without_broker()`, after existing status assertion, add:
    `TEST_ASSERT_TRUE(mqtt.getConfig().enabled);`

- [x] **Task 8: P0 Test -- loop() Processes When Connected + enabled=false (Fix #1)**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `test_mqtt_loop_processes_when_connected_and_disabled()`:
    Start with `enabled=false`, broker configured, `autoReconnect=false`.
    `begin()` -> `setConnectedForTest(true)` -> `connect()` -> verify `isConnected()=true` AND `enabled=false`.
    Call `loop()`, assert `debugLoopCount()` incremented. Call `shutdown()`.
  - Notes: KEY: start with `enabled=false` to bypass Fix #2. `connect()` ignores `enabled`.

- [x] **Task 9: P0 Test -- setConfig() Preserves enabled When Connected (Fix #2)**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `test_mqtt_setconfig_preserves_enabled_when_connected()`:
    Connect with `enabled=true`, `autoReconnect=false`. Then `setConfig()` with `enabled=false`.
    Assert `getConfig().enabled == true`. Call `shutdown()`.

- [x] **Task 10: P0 Test -- begin() Empty Broker Does NOT Clear enabled (Fix #3)**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `test_mqtt_begin_empty_broker_does_not_clear_enabled()`:
    Empty broker, `enabled=true`. `begin()`. Assert `enabled` still `true`. Call `shutdown()`.

- [x] **Task 11: P0 Test -- Full Lifecycle (All Fixes)**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `test_mqtt_full_lifecycle_empty_to_configured()`:
    Phase 1: empty broker, `begin()`, assert enabled preserved.
    Phase 2: `setConfig()` with broker.
    Phase 3: `setConnectedForTest(true)`, `connect()`, assert connected.
    Phase 4: `loop()`, assert `debugLoopCount()` incremented. `shutdown()`.

- [x] **Task 12: P0 Test -- Config Reload During Active Connection (Fix #1+#2)**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `test_mqtt_config_reload_preserves_active_connection()`:
    Connect, then `setConfig()` with `enabled=false`. Assert enabled preserved (Fix #2).
    `loop()`, assert `debugLoopCount()` incremented. `shutdown()`.

- [x] **Task 13: P1 Test -- handleReconnection() Blocked When Disabled**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `test_mqtt_reconnect_blocked_when_disabled()`:
    `enabled=false`, `autoReconnect=true`, broker set. `begin()`, loop 10x.
    Assert not connected, `reconnectCount == 0`. `shutdown()`.

- [x] **Task 14: P1 Test -- setConfig() NOT Preserved When Disconnected**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `test_mqtt_setconfig_does_not_preserve_when_disconnected()`:
    NOT connected. `setConfig()` with `enabled=false`. Assert `enabled == false`.

- [x] **Task 15: P1 Test -- loop() Empty Broker Early Return**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add `test_mqtt_loop_empty_broker_early_return()`:
    Connect, then `setBroker("", 0)`. `loop()` no crash. Assert `isConnected()` still true (stale). `shutdown()`.

- [x] **Task 16: tearDown() Cleanup**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: In `tearDown()`, add: `HAL::WiFiHAL::setConnectedForTest(false);`

- [x] **Task 17: Register New Tests**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add RUN_TEST() calls in `main()` for all 8 new tests in two sections.

- [x] **Task 18: Run Full Test Suite**
  - Action: `pio test -e native` in `DomoticsCore-MQTT/`
  - Notes: 25 existing + 8 new = **33 tests** must pass. Zero regressions.

### Acceptance Criteria

- [x] **AC 1:** Given empty broker + `enabled=true`, when `begin()`, then `enabled` remains `true`.
- [x] **AC 2:** Given active connection + `setConfig(enabled=false)`, then `enabled` preserved as `true`.
- [x] **AC 3:** Given Connected + `enabled=false`, when `loop()`, then `mqttClient->loop()` called (verified via `debugLoopCount()` > 0).
- [x] **AC 4:** Given disconnected + `enabled=false` + `autoReconnect=true`, when `loop()`, then no reconnection attempted.
- [x] **AC 5:** Given empty broker -> setConfig(broker) -> connect -> loop, then full lifecycle works without `enabled` corruption.
- [x] **AC 6:** Given disconnected + `setConfig(enabled=false)`, then `enabled` IS set to `false`.
- [x] **AC 7:** All 25 existing tests pass with zero regressions.
- [x] **AC 8:** `WiFiHAL::setConnectedForTest(true)` compiles and works on native platform.

## Additional Context

### Dependencies

- Bug report: `specs/006-mqtt-loop-config-enabled-bug/bug-report.md`
- PubSubClient: `mqttClient->loop()` must be called to read TCP socket
- WiFi HAL: two-namespace delegation pattern (`WiFiImpl` -> `WiFiHAL`)
- MQTT HAL: virtual `getLoopCallCount()` follows `tcpAvailable()` pattern

### Testing Strategy

- **33 total tests** (25 existing + 8 new, with 1 existing enhanced)
- **Fix #1 test strategy**: Start with `enabled=false` + broker -> `begin()` -> `connect()` -> Connected + enabled=false (bypasses Fix #2)
- **Observable verification**: `debugLoopCount()` proves `mqttClient->loop()` was called
- **Test hygiene**: `autoReconnect=false`, `shutdown()` calls, `tearDown()` WiFi reset

| # | Test | Priority | Validates | Key Verification |
|---|------|----------|-----------|------------------|
| 1 | loop() connected + enabled=false | P0 | Fix #1 | `debugLoopCount()` increments |
| 2 | setConfig() preserves enabled | P0 | Fix #2 | `enabled == true` |
| 3 | begin() empty broker | P0 | Fix #3 | `enabled == true` |
| 4 | Full lifecycle | P0 | All 3 | End-to-end |
| 5 | Config reload active connection | P0 | Fix #1+#2 | Preserved + loop works |
| 6 | Reconnection blocked | P1 | loop() guard | `reconnectCount == 0` |
| 7 | setConfig() disconnected | P1 | Fix #2 edge | `enabled == false` |
| 8 | Empty broker early return | P1 | loop() guard | No crash, stale state |

### Notes

- Fix #3 is a single-line removal -- minimal risk, maximum impact
- Fixes #1+#2 are applied but untested until this spec
- SystemPersistence is propagation vector, needs no modification
- HomeAssistant does NOT interact with MQTT config.enabled
- Adversarial review findings F1-F11 all addressed in this revision

## Review Notes

- Adversarial review completed (15 findings)
- Findings: 4 valid fixed, 11 noise/out-of-scope skipped
- Resolution approach: auto-fix
- F1: Fixed stale "Auto-disable" comment in begin()
- F2: Added doxygen documentation for getLoopCallCount() in MQTT_HAL.h
- F3: Removed unused resetLoopCount() dead code from MQTT_Stub.h
- F4: Improved reconnect test to eliminate timer ambiguity (reconnectDelay=0, WiFi enabled)
