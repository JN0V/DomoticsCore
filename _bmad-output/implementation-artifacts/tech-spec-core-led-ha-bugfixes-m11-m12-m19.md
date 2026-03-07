---
title: 'Core, LED & HomeAssistant Bug Fixes (M11, M12, M19)'
slug: 'core-led-ha-bugfixes-m11-m12-m19'
created: '2026-03-07'
status: 'completed'
stepsCompleted: [1, 2, 3, 4]
tech_stack: ['C++17', 'PlatformIO', 'Arduino', 'ESP32', 'ESP8266', 'Unity test framework']
files_to_modify:
  - 'DomoticsCore-Core/include/DomoticsCore/Core.h'
  - 'DomoticsCore-LED/include/DomoticsCore/LED.h'
  - 'DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h'
  - 'DomoticsCore-LED/examples/LEDWithWebUI/src/main.cpp'
  - 'docs/components/led/README.md'
  - 'docs/components/system/technical-reference.md'
  - 'DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp'
files_to_create:
  - 'DomoticsCore-LED/test/test_led_component/test_led_component.cpp'
  - 'DomoticsCore-HomeAssistant/test/test_ha_events/test_ha_events.cpp'
code_patterns:
  - 'IComponent::emit() sticky pattern (IComponent.h:222-226)'
  - 'metadata.name short naming convention (Wifi, MQTT, NTP, etc.)'
  - 'HAEvents::EVENT_ENTITY_ADDED emission in addSensor() (HomeAssistant.h:153)'
  - 'EventBus publish/publishSticky dual API (EventBus.h:119-149)'
test_patterns:
  - 'Unity framework with RUN_TEST() macros'
  - 'Lambda-based event capture for subscribe verification'
  - 'HeapTracker for memory stability (not needed here — functional tests only)'
versions:
  core: '1.5.1'
  led: '1.3.0'
  homeassistant: '1.6.0'
  root: '1.8.1'
---

# Tech-Spec: Core, LED & HomeAssistant Bug Fixes (M11, M12, M19)

**Created:** 2026-03-07

## Overview

### Problem Statement

Three API inconsistencies exist across the DomoticsCore framework, violating constitution principles (VI — EventBus Architecture, XIII — Anti-Pattern Avoidance):

1. **M11**: `Core::emit()` lacks a `sticky` parameter while `IComponent::emit()` supports it. Code using `core.emit()` directly cannot create sticky events, breaking API symmetry.
2. **M12**: `LEDComponent` sets `metadata.name = "LEDComponent"` while all other components use short names (`"Wifi"`, `"MQTT"`, `"NTP"`, etc.). This breaks the naming convention and causes `getComponent("LED")` to fail.
3. **M19**: Only `addSensor()` emits `ha/entity_added` event. The 5 other registration methods (`addBinarySensor`, `addSwitch`, `addLight`, `addButton`, `addAlarmControlPanel`) do not emit it, breaking event consistency.

### Solution

Align all three APIs with their established patterns:
- M11: Add `bool sticky = false` parameter to `Core::emit()` template overload only, forwarding to `publishSticky()` when true (same pattern as `IComponent::emit()`). No-payload overload left unchanged to avoid template resolution ambiguity with `core.emit("topic", true)`.
- M12: Change `metadata.name` from `"LEDComponent"` to `"LED"`. Clean break, no alias.
- M19: Replace the raw `String` emit in `addSensor()` with `HAEntityAddedEvent` struct (fixes C21), and add the same struct-based emit to all 5 missing `addXxx()` methods. Uses `snprintf` to populate `ev.id` and `ev.component` — zero-heap, fixed-size, safe for `reinterpret_cast` serialization.

### Scope

**In Scope:**
- M11: Add sticky parameter to `Core::emit()` template overload only (no-payload overload unchanged — avoids resolution ambiguity)
- M12: Fix LED metadata.name, update any references in examples/tests/docs
- M19: Replace raw String emit in addSensor() with HAEntityAddedEvent struct (C21 fix), add same struct-based emit to addBinarySensor, addSwitch, addLight, addButton, addAlarmControlPanel
- Unit tests for all three fixes
- Version bumps: Core PATCH, LED MINOR, HomeAssistant PATCH

**Out of Scope:**
- Making `ha/entity_added` a sticky event (separate discussion)
- R24 virtual dispatch refactoring
- R26 EventBus command emission architecture
- Any other roadmap items

## Context for Development

### Codebase Patterns

- **Sticky emit pattern** (IComponent.h:222-226): `if (sticky) publishSticky() else publish()` — replicate exactly in Core::emit().
- **EventBus dual API** (EventBus.h:119-149): Both `publish()` and `publishSticky()` have template+payload and no-payload overloads.
- **Metadata naming convention**: All components use short names without "Component" suffix (Wifi, MQTT, NTP, HomeAssistant, Storage, OTA, etc.).
- **Entity registration pattern** (HomeAssistant.h:144-157): `addSensor()` is the reference — create entity, push to vector, increment stats, log, emit event (currently raw String — to fix with HAEntityAddedEvent struct), republish if connected. All 5 other `addXxx()` methods follow the same structure but omit the emit entirely.
- **Test pattern**: Unity framework, `RUN_TEST()` macros, lambda-based event capture via `eventBus.subscribe()`.
- **Constitution VI**: All inter-component communication MUST use EventBus. Events are mandatory, not optional.
- **Constitution VII**: 800 lines max per file (excluding blank lines and comments). `test_ha_component.cpp` has 958 total lines but only 629 non-blank/non-comment lines — under the 800 limit, no split required.
- **Constitution VIII**: Progressive refactoring — preserve existing APIs, deprecate before removing.
- **Constitution XV**: Semantic versioning — breaking change = MINOR bump minimum. Use `tools/bump_version.py` and `tools/check_versions.py`.

### Files to Reference

| File | Purpose | Lines |
| ---- | ------- | ----- |
| `DomoticsCore-Core/include/DomoticsCore/Core.h:162-173` | Core::emit() — missing sticky parameter | 2 overloads |
| `DomoticsCore-Core/include/DomoticsCore/IComponent.h:222-226` | IComponent::emit() — reference pattern with sticky | 1 overload (template only) |
| `DomoticsCore-Core/include/DomoticsCore/EventBus.h:119-149` | publish() and publishSticky() — both with/without payload | 4 methods |
| `DomoticsCore-LED/include/DomoticsCore/LED.h:94` | metadata.name = "LEDComponent" — to fix | 1 line |
| `DomoticsCore-LED/examples/LEDWithWebUI/src/main.cpp:74` | getComponent("LEDComponent") — to update | 1 line |
| `docs/components/led/README.md:50,74` | "LEDComponent" references — to update | 2 lines |
| `docs/components/system/technical-reference.md:300` | "LEDComponent" in registry table — to update | 1 line |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h:144-240` | All 6 addXxx() methods | addSensor has emit, 5 others don't |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h:33-36` | HAEntityAddedEvent struct (char id[64], char component[32]) | Defined but unused — C21 debt |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEvents.h:14-18` | EVENT_ENTITY_ADDED constant | Defined, used only by addSensor |
| `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp` | Existing EventBus tests — add M11 tests here | 499 lines (safe) |
| `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp` | Existing HA tests — 958 total lines, 629 non-blank/non-comment | Under 800 effective limit |
| `DomoticsCore-LED/README.md:37` | Already uses `getComponent("LED")` — no change needed | Verified correct |

### Technical Decisions

1. **M11 — No breaking change**: `bool sticky = false` default preserves all existing call sites. Applied to template overload only. The no-payload overload `emit(const String& topic)` is left unchanged because adding `bool sticky = false` would create ambiguity: `core.emit("topic", true)` would match both the template `emit<bool>(topic, payload)` and `emit(topic, sticky)`. This mirrors `IComponent::emit()` which also only has the template overload with sticky.
2. **M12 — Clean break, no alias**: Constitution IV (YAGNI) forbids dead code. An alias would be speculative compatibility. LED MINOR bump with `!` conventional commit prefix and BREAKING CHANGE footer signals the change. Impact: 1 example + 2 doc files. Note: `DomoticsCore-LED/README.md` already uses `"LED"` (line 37) — no change needed there.
3. **M19 — HAEntityAddedEvent struct + uniform emission**: The struct was defined (C21) but `addSensor()` emitted a raw `String` instead — dangling pointer risk via `reinterpret_cast`. Fix: all 6 `addXxx()` methods now use `HAEntityAddedEvent` with `snprintf` to populate `ev.id` (max 63 chars) and `ev.component` (type string). Block-scoped to avoid polluting method namespace. This simultaneously fixes M19 (5 missing emits) and C21 (unused struct / unsafe serialization).
4. **No test file split needed**: `test_ha_component.cpp` has 958 total lines but only 629 non-blank/non-comment lines, well under the Constitution VII 800-line limit. M19 event tests go in a new dedicated `test_ha_events/test_ha_events.cpp` for logical separation, not for size compliance.
5. **Version bumps**: Core 1.5.1→1.5.2 (PATCH), LED 1.3.0→1.4.0 (MINOR), HA 1.6.0→1.6.1 (PATCH), Root 1.8.1→1.9.0 (MINOR). Via `tools/bump_version.py` + `tools/check_versions.py`.

## Implementation Plan

### Tasks

#### Commit 1: `fix(core): add sticky parameter to Core::emit() (M11)`

- [x] Task 1: Add sticky parameter to Core::emit() template overload
  - File: `DomoticsCore-Core/include/DomoticsCore/Core.h`
  - Action: Replace lines 162-165 with:
    ```cpp
    template<typename PayloadT>
    void emit(const String& topic, const PayloadT& payload, bool sticky = false) {
        if (sticky) componentRegistry.getEventBus().publishSticky(topic, payload);
        else componentRegistry.getEventBus().publish(topic, payload);
    }
    ```
  - Notes: Same pattern as IComponent.h:222-226. Default `false` preserves all existing call sites.

- [x] Task 2: Add M11 tests to existing EventBus test file
  - File: `DomoticsCore-Core/test/test_eventbus/test_eventbus.cpp`
  - Action: Add 2 test functions before the test runner section (~line 475), and register them with `RUN_TEST()`:
    - `test_core_emit_sticky_with_payload`: Create Core, emit with sticky=true, subscribe late with replayLast=true, verify payload received.
    - `test_core_emit_non_sticky_default`: Create Core, emit without sticky param, subscribe late with replayLast=true, verify nothing received.
  - Notes: File is at 499 lines, safe margin. Follow existing test patterns (lambda capture, eventBus.poll()). No test for no-payload sticky — that overload is unchanged.

#### Commit 2: `fix(ha): use HAEntityAddedEvent struct in all addXxx() methods (M19, C21)`

- [x] Task 3: Fix addSensor() — replace raw String emit with HAEntityAddedEvent struct
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Replace `emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, id);` (line 153) with:
    ```cpp
    {
        HAEntityAddedEvent ev{};
        snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
        snprintf(ev.component, sizeof(ev.component), "sensor");
        emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
    }
    ```
  - Notes: Fixes C21 — the struct was defined but never used. `snprintf` truncates safely if id exceeds 63 chars. Block scope avoids polluting the method namespace.

- [x] Task 4: Add emit to addBinarySensor() with HAEntityAddedEvent struct
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Add after `DLOG_I(LOG_HA, "Added binary sensor: ...")`, before `if (mqttConnected)`:
    ```cpp
    {
        HAEntityAddedEvent ev{};
        snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
        snprintf(ev.component, sizeof(ev.component), "binary_sensor");
        emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
    }
    ```

- [x] Task 5: Add emit to addSwitch() with HAEntityAddedEvent struct
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Add after `DLOG_I(LOG_HA, "Added switch: ...")`, before `if (mqttConnected)`:
    ```cpp
    {
        HAEntityAddedEvent ev{};
        snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
        snprintf(ev.component, sizeof(ev.component), "switch");
        emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
    }
    ```

- [x] Task 6: Add emit to addLight() with HAEntityAddedEvent struct
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Add after `DLOG_I(LOG_HA, "Added light: ...")`, before `if (mqttConnected)`:
    ```cpp
    {
        HAEntityAddedEvent ev{};
        snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
        snprintf(ev.component, sizeof(ev.component), "light");
        emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
    }
    ```

- [x] Task 7: Add emit to addButton() with HAEntityAddedEvent struct
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Add after `DLOG_I(LOG_HA, "Added button: ...")`, before `if (mqttConnected)`:
    ```cpp
    {
        HAEntityAddedEvent ev{};
        snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
        snprintf(ev.component, sizeof(ev.component), "button");
        emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
    }
    ```

- [x] Task 8: Add emit to addAlarmControlPanel() with HAEntityAddedEvent struct
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Add after `DLOG_I(LOG_HA, "Added alarm_control_panel: ...")`, before `if (mqttConnected)`:
    ```cpp
    {
        HAEntityAddedEvent ev{};
        snprintf(ev.id, sizeof(ev.id), "%s", id.c_str());
        snprintf(ev.component, sizeof(ev.component), "alarm_control_panel");
        emit(DomoticsCore::HAEvents::EVENT_ENTITY_ADDED, ev);
    }
    ```

- [x] Task 9: Create test_ha_events.cpp with M19 entity_added event tests
  - File: `DomoticsCore-HomeAssistant/test/test_ha_events/test_ha_events.cpp` (NEW)
  - Action: Create test file with 6 tests, one per addXxx() method. Each test:
    1. Creates Core + HomeAssistantComponent (no `begin()` needed, no MQTT simulation)
    2. Subscribes to `ha/entity_added` with lambda capturing the received `HAEntityAddedEvent`
    3. Calls `addXxx()` with a known id
    4. Calls `eventBus.poll()`
    5. Asserts `ev.id` matches the expected entity id
    6. Asserts `ev.component` matches the expected component type string (e.g., `"sensor"`, `"binary_sensor"`, `"switch"`, `"light"`, `"button"`, `"alarm_control_panel"`)
  - Notes: Setup is minimal — Core constructor + `addComponent()` is sufficient, `poll()` dispatches the queued event. Follow existing test patterns from `test_ha_component.cpp` for setup/teardown. Include Unity framework headers. Use `reinterpret_cast<const HAEntityAddedEvent*>(data)` in the subscribe callback to deserialize the struct.

#### Commit 3: `fix(led)!: change metadata.name from "LEDComponent" to "LED" (M12)`

- [x] Task 10: Fix LED metadata.name
  - File: `DomoticsCore-LED/include/DomoticsCore/LED.h`
  - Action: Change line 94 from `metadata.name = "LEDComponent";` to `metadata.name = "LED";`

- [x] Task 11: Update LED example
  - File: `DomoticsCore-LED/examples/LEDWithWebUI/src/main.cpp`
  - Action: Change line 74 from `getComponent<LEDComponent>("LEDComponent")` to `getComponent<LEDComponent>("LED")`

- [x] Task 12: Update LED README
  - File: `docs/components/led/README.md`
  - Action: Replace all occurrences of `getComponent<LEDComponent>("LEDComponent")` with `getComponent<LEDComponent>("LED")`. Remove any bug notation comments about the name.

- [x] Task 13: Update System technical reference
  - File: `docs/components/system/technical-reference.md`
  - Action: Change `"LEDComponent"` to `"LED"` in the component registry table (~line 300).

- [x] Task 14: Create test_led_component.cpp with metadata name and lookup tests
  - File: `DomoticsCore-LED/test/test_led_component/test_led_component.cpp` (NEW)
  - Action: Create test file with 2 tests:
    - `test_led_metadata_name_is_short`: Create LEDComponent, call getMetadata(), assert `metadata.name == "LED"`.
    - `test_led_component_lookup_by_short_name`: Create Core, add LEDComponent, call `core.getComponent<LEDComponent>("LED")`, assert result is not `nullptr`.
  - Notes: Include Unity framework headers and LED.h. Second test validates AC-5 (runtime lookup by short name).

#### Commit 4: `chore(release): bump Core 1.5.2, HA 1.6.1, LED 1.4.0, Root 1.9.0`

- [x] Task 15: Run version bump scripts
  - Action: Execute in order:
    ```bash
    python tools/bump_version.py Core patch
    python tools/bump_version.py HomeAssistant patch
    python tools/bump_version.py LED minor
    python tools/check_versions.py --verbose
    ```
  - Notes: Order matters — LED minor MUST be last because each call bumps root cumulatively (Core patch → root 1.8.2, HA patch → root 1.8.3, LED minor → root 1.9.0). Running LED minor earlier would result in incorrect root version. check_versions.py validates consistency across all library.json and metadata.version assignments. Do NOT manually edit version numbers.

### Acceptance Criteria

#### M11 — Core::emit() sticky parameter

- [x] AC-1: Given a Core instance, when `core.emit("topic", payload, true)` is called and a late subscriber subscribes with `replayLast=true`, then the subscriber receives the sticky payload.
- [x] AC-2: Given a Core instance, when `core.emit("topic", payload)` is called (default sticky=false) and a late subscriber subscribes with `replayLast=true`, then the subscriber receives nothing.
- [x] AC-3: Given existing code calling `core.emit("topic", payload)` or `core.emit("topic")`, when compiled, then no compilation errors occur (backward compatibility preserved).

#### M12 — LED metadata.name

- [x] AC-4: Given a LEDComponent instance, when `getMetadata()` is called, then `metadata.name` equals `"LED"`.
- [x] AC-5: Given a Core with LEDComponent registered, when `core.getComponent<LEDComponent>("LED")` is called, then the component is returned.
- [x] AC-6: Given the example `LEDWithWebUI/src/main.cpp`, when compiled, then it uses `getComponent<LEDComponent>("LED")` and compiles without errors.

#### M19 — ha/entity_added event emission (+ C21 struct fix)

- [x] AC-7: Given a HomeAssistantComponent, when `addSensor(id, ...)` is called, then `ha/entity_added` event is emitted with `HAEntityAddedEvent` payload where `ev.id == id` and `ev.component == "sensor"`.
- [x] AC-8: Given a HomeAssistantComponent, when `addBinarySensor(id, ...)` is called, then `ha/entity_added` event is emitted with `HAEntityAddedEvent` payload where `ev.id == id` and `ev.component == "binary_sensor"`.
- [x] AC-9: Given a HomeAssistantComponent, when `addSwitch(id, ...)` is called, then `ha/entity_added` event is emitted with `HAEntityAddedEvent` payload where `ev.id == id` and `ev.component == "switch"`.
- [x] AC-10: Given a HomeAssistantComponent, when `addLight(id, ...)` is called, then `ha/entity_added` event is emitted with `HAEntityAddedEvent` payload where `ev.id == id` and `ev.component == "light"`.
- [x] AC-11: Given a HomeAssistantComponent, when `addButton(id, ...)` is called, then `ha/entity_added` event is emitted with `HAEntityAddedEvent` payload where `ev.id == id` and `ev.component == "button"`.
- [x] AC-12: Given a HomeAssistantComponent, when `addAlarmControlPanel(id, ...)` is called, then `ha/entity_added` event is emitted with `HAEntityAddedEvent` payload where `ev.id == id` and `ev.component == "alarm_control_panel"`.

#### Version bumps

- [x] AC-13: Given all fixes are committed, when `python tools/check_versions.py --verbose` is run, then it reports no inconsistencies. Final versions: Core 1.5.2, LED 1.4.0, HomeAssistant 1.6.1, Root 1.9.0.

## Additional Context

### Dependencies

- No inter-task dependencies between M11, M12, and M19 — they can be implemented in any order.
- M12 impact fully mapped: 1 example + 2 doc files reference `"LEDComponent"`. `DomoticsCore-LED/README.md` already uses `"LED"` — no change needed.
- M19 event tests go in a new `test_ha_events/` directory for logical separation (not for Constitution VII — file is under limit).
- Version bumps depend on all three fixes being committed first. LED minor bump MUST be last (see Task 14 notes).

### Testing Strategy

- **M11** (2 tests in `test_eventbus.cpp` — currently 499 lines, safe):
  - `test_core_emit_sticky_with_payload`: core.emit(topic, payload, true) -> late subscriber receives via replayLast
  - `test_core_emit_non_sticky_default`: core.emit(topic, payload) -> late subscriber receives nothing
- **M12** (2 tests in new `test_led_component/test_led_component.cpp`):
  - `test_led_metadata_name_is_short`: metadata.name == "LED"
  - `test_led_component_lookup_by_short_name`: core.getComponent("LED") != nullptr
- **M19** (6 tests in new `test_ha_events/test_ha_events.cpp`):
  - `test_add_sensor_emits_entity_added`
  - `test_add_binary_sensor_emits_entity_added`
  - `test_add_switch_emits_entity_added`
  - `test_add_light_emits_entity_added`
  - `test_add_button_emits_entity_added`
  - `test_add_alarm_control_panel_emits_entity_added`
- All tests run on native platform (no hardware required).
- Run full test suite after each commit to ensure green-to-green.

### Commit Strategy

1. `fix(core): add sticky parameter to Core::emit() (M11)` — Core.h + 2 tests
2. `fix(ha): use HAEntityAddedEvent struct in all addXxx() methods (M19, C21)` — HomeAssistant.h (fix addSensor + add 5 missing emits) + 6 tests in new file. Commit body references C21 fix.
3. `fix(led)!: change metadata.name from "LEDComponent" to "LED" (M12)` — LED.h + example + docs + 2 tests in new file. Commit body includes `BREAKING CHANGE: getComponent("LEDComponent") no longer works. Use getComponent("LED").`
4. `chore(release): bump Core 1.5.2, HA 1.6.1, LED 1.4.0, Root 1.9.0` — via bump_version.py + check_versions.py (LED minor MUST be last)

### Notes

- Party Mode consensus: clean break for M12 (no backward compat alias), Constitution IV (YAGNI). MINOR bump (not MAJOR) — `metadata.name` is an internal registration key, `!` conventional commit prefix signals the breaking change.
- M19 fixes C21 (REVIEW-FINDINGS.md:47): `HAEntityAddedEvent` struct was defined at `HomeAssistant.h:33-36` but never used — `addSensor()` emitted a raw `String` via `reinterpret_cast`, risking dangling pointer on dispatch. All 6 `addXxx()` methods now use the struct with `snprintf` — fixed-size, zero-heap, safe for EventBus serialization.
- M19 edge cases (duplicate entity IDs, no subscribers) are covered by existing EventBus tests — M19 tests focus solely on verifying the emit call is present in each `addXxx()` method.

## Review Notes
- Adversarial review completed
- Findings: 8 total, 3 fixed (F3, F4, F5), 5 skipped (F1 intended behavior, F2 by design, F6/F7/F8 out of scope)
- Resolution approach: auto-fix
