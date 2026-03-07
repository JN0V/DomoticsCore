---
title: 'HAEntity snprintf topics + System HAL millis fix'
slug: 'ha-snprintf-topics-hal-millis'
created: '2026-03-06'
status: 'completed'
stepsCompleted: [1, 2, 3, 4]
tech_stack: [C++, Arduino String, snprintf, PlatformIO, Unity]
files_to_modify:
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HALight.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HAButton.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h
  - DomoticsCore-System/include/DomoticsCore/System.h
code_patterns: [snprintf-over-string-concat, HAL-isolation, MQTT_EVENT_TOPIC_SIZE=128]
test_patterns: [HeapTracker, Unity TEST_ASSERT]
---

# Tech-Spec: HAEntity snprintf topics + System HAL millis fix

**Created:** 2026-03-06

## Overview

### Problem Statement

1. **R5** — The 5 topic-generation methods in `HAEntity` (`getDiscoveryTopic`, `getStateTopic`, `getCommandTopic`, `getAttributesTopic`, `getUniqueId`) use Arduino `String` concatenation, creating 4-5 temporary heap allocations per call. During discovery with N entities, this produces ~20N unnecessary allocations that fragment ESP8266 heap.

2. **M16/R8** — `System.h:530` calls `millis()` directly instead of `HAL::Platform::getMillis()`, violating Constitution IX (HAL isolation).

### Solution

1. **R5** — Replace all 5 methods with `void` functions that write into a caller-provided `char[]` buffer via `snprintf()`. Buffer size is 128 bytes, matching `MQTT_EVENT_TOPIC_SIZE`. Update all ~20 call sites in `HomeAssistant.h` and the 4 derived entity classes. Also convert `commandTopicFilter` (line 526) from String concatenation to snprintf.

2. **M16/R8** — Replace `millis()` with `HAL::Platform::getMillis()` at System.h:530.

### Scope

**In Scope:**
- 5 HAEntity methods -> char buffer API
- ~12 call sites in HomeAssistant.h
- ~8 call sites in 4 derived entities (buildDiscoveryPayload)
- commandTopicFilter String (HomeAssistant.h:526) -> snprintf
- System.h millis() -> HAL::Platform::getMillis()
- Memory stability tests

**Out of Scope:**
- R6 (String fields -> char[] in HAConfig) — larger refactor
- R7 (logging snprintf) — different components
- R24 (handleCommand virtual dispatch refactoring)

## Context for Development

### Codebase Patterns

- All MQTT topics use max 128 bytes (`MQTT_EVENT_TOPIC_SIZE` in MQTT.h:36)
- `mqttPublish()` already does `strncpy(ev.topic, topic.c_str(), MQTT_EVENT_TOPIC_SIZE - 1)` — so the String is immediately copied into a char buffer anyway
- Derived entities call `getCommandTopic()` / `getStateTopic()` inside `buildDiscoveryPayload()` and assign the result to `doc["key"]` (ArduinoJson accepts `const char*`)
- `commandTopicFilter` is a `String` member kept alive for EventBus pointer validity — can become `char[128]` member instead
- HeapTracker pattern used for memory stability tests (see EventBus tests for reference)

### Files to Reference

| File | Purpose |
| ---- | ------- |
| DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h | Base entity — 5 methods to change |
| DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h | Main component — 12 call sites + commandTopicFilter |
| DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h | buildDiscoveryPayload — 1 call (getCommandTopic) |
| DomoticsCore-HomeAssistant/include/DomoticsCore/HALight.h | buildDiscoveryPayload — 3 calls (getCommandTopic x2, getStateTopic) |
| DomoticsCore-HomeAssistant/include/DomoticsCore/HAButton.h | buildDiscoveryPayload — 2 calls (getUniqueId, getCommandTopic) |
| DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h | buildDiscoveryPayload — 1 call (getCommandTopic) |
| DomoticsCore-MQTT/include/DomoticsCore/MQTT.h | MQTT_EVENT_TOPIC_SIZE constant (128) |
| DomoticsCore-System/include/DomoticsCore/System.h | millis() fix at line 530 |
| DomoticsCore-Core/include/DomoticsCore/Platform_HAL.h | HAL::Platform::getMillis() reference |

### Technical Decisions

1. **Buffer size = 128 bytes** — Matches MQTT_EVENT_TOPIC_SIZE. Typical topic: `homeassistant/alarm_control_panel/mydevice/front_door/config` = ~60 chars. 128 is generous.
2. **Caller-provided buffer (not static)** — Avoids thread-safety issues and re-entrancy concerns. Stack allocation is free.
3. **`commandTopicFilter` becomes `char[128]` member** — Eliminates the last String in the command subscription path. The member was kept as String solely for pointer lifetime.
4. **No return value from topic methods** — Caller declares `char buf[128]` on stack, passes it. Zero heap allocation.
5. **Private `buildTopic()` helper** — All 4 topic methods share the same format `prefix/component/nodeId/entityId/suffix`. Extract a single private `buildTopic(buf, len, discoveryPrefix, nodeId, suffix)` method (parameter order matches snprintf format string order: prefix first, then nodeId); public methods become one-liners calling it. Centralizes the format string for future changes.
6. **Callers pass `char[]` directly to `mqttPublish()`** — No intermediate `String topic` variable. The `char[128]` on stack goes straight to `mqttPublish(const char*, ...)`.
7. **Keep `const String&` in `buildDiscoveryPayload()` signature** — Public API unchanged. Internal calls use `.c_str()` when calling topic methods. Minimizes API surface change.
8. **Test topic methods directly with zero tolerance** — HeapTracker tests target `getXxxTopic()` methods directly (pure stack ops, zero heap), not via publishDiscovery() which has EventBus/MQTT noise.

## Implementation Plan

### Tasks

- [x] **T1: Change HAEntity 5 methods to snprintf char buffer API**
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h`
  - Add private helper: `void buildTopic(char* buf, size_t len, const char* discoveryPrefix, const char* nodeId, const char* suffix) const` using `snprintf(buf, len, "%s/%s/%s/%s/%s", discoveryPrefix, component.c_str(), nodeId, id.c_str(), suffix)` — parameter order matches format string order
  - Replace each `String getXxx(...)` with `void getXxx(char* buf, size_t len, const char* nodeId, const char* discoveryPrefix = "homeassistant") const` — each is a one-liner calling `buildTopic()` with the appropriate suffix
  - Suffixes: `"config"`, `"state"`, `"set"`, `"attributes"`
  - `getUniqueId` is separate (different format): `snprintf(buf, len, "%s_%s", nodeId, id.c_str())`
  - Note: `component`, `id` are `String` members — use `.c_str()` in snprintf args

- [x] **T2: Update HAEntity::buildDiscoveryPayload() base**
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h`
  - Lines 57-81: Replace `getUniqueId(nodeId)` and `getStateTopic(nodeId, discoveryPrefix)` with char buffer versions
  - Declare `char buf[128]` on stack, call new methods, assign to `doc["key"]`

- [x] **T3: Update all derived entity buildDiscoveryPayload()**
  - Files: `HASwitch.h`, `HALight.h`, `HAButton.h`, `HAAlarmControlPanel.h`
  - Pattern: declare `char buf[128]` on stack, call new topic methods with `.c_str()` args, assign to `doc["key"]`
  - HASwitch (line 37): 1 call — `getCommandTopic`
  - HALight (lines 32, 40, 41): 3 calls — `getCommandTopic` x2, `getStateTopic` x1
  - HAButton (lines 32, 51): 2 calls — `getUniqueId`, `getCommandTopic`. NOTE: HAButton does NOT call base `buildDiscoveryPayload()` — it rebuilds manually, so `getUniqueId` must be called explicitly here
  - HAAlarmControlPanel (line 85): 1 call — `getCommandTopic`

- [x] **T4: Update HomeAssistant.h call sites**
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Line 261: `publishState()` — getStateTopic
  - Line 305: `publishStateJson()` — getStateTopic
  - Line 320: `publishAttributes()` — getAttributesTopic
  - Line 374: `removeDiscovery()` — getDiscoveryTopic
  - Line 506: `publishEntityDiscovery()` — getDiscoveryTopic
  - Line 335: `setAvailable()` — passes `config.availabilityTopic` (String) directly to mqttPublish. After T6 signature change, must add `.c_str()`: `mqttPublish(config.availabilityTopic.c_str(), payload, 0, true)`
  - Pattern: declare `char topic[128]`, call method, pass `topic` directly to `mqttPublish()` — NO intermediate `String topic` variable
  - Note: `mqttPublish()` signature changes from `const String& topic` to `const char* topic` (see T6)

- [x] **T5: Convert commandTopicFilter to char[128] member + snprintf**
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Line 442: Change `String commandTopicFilter` to `char commandTopicFilter[128]`
  - Line 526: Replace String concatenation with `snprintf(commandTopicFilter, sizeof(commandTopicFilter), "%s/+/%s/+/set", config.discoveryPrefix.c_str(), config.nodeId.c_str())`
  - Line 530: Already uses `strncpy` from commandTopicFilter — now just `commandTopicFilter` directly (it's already a char*)
  - Line 536: Change `DLOG_D(..., commandTopicFilter.c_str())` to `DLOG_D(..., commandTopicFilter)` — `.c_str()` won't compile on `char[]`

- [x] **T6: Update mqttPublish() signature**
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Line 459: Change `const String& topic` to `const char* topic`
  - Line 464: Change `strncpy(ev.topic, topic.c_str(), ...)` to `strncpy(ev.topic, topic, ...)`
  - Note: payload stays `const String&` — separate concern (R6/R7 scope)

- [x] **T7: Fix System.h millis() -> HAL (M16/R8)**
  - File: `DomoticsCore-System/include/DomoticsCore/System.h`
  - Line 530: Replace `millis()` with `HAL::Platform::getMillis()`

- [x] **T8: Add zero-allocation test for topic methods**
  - File: `DomoticsCore-HomeAssistant/test/test_ha_entity/test_ha_entity.cpp` (NEW FILE — separate from test_ha_component.cpp which is at ~959 lines, near Constitution VII 800-line limit)
  - Only needs `#include <DomoticsCore/HAEntity.h>` and `#include <DomoticsCore/Testing/HeapTracker.h>` — no Core, EventBus, or MQTT
  - Test `test_ha_topic_methods_zero_heap`: Create HAEntity (String members allocated), THEN HeapTracker checkpoint, call all 5 topic methods 100 times into `char buf[128]`, assert heap delta = 0 (zero tolerance — pure stack ops, no EventBus noise)
  - Test `test_ha_topic_format_correctness`: Verify each method produces expected string. Use `alarm_control_panel` component (longest name) to validate no truncation. Example: `"homeassistant/alarm_control_panel/mynode/front_door/config"`
  - Test `test_ha_topic_truncation_safety`: Create HAEntity with 60-char id and 60-char nodeId, call getDiscoveryTopic, assert `strlen(buf) == 127` (truncated, not overflowed) — validates AC4
  - Verifies zero heap allocation and truncation safety

- [x] **T9: Update CODE-ROADMAP.md tracking**
  - File: `docs/CODE-ROADMAP.md`
  - Mark R5: DONE, M16/R8: DONE in both tracking tables

### Acceptance Criteria

- [x] **AC1:** Given HAEntity with any entity type, when calling any topic method, then zero heap allocations occur (snprintf into caller buffer only)
  - Verify: All 5 methods use `void` return + `char* buf` parameter
- [x] **AC2:** Given HAEntity topic methods, when called 100 times in a loop, then heap delta is exactly zero (pure stack operations)
- [x] **AC3:** Given System::getSystemStatus(), when called, then it uses HAL::Platform::getMillis() not millis()
- [x] **AC4:** Given any topic generation, when prefix + component + nodeId + entityId combined exceed 127 chars, then output is safely truncated (snprintf guarantees)
- [x] **AC5:** Given all existing HA tests, when run after changes, then all pass (no regression)

## Additional Context

### Dependencies

- No external dependencies. All changes are internal to HAEntity API.
- Derived entities (HASwitch, HALight, HAButton, HAAlarmControlPanel) must be updated atomically with HAEntity.

### Testing Strategy

- **Unit test (zero-alloc):** HeapTracker test calling topic methods directly — zero tolerance (pure stack ops, no EventBus/MQTT noise)
- **Unit test (format):** Verify topic string format correctness for all 5 methods
- **Unit test (truncation):** Verify snprintf truncation safety with oversized inputs (AC4)
- **Regression:** All existing HA component tests must pass
- **Manual verification:** Discovery topics in MQTT match expected format (same as before, just built differently)

### Notes

- The `buildDiscoveryPayload()` methods in derived entities pass topic strings to ArduinoJson `doc["key"] = buf` — ArduinoJson copies the string content, so the stack buffer is safe (it outlives the assignment).
- `mqttPublish()` internally does `strncpy` into `MQTTPublishEvent.topic` — the String→char* transition is seamless since it already discarded the String immediately.
- `commandTopicFilter` as `char[128]` member uses the same memory as the String (which had a ~60 byte internal buffer + String object overhead), so net memory is neutral or slightly better.
- Stack usage: base + derived `buildDiscoveryPayload()` each declare `char buf[128]` = 256 bytes stack total. ESP8266 stack is 4KB, call depth is shallow — no concern.
- HAButton does NOT call `HAEntity::buildDiscoveryPayload()` — it rebuilds discovery manually. Must call `getUniqueId()` and `getCommandTopic()` explicitly in T3.

## Review Notes
- Adversarial review completed
- Findings: 12 total, 3 fixed (F1, F4, F9), 9 skipped (noise/pre-existing/out-of-scope)
- Resolution approach: auto-fix (real findings in scope)
