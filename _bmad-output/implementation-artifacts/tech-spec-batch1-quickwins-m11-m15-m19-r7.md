---
title: 'Batch 1 Roadmap Quick Wins - M11, M15, M19, R7'
slug: 'batch1-quickwins-m11-m15-m19-r7'
created: '2026-03-07'
status: 'completed'
stepsCompleted: [1, 2, 3, 4]
tech_stack: [C++17, Arduino/ESP32, PlatformIO, header-only components]
files_to_modify: [StorageEvents.h, Storage.h, RemoteConsole.h, System.h]
code_patterns: [emit-event-pattern, snprintf-safe-buffers, HAEntityAddedEvent-struct, POD-struct-payloads, cache-std-map]
test_patterns: [unity-test-framework, test_storage_events.cpp-pattern, HeapTracker-memory-tests]
---

# Tech-Spec: Batch 1 Roadmap Quick Wins — M11, M15, M19, R7

**Created:** 2026-03-07

## Overview

### Problem Statement

The roadmap identifies 4 quick-win items across Core, Storage, HA, and logging components:
- **M11**: `Core::emit` was missing the `sticky` parameter — **ALREADY FIXED** in prior batch.
- **M15**: Storage does not emit change events when values are updated via `put*`, `remove()`, or `clear()` methods, preventing reactive patterns from downstream components. Additionally, `putULong64` has a cache gap (doesn't update internal cache unlike other `put*` methods).
- **M19**: `ha/entity_added` event was missing from `addBinarySensor/addSwitch/addLight/addButton/addAlarmControlPanel` — **ALREADY FIXED** in prior batch.
- **R7**: Hot-path logging in RemoteConsole, Storage, and System uses `String` concatenation instead of `snprintf` with static buffers, causing heap fragmentation and potential buffer overflow.

### Solution

- **M11/M19**: Document as "already implemented" for traceability (no code changes needed).
- **M15**: Add `storage/changed` event constant + lightweight POD struct payload (`char key[64]`) + emit in `put*`/`remove()`/`clear()` methods **with dirty check** (compare against cached value before emitting to avoid spamming the event bus in tight loops). Fix `putULong64` cache gap.
- **R7**: Replace `String` concatenation with `snprintf()` + stack-allocated `char[]` buffers in logging hot paths across RemoteConsole, Storage, and System.

### Scope

**In Scope:**
- M15: New event constant in `StorageEvents.h`, new `StorageChangedEvent` POD struct, dirty-check + emit calls in all `put*`/`remove()`/`clear()` methods in `Storage.h`, fix `putULong64` cache gap
- R7: Refactor String concatenation to snprintf in RemoteConsole.h, Storage.h, System.h hot paths
- Traceability documentation for M11/M19 (already done)

**Out of Scope:**
- General logging refactoring beyond hot paths
- Additional Storage events beyond `changed`
- Modification of Storage public API signatures
- Performance benchmarking (follow-up if needed)

## Context for Development

### Codebase Patterns

- Events use **POD struct** payloads with fixed-size `char[]` fields (e.g., `HAEntityAddedEvent { char id[64]; char component[32]; }`)
- EventBus copies payloads as **byte arrays** via `reinterpret_cast` + `std::vector<uint8_t>.assign()` — structs MUST be POD (no `String` members)
- Event constants are defined as `static constexpr const char*` in `*Events.h` headers
- Components emit via `emit(EventConstant, payload)` inherited from `IComponent` (IComponent.h:223)
- Handlers receive `const void*` — subscribers cast to expected struct type
- Storage internal cache: `std::map<String, StorageEntry> cache` (Storage.h:83). **Note**: `StorageEntry` is NOT POD (contains `String`, `std::vector`) — it is the internal cache type, NOT an event payload. The new `StorageChangedEvent` is the event payload and MUST be POD.
- Getters bypass cache (read from HAL backend directly) — cache is write-side only
- Logging uses `DLOG_I`, `DLOG_W`, `DLOG_D` macros with printf-style format strings
- Safe string copy pattern: `snprintf(dest, sizeof(dest), "%s", src.c_str())`
- Existing emit reference: `emit(StorageEvents::EVENT_READY, storageConfig.namespace_name)` at Storage.h:140

### Files to Reference

| File | Purpose | Investigation Notes |
| ---- | ------- | ------------------- |
| `DomoticsCore-Storage/include/DomoticsCore/StorageEvents.h` | Storage event constants — add EVENT_CHANGED + struct | Only has EVENT_READY (line 15) |
| `DomoticsCore-Storage/include/DomoticsCore/Storage.h` | Storage put*/remove/clear + M15 emit + R7 logging | put* at lines 176-293, remove at 372, clear at 388, cache at line 83, getStorageInfo at 421, dumpContents at 475 |
| `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` | RemoteConsole logging — R7 refactor | 7 MUST FIX locations: formatLogEntry(568-596), commands(396-434), handleClient(549) |
| `DomoticsCore-System/include/DomoticsCore/System.h` | System logging — R7 refactor | 4 MUST FIX: getSystemStatus(527-533), getBootDiagnostics(558-577), WiFi AP SSID(250) |
| `DomoticsCore-Storage/test/test_storage_events.cpp` | Test pattern for Storage events | Subscribe+begin+loop+assert pattern (reference for M15 tests) |
| `DomoticsCore-Storage/test/test_storage_api.cpp` | Test pattern for put/get + HeapTracker | HeapTracker memory stability pattern |
| `DomoticsCore-Core/include/DomoticsCore/Core.h` | M11 reference — sticky param already present | Core.h:163-168 |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` | M19 reference — entity_added already present | HomeAssistant.h:144-275 |

### Technical Decisions

- **M15 Payload**: POD struct `StorageChangedEvent { char key[64]; }` — key only, no `String` members (EventBus byte-copy requirement). Subscribers re-read if they need the value.
- **M15 Dirty Check**: Compare against cache before emitting. For `putString`: compare `cache[key].stringValue == value`. For `putInt`/`putBool`/`putULong64`: exact `==` comparison. For `putFloat`: accept false positives (use `==`, no epsilon — the cost of an extra event is negligible vs epsilon complexity). For `putBlob`: skip dirty check (blob comparison too expensive). For `remove()`/`clear()`: always emit (destructive ops). **Order of operations**: check cache → HAL write → cache update → emit. Cost: one comparison per `put*` call — negligible.
- **M15 putULong64 Cache Fix**: `StorageEntry` struct (Storage.h:37-48) lacks a `uint64_t` field AND `StorageValueType` enum (Storage.h:28-34) lacks a `UInt64` variant — both are root causes. Fix requires: (1) add `UInt64` to enum, (2) add `uint64_t uint64Value = 0;` field to `StorageEntry`, (3) add `cache[key] = entry` in `putULong64()` after successful write.
- **M15 Re-entrance Safety**: Document that subscribers MUST NOT call `put*` on the same key inside their `storage/changed` handler to avoid infinite loops. The EventBus does not provide re-entrance guards — this is a convention.
- **M15 remove()/clear()**: `remove()` emits with `key` set to the removed key. `clear()` emits with `key = "*"` (wildcard convention to signal all keys cleared).
- **R7 Buffer Size**: Use `char buf[256]` for multi-line info strings (getSystemStatus, getStorageInfo). Use `char buf[384]` for RemoteConsole `info` command (7 lines with variable-length device/WiFi names). Use `char buf[512]` for `getBootDiagnostics` (multi-section output). Use `char buf[128]` for single-line responses (level, filter, heap, error commands).
- **R7 formatLogEntry**: Special case — use `char buf[256]` since it combines timestamp + level + tag + message. This is the hottest path (every remote log line).
- **R7 dumpContents**: Use snprintf per-entry inside loop, append to a `String result` (can't avoid String for variable-length dump output, but eliminate per-field concatenation).
- **R7 Truncation**: `snprintf` silently truncates. Acceptable for logging — no runtime error needed.
- **R7 Format Specifier Portability**: Use `(unsigned long)` casts with `%lu` for `uint32_t`/`size_t` values. Do NOT use `%zu` (may not be supported by newlib-nano on some ESP32 toolchains). For `uint64_t`, use `(unsigned long long)` with `%llu`.

### R7 Investigation Detail — Locations to Refactor

**RemoteConsole.h (7 MUST FIX locations):**

| Lines | Method | Issue |
| ----- | ------ | ----- |
| 568-596 | `formatLogEntry()` | String concat building formatted log line + ANSI color codes (HOTTEST path) |
| 396 | `level` cmd (get) | `"Current log level: " + String((int)currentLogLevel)` |
| 405 | `level` cmd (set) | `"Log level set to: " + String(level)` |
| 417 | `filter` cmd | `"Filtering logs by tag: " + args` |
| 423-428 | `info` cmd | 6 lines of `info += "..." + String(val)` |
| 434 | `heap` cmd | `"Free Heap: " + String(HAL::getFreeHeap())` |
| 549 | `handleClient()` | `"Unknown command: " + cmd` (note: not a lambda, uses `client.println`) |

**Deliberately excluded:** `help` command handler (lines 363-384) — cold path, called at most once per telnet session. The static text portion is a single string literal, and the dynamic loop iterates over registered commands (typically <10). Cost of refactoring outweighs benefit for this frequency.

**System.h (4 MUST FIX):**

| Lines | Method | Issue |
| ----- | ------ | ----- |
| 527-533 | `getSystemStatus()` | 5 lines of String concat with int conversion |
| 558-577 | `getBootDiagnostics()` | 10+ lines of String concat with int conversion |
| 250 | `registerWifiComponent()` | WiFi AP SSID hex conversion + concat |

**Storage.h (2 MUST FIX):**

| Lines | Method | Issue |
| ----- | ------ | ----- |
| 421-431 | `getStorageInfo()` | 6 lines of String concat |
| 475-521 | `dumpContents()` | Heavy concat in loop + type conversions |

## Implementation Plan

### Tasks

#### M15 — Storage Change Events

- [x] Task 1: Define `StorageChangedEvent` struct and `EVENT_CHANGED` constant
  - File: `DomoticsCore-Storage/include/DomoticsCore/StorageEvents.h`
  - Action: Add inside existing `DomoticsCore::StorageEvents` namespace (lines 11-17), after `EVENT_READY` (line 15):
    1. Add `EVENT_CHANGED` constant: `static constexpr const char* EVENT_CHANGED = "storage/changed";`
    2. Add POD struct (inside namespace): `struct StorageChangedEvent { char key[64]; };`
  - Notes: Struct must be POD (no String members) — EventBus byte-copies payloads via `reinterpret_cast`. Place in `DomoticsCore::StorageEvents` namespace to match `HAEntityAddedEvent` pattern in `DomoticsCore::HAEvents`.

- [x] Task 2: Fix `putULong64` cache gap
  - File: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
  - Action:
    1. Add `UInt64` to the `StorageValueType` enum (Storage.h lines 28-34, after `Blob`).
    2. Add `uint64_t uint64Value = 0;` field to `StorageEntry` struct (line 43, after `boolValue`).
    3. In `putULong64()` (lines 260-272), construct `StorageEntry entry` with `entry.uint64Value = value; entry.type = StorageValueType::UInt64;` and add `cache[key] = entry;` after successful HAL write.
  - Notes: Prerequisite for dirty check on `putULong64`. All other `put*` methods already do `cache[key] = entry`. Both the missing enum value AND the missing struct field are root causes of the cache gap. Also add `uint64Value(0)` to the existing constructor initializer list at line 47 for consistency with `intValue(0), floatValue(0.0f), boolValue(false)`.

- [x] Task 3: Add dirty-check + emit to all `put*` methods
  - File: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
  - Action: In each `put*` method, follow this exact order:
    1. **Check cache** (before any write): `auto it = cache.find(key);`
    2. If found AND value unchanged → return `true` early (no HAL write, no cache update, no emit)
    3. **HAL write** — existing code
    4. **Cache update** — existing `cache[key] = entry;`
    5. **Emit** the change event (after cache is updated):
       ```cpp
       StorageChangedEvent ev{};
       snprintf(ev.key, sizeof(ev.key), "%s", key.c_str());
       emit(StorageEvents::EVENT_CHANGED, ev);
       ```
  - Methods to modify (6 total) — `StorageEntry` fields verified at Storage.h:37-48:
    - `putString()` (line 176) — compare: `it->second.stringValue == value` (String ==)
    - `putInt()` (line 197) — compare: `it->second.intValue == value` (int32_t ==)
    - `putFloat()` (line 218) — compare: `it->second.floatValue == value` (float ==, accept false positives — no epsilon)
    - `putBool()` (line 239) — compare: `it->second.boolValue == value` (bool ==)
    - `putULong64()` (line 260) — compare: `it->second.uint64Value == value` (uint64_t ==, field added in Task 2)
    - `putBlob()` (line 274) — no dirty check (blob comparison too expensive), always emit
  - Notes: Include `StorageEvents.h` if not already included. Emit call uses `StorageEvents::StorageChangedEvent` (namespaced). **Dirty check `return true` semantics**: The early return changes observable behavior — previously every `put*` call with `isOpen == true` would perform a HAL write. Now `true` is returned without touching HAL when value is unchanged. This is acceptable: no callers in the codebase depend on every `put*` reaching HAL (verified: callers use `put*` for state persistence, not wear-leveling tracking). The `autoCommit` flag is only checked during HAL write, so skipping HAL write is safe.

- [x] Task 4: Add emit to `remove()` and `clear()`
  - File: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
  - Action:
    1. In `remove()` (line 372): After successful HAL remove + cache erase, emit:
       ```cpp
       StorageChangedEvent ev{};
       snprintf(ev.key, sizeof(ev.key), "%s", key.c_str());
       emit(StorageEvents::EVENT_CHANGED, ev);
       ```
    2. In `clear()` (line 388): After successful HAL clear + cache clear, emit:
       ```cpp
       StorageChangedEvent ev{};
       snprintf(ev.key, sizeof(ev.key), "*");
       emit(StorageEvents::EVENT_CHANGED, ev);
       ```
  - Notes: No dirty check for destructive ops — always emit.

- [x] Task 5: Add M15 unit tests
  - File: `DomoticsCore-Storage/test/test_storage_events.cpp` (extend existing)
  - Action: Add test cases following the existing `subscribe+begin+loop+assert` pattern:
    1. `test_storage_changed_on_putString` — putString triggers `storage/changed` with correct key
    2. `test_storage_changed_dirty_check_string` — putString same value twice, second call does NOT emit
    3. `test_storage_changed_on_putInt` — putInt triggers event
    4. `test_storage_changed_on_putFloat` — putFloat triggers event
    5. `test_storage_changed_on_putBool` — putBool triggers event
    6. `test_storage_changed_on_putULong64` — putULong64 triggers event (validates cache fix)
    7. `test_storage_changed_dirty_check_ulong64` — putULong64 same value twice, second does NOT emit (validates cache + dirty check)
    8. `test_storage_changed_on_putBlob` — putBlob triggers event (always emits, no dirty check)
    9. `test_storage_changed_on_remove` — remove triggers event with key
    10. `test_storage_changed_on_clear` — clear triggers event with key="*"
    11. `test_storage_changed_key_truncation` — key >64 chars is truncated without crash
    12. `test_storage_changed_no_emit_on_failure` — put* before begin() (HAL not initialized) returns false, no event emitted
  - Notes: Use `testCore->loop()` after each operation to process queued events. Use static counters to verify emission count (especially dirty check tests). For test 12: call put* without calling begin() first — HAL returns false.
  - **Important**: The existing EVENT_READY tests use `static_cast<const String*>(payload)` because EVENT_READY emits a raw `String`. The new M15 tests use a **different** cast pattern for the POD struct:
    ```cpp
    #include "DomoticsCore/StorageEvents.h"  // for StorageChangedEvent
    testCore->getEventBus().subscribe(StorageEvents::EVENT_CHANGED, [](const void* payload) {
        auto* ev = static_cast<const StorageEvents::StorageChangedEvent*>(payload);
        lastChangedKey = ev->key;  // copy key for assertion
        changedCount++;
    });
    ```
    Do NOT copy the String cast pattern from EVENT_READY tests — it will crash at runtime.

#### R7 — snprintf in Hot Paths

- [x] Task 6: Refactor `RemoteConsole::formatLogEntry()` (hottest path)
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
  - Action:
    1. **Extract helper**: The ANSI color logic at lines 573-579 is an inline `switch` statement. Extract it into a new private method:
       ```cpp
       static const char* getColorForLevel(LogLevel level) {
           switch (level) {
               case LogLevel::Error:   return "\033[31m";
               case LogLevel::Warning: return "\033[33m";
               case LogLevel::Info:    return "\033[32m";
               default:                return "\033[0m";
           }
       }
       ```
    2. **Replace** String concatenation at lines 568-596 with:
       ```cpp
       char buf[256];
       snprintf(buf, sizeof(buf), "%s[%lu][%s][%s] %s\033[0m",
                getColorForLevel(entry.level),
                (unsigned long)entry.timestamp,
                logLevelToString(entry.level).c_str(),
                entry.tag.c_str(), entry.message.c_str());
       String formatted = buf;
       ```
  - Notes: This is the hottest path — called for every remote log line. Eliminates 5+ temporary String allocations per call. **Important**: `logLevelToString()` returns `String` — must use `.c_str()` when passing to `%s` in snprintf (UB/crash without it). Cast `entry.timestamp` (`uint32_t`) to `(unsigned long)` to match `%lu` format specifier. Verify exact switch cases against actual code at lines 573-579.

- [x] Task 7: Refactor RemoteConsole command handlers
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
  - Action: Replace String concat in each command handler with `snprintf` + `char buf[128]`:
    - Line 396 (`level` get): `snprintf(buf, sizeof(buf), "Current log level: %d\n", (int)currentLogLevel);`
    - Line 405 (`level` set): `snprintf(buf, sizeof(buf), "Log level set to: %d\n", level);`
    - Line 417 (`filter`): `snprintf(buf, sizeof(buf), "Filtering logs by tag: %s\n", args.c_str());`
    - Lines 423-428 (`info`): Replace 6 `info +=` lines with single `snprintf(buf, sizeof(buf), "System Info:\n  Uptime: %lus\n  Free Heap: %lu bytes\n  Chip: %s Rev%d\n  CPU Freq: %lu MHz\n  WiFi: %s (%s)\n  RSSI: %d dBm\n", (unsigned long)(HAL::Platform::getMillis()/1000), (unsigned long)HAL::getFreeHeap(), ...);`
    - Line 434 (`heap`): `snprintf(buf, sizeof(buf), "Free Heap: %lu bytes\n", (unsigned long)HAL::getFreeHeap());`
    - Line 549 (`handleClient` error): **Note: this is in `handleClient()` body, NOT in a command lambda.** Use `snprintf(buf, sizeof(buf), "Unknown command: %s (type 'help' for commands)", cmd.c_str()); client.println(buf);` — do NOT return String(buf).
  - Notes: Command lambdas return `String(buf)`. Line 549 uses `client.println(buf)` directly. Buffer size 128 for single-line responses. **Info command uses `char buf[384]`** (7 lines with device name + SSID can exceed 256). **Format specifiers**: use `(unsigned long)` casts with `%lu` for `uint32_t`/`size_t` values to ensure portability across ESP32 toolchains.

- [x] Task 8: Refactor `System::getSystemStatus()` and `getBootDiagnostics()`
  - File: `DomoticsCore-System/include/DomoticsCore/System.h`
  - Action:
    1. `getSystemStatus()` (lines 527-533): Replace 5 String concat lines with:
       ```cpp
       char buf[256];
       snprintf(buf, sizeof(buf), "System Status:\n  Device: %s v%s\n  Uptime: %lus\n  Free Heap: %lu bytes\n  State: %s\n",
                config.deviceName.c_str(), config.firmwareVersion.c_str(),
                (unsigned long)(HAL::getMillis() / 1000), (unsigned long)HAL::getFreeHeap(), systemStateToString(state));
       return String(buf);
       ```
    2. `getBootDiagnostics()` (lines 558-577): Replace 10+ String concat lines with snprintf into `char buf[512]` (larger buffer due to boot data + persisted values).
    3. WiFi AP SSID (line 250): Replace with:
       ```cpp
       char apBuf[64];
       snprintf(apBuf, sizeof(apBuf), "%s-%08X", config.deviceName.c_str(), (uint32_t)(chipid >> 32));
       apSSID = apBuf;
       ```
  - Notes: `getBootDiagnostics` needs 512 due to multi-section output. Single snprintf may need splitting if >512 chars. **HAL namespace**: System.h uses `HAL::getMillis()` / `HAL::getFreeHeap()` while RemoteConsole.h uses `HAL::Platform::getMillis()`. Preserve the existing qualified names in each file — do NOT harmonize during R7 refactoring.

- [x] Task 9: Refactor `Storage::getStorageInfo()` and `dumpContents()`
  - File: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
  - Action:
    1. `getStorageInfo()` (lines 421-431): Replace 6 String concat lines with snprintf. **IMPORTANT**: Preserve the existing `if (isOpen)` guard — the "Registered keys" and "Stored values" lines are only appended when storage is open. Calling `getStoredKeyCount()` on closed storage can crash.
       ```cpp
       char buf[256];
       int pos = snprintf(buf, sizeof(buf), "Storage: HAL PlatformStorage\nNamespace: %s\nOpen: %s\nRead-only: %s",
                storageConfig.namespace_name.c_str(), isOpen ? "Yes" : "No",
                storageConfig.readOnly ? "Yes" : "No");
       if (isOpen) {
           snprintf(buf + pos, sizeof(buf) - pos, "\nRegistered keys: %lu\nStored values: %lu",
                    (unsigned long)registeredKeys.size(), (unsigned long)storedCount);
       }
       return String(buf);
       ```
    2. `dumpContents()` (lines 475-521): Replace per-field String concat in loop with snprintf per entry:
       ```cpp
       char entryBuf[128];
       // For each key, snprintf the formatted line into entryBuf, then result += entryBuf;
       ```
  - Notes: `dumpContents` still accumulates into `String result` (variable-length output cannot use fixed buffer). The improvement is eliminating per-field `+` concatenation inside the loop — each iteration does one `snprintf` + one `result +=` instead of 5-6 `+=` calls. This is a deliberate compromise: we reduce allocations per iteration, not total allocations. **Fix existing bug**: Line 499 truncates `uint64_t` to `unsigned long` (32-bit on ESP32). Use `%llu` with `(unsigned long long)` cast for the `'u'` type branch in the snprintf refactoring.

### Acceptance Criteria

#### M15 — Storage Change Events

- [x] AC-1: Given a Storage component with EventBus, when `putString("key", "value")` is called successfully, then a `storage/changed` event is emitted with `StorageChangedEvent.key == "key"`.
- [x] AC-2: Given a Storage component, when `putString("key", "value")` is called twice with the same value, then the `storage/changed` event is emitted only ONCE (dirty check prevents duplicate).
- [x] AC-3: Given a Storage component, when `putInt("temp", 42)` is called and then `putInt("temp", 99)` is called, then two `storage/changed` events are emitted (value changed).
- [x] AC-4: Given a Storage component, when `remove("key")` is called successfully, then a `storage/changed` event is emitted with `StorageChangedEvent.key == "key"`.
- [x] AC-5: Given a Storage component, when `clear()` is called, then a `storage/changed` event is emitted with `StorageChangedEvent.key == "*"`.
- [x] AC-6: Given a key longer than 63 characters, when `putString(longKey, "val")` is called, then the event is emitted with a truncated key (no crash, no buffer overflow).
- [x] AC-7a: Given `putULong64("counter", 100)`, when the value is read back with `getULong64("counter")`, then it returns `100` (cache gap fix — storage round-trip).
- [x] AC-7b: Given `putULong64("counter", 100)` called twice, when the second call is made with the same value, then only ONE `storage/changed` event is emitted (cache gap fix — dirty check works for uint64).
- [x] AC-8: Given a `put*` call before `begin()` (HAL not initialized), when the operation returns `false`, then NO `storage/changed` event is emitted.

#### R7 — snprintf in Hot Paths

- [x] AC-9: Given the RemoteConsole `formatLogEntry()` method, when a log entry is formatted, then no `String` concatenation using `+` operator occurs (only `snprintf` + `char[]` buffer).
- [x] AC-10: Given the RemoteConsole `info` command, when executed, then the output contains the same information (uptime, heap, chip, WiFi) as before the refactor.
- [x] AC-11: Given the System `getSystemStatus()` method, when called, then the returned string contains device name, version, uptime, heap, and state — formatted identically to pre-refactor output.
- [x] AC-12: Given the System WiFi AP SSID generation, when called, then the SSID follows the pattern `{deviceName}-{hexChipId}` using snprintf.
- [x] AC-13: Given `Storage::getStorageInfo()`, when called, then the output contains namespace, open state, read-only state, key counts — using snprintf.
- [x] AC-14: Given all R7 refactored methods, when compiled, then no `String()` constructor calls for integer/float conversion remain in the modified methods.

#### Traceability — M11/M19

- [x] AC-15: Given `Core::emit(topic, payload)`, when called without sticky param, then `sticky` defaults to `false` (already implemented — verify via code inspection).
- [x] AC-16: Given `HomeAssistant::addSwitch()`, when called, then `ha/entity_added` event is emitted with component="switch" (already implemented — verify via code inspection).

## Additional Context

### Dependencies

- No external dependencies. All changes are internal header modifications.
- Task 2 (putULong64 cache fix) must complete before Task 3 (dirty check requires working cache).
- Task 1 (StorageChangedEvent struct) must complete before Tasks 3 and 4 (emit calls reference it).
- R7 tasks (6-9) are independent of M15 tasks (1-5) and can be parallelized.

### Testing Strategy

**Unit Tests (Task 5):**
- Extend `test_storage_events.cpp` with 12 new test cases for `storage/changed` event
- Pattern: subscribe to event → perform operation → `testCore->loop()` → assert event received/not received
- Use static counters to verify emission count (especially for dirty check tests)
- Test key truncation with 65+ char keys

**Compilation Verification (R7):**
- R7 changes are internal refactoring — no behavioral change
- Verify via: `pio run` across all target environments
- Manual spot-check: connect RemoteConsole, run `info`, `heap`, `level` commands, verify identical output

**Memory Stability (Mandatory on ESP32):**
- Use existing `HeapTracker` pattern from `test_storage_api.cpp` to verify M15 emit calls don't leak memory
- Run put/emit cycle 100x, assert heap stable within tolerance (512 bytes)
- Each `emit` creates a `QueuedEvent` with `std::vector<uint8_t>` — repeated calls can fragment heap if events are not consumed

### Notes

**Traceability:**
- M11 was fixed in commit `ab026e2` (batch R9/R17/R19/R22/R23/R25) — `Core::emit` sticky param at Core.h:163-168
- M19 was fixed in the same batch via `HAEntityAddedEvent` struct pattern — all 6 `add*` methods at HomeAssistant.h:144-275
- This spec covers the remaining 2 actionable items (M15, R7) plus traceability for the 2 already-done items

**High-Risk Items:**
- **Re-entrance**: If a subscriber calls `put*` inside a `storage/changed` handler, infinite loop. Mitigated by convention (documented), not by code guard.
- **R7 Output Parity**: snprintf refactoring must produce identical output strings. Risk of subtle formatting differences (e.g., float precision, hex casing). Mitigate with manual spot-check.
- **getBootDiagnostics buffer**: 512 bytes on stack is significant for ESP32. Monitor stack usage if the method is called in a tight context.

**Known Limitations:**
- **Dirty check only works within a single power cycle.** After reboot, cache is empty and the first `put*` for each key will always emit `storage/changed`, even if the value matches what's already persisted in NVS/LittleFS. This causes unnecessary flash writes and spurious events on boot. Pre-populating cache from getters on `begin()` would fix this but is out of scope (follow-up optimization).
- `putBlob` always emits (no dirty check) — blob comparison is too expensive for the benefit.
- `clear()` emits a single event with key="*" — subscribers cannot distinguish which keys were cleared.
- Getters still bypass cache (out of scope — documented in project-context.md as known gap).

### Party Mode Insights

**Session 1 (Step 1 — Scope Review):**
Collaborative review by Winston (Architect), Amelia (Dev), Quinn (QA), Murat (TEA):
- **Dirty check** on M15 `put*` methods — consensus to compare before emitting (prevents loop() spam)
- **Re-entrance risk** — subscribers must not `put*` same key in handler (no EventBus guard)
- **Buffer sizing** — 256 for multi-line info, 128 for single-line log
- **Storage.h R7** — already mostly clean, minimal refactoring needed
- **Test coverage** — dirty check (no-emit on same value), re-entrance scenario, key truncation >64 chars

**Session 2 (Step 4 — Spec Review):**
10 findings applied (F1-F10):
- F1: Float `==` comparison — accept false positives (no epsilon complexity)
- F2: Explicit operation order: check → HAL write → cache update → emit
- F3: `StorageChangedEvent` in `DomoticsCore::StorageEvents` namespace
- F4: `StorageEntry` fields verified (stringValue, intValue, floatValue, boolValue, blobValue) — **no uint64 field exists** → Task 2 must add `uint64Value`
- F5: Info command buffer 384 (was 128, too small for 7-line output)
- F6: `dumpContents` compromise documented explicitly
- F7: AC-7 split into 7a (round-trip) + 7b (dirty check for uint64)
- F8: Added tests for putFloat, putBool, putULong64, putBlob (was missing)
- F9: AC-8 clarified: put* before begin() = HAL failure simulation
- F10: Memory stability test upgraded from optional to mandatory

**Adversarial Review (Step 4):**
10 findings identified and applied:
- F1 [Critical]: Fixed file paths — RemoteConsole.h in `DomoticsCore-RemoteConsole/`, System.h in `DomoticsCore-System/`
- F2 [High]: Added `UInt64` to `StorageValueType` enum in Task 2
- F3 [High]: Added `.c_str()` on `logLevelToString()` return in Task 6 snprintf
- F4 [High]: Documented dirty check reboot limitation (cache empty after power cycle)
- F5 [Medium]: Corrected "11 MUST FIX" to "7 MUST FIX locations" for RemoteConsole
- F6 [Medium]: Updated Testing Strategy from "6 test cases" to "12 test cases"
- F7 [Medium]: Included ANSI color code prefix/reset in Task 6 snprintf (was ignored)
- F8 [Medium]: Added explicit `StorageChangedEvent*` cast pattern in Task 5 (different from EVENT_READY `String*` pattern)
- F9 [Low]: Added HAL namespace preservation note (`HAL::` vs `HAL::Platform::`)
- F10 [Low]: Clarified `StorageEntry` is NOT POD, only `StorageChangedEvent` is POD

**Adversarial Review Round 2 (Step 4):**
9 findings identified and applied:
- F1 [High]: `getColorForLevel()` doesn't exist — added explicit helper extraction sub-step in Task 6
- F2 [High]: Format specifier `%lu` for `uint32_t` — added `(unsigned long)` casts throughout all snippets
- F3 [High]: Task 9 `getStorageInfo()` dropped `if (isOpen)` guard — restored with split snprintf
- F4 [Medium]: `help` command excluded from R7 — added explicit justification (cold path)
- F5 [Medium]: Line 549 is in `handleClient()` not a lambda — corrected to `client.println(buf)`
- F6 [Medium]: `dumpContents` uint64 truncation bug — added `%llu` + `(unsigned long long)` cast note
- F7 [Medium]: Dirty check `return true` without HAL write — callers verified, semantics documented
- F8 [Low]: `StorageEntry` constructor — added `uint64Value(0)` to initializer list instruction
- F9 [Low]: `%zu` portability — standardized on `(unsigned long)` + `%lu`, avoid `%zu`

## Review Notes

- Adversarial review completed during implementation
- Findings: 10 total, 7 fixed, 3 skipped (by design)
- Resolution approach: auto-fix
- F1 [High] Fixed: Dirty check now validates StorageValueType before comparing value fields
- F3 [High] Fixed: formatLogEntry uses dynamic fallback for messages exceeding 256-byte buffer
- F5/F6 [Medium] Fixed: Clamped snprintf return value in chained calls (getStorageInfo, getBootDiagnostics)
- F8 [Low] Fixed: dumpContents entryBuf increased to 256 bytes
- F9 [Low] Fixed: Added dirty-check tests for putInt, putFloat, putBool
- Skipped F2 (putBlob no dirty check — spec decision), F4 (telnet truncation — acceptable), F7 (float == — spec decision), F10 (noise)
