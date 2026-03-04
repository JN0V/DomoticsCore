---
title: 'HA Alarm Control Panel Entity'
slug: 'ha-alarm-control-panel'
created: '2026-03-04'
status: 'ready-for-dev'
stepsCompleted: [1, 2, 3, 4]
tech_stack: ['C++', 'PlatformIO', 'ArduinoJson 7', 'ESP32/ESP8266']
files_to_modify:
  - 'DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h (new)'
  - 'DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h (modify)'
  - 'DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h (modify)'
  - 'DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp (new)'
code_patterns:
  - 'HAEntity base class with virtual buildDiscoveryPayload()'
  - 'Entity registration via addXxx() methods in HomeAssistantComponent'
  - 'Command routing via entity->component string matching + static_cast'
  - 'EventBus-based MQTT communication (no direct MQTT access)'
  - 'State publishing via publishState(id, value)'
test_patterns:
  - 'Unity test framework with TEST_CASE sections'
  - 'simulateMqttConnect() / simulateSwitchCommand() helpers'
  - 'Existing tests in test_ha_component.cpp (777 lines, near 800-line limit)'
  - 'New alarm tests in separate test_ha_alarm_panel.cpp (Constitution VII compliance)'
---

# Tech-Spec: HA Alarm Control Panel Entity

**Created:** 2026-03-04

## Overview

### Problem Statement

The HomeAssistant component currently supports `sensor`, `binary_sensor`, `switch`, `light`, and `button` entities but does **not** support the `alarm_control_panel` entity type — the native HA integration for alarm systems.

Consumers building alarm systems are forced to use `HASwitch` as a workaround, causing critical UX problems:

1. **No intermediate states** — A switch is binary (ON/OFF). Alarm systems need `arming`, `pending`, `triggered`.
2. **State confirmation timing** — Exit delays cause the switch to visually revert in HA.
3. **No code support** — HA alarm panels support PIN codes; switches have no mechanism for this.
4. **Wrong HA card** — Switches render as toggles instead of the native `alarm-panel` Lovelace card with keypad and color-coded status.
5. **No arm mode differentiation** — Alarm systems distinguish `armed_home`, `armed_away`, `armed_night`, etc.

### Solution

Add a new `HAAlarmControlPanel` entity type following the existing entity pattern, with full support for the HA MQTT `alarm_control_panel` protocol: multiple arm modes, intermediate states, command handling with optional PIN code passthrough, and correct discovery payload for native Lovelace card rendering.

Additionally, add `virtual handleCommand()` to `HAEntity` base class to enable polymorphic dispatch for the new entity type (progressive improvement toward SOLID compliance).

### Scope

**In Scope:**

- New header file `HAAlarmControlPanel.h` with entity class, state constants, command constants, feature enum
- Add `virtual void handleCommand(const String& payload) {}` to `HAEntity` base class
- Add `#include "HAAlarmControlPanel.h"` to `HomeAssistant.h`
- Add `addAlarmControlPanel()` method to `HomeAssistantComponent`
- Add `alarm_control_panel` branch in command routing (`handleCommand`)
- PIN code support (passthrough — DomoticsCore does not validate codes, only passes them to consumer callback)
- `command_template` in discovery payload for code passthrough (conditional — only when code config is active)
- 12 tests in new test file (split from existing 777-line file per Constitution VII)

**Out of Scope:**

- WebUI integration for alarm panel (no detail context, no specific badge)
- Refactoring existing command routing for `switch`/`light`/`button` to use virtual dispatch
- PIN code validation logic (consumer responsibility)
- `DisarmPending`/`pending` state mapping (future, when AlarmControl supports entry delay)

## Context for Development

### Codebase Patterns

- **Entity inheritance:** All HA entities extend `HAEntity` and override `buildDiscoveryPayload()` to add component-specific JSON fields
- **Topic generation:** `HAEntity` provides `getDiscoveryTopic()`, `getStateTopic()`, `getCommandTopic()` — base class handles topic format
- **Entity registration:** `HomeAssistantComponent` has `addSensor()`, `addSwitch()`, `addLight()`, `addButton()` methods that create `unique_ptr<HAXxx>`, configure it, push to `entities` vector, increment `stats.entityCount`, and optionally republish if MQTT connected
- **Command routing:** `handleCommand(topic, payload)` extracts entity ID from topic, finds entity via `findEntity()`, then routes by `entity->component` string with `static_cast`
- **State publishing:** Consumer calls `publishState(id, stringValue)` — component handles MQTT publish via EventBus
- **No auto-publish for alarm:** Unlike `HASwitch` (which has `autoPublishState`), the alarm panel should **never** auto-publish state. The consumer always manages alarm state transitions.

### Files to Reference

| File | Purpose |
| ---- | ------- |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h` | Base entity class — add `virtual handleCommand()` here |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h` | Reference pattern for command-handling entity |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAButton.h` | Reference pattern for simple entity |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` | Main component — add include, `addAlarmControlPanel()`, command routing |
| `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp` | Existing 68 tests (777 lines) — reference for test patterns |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HALight.h` | Reference for JSON command parsing pattern |
| `docs/project-context.md` | Project-wide conventions (header-only design, component model) |
| `specs/007-ha-alarm-control-panel/feature-request.md` | Full feature request with proposed implementation and HA MQTT protocol reference |

### Technical Decisions

| Decision | Rationale | Constitution Reference |
| -------- | --------- | ---------------------- |
| Add `virtual handleCommand()` to `HAEntity` | Enables polymorphic dispatch; follows Liskov Substitution. Default empty body = no breaking change. | I. SOLID, VIII. Progressive Refactoring |
| Do NOT refactor existing routing | Progressive refactoring — one component at a time. Existing switch/light/button routing unchanged. | III. KISS, VIII. Progressive Refactoring |
| Include PIN code passthrough | Intrinsic to HA `alarm_control_panel` MQTT protocol. Not speculative. DomoticsCore only passes code to callback. | IV. YAGNI (protocol completeness ≠ speculation) |
| Split test file | Current test file is 777 lines. Adding ~100-150 lines of alarm tests exceeds 800-line limit. Create separate `test_ha_alarm_panel.cpp`. | VII. File Size Limits |
| WebUI out of scope | Not requested in feature request. YAGNI. | IV. YAGNI |
| No auto-publish on command | Alarm state transitions are complex (delays, confirmations). Consumer always manages state. | Architectural consistency with alarm domain |
| Command payloads as `constexpr` not `String` members | 7 payload Strings = ~84 bytes + 7 heap allocs per instance. HA command payloads are protocol constants, not configurable. Use `constexpr const char*` in namespace = zero heap, stored in flash/rodata. | V. Performance First, XIV. Memory Leak Prevention |
| `supportedFeatures` as `uint8_t` bitmask | 6 features max. `std::vector` = 24 bytes + heap alloc. `uint8_t` = 1 byte, zero heap. | V. Performance First, XIV. Memory Leak Prevention |
| `code` stays as `String` | PIN code is user-defined at runtime. `const char*` risks dangling pointer if consumer passes temporary buffer. One conditional heap alloc is acceptable for safety. | XIV. Memory Leak Prevention (safety > micro-optimization) |
| Callback by `const&` not by value | `std::function` copy on ESP8266 (80KB heap) is wasteful. Take by `const&`, store via copy in member init. | V. Performance First |
| Conditional `command_template` in discovery | HA default `command_template` is `{{ action }}` — codes NOT sent to device. Must set `command_template` when code config is active for passthrough to work. Omit when no code config to avoid unnecessary complexity. | III. KISS, Protocol correctness |
| `handleCommand()` trims and validates input | MQTT payloads can have trailing whitespace or be empty. Defensive parsing prevents undefined behavior on malformed input. | V. Performance First (no UB), XIII. Anti-Pattern Avoidance |
| Code params in `addAlarmControlPanel()` API | Entity is unreachable after `unique_ptr` move into vector. Without API params, `code`/`code*Required` fields are dead code. Follows `addSwitch()` pattern. | IV. YAGNI (no dead code), I. SOLID (usable API) |
| Heap stability test required | Constitution XIV mandates heap stability verification for every feature. 10-cycle create/destroy loop with heap delta assertion. | XIV. Memory Leak Prevention (NON-NEGOTIABLE) |
| Virtual shadow documented as tech debt | Existing `handleCommand()` in switch/light/button shadows new virtual. Documented with TODO, not fixed here. | VIII. Progressive Refactoring |
| `codeDisarmRequired` defaults to `false` | Feature request defaulted to `true`. Changed to `false` because: (1) with `true`, `command_template` conditional always fires, injecting code passthrough even without code config (breaks AC 11), (2) matches HA's own default, (3) consumers explicitly opt in to code support via API. | III. KISS, IV. YAGNI |

## Implementation Plan

### Tasks

- [ ] Task 1: Add `virtual handleCommand()` to `HAEntity` base class
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h`
  - Action: Add `virtual void handleCommand(const String& payload) {}` after line 81 (`}` closing `buildDiscoveryPayload()`), before line 83 (closing `};` of `HAEntity` class)
  - Notes: Default empty body = no breaking change. Existing entities (`HASwitch`, `HALight`, `HAButton`) already have non-virtual `handleCommand()` methods that shadow this — they will continue working via `static_cast` routing unchanged.
  - **Tech Debt (documented):** The shadow creates a maintenance trap — calling `handleCommand()` via `HAEntity*` on switch/light/button invokes the empty base, not the derived method. Add a `// TODO: make handleCommand() virtual override in HASwitch/HALight/HAButton (progressive refactoring)` comment above the declaration. This is tracked for a future refactoring pass per Constitution VIII (Progressive Refactoring) and not in scope here.

- [ ] Task 2: Create `HAAlarmControlPanel.h` header file
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h` (new)
  - Action: Create header-only entity class with:
    - `AlarmFeature` enum as `uint8_t` bit flags: `ArmHome = 0x01`, `ArmAway = 0x02`, `ArmNight = 0x04`, `ArmVacation = 0x08`, `ArmCustomBypass = 0x10`, `Trigger = 0x20`
    - `AlarmPanelState` namespace with `constexpr const char*` for all 10 states: `disarmed`, `arming`, `armed_home`, `armed_away`, `armed_night`, `armed_vacation`, `armed_custom_bypass`, `pending`, `triggered`, `disarming`
      - **Note:** `disarming` is included as a **consumer convenience constant** only. There is no HA command that triggers this state and no built-in transition to it. Consumers may publish it if their alarm system supports a disarming delay (e.g., entry delay before full disarm). This is documented in the code with a comment.
    - `AlarmPanelCommand` namespace with `constexpr const char*` for all 7 commands: `ARM_HOME`, `ARM_AWAY`, `ARM_NIGHT`, `ARM_VACATION`, `ARM_CUSTOM_BYPASS`, `DISARM`, `TRIGGER`
    - `HAAlarmControlPanel` class extending `HAEntity` with:
      - Constructor: `(const String& id, const String& name, const std::function<void(const String&, const String&)>& commandCallback, const String& icon = "mdi:shield-home")` — callback taken by **const reference** to avoid unnecessary copy on resource-constrained MCUs (Constitution V). Stored internally via copy in member initializer.
      - Members: `String code` (default empty), `uint8_t supportedFeatures` (default `AlarmFeature::ArmAway`), `bool codeArmRequired` (false), `bool codeDisarmRequired` (false), `bool codeTriggerRequired` (false), `commandCallback`
      - **Default change from feature request:** `codeDisarmRequired` defaults to `false` (not `true`). Reason: With `true` as default, the `command_template` conditional (see below) would always fire, injecting code passthrough even when no code support is configured — breaking AC 11. Consumers who want code support explicitly set `codeDisarmRequired = true` via `addAlarmControlPanel()`. This is consistent with HA's own default (`false`).
      - `buildDiscoveryPayload()` override: calls base, adds `command_topic`, code config fields, command payload constants from namespace, `supported_features` array built from bitmask, and **`command_template`** (see critical note below)
      - `handleCommand(const String& payload)` override: trims input, parses command and optional code via `indexOf(' ')`, validates non-empty command, calls `commandCallback(command, code)`, logs warning for empty/malformed payloads
  - **Critical — `command_template` in discovery payload:** The HA MQTT `alarm_control_panel` default `command_template` is `{{ action }}` — codes are **NOT** sent to the device by default. HA validates codes locally. For code passthrough to work (DomoticsCore receives the code in the MQTT payload), `buildDiscoveryPayload()` **MUST** include `command_template` when `code` is non-empty or any `code*Required` flag is true:
    ```cpp
    // In buildDiscoveryPayload(), after code_trigger_required:
    if (!code.isEmpty() || codeArmRequired || codeDisarmRequired || codeTriggerRequired) {
        doc["command_template"] = "{{ action }}{% if code %} {{ code }}{% endif %}";
    }
    ```
    Without this, ACs 2 and 4 (code passthrough) are non-functional. Verified against HA MQTT docs: https://www.home-assistant.io/integrations/alarm_control_panel.mqtt/
  - **Input validation in `handleCommand()`:** The method must handle edge cases defensively (Constitution V — Performance First, avoid undefined behavior):
    ```cpp
    void handleCommand(const String& payload) override {
        if (!commandCallback) return;
        String trimmed = payload;
        trimmed.trim();
        if (trimmed.isEmpty()) {
            DLOG_W(LOG_HA, "Empty alarm command payload");
            return;
        }
        int spaceIdx = trimmed.indexOf(' ');
        String command = (spaceIdx > 0) ? trimmed.substring(0, spaceIdx) : trimmed;
        String codeValue = (spaceIdx > 0) ? trimmed.substring(spaceIdx + 1) : String();
        codeValue.trim();  // Handle trailing spaces
        commandCallback(command, codeValue);
    }
    ```
    Note: Command string validation (checking against known `AlarmPanelCommand::*` values) is intentionally omitted — the consumer callback handles all validation. This keeps the entity generic and follows the same pattern as `HASwitch` (which passes any payload to callback without validation). A `DLOG_W` for empty payloads is sufficient.
  - Notes: Header-only, no `.cpp`. All command payloads are `constexpr` in namespace — zero per-instance heap cost. `supportedFeatures` is `uint8_t` bitmask — 1 byte vs 24+ for vector. Total instance overhead: ~40 bytes + 0-1 heap alloc (PIN code only). **Important:** `handleCommand()` is declared `override` here — this depends on Task 1 adding the `virtual` in `HAEntity` first.

- [ ] Task 3: Integrate `HAAlarmControlPanel` into `HomeAssistant.h`
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action (3 changes):
    1. **Add include** after `#include "HAButton.h"` (line 21): `#include "HAAlarmControlPanel.h"`
    2. **Add `addAlarmControlPanel()` method** after the `addButton()` method (after `}` closing brace at line 215, before `// ========== State Publishing ==========`):
       ```cpp
       void addAlarmControlPanel(
           const String& id, const String& name,
           const std::function<void(const String& command, const String& code)>& commandCallback,
           const String& icon = "mdi:shield-home",
           uint8_t features = AlarmFeature::ArmAway,
           const String& code = "",
           bool codeArmRequired = false,
           bool codeDisarmRequired = false,
           bool codeTriggerRequired = false) {
           auto panel = std::make_unique<HAAlarmControlPanel>(id, name, commandCallback, icon);
           panel->supportedFeatures = features;
           panel->code = code;
           panel->codeArmRequired = codeArmRequired;
           panel->codeDisarmRequired = codeDisarmRequired;
           panel->codeTriggerRequired = codeTriggerRequired;
           entities.push_back(std::move(panel));
           stats.entityCount++;
           DLOG_I(LOG_HA, "Added alarm_control_panel: %s", id.c_str());
           if (mqttConnected) { republishEntity(id); }
       }
       ```
       **Rationale for code parameters:** The entity is moved into a `unique_ptr` in the vector and becomes unreachable after registration. Without these parameters, consumers cannot configure PIN code fields — making `code`, `codeArmRequired`, `codeDisarmRequired`, `codeTriggerRequired` dead code. This follows the `addSwitch()` pattern which exposes `autoPublishState` and `optimistic` as parameters.
    3. **Add command routing branch** after the button branch (`// Buttons don't have state` at line 558), before the closing `}` of `handleCommand()`:
       ```cpp
       else if (entity->component == "alarm_control_panel") {
           entity->handleCommand(payload);
           // No auto-publish: consumer manages alarm state
       }
       ```
  - Notes: Routing uses virtual dispatch via `HAEntity::handleCommand()` for the new type. Existing switch/light/button routing unchanged. No auto-publish — alarm state is consumer-managed. Do NOT emit `EVENT_ENTITY_ADDED` — consistent with `addSwitch()`/`addLight()`/`addButton()` which don't emit it (only `addSensor()` does — existing inconsistency, not in scope to fix). Line numbers are approximate — always use anchor descriptions (method names, comments) as primary reference since earlier tasks may shift line numbers.

- [ ] Task 4: Create test file `test_ha_alarm_panel.cpp`
  - File: `DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp` (new)
  - **Build configuration:** PlatformIO auto-discovers test directories under `test/`. Verify that `platformio.ini` for the HomeAssistant component does not have a restrictive `test_filter` or `test_dir` that would exclude the new `test_ha_alarm_panel/` directory. If it does, add the new directory to the filter. Run `pio test -e native` after creation to confirm discovery.
  - Action: Create test file with **12 tests** following existing patterns from `test_ha_component.cpp`:
    1. `test_alarm_panel_discovery_payload` — verify `buildDiscoveryPayload()` generates correct JSON: `command_topic`, `state_topic`, `code_arm_required`, `code_disarm_required`, `code_trigger_required`, `supported_features` array, command payload constants. **Also verify `command_template` is present when code config is set** (see Task 2 critical note).
    2. `test_alarm_panel_discovery_supported_features` — verify `supported_features` JSON array matches bitmask: single flag, multiple flags, all flags
    3. `test_alarm_panel_discovery_code_fields` — verify `code` field present when set, absent when empty; `code_arm_required`, `code_disarm_required` correct. **Verify `command_template` is absent when no code config is set, and present with correct Jinja2 template when code config is active.**
    4. `test_alarm_panel_handle_command_basic` — verify `handleCommand("ARM_AWAY")` calls callback with `("ARM_AWAY", "")`
    5. `test_alarm_panel_handle_command_with_code` — verify `handleCommand("DISARM 1234")` calls callback with `("DISARM", "1234")`
    6. `test_alarm_panel_handle_command_no_callback` — verify `handleCommand()` does not crash when `commandCallback` is null
    7. `test_alarm_panel_handle_command_edge_cases` — verify edge cases: empty payload (no callback invoked), payload with trailing space `"ARM_AWAY "` (command = `"ARM_AWAY"`, code = `""`), payload with only whitespace (no callback invoked). Constitution V compliance — no undefined behavior on malformed input.
    8. `test_alarm_panel_state_publish` — verify `publishState("alarm", "arming")` publishes to correct state topic `homeassistant/alarm_control_panel/{nodeId}/alarm/state`
    9. `test_alarm_panel_no_auto_publish` — verify `handleCommand()` in component routing does NOT trigger `publishState` (unlike switch). **Assertion mechanism:** Subscribe to `EVENT_PUBLISH` on EventBus before invoking command, capture all emitted MQTT publish events into a vector, assert that no publish event targets the alarm entity's state topic. Only the command callback should fire. Follow the pattern from `test_switch_auto_publish_*` tests in `test_ha_component.cpp`.
    10. `test_alarm_panel_add_method` — verify `addAlarmControlPanel()` registers entity, increments `entityCount`, and publishes discovery when connected. **Also verify code-related parameters are correctly passed through** (`code`, `codeArmRequired`, `codeDisarmRequired`, `codeTriggerRequired`).
    11. `test_alarm_panel_command_routing` — verify command on topic `homeassistant/alarm_control_panel/{nodeId}/alarm/set` routes to `HAAlarmControlPanel::handleCommand()`
    12. `test_alarm_panel_heap_stability` — **Constitution XIV (Memory Leak Prevention — NON-NEGOTIABLE):** Use the project's `HeapTracker` utility (`DomoticsCore-Core/include/DomoticsCore/Testing/HeapTracker.h`). Pattern: `HEAP_CHECKPOINT(tracker, "before")`, perform 10 cycles of: create entity → build discovery payload → handle command → destroy entity. `HEAP_CHECKPOINT(tracker, "after")`, then `HEAP_ASSERT_STABLE(tracker, "before", "after", 100)`. This uses the established heap testing pattern (see `docs/technical-reference.md` Memory Management section) instead of raw `ESP.getFreeHeap()` calls.
  - Notes: Reuse test helpers from `test_ha_component.cpp` patterns (`simulateMqttConnect`, EventBus emit). New file avoids exceeding 800-line limit on existing test file. **Test structure:** Tests 1-7 are **unit tests** (only `HAAlarmControlPanel` instance needed, no `HomeAssistantComponent`). Tests 8-12 are **integration tests** (require `HomeAssistantComponent` with EventBus setup — follow existing integration test patterns from `test_ha_component.cpp`). **Line count estimate:** 12 tests × ~15 lines average = ~180 lines + setup/helpers ~60 lines = ~240 lines. Well within 300-line AC and 800-line constitution limit.

- [ ] Task 5: Version bump
  - Action: Run `python tools/bump_version.py HomeAssistant minor` to bump HomeAssistant component version `1.5.0` → `1.6.0`
  - Notes: Constitution XV — use tooling, never edit versions manually. The `bump_version.py` script handles **all** version propagation automatically:
    1. Component `library.json` (`DomoticsCore-HomeAssistant/library.json`): `1.5.0` → `1.6.0`
    2. Component `metadata.version` in C++ code (`HomeAssistant.h`): `"1.5.0"` → `"1.6.0"`
    3. Root `library.json` (DomoticsCore framework): MINOR bump (component MINOR → root MINOR, reset root patch)
    4. Verify with `python tools/check_versions.py --verbose` after bump

### Acceptance Criteria

- [ ] AC 1: Given an `HAAlarmControlPanel` entity is created with `supportedFeatures = ArmAway | ArmHome | Trigger`, when `buildDiscoveryPayload()` is called, then the JSON contains `"supported_features": ["arm_away", "arm_home", "trigger"]` and `"command_topic"` with correct format.

- [ ] AC 2: Given an `HAAlarmControlPanel` entity with `code = "1234"` and `codeDisarmRequired = true`, when `buildDiscoveryPayload()` is called, then the JSON contains `"code": "1234"`, `"code_disarm_required": true`, `"command_template": "{{ action }}{% if code %} {{ code }}{% endif %}"`, and all 7 command payload fields (`payload_arm_home`, `payload_arm_away`, etc.) with correct HA protocol values.

- [ ] AC 3: Given an `HAAlarmControlPanel` entity with a command callback, when `handleCommand("ARM_AWAY")` is called, then the callback receives `command = "ARM_AWAY"` and `code = ""`.

- [ ] AC 4: Given an `HAAlarmControlPanel` entity with a command callback, when `handleCommand("DISARM 1234")` is called, then the callback receives `command = "DISARM"` and `code = "1234"`.

- [ ] AC 5: Given an `HAAlarmControlPanel` entity with no callback (nullptr), when `handleCommand("ARM_AWAY")` is called, then no crash occurs and the function returns silently.

- [ ] AC 6: Given a `HomeAssistantComponent` with an alarm panel entity registered via `addAlarmControlPanel()`, when an MQTT message arrives on `homeassistant/alarm_control_panel/{nodeId}/alarm/set` with payload `"ARM_AWAY"`, then `entity->handleCommand("ARM_AWAY")` is called and `stats.commandsReceived` is incremented. The component does NOT auto-publish state (verified via EventBus publish event capture).

- [ ] AC 7: Given a `HomeAssistantComponent` with an alarm panel entity, when `publishState("alarm", "arming")` is called, then an MQTT publish event is emitted to topic `homeassistant/alarm_control_panel/{nodeId}/alarm/state` with payload `"arming"`.

- [ ] AC 8: Given `HAEntity` base class, when a new entity type subclass overrides `handleCommand(const String&)`, then the override is called polymorphically. Existing `HASwitch`/`HALight`/`HAButton` behavior is unchanged.

- [ ] AC 9: **(Code review checklist — not runtime-testable)** Given the `HAAlarmControlPanel` class source code, when reviewed: (a) command payloads are `constexpr const char*` in namespace (zero per-instance heap), (b) `supportedFeatures` is `uint8_t` bitmask (1 byte, zero heap), (c) `code` is `String` with at most one conditional heap allocation (only if PIN code is set), (d) no `std::vector` or `String` members used for protocol constants. This is a design constraint verified by code inspection, not a runtime assertion.

- [ ] AC 10: Given all implementation files, when line counts are checked, then `HAAlarmControlPanel.h` < 200 lines, `HomeAssistant.h` < 600 lines, `test_ha_alarm_panel.cpp` < 300 lines, and no file exceeds the 800-line constitution limit (Constitution VII).

- [ ] AC 11: Given an `HAAlarmControlPanel` entity with **no code configuration** (empty `code`, all `code*Required` flags false), when `buildDiscoveryPayload()` is called, then the JSON does **NOT** contain `"command_template"` or `"code"`. This ensures HA uses default behavior (no keypad, no code passthrough).

- [ ] AC 12: Given an `HAAlarmControlPanel` entity created and destroyed in a loop 10 times (with discovery payload generation and command handling each iteration), when `HeapTracker` checkpoints are taken before and after, then `HEAP_ASSERT_STABLE` passes with ≤ 100 bytes tolerance. No cumulative memory leak. (Constitution XIV — Memory Leak Prevention, NON-NEGOTIABLE). Must use project's `HeapTracker` utility, not raw `ESP.getFreeHeap()`.

- [ ] AC 13: Given `addAlarmControlPanel()` is called with `code = "5678"`, `codeArmRequired = true`, `codeDisarmRequired = true`, `codeTriggerRequired = false`, when the registered entity's discovery payload is built, then all code-related fields are correctly set in the JSON. The `addAlarmControlPanel()` API exposes all code configuration parameters.

## Additional Context

### Dependencies

- **ArduinoJson 7.0.0** — Used for `buildDiscoveryPayload()` JSON generation (already a dependency)
- No new external dependencies required
- `<vector>` no longer needed for `supportedFeatures` (changed to `uint8_t` bitmask)

### Testing Strategy

- **12 tests** in new `test_ha_alarm_panel/test_ha_alarm_panel.cpp` covering:
  - Discovery payload generation (JSON fields, supported_features, code configuration, **command_template conditional presence**)
  - Command handling (basic, with code, no callback, **edge cases: empty payload, trailing spaces, whitespace-only**)
  - State publishing (correct topic, no auto-publish **with explicit EventBus publish event capture assertion**)
  - Entity registration (`addAlarmControlPanel()` **with code parameter passthrough verification**)
  - Command routing in `HomeAssistantComponent`
  - **Heap stability** (Constitution XIV — NON-NEGOTIABLE): lifecycle loop test verifying zero cumulative heap leak
- **File size compliance:** New test file instead of modifying 777-line existing file (Constitution VII: 800-line limit)
- **Build configuration:** Verify PlatformIO discovers new `test_ha_alarm_panel/` directory. Run `pio test -e native` after creation.

### Notes

- The HA MQTT `alarm_control_panel` protocol reference: https://www.home-assistant.io/integrations/alarm_control_panel.mqtt/
- Consumer (e.g., AlarmControl) is responsible for state mapping (AlarmState → HA alarm panel state string)
- Constitution v1.6.0 compliance verified for all design decisions
- **Adversarial review integration (2026-03-04):** 12 findings integrated + 3 additional constitution compliance items. Key fixes: `command_template` for code passthrough, `addAlarmControlPanel()` API completeness, heap stability test, input validation edge cases, virtual shadow tech debt documentation.

### Step 2 Investigation Results

#### Party Mode Points — Resolved

| # | Point | Finding | Decision |
|---|-------|---------|----------|
| 1 | `vector<AlarmFeature>` → bitmask | 6 features max → `uint8_t` bitmask eliminates heap allocation. ESP8266 friendly. | Use `uint8_t` with `AlarmFeature` enum as bit flags |
| 2 | HA command format with code | Default `command_template` is `{{ action }}` — code NOT sent by default. Code is validated locally by HA. With custom template, code IS sent space-separated. | **CRITICAL FIX:** Must set `command_template` in discovery when code config is active. Template: `{{ action }}{% if code %} {{ code }}{% endif %}`. Without this, code passthrough is non-functional. Keep `indexOf(' ')` parsing for payload. |
| 3 | Test file line count | 777 lines. Adding 10 tests (~100-150 lines) → ~920 lines. **Exceeds 800-line limit.** | Create separate `test_ha_alarm_panel.cpp` |
| 4 | HALight callback pattern | `HALight`: `std::function<void(bool, uint8_t)>`, `HASwitch`: `std::function<void(bool)>`, `HAButton`: `std::function<void()>`. Each entity has unique callback signature. | Alarm panel's `std::function<void(const String&, const String&)>` is consistent with per-entity-type signatures |

#### Code Analysis — Key Anchors

| File | Lines | Key Findings |
|------|-------|-------------|
| `HAEntity.h` | 88 | No `handleCommand` virtual — must add. Has `virtual ~HAEntity()` and `virtual buildDiscoveryPayload()`. Clean insertion point at line 82 (before closing brace). |
| `HASwitch.h` | 63 | `handleCommand(const String&)` NOT virtual/override. `autoPublishState=true`. Reference pattern for command entity. |
| `HALight.h` | 81 | JSON command parsing via `deserializeJson()`. Different callback signature `(bool, uint8_t)`. |
| `HAButton.h` | 69 | Simplest entity. `handleCommand` checks `payload == payloadPress`. No state. |
| `HomeAssistant.h` | 566 | Command routing at lines 541-559. Three branches: switch (auto-publish), light (TODO), button (no state). Insert alarm_control_panel branch at line 558. `addAlarmControlPanel()` goes after `addButton()` at line 215. Include at line 21. |
| `test_ha_component.cpp` | 777 | 68 tests. Unity framework. Helpers: `simulateMqttConnect()`, `simulateSwitchCommand()`. Near 800-line limit — must split. |

#### Memory Optimization Audit (Party Mode Step 2)

Feature request original: ~150 bytes/instance + 8 heap allocs.
Optimized design: ~40 bytes/instance + 1 conditional heap alloc (PIN code).

| Member | Original | Optimized | Heap Allocs | Rationale |
|--------|----------|-----------|-------------|-----------|
| `supportedFeatures` | `std::vector<AlarmFeature>` (24B + heap) | `uint8_t` bitmask (1B) | 0 | 6 flags max = 6 bits |
| `payloadArmHome` etc. (x7) | 7 × `String` (~84B + 7 heap) | `constexpr const char*` in namespace (0B/instance) | 0 | Protocol constants, not configurable |
| `code` | `String` (12B + heap) | `String` (12B + heap) | 0-1 | Safety: consumer may pass runtime value |
| `codeArmRequired` etc. (x3) | `bool` (3B) | `bool` (3B) | 0 | — |
| `commandCallback` | `std::function` (~32B) | `std::function` (~32B) | 0 | Pattern consistency |
| **Total per instance** | **~150B + 8 heap** | **~40B + 0-1 heap** | **73% reduction** | |

#### Additional Findings

- **`disarming` state**: HA docs list `disarming` as valid state (not in original feature request). Added as **consumer convenience constant** only — no built-in transition triggers it. Documented with code comment.
- **`command_template` is REQUIRED for code passthrough**: Verified against HA MQTT docs. Default template is `{{ action }}` — codes validated locally by HA, never sent to device. Must set `command_template: "{{ action }}{% if code %} {{ code }}{% endif %}"` in discovery when code config is active. Without this, AC 2/4 are broken.
- **`supported_features` as string array**: Verified against HA MQTT docs — `supported_features` accepts an array of strings (`["arm_away", "arm_home", ...]`). The spec's bitmask-to-array approach is correct.
- **Header-only design**: Confirmed from `project-context.md` — `HAAlarmControlPanel.h` must be header-only (no `.cpp`).
- **HomeAssistant component version**: Currently `1.5.0`. Adding a new entity type = MINOR bump → `1.6.0`.
- **No `project-context.md` per component** yet for HomeAssistant (docs being written by JN0V).
- **Virtual shadow trap**: Adding `virtual handleCommand()` to `HAEntity` while existing `HASwitch`/`HALight`/`HAButton` have non-virtual versions creates a maintenance trap. Calling via `HAEntity*` on existing entities invokes the empty base. Documented as tech debt with TODO comment per Constitution VIII.
- **`addAlarmControlPanel()` must expose code parameters**: Entity is moved into `unique_ptr` and becomes unreachable. Without code params in the API, the `code`/`code*Required` fields are dead code (Constitution IV — no dead code).
- **PlatformIO test discovery**: New `test_ha_alarm_panel/` directory must be auto-discovered by PlatformIO. Verify no restrictive `test_filter` in `platformio.ini`.

#### Exact Insertion Points

| Change | File | Anchor (primary) | Approx. Line (secondary) |
|--------|------|-------------------|--------------------------|
| `virtual void handleCommand(const String&) {}` | `HAEntity.h` | After `buildDiscoveryPayload()` closing `}`, before class closing `};` | ~82 |
| `#include "HAAlarmControlPanel.h"` | `HomeAssistant.h` | After `#include "HAButton.h"` | ~21 |
| `addAlarmControlPanel()` method | `HomeAssistant.h` | After `addButton()` method closing `}`, before `// ========== State Publishing ==========` | ~215 |
| `alarm_control_panel` routing branch | `HomeAssistant.h` | After button branch (`// Buttons don't have state`), before `handleCommand()` closing `}` | ~558 |

**Note:** Line numbers are approximate and may shift after Task 1 modifies `HAEntity.h`. Always use anchor descriptions as primary reference.
