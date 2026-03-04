---
title: 'HASwitch Auto-Publish State Control'
slug: 'ha-switch-auto-publish-fix'
created: '2026-03-03'
status: 'completed'
stepsCompleted: [1, 2, 3, 4]
tech_stack: ['C++17', 'PlatformIO (native env)', 'Unity Test Framework', 'ArduinoJson 7.x', 'ESP32/ESP8266']
files_to_modify: ['DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h', 'DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h', 'DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp']
code_patterns: ['header-only library', 'EventBus pub/sub via emit()', 'std::unique_ptr entity ownership', 'static_cast entity routing in handleCommand', 'bool flags for entity behavior (optimistic pattern)', 'namespace DomoticsCore::Components::HomeAssistant']
test_patterns: ['Unity TEST_ASSERT macros', 'test_ prefix naming', 'setUp/tearDown empty stubs', 'UNITY_BEGIN/UNITY_END runner', 'native platform tests (no hardware)', 'lambda capture for callback verification']
---

# Tech-Spec: HASwitch Auto-Publish State Control

**Created:** 2026-03-03

## Overview

### Problem Statement

When a Home Assistant switch command is received, `HomeAssistantComponent::handleCommand()` **always** publishes the commanded state back to the HA state topic, regardless of whether the consumer's callback succeeded or failed.

This causes HA to believe the switch changed state when it did not (e.g., a state machine guard rejected the command). The switch becomes permanently stuck in the wrong state in HA's UI -- the user cannot retry because HA won't re-send a command it thinks already succeeded.

This is particularly dangerous for safety-critical switches (alarm systems, locks) where false-positive state display has security implications.

### Solution

Add a `bool autoPublishState = true` public member to `HASwitch` that controls whether `handleCommand()` auto-publishes the commanded state. Default `true` preserves backward compatibility. Consumers that need to manage state manually (e.g., after async validation) set it to `false` and call `publishState()` themselves.

### Scope

**In Scope:**
- Add `autoPublishState` public member to `HASwitch` class (follows existing `optimistic` pattern)
- Add `autoPublishState` optional parameter to `HomeAssistantComponent::addSwitch()`
- Condition auto-publish in `handleCommand()` on the new flag
- Unit tests for default behavior, disabled auto-publish, manual publish, optimistic interaction, and no-callback edge case
- MINOR version bump via `tools/bump_version.py HomeAssistant minor`

**Out of Scope:**
- HALight auto-publish flag (YAGNI: HALight has no auto-publish yet -- `HomeAssistant.h:551` is a TODO. Add flag when auto-publish is implemented)
- Changing callback signature from `void(bool)` to `bool(bool)` (KISS/YAGNI per constitution)
- Discovery payload changes (consumer manages state timing)
- Fixing the non-functional `publishing` re-entrancy guard (existing debt, see Known Issues)
- Fixing `publishState(id, bool)` hardcoded `"ON"`/`"OFF"` vs custom payloads (existing debt, see Known Issues)

## Context for Development

### Codebase Patterns

- **Header-only library**: All HA entity types and the component itself are implemented in `.h` files
- **EventBus communication**: MQTT publish/subscribe goes through `emit()` events, not direct MQTT calls
- **Entity ownership**: `std::vector<std::unique_ptr<HAEntity>>` in `HomeAssistantComponent`
- **Fire-and-forget callbacks**: Current pattern is `std::function<void(...)>` for all entity command callbacks
- **Public bool flags for entity behavior**: `optimistic` is a public member set after construction (not a constructor parameter). `autoPublishState` follows this same pattern.

### Files to Reference

| File | Purpose | Lines of Interest |
| ---- | ------- | ----------------- |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h` | HASwitch entity -- add `autoPublishState` member | L14-57 (full class) |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` | Main component -- modify `addSwitch()` and `handleCommand()` | L175-184 (addSwitch), L538-547 (handleCommand switch block) |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h` | Base entity class (read-only reference) | L19-83 (no changes needed) |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HALight.h` | HALight entity (read-only reference, no changes) | L55-75 (handleCommand, no auto-publish) |
| `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp` | Unit tests -- add auto-publish test cases | L199-225 (existing switch tests), L460-525 (test runner, 31 existing tests) |
| `DomoticsCore-HomeAssistant/platformio.ini` | Test build config (native env, Unity) | L1-20 |
| `DomoticsCore-HomeAssistant/library.json` | Version: 1.4.0, will bump to 1.5.0 | L3 |
| `specs/005-ha-switch-auto-publish-fix/bug-report.md` | Original bug report with reproduction steps | -- |

### Technical Decisions

- **Flag approach over callback return type**: KISS principle (Constitution III) -- a simple bool flag is clear, minimal, and non-breaking. The `bool(bool)` callback alternative was rejected as "clever over clear" with unnecessary complexity (wrapper lambdas, dual overloads, heap allocation)
- **Public member, not constructor parameter**: Follows the existing `optimistic` pattern. The flag is a public `bool` member with a default value, set after construction via `addSwitch()` or direct member access. No constructor signature change needed -- simpler and consistent (Constitution III - KISS).
- **Default `true`**: Preserves 100% backward compatibility (Constitution VIII - Progressive Refactoring)
- **No discovery payload change**: When `autoPublishState = false`, consumer is responsible for timely state publishing; no need to declare optimistic to HA
- **HALight deferred (YAGNI)**: HALight has no auto-publish implementation yet (HomeAssistant.h:551 is a TODO). Adding a flag that controls nothing violates YAGNI. Flag will be added when light auto-publish is implemented.
- **MINOR version bump**: New backward-compatible feature per Constitution XV (Semantic Versioning). Use `tools/bump_version.py HomeAssistant minor`

## Implementation Plan

### Tasks

> **TDD Discipline (Constitution II):** Adapted Red-Green-Refactor for compiled C++. In C++, RED means "tests compile and execute but fail on assertions" -- not "code doesn't compile". Therefore: scaffolding structure first (compilable), then tests (RED on logic), then implementation (GREEN).
>
> **Quality Gate vs RED phase:** Constitution II requires both Red-Green-Refactor AND 100% green for phase transitions. These interact as follows: Phase 2 is complete when all tests are *written*. The deliberate RED failure (`test_switch_command_no_auto_publish_when_disabled`) is the entry condition for Phase 3. Phase 3's Quality Gate requires all tests GREEN before proceeding to Phase 4. The RED-to-GREEN transition spans Phase 2 into Phase 3, not within a single phase.

#### Phase 1: SCAFFOLD -- Minimal structure for tests to compile

- [x] Task 1: Add `autoPublishState` member to HASwitch
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h`
  - Action: Add `bool autoPublishState = true;` public member after `bool optimistic = false;` (line 25)
  - Notes: Public member with default value, following the existing `optimistic` pattern. No constructor signature change. This is ONLY the data structure -- no logic change yet.

- [x] Task 2: Add `autoPublishState` parameter to `addSwitch()`
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Add `bool autoPublishState = true` as final parameter to `addSwitch()` (line 176). After constructing the switch, set the flag: `sw->autoPublishState = autoPublishState;` before `entities.push_back()`.
  - Notes: Optional param with default value -- all existing consumers compile unchanged. NO logic change in `handleCommand()` yet.

#### Phase 2: RED -- Write tests that compile, one integration test fails

- [x] Task 3: Write unit tests for HASwitch flag structure
  - File: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
  - Action: Add 4 unit test functions after existing switch tests (after line 225). Register them in `main()` test runner. Tests:
    1. `test_switch_auto_publish_default_true` -- instantiate `HASwitch` directly, verify `autoPublishState` defaults to `true` (will PASS -- scaffolding in place)
    2. `test_switch_auto_publish_set_false` -- instantiate `HASwitch`, set `autoPublishState = false`, verify it holds (will PASS -- scaffolding in place)
    3. `test_switch_handle_command_calls_callback` -- verify `HASwitch::handleCommand("ON")` calls callback with `true`, `handleCommand("OFF")` calls with `false` (will PASS -- existing behavior)
    4. `test_switch_handle_command_no_callback_no_crash` -- verify `HASwitch::handleCommand()` with nullptr callback does not crash (will PASS -- existing behavior)
  - Notes: These unit tests verify HASwitch entity-level structure and callback behavior. They exercise `HASwitch::handleCommand()` directly, which does NOT include auto-publish logic (auto-publish lives in `HomeAssistantComponent::handleCommand()` at HomeAssistant.h:538-547). All 4 tests pass after scaffolding -- they are structural verification, not RED tests.

- [x] Task 4: Write integration tests for auto-publish via EventBus
  - File: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
  - Action: Add 5 integration tests using Core + EventBus:
    1. `test_switch_command_auto_publishes_state` -- create Core + HA component with switch (default `autoPublishState = true`), listen for `MQTTPublishEvent`, emit `MQTTMessageEvent` simulating switch command, assert state publish emitted (will PASS -- existing behavior, **AC 1**)
    2. `test_switch_command_no_auto_publish_when_disabled` -- same with `autoPublishState = false`, assert state publish NOT emitted (will **FAIL** -- RED: auto-publish still fires because `handleCommand()` ignores the flag, **AC 2**)
    3. `test_switch_optimistic_overrides_auto_publish` -- switch with `optimistic = true, autoPublishState = true`, send command, assert NO publish emitted (will PASS -- existing optimistic behavior, **AC 4**)
    4. `test_switch_manual_publish_after_auto_disabled` -- switch with `autoPublishState = false`, send command (no auto-publish expected), then explicitly call `publishState(id, true)` on the component, assert `MQTTPublishEvent` emitted (will PASS -- `publishState()` is independent of `autoPublishState`, **AC 3**)
    5. `test_switch_optimistic_true_auto_publish_false` -- switch with `optimistic = true, autoPublishState = false`, send command, assert NO publish emitted (will PASS -- optimistic alone suppresses publish; completes the 2x2 interaction matrix)
  - Notes: Follows pattern from existing `test_ha_full_lifecycle`. Must emit `mqtt/connected` first to set `mqttConnected = true`, then `mqtt/message` with topic `homeassistant/switch/{nodeId}/{entityId}/set`. If `Core::on()` is not accessible for external listeners, create a minimal mock component that subscribes to `MQTTPublishEvent` and records captures.
  - **Expected RED**: `test_switch_command_no_auto_publish_when_disabled` FAILS -- this is the core bug we're fixing. All other integration tests pass.

#### Phase 3: GREEN -- Implement the fix

- [x] Task 5: Condition auto-publish on the flag in `handleCommand()`
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Change line 544 from `if (!sw->optimistic)` to `if (!sw->optimistic && sw->autoPublishState)`
  - Notes: This is the core fix. One line change. Makes `test_switch_command_no_auto_publish_when_disabled` pass.

- [x] Task 6: Verify all tests GREEN
  - Action: Run `cd DomoticsCore-HomeAssistant && pio test -e native`
  - Notes: All 40 tests (31 existing + 9 new) must pass. If any fail, fix before proceeding.

#### Phase 4: SHIP

- [x] Task 7: Version bump
  - Action: Run `python tools/bump_version.py HomeAssistant minor`
  - Notes: Bumps `library.json` from 1.4.0 to 1.5.0 and propagates to root. Run AFTER all tests pass. Verify with `python tools/check_versions.py --verbose`.

### Acceptance Criteria

- [x] AC 1: Given a default HASwitch (no `autoPublishState` argument), when `addSwitch()` is called, then `autoPublishState` is `true` and existing behavior is unchanged (state auto-published after command).

- [x] AC 2: Given an HASwitch with `autoPublishState = false`, when HA sends a switch command (ON/OFF), then the command callback fires but NO state is auto-published to the MQTT state topic.

- [x] AC 3: Given an HASwitch with `autoPublishState = false`, when the consumer calls `publishState()` manually after its own validation, then the state IS published to the MQTT state topic.

- [x] AC 4: Given an HASwitch with `optimistic = true` (regardless of `autoPublishState` value), when HA sends a switch command, then NO state is auto-published (optimistic behavior unchanged).

- [x] AC 5: Given an HASwitch with no command callback (nullptr), when a command is received, then no crash occurs. Auto-publish behavior follows the `autoPublishState` flag normally (state is published if `autoPublishState = true` and `optimistic = false`).

- [x] AC 6: Given existing consumer code using `addSwitch()` without the new parameter, when compiled against updated library, then compilation succeeds with no changes required (backward compatibility).

- [x] AC 7: Given all changes complete, when `pio test -e native` is run in `DomoticsCore-HomeAssistant/`, then all tests pass (existing + new).

## Additional Context

### Dependencies

None -- this is a self-contained change within the DomoticsCore-HomeAssistant library.

### Testing Strategy

- **Framework**: Unity (PlatformIO native env, C++17)
- **Test file**: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
- **Run command**: `cd DomoticsCore-HomeAssistant && pio test -e native`
- **TDD Order**: SCAFFOLD (Tasks 1-2, structure only) -> RED (Tasks 3-4, write tests, `test_switch_command_no_auto_publish_when_disabled` fails) -> GREEN (Tasks 5-6, one-line fix, all tests pass) -> SHIP (Task 7)
- **Unit tests (Task 3)**: Direct HASwitch instantiation. Verify flag defaults, member assignment, callback invocation, and no-crash for nullptr. 4 test functions. These verify entity structure, not auto-publish behavior (which lives in `HomeAssistantComponent::handleCommand()`).
- **Integration tests (Task 4)**: Core + HomeAssistantComponent + EventBus. Simulate MQTT message flow, capture `MQTTPublishEvent` to verify auto-publish behavior. 5 test functions covering: default auto-publish (AC 1), disabled auto-publish (AC 2), manual publish (AC 3), optimistic override (AC 4), and optimistic+disabled interaction matrix.
- **Total new tests**: 9
- **Existing tests**: 31
- **Regression**: All 31 existing tests must continue to pass unchanged.
- **Coverage (Constitution II)**: All branches of the `handleCommand()` switch block must be covered: `autoPublishState=true/false` x `optimistic=true/false` = 4 combinations. Task 4 tests 1, 2, 3, and 5 cover all four.
- **Memory (Constitution XIV)**: This change adds a single `bool` member (~1 byte + alignment) per `HASwitch` entity. No heap allocation, no `String` concatenation, no container operations. Memory impact is negligible. Hardware soak testing (heap stability over 1h) must be performed before merge per constitution, but is outside the scope of native unit tests.

### Known Issues (Existing, Out of Scope)

- **`publishing` flag is non-functional**: The `volatile bool publishing` member in `HomeAssistantComponent` (HomeAssistant.h:404) is set during `publishState()` but **never checked** anywhere -- there is no `if (publishing) return;` guard. It does not prevent re-entrant publishes. Calling `publishState()` from within a switch callback is safe (the flag is `false` during callback execution) but will result in a double publish if `autoPublishState = true`: once from the callback's manual call, once from auto-publish. A follow-up should either implement the guard or remove the dead flag.
- **`publishState(id, bool)` uses hardcoded `"ON"`/`"OFF"`**: The `publishState(const String& id, bool state)` overload (HomeAssistant.h:250-252) publishes `state ? "ON" : "OFF"` regardless of the entity's custom `payloadOn`/`payloadOff` values. Consumers using custom payloads who call the `bool` overload for manual publish (AC 3) will publish incorrect values. Use the `String` overload with the correct payload value instead.
- **Thread safety on ESP32 dual-core**: The `autoPublishState` flag (like `optimistic`) is a plain `bool`, not `volatile` or atomic. Set `autoPublishState` before calling `core.begin()`. Modifying it at runtime from a different FreeRTOS task than the one running `handleCommand()` is a data race.

### Notes

- Bug discovered in AlarmControl project (consumer of DomoticsCore 1.6.1)
- The existing `optimistic` flag has different HA semantics and is not appropriate for this use case
- Future work: when HALight auto-publish is implemented (HomeAssistant.h:551 TODO), apply the same `autoPublishState` pattern

## Review Notes

- Adversarial review completed
- Findings: 8 total, 1 fixed (F4: SPECIFICATIONS.md updated), 7 skipped (out of scope or intended behavior per tech-spec)
- Resolution approach: auto-fix
- F1 (auto-publish assumes success): out of scope, KISS decision documented
- F2 (boolean params): follows existing codebase pattern, YAGNI
- F3 (null callback + auto-publish): intended per AC 5
- F5 (discovery mismatch): out of scope per tech-spec
- F6/F7/F8: low severity, existing patterns
