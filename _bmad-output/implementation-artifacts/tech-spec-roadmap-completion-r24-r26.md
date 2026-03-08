---
title: 'Roadmap Completion — Virtual Dispatch + EventBus Commands'
slug: 'roadmap-completion-r24-r26'
created: '2026-03-08'
status: 'done'
stepsCompleted: [1, 2, 3, 4, 5, 6]
tech_stack:
  - C++ (header-only)
  - PlatformIO / Arduino framework
  - ESP32, ESP32-C3, ESP8266
  - ArduinoJson ^7.0.0 (MQTT, HomeAssistant, WebUI)
  - PubSubClient ^2.8 (MQTT)
files_to_modify:
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h (R24+R26 — handleCommand refactor + remove callback params from addXxx)
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HAEvents.h (R26 — add EVENT_COMMAND + HACommandEvent struct)
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h (R26 — remove commandCallback field + update handleCommand)
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HALight.h (R26 — remove commandCallback field + update handleCommand)
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HAButton.h (R26 — remove pressCallback field + update handleCommand)
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h (R26 — remove commandCallback field + update handleCommand)
  - DomoticsCore-HomeAssistant/examples/BasicHA/src/main.cpp (R26 — migrate to EventBus)
  - DomoticsCore-HomeAssistant/examples/HAWithWebUI/src/main.cpp (R26 — migrate to EventBus)
  - DomoticsCore-System/examples/FullStack/src/main.cpp (R26 — migrate to EventBus)
  - DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp (R26 — 13 addXxx call sites: 9×addSwitch, 2×addLight, 2×addButton)
  - DomoticsCore-HomeAssistant/test/test_ha_events/test_ha_events.cpp (R26 — 4 signature fixes, no test logic changes)
  - DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp (R26 — 4 addAlarmControlPanel calls + ~10 direct HAAlarmControlPanel constructor calls)
  - DomoticsCore-System/examples/FullStack/test/test_fullstack.cpp (R26 — 2 call sites; WARNING: may have pre-existing compilation issues with HomeAssistantComponent constructor — verify before migrating)
code_patterns:
  - header-only design with _impl.h separation (ref: MQTT.h includes MQTT_impl.h at line 435)
  - IComponent lifecycle (begin/loop/shutdown)
  - EventBus publish/subscribe with POD event structs (fixed-size char[] fields)
  - HAL isolation (#ifdef only in HAL files)
  - snprintf with stack-allocated char[] buffers (no String concatenation)
  - Entity virtual dispatch (HAEntity base with virtual handleCommand, all 4 entities have override)
test_patterns:
  - Native platform tests (no hardware)
  - Mocks in tests/mocks/
  - HeapTracker for memory leak detection
  - Component-level tests in DomoticsCore-*/test/
  - simulateSwitchCommand() helper creates MQTTMessageEvent + core.loop() drain pattern
  - EventBus subscription verification: subscribe → act → assert fired flag
---

# Tech-Spec: Roadmap Completion — Virtual Dispatch + EventBus Commands

**Created:** 2026-03-08

## Overview

### Problem Statement

The HomeAssistant command routing uses `static_cast` + string matching instead of leveraging virtual dispatch (R24 partial). Incoming HA commands are not emitted on the EventBus, and consumers rely on direct callbacks violating Constitution VI (R26).

Note: R11-R13 (file splits for WebUI.h, StreamingContextSerializer.h, Wifi.h) were investigated and found to be **already compliant** with Constitution VII when counting correctly (excluding blanks/comments): WebUI.h = 767, StreamingContextSerializer.h = 745, Wifi.h = 671. All under the 800-line limit. Removed from scope.

### Solution

Complete R24 by replacing `static_cast` routing with virtual dispatch in `HomeAssistantComponent::handleCommand()`. Implement R26 by fully replacing callbacks with EventBus `ha/command` event emission — callbacks are removed from all `addXxx()` signatures. This is a breaking API change requiring a v2.0.0 major version bump.

### Scope

**In Scope:**
- R24 — Replace static_cast routing with virtual dispatch in HomeAssistant::handleCommand()
- R26 — Remove callback parameters from all `addXxx()` methods, add `HACommandEvent` struct + `EVENT_COMMAND` constant, emit on EventBus. Full callback replacement (NOT dual mode).
- Version bump to v2.0.0 (breaking API change)
- Migration of all examples and tests from callbacks to EventBus subscriptions

**Out of Scope:**
- R11/R12/R13 — File splits (all 3 files already compliant with Constitution VII when excluding blanks/comments)
- New features beyond what's in the roadmap
- Removing entity internal state (entities remain source of truth for their own state)

## Context for Development

### Codebase Patterns

- **Header-only design**: Most components are `.h` only. Some use `_impl.h` to separate declaration from implementation.
- **Component model**: Extends `IComponent` with `begin()`, `loop()`, `shutdown()`, `getDependencies()`, `getMetadata()`.
- **EventBus**: Instance-based, queue-based pub/sub. Events are POD structs with fixed-size `char[]` fields (no String). Published via `publish()` (or `emit()` wrapper on `IComponent`), consumed via `subscribe(topic, handler, owner, replayLast)`.
- **HAL isolation**: `#ifdef` only in `*_HAL.h` files. Business logic must be platform-agnostic.
- **Constitution VII**: 800 lines max per file (excluding blanks/comments).
- **Constitution VI**: Components communicate via EventBus, not direct callbacks.
- **Constitution XIV**: Memory safety is ABSOLUTE PRIORITY. No heap allocation in event structs.

### Files to Reference

| File | Purpose | Key Lines |
| ---- | ------- | --------- |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` | R24+R26 — handleCommand routing | Lines 657-680: `if/else` + `static_cast` chain to replace (within `handleCommand()` method at lines 627-681). Lines 225-313: addXxx signatures to change. Note: `addSensor()` (line 182) and `addBinarySensor()` (line 205) have NO callbacks and are unaffected. |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEvents.h` | R26 — add EVENT_COMMAND + HACommandEvent | Currently only 22 lines with 2 event constants. Note: `HAEntityAddedEvent` struct is defined in `HomeAssistant.h` (lines 55-58), not here — organizational inconsistency, out of scope to fix. |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h` | R24 — base class | Line 92: change `virtual void handleCommand(const String&) {}` to `virtual bool handleCommand(const String&) { return true; }` (return false = invalid command, skip event emission). Line 89: stale TODO comment to remove (cleanup in Task 7). |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h` | R26 — remove callback | Line 27: `std::function<void(bool)> commandCallback`, Line 54: handleCommand override |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HALight.h` | R26 — remove callback | Line 22: `std::function<void(bool, uint8_t)> commandCallback`, Line 59: handleCommand override |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAButton.h` | R26 — remove callback | Line 24: `std::function<void()> pressCallback`, Line 62: handleCommand override |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h` | R26 — remove callback | Line 86: `std::function<void(const String&, const String&)> commandCallback`, Line 137: handleCommand with code parsing |
| `.specify/memory/constitution.md` | Project constitution (non-negotiable rules) | |
| `docs/CODE-ROADMAP.md` | Source roadmap with item descriptions | |

### Technical Decisions

- **R11/R12/R13 removed from scope**: Investigation confirmed all 3 files are under 800 lines when counting correctly (excluding blanks/comments per Constitution VII): WebUI.h = 767, StreamingContextSerializer.h = 745, Wifi.h = 671. No constitutional violation.
- **R24 virtual dispatch**: All 4 entity classes already have `override` on `handleCommand()`. The refactoring replaces the `if/else` chain + `static_cast` (HomeAssistant.h lines 657-680) with a single `entity->handleCommand(payload)` call. The auto-publish logic for switches (lines 661-666) moves INTO `HASwitch::handleCommand()`. **Note:** Two `static_cast` usages remain post-R24 for data access (alarm `lastCommand`/`lastCode`, switch `optimistic`/`autoPublishState`) — these are field reads, not method dispatch, and are acceptable. R24 eliminates `static_cast` for **polymorphic method routing** only.
- **R26 full callback replacement (NOT dual mode)**: Callbacks removed from signatures and entity fields:
  - `HASwitch`: remove `std::function<void(bool)> commandCallback` (line 27), remove from constructor (line 17)
  - `HALight`: remove `std::function<void(bool, uint8_t)> commandCallback` (line 22), remove from constructor (line 17)
  - `HAButton`: remove `std::function<void()> pressCallback` (line 24), remove from constructor (line 17)
  - `HAAlarmControlPanel`: remove `std::function<void(const String&, const String&)> commandCallback` (line 86), remove from constructor (line 75)
  - Each entity's `handleCommand()` keeps state update logic but removes callback invocation
  - `HomeAssistant.h addXxx()`: remove callback parameters from all 4 method signatures (lines 225-313)
- **R26 HACommandEvent struct**: Fixed-size POD (256 bytes), zero heap allocation in the struct itself. Fields: `entityId[64]`, `component[32]`, `command[128]`, `code[32]`. Note: EventBus `publish()` copies the struct into a `std::vector<uint8_t>` (heap allocation in EventBus infrastructure) — this is by design and not a Constitution XIV violation (the struct is POD, the queue is infrastructure). The `HACommandEvent` only contains `char[]` fields, so `reinterpret_cast` from `void*` is alignment-safe (byte-aligned). If non-char fields are ever added, a `memcpy` pattern must be used instead.
- **`ev.command` contains raw MQTT payload, not parsed state**: For switches, `ev.command` is the raw payload string (default "ON"/"OFF", but configurable via `payloadOn`/`payloadOff`). The entity's `state` field holds the parsed boolean. EventBus subscribers should use `ev.command` for MQTT-level comparisons or access the entity's `state` field directly for semantic state. Example code uses `strcmp(ev.command, "ON")` which only works with default payloads — documented as a limitation.
- **Constitution VIII waiver**: Constitution VIII (Progressive Refactoring) mandates "deprecate before removing." This spec removes callbacks without a deprecation period. Justification: the callback API is internal (no published external consumers), all call sites are within this repository (~26 total), and a dual-mode period would add complexity with zero benefit since all consumers are migrated atomically. The v2.0.0 major version bump per Constitution XV signals the breaking change.
- **R26 event emission is centralized**: `HomeAssistantComponent::handleCommand()` emits the event AFTER `entity->handleCommand(payload)`. Individual entities do NOT emit events — they only update internal state (+ auto-publish if configured). The `code` field is populated only for `alarm_control_panel` entities: `HAAlarmControlPanel::handleCommand()` already parses command/code from payload (lines 137-167) — the parsed values need to be accessible for the event struct (add public getters or fields `lastCommand`/`lastCode`).
- **Entity internal state — NEW fields required**: `HASwitch::state`, `HALight::state`, `HALight::brightness` do NOT currently exist in the codebase — they are **created** by this spec (Tasks 2 and 3). Previously, state was only passed through callbacks and never stored. `HAAlarmControlPanel::lastCommand`/`lastCode` are also new fields (Task 5). Only `HASwitch::optimistic` and `HASwitch::autoPublishState` are pre-existing fields.
- **Version bump**: v2.0.0 — breaking API change on all `addXxx()` signatures.
- **`bump_version.py` bug — `library.properties` not updated**: The current `tools/bump_version.py` only updates `library.json` and `metadata.version` in source files. It does NOT update `library.properties`. This has caused version drift: root `library.json` = `1.9.0`, root `library.properties` = `1.5.0`. Task 19 must either fix the script or manually sync `library.properties`.

## Implementation Plan

### Tasks

#### Phase 1: Event Infrastructure (no breaking changes yet)

- [x] Task 1: Add `HACommandEvent` struct and `EVENT_COMMAND` constant to `HAEvents.h`
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEvents.h`
  - Action: Add after existing event constants:
    ```cpp
    static constexpr const char* EVENT_COMMAND = "ha/command";

    struct HACommandEvent {
        char entityId[64];
        char component[32];
        char command[128];
        char code[32];
    };
    ```
  - Notes: Fixed-size POD struct, ~256 bytes, zero heap allocation. `code` field used only by alarm_control_panel, empty string for other entity types.

#### Phase 2: Entity Refactoring (remove callbacks, update state management)

- [x] Task 2: Refactor `HASwitch` — remove callback, internalize auto-publish
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h`
  - Action:
    - Remove `std::function<void(bool)> commandCallback` field (line 27)
    - Remove `commandCallback` parameter from constructor (line 17) and initializer list (line 19)
    - Add public `bool state = false` field (does NOT currently exist — state was only passed through callback)
    - Change `handleCommand()` signature to `bool handleCommand(const String& payload) override`. Remove `if (commandCallback)` guard, always parse state from payload (`state = (payload == payloadOn)`), return `true` (switch commands are always valid)
    - Move auto-publish logic INTO handleCommand: need access to parent component's `publishState()`. Alternative: store parsed state, let `HomeAssistantComponent` handle auto-publish after virtual dispatch call.
  - Notes: Auto-publish requires access to `HomeAssistantComponent::publishState()`. Since entities don't hold a back-reference to the parent, keep auto-publish in `HomeAssistantComponent::handleCommand()` after the virtual dispatch call. `HASwitch::handleCommand()` just updates `state` field.

- [x] Task 3: Refactor `HALight` — remove callback
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HALight.h`
  - Action:
    - Remove `std::function<void(bool, uint8_t)> commandCallback` field (line 22)
    - Remove `commandCallback` parameter from constructor (line 17) and initializer list
    - Add public fields: `bool state = false`, `uint8_t brightness = 0` (neither exists currently — state was only passed through callback)
    - Change `handleCommand()` signature to `bool handleCommand(const String& payload) override`. Remove `if (!commandCallback) return` guard, keep JSON parsing logic, store parsed values in `state` and `brightness` fields instead of invoking callback. Add validation: if `deserializeJson` fails AND payload is not "ON" or "OFF", log `DLOG_W` and return `false` (invalid command — no event emission). Otherwise return `true`.
  - Notes: **Behavioral change**: currently, lights without a callback silently ignore commands (`if (!commandCallback) return`). After this refactor, ALL valid light commands are processed and update `state`/`brightness` fields. Invalid/garbage payloads are rejected via `return false` (prevents silent state corruption to OFF/brightness=0). No light auto-publish is added (existing TODO at HomeAssistant.h line 670 remains out of scope — light state requires JSON serialization).

- [x] Task 4: Refactor `HAButton` — remove callback, make handleCommand a no-op
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAButton.h`
  - Action:
    - Remove `std::function<void()> pressCallback` field (line 24)
    - Remove `pressCallback` parameter from constructor (line 17) and initializer list (line 19)
    - Do NOT add a `pressed` field — buttons are stateless triggers. The `ha/command` EventBus event with `command="PRESS"` is the sole notification mechanism.
    - Change `handleCommand()` signature from `void` to `bool` (matches base class change in HAEntity.h): `bool handleCommand(const String& payload) override`. Return `true` if `payload == payloadPress` (valid), `false` otherwise (invalid — centralized emission in Task 7 will skip the event). Log via DLOG_D on valid press.
    - No additional fields needed — the return value replaces the need for a `lastCommandValid` field, keeping the entity's public API clean (Constitution III — KISS).

- [x] Task 5: Refactor `HAAlarmControlPanel` — remove callback, expose parsed command/code
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h`
  - Action:
    - Remove `std::function<void(const String&, const String&)> commandCallback` field (line 86)
    - Remove constructor parameter `onCommand` (line 75 — note: parameter is named `onCommand`, field is named `commandCallback`; both are removed) and its initializer list assignment (line 77)
    - Add public fields: `char lastCommand[64] = {}`, `char lastCode[32] = {}`
    - Change `handleCommand()` signature to `bool handleCommand(const String& payload) override`. Remove `if (!commandCallback) return` guard, **zero-initialize `lastCommand` and `lastCode` at the start** (prevents stale data from previous commands leaking if parsing fails), keep full parsing logic (trim, space split), store parsed command in `lastCommand` via `strncpy()` and code in `lastCode` via `strncpy()` instead of calling `commandCallback(command, codeValue)`. Return `true` (alarm commands are always valid — parsing always succeeds even if code is empty).

#### Phase 3: HomeAssistant Component Refactoring (R24 + R26 core)

- [x] Task 6: Remove callback parameters from all `addXxx()` signatures
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action:
    - `addSwitch()` (line 225): remove `std::function<void(bool)> commandCallback` parameter. Update entity construction to not pass callback.
    - `addLight()` (line 248): remove `std::function<void(bool, uint8_t)> commandCallback` parameter. Update entity construction.
    - `addButton()` (line 268): remove `std::function<void()> pressCallback` parameter. Update entity construction.
    - `addAlarmControlPanel()` (line 288): remove `const std::function<void(const String&, const String&)>& commandCallback` parameter. Update entity construction.
  - Notes: This is the breaking API change. All downstream code (examples, tests) must be updated.

- [x] Task 7: Replace `handleCommand()` routing with virtual dispatch + EventBus emission
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Replace the entire `if/else` chain at lines 657-680 (NOT lines 627-656 which are topic parsing preamble — those are retained) with:
    ```cpp
    // R24: Virtual dispatch — replaces static_cast chain
    // R26: handleCommand returns false if command is invalid (e.g., button with wrong payload, light with garbage)
    bool valid = entity->handleCommand(payload);
    if (!valid) return;

    // R26: Emit ha/command EventBus event
    HACommandEvent ev{};
    strncpy(ev.entityId, entityId.c_str(), sizeof(ev.entityId) - 1);
    strncpy(ev.component, entity->component.c_str(), sizeof(ev.component) - 1);
    strncpy(ev.command, payload.c_str(), sizeof(ev.command) - 1);

    // Log warning if entity ID or payload was truncated (use .length() for O(1))
    if (entityId.length() >= sizeof(ev.entityId)) {
        DLOG_W(LOG_HA, "Entity ID truncated: %s (%u > %zu)",
               entityId.c_str(), entityId.length(), sizeof(ev.entityId) - 1);
    }
    if (payload.length() >= sizeof(ev.command)) {
        DLOG_W(LOG_HA, "Command payload truncated for entity %s (%u > %zu)",
               entityId.c_str(), payload.length(), sizeof(ev.command) - 1);
    }

    // Populate code field for alarm_control_panel
    // Note: overwrites ev.command with parsed command (e.g., "ARM_AWAY" instead of raw "ARM_AWAY 1234")
    // Raw payload is NOT preserved in the event for alarm entities — by design.
    if (entity->component == "alarm_control_panel") {
        auto* alarm = static_cast<HAAlarmControlPanel*>(entity);
        strncpy(ev.command, alarm->lastCommand, sizeof(ev.command) - 1);
        strncpy(ev.code, alarm->lastCode, sizeof(ev.code) - 1);
    }

    emit(HAEvents::EVENT_COMMAND, ev);

    // Auto-publish for switches (moved from old if/else chain)
    if (entity->component == "switch") {
        auto* sw = static_cast<HASwitch*>(entity);
        if (!sw->optimistic && sw->autoPublishState) {
            publishState(entityId, payload);
        }
    }
    ```
  - Also: remove stale TODO comment at `HAEntity.h` line 89 (`TODO: make handleCommand() virtual override in HASwitch/HALight/HAButton`) — this is now completed by R24.
  - Notes:
    - Two `static_cast` uses remain (alarm, switch) for field access — justified as data reads, not method dispatch. Button no longer needs `static_cast` thanks to the `bool` return value approach.
    - Truncation warnings added for both entity ID (>63 bytes) and payload (>127 bytes). Use `entityId.length()` instead of `strlen(entityId.c_str())` for O(1) check.
    - **Command validation via return value**: `handleCommand()` returns `false` for invalid commands (e.g., button with wrong payload, light with garbage payload). The centralized handler skips event emission on `false`. No entity-specific `static_cast` or field checks needed.
    - **Alarm stale data defense**: Task 5 ensures `lastCommand`/`lastCode` are zero-initialized at the START of `handleCommand()` before parsing, preventing stale data leakage.
    - **Alarm `ev.command` overwrite**: For alarm entities, `ev.command` is overwritten from raw payload to parsed command (e.g., `"ARM_AWAY"` instead of `"ARM_AWAY 1234"`). The raw payload is NOT available in the event. This is by design — subscribers use the parsed `command` + `code` fields.
    - **`ev.command` semantics vary by entity type**: switches get raw payload (e.g., "ON"), alarms get parsed command, lights get raw JSON, buttons get "PRESS". Subscribers must branch on `ev.component` to interpret `ev.command`. This is documented, not a bug.
    - **Auto-publish re-entrancy**: The `publishing` guard in `HomeAssistantComponent` (line 521) protects `publishState()` against re-entrant calls. Since `handleCommand()` is triggered from an MQTT event handler (synchronous), and `publishState()` is also synchronous, re-entrancy cannot occur in the current single-threaded Arduino loop. No additional guard needed in Task 7.
  - `#include "HAEvents.h"` is already present in `HomeAssistant.h` (line 15) — no include change needed.

- [x] Task 8: Update component metadata version
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Change `metadata.version = "1.6.1"` to `metadata.version = "2.0.0"` in constructor (line ~107)

#### Phase 4: Example Migration

- [x] Task 9: Migrate `BasicHA/src/main.cpp` to EventBus
  - File: `DomoticsCore-HomeAssistant/examples/BasicHA/src/main.cpp`
  - Action:
    - Remove callback lambdas from `addSwitch()` (line 158), `addButton()` (line 165), `addAlarmControlPanel()` (line 172)
    - Add EventBus subscriptions after `core.begin()` — **one handler per component type** (not a monolithic if/else):
      ```cpp
      // Switch command handler
      core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [](const void* data) {
          auto& ev = *reinterpret_cast<const HACommandEvent*>(data);
          if (strcmp(ev.component, "switch") != 0) return;
          // Note: ev.command contains the raw MQTT payload (default "ON"/"OFF", but configurable via payloadOn/payloadOff).
          // For switches with custom payloads, compare against the entity's configured values instead.
          bool state = (strcmp(ev.command, "ON") == 0);
          HAL::Platform::digitalWrite(LED_BUILTIN, state ? HAL::ledBuiltinOn() : HAL::ledBuiltinOff());
          DLOG_I(LOG_APP, "Relay set to: %s", state ? "ON" : "OFF");
      }, nullptr);

      // Button command handler
      core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [](const void* data) {
          auto& ev = *reinterpret_cast<const HACommandEvent*>(data);
          if (strcmp(ev.component, "button") != 0) return;
          DLOG_I(LOG_APP, "Restart button pressed from Home Assistant");
          HAL::Platform::delayMs(1000);
          HAL::Platform::restart();
      }, nullptr);

      // Alarm command handler
      core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [](const void* data) {
          auto& ev = *reinterpret_cast<const HACommandEvent*>(data);
          if (strcmp(ev.component, "alarm_control_panel") != 0) return;
          DLOG_I(LOG_APP, "Alarm command: %s (code: %s)", ev.command, ev.code);
          // ... alarm state machine logic ...
      }, nullptr);
      ```
    - **Alarm state machine migration** (most complex part): The current alarm callback (lines 172-200) is ~67 lines of stateful logic with state transitions, timers (`alarmDelayStart`), and multiple `haPtr->publishState()` calls. These variables (`haPtr`, `alarmState`, `alarmDelayStart`) are currently closure-captured in the lambda. In the EventBus handler, use file-scope or `static` local variables for the alarm state, and capture `haPtr` in the lambda capture list (EventBus handlers support stateful lambdas). Keep the full state machine logic intact — just move it from the `addAlarmControlPanel()` callback lambda into the EventBus subscribe lambda.
  - Notes: `#include "HAEvents.h"` must be added.
  - **Performance consideration**: Multiple subscriptions to the same topic means every `ha/command` event triggers ALL handlers, with most doing a `strcmp` and returning. On ESP8266 with limited CPU, consider using a single handler with an `if/else` on `ev.component` instead. Trade-off: single handler is more efficient but less modular. For examples with ≤3 entity types, multiple subscriptions are acceptable for clarity. For production code with many entity types, prefer a single handler or use sub-topics (e.g., `ha/command/switch`).

- [x] Task 10: Migrate `HAWithWebUI/src/main.cpp` to EventBus
  - File: `DomoticsCore-HomeAssistant/examples/HAWithWebUI/src/main.cpp`
  - Action: Same pattern as Task 9 — remove callbacks from `addSwitch()` (line 142) and `addButton()` (line 149), add EventBus subscription.

- [x] Task 11: Migrate `FullStack/src/main.cpp` to EventBus
  - File: `DomoticsCore-System/examples/FullStack/src/main.cpp`
  - Action: Same pattern — remove callbacks from `addSwitch()` (line 184) and `addButton()` (line 194), add EventBus subscription.

#### Phase 5: Test Migration + New Tests

- [x] Task 12: Migrate `test_ha_component.cpp` — remove callbacks, add EventBus verification
  - File: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
  - Action:
    - All `addSwitch()` calls (9): remove callback parameter, keep remaining args
    - All `addLight()` calls (2): remove callback parameter
    - All `addButton()` calls (2): remove callback parameter
    - Tests that verify callback was invoked: replace with EventBus subscription + flag assertion
    - Example migration for switch callback test (lines 206-208):
      ```cpp
      // BEFORE:
      ha->addSwitch("relay", "Relay Switch", [&switchState](bool state) { switchState = state; }, "mdi:electric-switch");
      // AFTER:
      ha->addSwitch("relay", "Relay Switch", "mdi:electric-switch");
      bool eventFired = false;
      bool receivedState = false;
      core.getEventBus().subscribe(String(HAEvents::EVENT_COMMAND), [&](const void* data) {
          auto& ev = *reinterpret_cast<const HACommandEvent*>(data);
          if (strcmp(ev.entityId, "relay") == 0) {
              eventFired = true;
              receivedState = (strcmp(ev.command, "ON") == 0);
          }
      }, nullptr);
      ```
    - Tests verifying switch state: access `HASwitch::state` directly instead of callback capture variable

- [x] Task 13: Fix `test_ha_events.cpp` — update `addXxx()` signatures (no test logic changes)
  - File: `DomoticsCore-HomeAssistant/test/test_ha_events/test_ha_events.cpp`
  - Action: Remove callback parameters from `addSwitch()` (line 80), `addLight()` (line 103), `addButton()` (line 126), `addAlarmControlPanel()` (line 149). These tests verify `ha/entity_added` event — only the `addXxx()` call signatures change, test assertions remain identical.

- [x] Task 14: Migrate `test_ha_alarm_panel.cpp` — replace callbacks with state/event checks
  - File: `DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp`
  - Action:
    - Remove callback parameters from `addAlarmControlPanel()` calls via HomeAssistantComponent (lines 278, 319, 355, 402)
    - **Also update ~10 direct `HAAlarmControlPanel(...)` constructor calls** in unit tests (lines ~59, 109, 152, 167, 193, 213, 231, 246, 425, 444) that pass callback lambdas directly — remove the `onCommand` parameter from these constructors too
    - Tests that captured command/code via callback: replace with `panel.lastCommand` / `panel.lastCode` field access
    - Direct entity tests (e.g., `panel.handleCommand("ARM_AWAY")`): verify `panel.lastCommand` equals `"ARM_AWAY"` and `panel.lastCode` is empty
    - Code parsing tests (e.g., `panel.handleCommand("ARM_AWAY 1234")`): verify `panel.lastCommand` equals `"ARM_AWAY"` and `panel.lastCode` equals `"1234"`

- [x] Task 15: Migrate `test_fullstack.cpp`
  - File: `DomoticsCore-System/examples/FullStack/test/test_fullstack.cpp`
  - Action: Remove callback parameters from `addSwitch()` (line 108) and `addButton()` (line 111).
  - **Pre-check**: This test file has pre-existing compilation issues that must be fixed first:
    - Lines 98-99: `haCfg.nodeId = "test-device"` assigns string literal to `char[]` field — must use `strncpy()` or `snprintf()`.
    - Line 101: `HomeAssistantComponent(mqttComp, haCfg)` uses a two-argument constructor that may not match current API.
    Fix these before applying callback migration.

- [x] Task 16: Add new R24 virtual dispatch test
  - File: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
  - Action: Add test verifying virtual dispatch works through base pointer:
    ```cpp
    void test_virtual_dispatch_via_base_pointer() {
        HASwitch sw("test", "Test");
        HAEntity* base = &sw;
        base->handleCommand("ON");
        TEST_ASSERT_TRUE(sw.state);
        base->handleCommand("OFF");
        TEST_ASSERT_FALSE(sw.state);
    }
    ```

- [x] Task 17: Add new R26 EventBus emission tests
  - File: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
  - Action: Add tests for each entity type:
    - Test: switch command emits `ha/command` with `component="switch"`, `entityId`, `command="ON"`
    - Test: light command emits `ha/command` with `component="light"`, `command=<JSON payload>`
    - Test: button command emits `ha/command` with `component="button"`, `command="PRESS"`
    - Test: alarm command emits `ha/command` with `component="alarm_control_panel"`, `command="ARM_AWAY"`, `code="1234"`
    - Test: auto-publish fires for switch with `autoPublishState = true`

- [x] Task 18: Add HeapTracker memory test
  - File: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
  - Action: Add test verifying no heap growth after 10 command cycles:
    ```cpp
    void test_command_event_heap_stability() {
        // Setup HA with switch
        // HeapTracker checkpoint
        // Simulate 10 switch commands via MQTT
        // HeapTracker verify: delta < tolerance
    }
    ```

- [ ] Task 18b: Add negative/edge-case tests for command handling
  - File: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
  - Action: Add tests for edge cases not covered by happy-path tests:
    - Test: empty payload string does not crash and emits event with empty `command` field
    - Test: payload exceeding 127 bytes is truncated in event (verify truncation + DLOG_W fires)
    - Test: button with invalid payload (not "PRESS") does NOT emit `ha/command` event (validates guard logic from Task 4)
    - Test: alarm command without code (`"ARM_AWAY"` with no space) sets `lastCode` to empty string
    - Test: alarm command with multiple spaces (`"ARM_AWAY  1234"`) parses correctly
    - Test: command to unknown/unregistered entityId does not crash or emit event

#### Phase 6: Version Bump + Roadmap Update

- [x] Task 19: Bump library version to 2.0.0
  - Files: `DomoticsCore-HomeAssistant/library.json` (currently `1.6.1`), root `library.json` (currently `1.9.0`), root `library.properties` (currently `1.5.0` — out of sync)
  - Action:
    - Run `python tools/bump_version.py HomeAssistant major` — this bumps HomeAssistant `library.json` from `1.6.1` → `2.0.0`, syncs `metadata.version` in source files, and propagates a major bump to root `library.json` (`1.9.0` → `2.0.0`).
    - **Manually** update root `library.properties` version from `1.5.0` to `2.0.0` — `bump_version.py` does NOT update `library.properties` (this is a known bug in the script).
  - Notes: The version inconsistency between root `library.json` (`1.9.0`) and root `library.properties` (`1.5.0`) pre-dates this spec. It exists because `bump_version.py` only updates `library.json` and `metadata.version` in source files, never `library.properties`. This task corrects the drift.
  - **Follow-up (out of scope but recommended)**: Fix `bump_version.py` to also update `library.properties` to prevent future drift.

- [x] Task 20: Update CODE-ROADMAP.md tracking table
  - File: `docs/CODE-ROADMAP.md`
  - Action: Update tracking table entries:
    - R24: `R24 partial (override added)` → `R24: DONE — virtual dispatch replaces static_cast routing`
    - R26: `TODO — Impact analysis: LOW RISK` → `R26: DONE — ha/command EventBus event, callbacks removed (v2.0.0 breaking change)`
    - R11/R12/R13: Add note `N/A — files already compliant (excluding blanks/comments: WebUI.h=767, StreamingContextSerializer.h=745, Wifi.h=671)`

### Acceptance Criteria

#### R24 — Virtual Dispatch

- [x] AC1: Given a registered HASwitch entity, when an MQTT command "ON" arrives on its set topic, then `HASwitch::handleCommand()` is invoked via virtual dispatch (no `static_cast` in `HomeAssistantComponent::handleCommand()` for **method dispatch** — `static_cast` for field access on alarm/switch is acceptable)
- [x] AC2: Given a registered HALight entity, when an MQTT command JSON arrives, then `HALight::handleCommand()` is invoked via virtual dispatch and `state`/`brightness` fields are updated
- [x] AC3: Given a registered HAButton entity, when an MQTT command "PRESS" arrives, then `HAButton::handleCommand()` is invoked via virtual dispatch (no-op) and an `ha/command` event is emitted with `command="PRESS"`
- [x] AC4: Given a registered HAAlarmControlPanel entity, when an MQTT command "ARM_AWAY 1234" arrives, then `HAAlarmControlPanel::handleCommand()` is invoked via virtual dispatch, `lastCommand` contains "ARM_AWAY" and `lastCode` contains "1234"
- [x] AC5: Given an HAEntity base pointer to an HASwitch, when `handleCommand("ON")` is called on the base pointer, then the derived `HASwitch::handleCommand()` executes and updates `state = true`

#### R26 — EventBus Command Events

- [x] AC6: Given a registered HASwitch entity, when an MQTT command "ON" arrives, then an `ha/command` event is emitted on the EventBus with `entityId="<switch_id>"`, `component="switch"`, `command="ON"`, `code=""`
- [x] AC7: Given a registered HAAlarmControlPanel entity, when an MQTT command "ARM_AWAY 1234" arrives, then an `ha/command` event is emitted with `command="ARM_AWAY"` and `code="1234"`
- [x] AC8: Given a registered HASwitch entity with `autoPublishState = true`, when an MQTT command arrives, then the switch state is auto-published to MQTT after the command is processed
- [x] AC9: Given a consumer subscribed to `ha/command` via EventBus, when a switch command "ON" arrives, then the consumer receives the event with `entityId`, `component="switch"`, and `command="ON"` fields correctly populated
- [x] AC10: Given 10 sequential MQTT commands sent to a switch entity, when all commands are processed, then free heap returns to within tolerance of the baseline (HeapTracker verification)

#### Breaking API Change

- [x] AC11: Given a call to `addSwitch("id", "name", "icon")` without a callback parameter, then the entity is registered successfully and responds to commands via EventBus
- [x] AC12: Given all example files (BasicHA, HAWithWebUI, FullStack), when compiled on ESP32/ESP32-C3/ESP8266, then compilation succeeds with zero errors
- [x] AC13: Given all existing test files, when executed on native platform, then all tests pass (100% pass rate)

## Additional Context

### Dependencies

- R24+R26 are implemented together in a single pass on `handleCommand()` (same code, same commit).

### Testing Strategy

- **R24+R26 (virtual dispatch + EventBus commands)**:
  - Existing tests migrated from callback pattern to EventBus subscription pattern. 13 `addXxx()` in `test_ha_component.cpp`, 14 in `test_ha_alarm_panel.cpp` (4 via `addAlarmControlPanel()` + 10 direct constructor), 4 signature fixes in `test_ha_events.cpp`, 2 in `test_fullstack.cpp` = 33 test call sites + 7 example call sites + 4 declarations in `HomeAssistant.h` = 44 total.
  - New test: call `handleCommand()` via `HAEntity*` base pointer — verify derived method is invoked (virtual dispatch).
  - New tests for each entity type (switch, light, button, alarm_control_panel) verifying `ha/command` event is emitted with correct fields.
  - New test: alarm_control_panel `ha/command` event includes `code` field.
  - New test: entity internal state updated correctly after command (e.g., `HASwitch::state` reflects ON/OFF).
  - New test: auto-publish fires after command for switch with `autoPublishState = true`.
  - HeapTracker test: 10 commands in sequence, verify heap returns to baseline (Constitution XIV).
  - Ordering: entity `handleCommand()` executes before `emit()` — guaranteed by code sequence in Task 7 (not a runtime property to test).

### Notes

- ESP8266 memory impact of R26: +256 bytes transient per command in EventBus queue. Negligible vs 35-40KB free heap. See R26-ANALYSIS in CODE-ROADMAP.md.
- All entity `handleCommand()` overrides already verified present: HASwitch (line 54), HALight (line 59), HAButton (line 62), HAAlarmControlPanel (line 137).
- Call sites impacted by callback removal (44 total): 7 in examples (3 BasicHA + 2 HAWithWebUI + 2 FullStack), 13 `addXxx()` in `test_ha_component.cpp` (9 switch + 2 light + 2 button), 14 in `test_ha_alarm_panel.cpp` (4 `addAlarmControlPanel()` + 10 direct constructor), 4 signature fixes in `test_ha_events.cpp`, 2 in `test_fullstack.cpp`, 4 declarations in `HomeAssistant.h`.
- Party mode insights integrated: centralized event emission, `if` for alarm code (no abstraction), v2.0.0 bump, full callback removal (not dual mode).
- R11/R12/R13 removed: all 3 files under 800 non-blank/non-comment lines (WebUI.h=767, StreamingContextSerializer.h=745, Wifi.h=671).
- HAAlarmControlPanel code parsing: handleCommand() (lines 137-167) parses "COMMAND CODE" from payload with space separator + trim. Parsed command/code values currently passed directly to callback. After callback removal, need to store as `lastCommand`/`lastCode` fields so HomeAssistantComponent can read them for the HACommandEvent `code` field.
- **No rollback plan**: This is a full breaking API change with no dual-mode period. If issues are discovered mid-implementation, all changes must be reverted together. Recommend implementing on a feature branch and validating ALL tests + ALL example compilations before merging.
- **External consumers**: This spec only tracks call sites within the DomoticsCore repository. Any external code depending on `addSwitch(id, name, callback, ...)` signatures will break silently. The v2.0.0 major version bump signals this per semver convention, but a CHANGELOG entry or migration guide is recommended (out of scope for this spec).
- **EventBus queue overflow risk**: EventBus caps the queue at 32 entries, dropping the oldest on overflow. Since R26 makes EventBus the SOLE command notification mechanism (callbacks removed), a burst of commands could silently drop `ha/command` events. The entity's internal state is still updated (via `handleCommand()`), but EventBus subscribers would miss the event. Risk is low for typical HA usage (commands are human-triggered, not burst), but should be documented for automation-heavy deployments.
