---
title: 'Roadmap Batch - R9, R17, R19, R22, R23, R25: Dead Code Removal, HAL Fix, Enum Refactor'
slug: 'roadmap-r9-r17-r19-r22-r23-r25'
created: '2026-03-07'
status: 'completed'
stepsCompleted: [1, 2, 3, 4]
tech_stack:
  - C++ (header-only, Arduino/PlatformIO)
  - ESP32/ESP32-C3/ESP8266
  - PlatformIO native test runner (Unity)
files_to_modify:
  - DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h
  - DomoticsCore-MQTT/include/DomoticsCore/MQTT.h
  - DomoticsCore-MQTT/README.md
  - DomoticsCore-MQTT/SPECIFICATIONS.md
  - docs/components/mqtt/technical-reference.md
  - DomoticsCore-NTP/include/DomoticsCore/NTP.h
  - DomoticsCore-NTP/test/test_ntp_component/test_ntp_component.cpp
  - DomoticsCore-NTP/README.md
  - DomoticsCore-LED/include/DomoticsCore/LED.h
  - docs/components/led/technical-reference.md
  - docs/components/led/project-context.md
  - docs/components/ntp/technical-reference.md
  - docs/components/ntp/project-context.md
  - DomoticsCore-Storage/include/DomoticsCore/Storage.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h
code_patterns:
  - 'Non-blocking timer pattern (flag + millis check in loop)'
  - 'HAL abstraction (HAL::Platform::getMillis(), HAL::restart())'
  - 'enum class with constexpr bitwise operator overloads'
  - 'Utils::NonBlockingDelay in Timer.h (alternative timer pattern)'
  - 'Existing flag pattern: bool connectionInfoDisplayed in RemoteConsole.h:79'
test_patterns:
  - 'HeapTracker for memory assertions'
  - 'HAL mock stubs for platform calls'
  - 'Component-level unit tests in DomoticsCore-*/test/'
  - 'Unity test framework (TEST_ASSERT_* macros)'
  - 'Multi-cycle loop tests: for(i<10) { testCore->loop(); }'
---

# Tech-Spec: Roadmap Batch - R9, R17, R19, R22, R23, R25

**Created:** 2026-03-07

## Overview

### Problem Statement

Six roadmap items remain open from the adversarial code review. They span three categories: (1) a blocking `HAL::delay(100)` in the RemoteConsole reboot handler violating constitution Principle X (R9), (2) four dead code artifacts — an unimplemented `isValidTopic()` declaration (R17), an unused `retryDelayMs` config field (R19), an unused `effectDirection` state field (R22), and a fully-commented `#if DOMOTICSCORE_WEBUI_ENABLED` block (R23/R10) — violating constitution Principle IV (YAGNI), and (3) an unscoped `enum AlarmFeature` leaking names like `Trigger` into the namespace (R25), violating modern C++ best practices.

### Solution

Address all six items in a single batch: replace the blocking delay with a non-blocking flag+timer pattern, remove all dead code artifacts, and convert the unscoped enum to `enum class` with constexpr bitwise operator overloads.

### Scope

**In Scope:**

- R9: RemoteConsole blocking delay -> non-blocking reboot pattern
- R17: Remove `isValidTopic()` declaration from MQTT.h
- R19: Remove `retryDelayMs` from NTPConfig
- R22: Remove `effectDirection` from LEDState
- R23: Remove `#if DOMOTICSCORE_WEBUI_ENABLED` block from Storage.h (also fixes R10)
- R25: Convert `enum AlarmFeature` to `enum class AlarmFeature` with operator overloads

**Out of Scope:**

- R6, R7: Remaining memory safety items (String fields, logging hot paths)
- R11-R13: File size splits (WebUI, StreamingContextSerializer, Wifi)
- R18, R20, R21: Dead code items requiring implementation (MQTT limits, OTA password wiring, RemoteConsole auth)
- R24, R26: Progressive refactoring (HA virtual dispatch, EventBus command emission)

## Context for Development

### Codebase Patterns

- **Non-blocking timer pattern**: Set a flag + timestamp, check elapsed time in `loop()`. Used throughout the framework per constitution Principle X. `Utils::NonBlockingDelay` (Timer.h) is available but for this case a simple flag+millis is lighter weight.
- **HAL abstraction**: All platform calls go through `HAL::` namespace. `HAL::delay()`, `HAL::restart()`, `HAL::Platform::getMillis()`. HAL includes: `Platform_HAL.h`, `Wifi_HAL.h`, `WiFiServer_HAL.h`.
- **Header-only design**: Most components are `.h` files only. Changes are made directly in headers.
- **Constitution compliance**: All changes must comply with `.specify/memory/constitution.md`. Key principles: no `delay()` outside boot (X), YAGNI (IV), HAL isolation (IX).
- **Existing flag pattern**: `RemoteConsole.h:79` already uses `bool connectionInfoDisplayed` with a check in `loop()` at line 175 — serves as a model for R9.

### Files to Reference

| File | Purpose |
| ---- | ------- |
| `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` | R9: Reboot handler lines 429-437. Private members at line 64. `loop()` at line 171. |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h` | R17: `isValidTopic()` declaration + doxygen at lines 369-377. `topicMatches()` at line 385. |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h` | R17: Confirm no `isValidTopic()` body. `topicMatches()` impl at lines 486-524. |
| `DomoticsCore-MQTT/README.md` | R17: Reference at lines 254-255 (Helper Functions code block). |
| `DomoticsCore-MQTT/SPECIFICATIONS.md` | R17: Reference at line 188 (Section 4.2). |
| `docs/components/mqtt/technical-reference.md` | R17: Reference at lines 512-517 (Static Utility Methods, includes linker warning). |
| `DomoticsCore-NTP/include/DomoticsCore/NTP.h` | R19: `retryDelayMs` at line 39 in `NTPConfig` struct (lines 33-40). |
| `DomoticsCore-NTP/test/test_ntp_component/test_ntp_component.cpp` | R19: Assertions at lines 75, 414, 420. |
| `DomoticsCore-NTP/README.md` | R19: Config doc at line 126. |
| `DomoticsCore-LED/include/DomoticsCore/LED.h` | R22: `effectDirection` at line 72 in `LEDState` struct (lines 62-73). |
| `DomoticsCore-Storage/include/DomoticsCore/Storage.h` | R23: `#if DOMOTICSCORE_WEBUI_ENABLED` block at lines 588-646. Line 647 is `};` (class close). |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h` | R25: `enum AlarmFeature` at lines 12-19. Bitwise `&` usage at lines 102-117. Field at line 71. |
| `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` | R25: `addAlarmControlPanel()` default param at line 254: `uint8_t features = AlarmFeature::ArmAway`. |
| `DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp` | R25: OR operations at lines 61, 128, 137-138, 358, 446. No modification needed. |
| `DomoticsCore-HomeAssistant/examples/BasicHA/src/main.cpp` | R25: OR operation at line 197. No modification needed — operators handle it. |

### Technical Decisions

- **R9**: Flag+timer pattern chosen over `Utils::NonBlockingDelay` (simpler, one-shot). Add `bool rebootPending = false` + `unsigned long rebootRequestedAt = 0` to private members. In handler: set flags after println. In `loop()`: check `if (rebootPending && (HAL::Platform::getMillis() - rebootRequestedAt >= 100))` then `HAL::restart()`. **CRITICAL**: The reboot check MUST be placed BEFORE the status guard (`if (getLastStatus() != ... ) return;`) at line 172, otherwise a component in error state can never reboot via telnet. TCP stack flushes naturally during continued loop cycles.
- **R17**: Suppression chosen over implementation. `topicMatches()` covers the existing wildcard matching use case. Remove declaration + doxygen (MQTT.h:369-377) + all API doc references (README, SPECIFICATIONS, technical-reference). Also update README feature list (line 15): change "Topic validation and wildcard matching" to "Topic wildcard matching". Do NOT modify tracking docs (CODE-ROADMAP.md, REVIEW-FINDINGS.md) — those are historical records, just update status.
- **R19**: Suppression chosen over wiring. NTP sync works fine without configurable retry delay; YAGNI. Remove field + update 3 test assertions + README.
- **R22**: Pure dead field removal — never read or written anywhere. Single line delete.
- **R23**: Full block removal (lines 588-646) — code inside was already commented out, and the `#if` guard violates HAL isolation (R10). Block is followed by `};` class close.
- **R25**: `enum class` with `constexpr` operator overloads. Need THREE overloads: (1) `operator|(AlarmFeature, AlarmFeature) -> uint8_t` for combining two flags, (2) `operator|(uint8_t, AlarmFeature) -> uint8_t` for chaining (e.g., `ArmHome | ArmAway | Trigger` — first `|` returns uint8_t, subsequent ones need uint8_t|AlarmFeature), (3) `operator&(uint8_t, AlarmFeature) -> uint8_t` for testing flags. Default param in HomeAssistant.h:254 needs `static_cast<uint8_t>(AlarmFeature::ArmAway)`. All internal bitwise usage (lines 102-117) and test chaining patterns (line 137) covered by these 3 overloads.

## Implementation Plan

### Tasks

#### R22 — Remove `effectDirection` dead field from LEDState

- [x] Task 1: Remove `effectDirection` field from `LEDState` struct
  - File: `DomoticsCore-LED/include/DomoticsCore/LED.h`
  - Action: Delete line 72 (`bool effectDirection = true;`)
  - Notes: No test changes needed. LED tests (`test_led_types.cpp`) do not reference this field.

- [x] Task 1b: Remove `effectDirection` references from LED documentation
  - File: `docs/components/led/technical-reference.md`
  - Action: Remove line 112 (`bool effectDirection = true;` in struct listing) and line 116 (the `> **Dead code notice:**` blockquote paragraph about `effectDirection`). Also remove any blank lines that would leave a double gap.
  - File: `docs/components/led/project-context.md`
  - Action: Remove line 119 (item 8: "`effectDirection` is dead code" warning). Renumber remaining list items if needed (items after 8 shift down).

#### R19 — Remove `retryDelayMs` unused config field from NTPConfig

- [x] Task 2: Remove `retryDelayMs` field from `NTPConfig` struct
  - File: `DomoticsCore-NTP/include/DomoticsCore/NTP.h`
  - Action: Delete line 39 (`uint32_t retryDelayMs = 5000;    // Retry delay on failure`)

- [x] Task 3: Remove `retryDelayMs` assertions from NTP tests
  - File: `DomoticsCore-NTP/test/test_ntp_component/test_ntp_component.cpp`
  - Action: Delete line 75 (`TEST_ASSERT_EQUAL_UINT32(5000, config.retryDelayMs);`), delete line 414 (`newConfig.retryDelayMs = 15000;`), delete line 420 (`TEST_ASSERT_EQUAL_UINT32(15000, cfg.retryDelayMs);`)

- [x] Task 4: Remove `retryDelayMs` from NTP README config example
  - File: `DomoticsCore-NTP/README.md`
  - Action: Delete line 126 (`uint32_t retryDelayMs = 5000;           // Retry delay on failure`)

- [x] Task 4b: Remove `retryDelayMs` references from NTP documentation
  - File: `docs/components/ntp/technical-reference.md`
  - Action: Remove line 46 (`uint32_t retryDelayMs = 5000;` in config struct listing) and remove line 57 (the full markdown table row for `retryDelayMs`). Clean up any resulting double blank lines.
  - File: `docs/components/ntp/project-context.md`
  - Action: (a) At line 76, remove `, retryDelayMs` from the field list AND remove the trailing sentence `**Note**: retryDelayMs is declared but not enforced at runtime -- see "Areas for Future Attention" below.` (b) Remove line 175 only (the single bullet point `- **retryDelayMs field (C13 -- CRITICAL)**: ...`). Do NOT delete lines after 175 — the rest of the file must be preserved.

#### R17 — Remove `isValidTopic()` dead declaration from MQTT

- [x] Task 5: Remove `isValidTopic()` declaration and doxygen block
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h`
  - Action: Delete lines 369-377 (doxygen comment + `static bool isValidTopic(...)` declaration)

- [x] Task 6: Remove `isValidTopic()` from MQTT README
  - File: `DomoticsCore-MQTT/README.md`
  - Action: (a) Update feature list line 15: change "Topic validation and wildcard matching" to "Topic wildcard matching". (b) Remove lines 254-255 (the `isValidTopic` code example in Helper Functions section).

- [x] Task 7: Remove `isValidTopic()` from MQTT SPECIFICATIONS
  - File: `DomoticsCore-MQTT/SPECIFICATIONS.md`
  - Action: (a) Remove lines 187-188 (line 187: `// Topic validation` comment, line 188: `bool isValidTopic(...)` declaration) in Section 4.2 Helper Functions. Preserve the surrounding `topicMatches` entry. (b) At line 29, change `- **Topic validation** before publishing` to `- **Topic wildcard matching** for subscriptions` (or remove the line if topic validation is no longer a feature). (c) At line 475 in Section 12.1, remove or update `- Topic validation and matching` to `- Topic wildcard matching`.

- [x] Task 8: Remove `isValidTopic()` from MQTT technical reference
  - File: `docs/components/mqtt/technical-reference.md`
  - Action: Remove lines 512-517 (Static Utility Methods section entry for `isValidTopic`, including linker warning)

#### R23 — Remove dead `#if DOMOTICSCORE_WEBUI_ENABLED` block from Storage (also fixes R10)

- [x] Task 9: Remove entire `#if DOMOTICSCORE_WEBUI_ENABLED` block
  - File: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
  - Action: Delete lines 588-646 (from `#if DOMOTICSCORE_WEBUI_ENABLED` through `#endif`). Also remove any trailing blank line before the block to avoid double blank line before `};`. Line 647 (`};`) is the class closing brace and MUST be preserved.
  - Notes: All code inside the block is already commented out. This also resolves R10 (HAL isolation violation).

#### R9 — Replace blocking delay with non-blocking reboot pattern in RemoteConsole

- [x] Task 10: Add reboot pending state to private members
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
  - Action: Add after line 79 (`bool connectionInfoDisplayed = false;`):
    ```cpp
    bool rebootPending = false;
    unsigned long rebootRequestedAt = 0;
    ```

- [x] Task 11: Add non-blocking reboot check at TOP of `loop()`
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
  - Action: Insert after line 171 (`void loop() override {`), BEFORE the status guard at line 172:
    ```cpp
    // Non-blocking reboot (R9 — must be before status guard)
    if (rebootPending && (HAL::Platform::getMillis() - rebootRequestedAt >= 100)) {
        HAL::restart();
    }
    ```
  - Notes: MUST be before `if (getLastStatus() != ComponentStatus::Success || !telnetServer) return;` so reboot works even when component is in error state. The `unsigned long` subtraction pattern is safe across `millis()` wraparound (~49.7 days) due to unsigned integer arithmetic — do NOT add extra overflow handling. If `reboot` is issued twice, the timer simply resets — this is acceptable behavior (device still reboots 100ms after last command).

- [x] Task 12: Replace blocking delay in reboot handler with flag setting
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
  - Action: Replace the reboot handler body (lines 430-436) from:
    ```cpp
    registerCommand("reboot", [this](const String& args) {
        for (auto& client : clients) {
            client.println("Rebooting...");
        }
        HAL::delay(100);
        HAL::restart();
        return "";
    });
    ```
    to:
    ```cpp
    registerCommand("reboot", [this](const String& args) {
        for (auto& client : clients) {
            client.println("Rebooting...");
        }
        rebootRequestedAt = HAL::Platform::getMillis();
        rebootPending = true;
        return "";
    });
    ```

#### R25 — Convert `enum AlarmFeature` to `enum class` with operator overloads

- [x] Task 13: Convert enum, add operator overloads, and update default initializer
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h`
  - Action (a): Replace lines 12-19:
    ```cpp
    enum AlarmFeature : uint8_t {
    ```
    with:
    ```cpp
    enum class AlarmFeature : uint8_t {
    ```
  - Action (b): Add immediately after the closing `};` of the enum (after line 19):
    ```cpp

    // Bitwise operators for AlarmFeature bitmask usage
    constexpr uint8_t operator|(AlarmFeature a, AlarmFeature b) {
        return static_cast<uint8_t>(a) | static_cast<uint8_t>(b);
    }
    constexpr uint8_t operator|(uint8_t a, AlarmFeature b) {
        return a | static_cast<uint8_t>(b);
    }
    constexpr uint8_t operator&(uint8_t a, AlarmFeature b) {
        return a & static_cast<uint8_t>(b);
    }
    ```
  - Action (c): At line 71, change `uint8_t supportedFeatures = AlarmFeature::ArmAway;` to `uint8_t supportedFeatures = static_cast<uint8_t>(AlarmFeature::ArmAway);`
  - Notes: Without action (c), `enum class` implicit conversion to `uint8_t` fails and compilation breaks. **Limitation**: `AlarmFeature | uint8_t` (enum on left, uint8_t on right) is NOT covered by the overloads — only `uint8_t | AlarmFeature` works. This is acceptable because all existing code chains left-to-right starting with two AlarmFeature values.

- [x] Task 14: Update `addAlarmControlPanel()` default parameter
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: At line 254, change:
    ```cpp
    uint8_t features = AlarmFeature::ArmAway,
    ```
    to:
    ```cpp
    uint8_t features = static_cast<uint8_t>(AlarmFeature::ArmAway),
    ```

#### Verification

- [x] Task 15: Run all existing tests to confirm no regressions
  - Action: Run `./run_all_tests.sh` (or equivalent PlatformIO native test command)
  - Notes: Covers LED, NTP, MQTT, Storage, HomeAssistant, RemoteConsole test suites. All tests must pass. No new tests needed for dead code removal (R17, R19, R22, R23). R9 and R25 are covered by existing test infrastructure — R25 regression is fully covered by `test_ha_alarm_panel.cpp` bitwise operations. R9 test coverage is optional (no existing reboot tests, and mock HAL may not track `restart()` calls).

### Acceptance Criteria

#### R9 — Non-blocking reboot

- [ ] AC-1: Given RemoteConsole is running, when the `reboot` command is issued, then `HAL::restart()` is NOT called immediately (no blocking delay).
- [ ] AC-2: Given RemoteConsole is running, when the `reboot` command is issued and `loop()` is called after 100ms, then `HAL::restart()` is called.
- [ ] AC-3: Given RemoteConsole is in error state (`getLastStatus() != Success`), when the `reboot` command was previously issued and 100ms have elapsed, then `HAL::restart()` is still called (reboot check is before status guard).
- [ ] AC-4: Given RemoteConsole is running with connected telnet clients, when the `reboot` command is issued, then all clients receive "Rebooting..." before the restart occurs.

#### R17 — isValidTopic removal

- [ ] AC-5: Given the MQTT component, when compiled, then no `isValidTopic` symbol exists in the compilation unit.
- [ ] AC-6: Given the MQTT README, when reading the feature list, then it says "Topic wildcard matching" (not "Topic validation and wildcard matching").
- [ ] AC-7: Given the MQTT API docs (README, SPECIFICATIONS, technical-reference), when searching for `isValidTopic`, then no references are found.

#### R19 — retryDelayMs removal

- [ ] AC-8: Given a default `NTPConfig`, when accessing its fields, then `retryDelayMs` does not exist (compilation error if referenced).
- [ ] AC-9: Given the NTP test suite, when all tests are run, then all tests pass without referencing `retryDelayMs`.

#### R22 — effectDirection removal

- [ ] AC-10: Given a default `LEDState`, when accessing its fields, then `effectDirection` does not exist (compilation error if referenced).
- [ ] AC-11: Given the LED test suite, when all tests are run, then all tests pass.
- [ ] AC-11b: Given LED documentation (`docs/components/led/technical-reference.md`, `docs/components/led/project-context.md`), when searching for `effectDirection`, then no references are found.

#### R19 — retryDelayMs documentation cleanup

- [ ] AC-9b: Given NTP documentation (`docs/components/ntp/technical-reference.md`, `docs/components/ntp/project-context.md`), when searching for `retryDelayMs`, then no references are found.

#### R23 — Storage WebUI block removal

- [ ] AC-12: Given `Storage.h`, when searching for `DOMOTICSCORE_WEBUI_ENABLED`, then no matches are found.
- [ ] AC-13: Given the Storage component, when compiled with or without WebUI, then compilation succeeds (no conditional dependency).

#### R25 — AlarmFeature enum class

- [ ] AC-14: Given `AlarmFeature::ArmHome` and `AlarmFeature::ArmAway`, when combined with `|`, then the result is a `uint8_t` with value `0x03`.
- [ ] AC-15: Given three `AlarmFeature` values `ArmHome | ArmAway | Trigger`, when chaining two `|` operators across three enum values, then compilation succeeds and the result is `uint8_t` with value `0x23`.
- [ ] AC-16: Given a `uint8_t` features bitmask, when testing with `& AlarmFeature::ArmAway`, then the bitwise AND returns a `uint8_t`.
- [ ] AC-17: Given the HA alarm panel test suite (`DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp`), when all tests are run, then all tests pass without modification (operator overloads are transparent).
- [ ] AC-18: Given the BasicHA example (`DomoticsCore-HomeAssistant/examples/BasicHA/src/main.cpp`), when compiled, then `AlarmFeature::ArmHome | AlarmFeature::ArmAway | AlarmFeature::Trigger` compiles and works correctly.

## Additional Context

### Dependencies

- No inter-item dependencies between R9/R17/R19/R22/R23. However, R25 Tasks 13 and 14 MUST be applied together — Task 13 actions (a), (b), (c) and Task 14 are tightly coupled. Applying Task 13(a) alone (enum class conversion) without 13(c) and 14 will break compilation.
- R23 removal also resolves R10 (HAL isolation violation in Storage.h).
- No external library changes required.

### Testing Strategy

- **Dead code removal (R17, R19, R22, R23)**: No new tests needed. Existing test suites validate that removal doesn't break anything. Run full test suite after changes.
- **R9 (non-blocking reboot)**: Existing test file `test_remoteconsole_component.cpp` has no reboot tests. Adding a test is optional — the mock HAL may not track `restart()` calls. Manual verification: build, flash, issue `reboot` via telnet, confirm device reboots after ~100ms delay.
- **R25 (enum class)**: Existing `test_ha_alarm_panel.cpp` provides full regression coverage for all bitwise patterns. No new tests needed — if existing tests compile and pass, the operator overloads are correct.
- **Full regression**: Run `./run_all_tests.sh` after all changes.

### Notes

- Party Mode consensus (Round 1): All agents agreed on suppression for R17/R19/R22/R23 and flag+timer for R9.
- Party Mode review (Round 2): Three corrections integrated — (1) R9 reboot check before status guard, (2) R25 needs 3 operator overloads not 2, (3) R17 README feature list line 15 needs update.
- R25 is a minor breaking change for external code using unqualified `AlarmFeature` values (e.g., bare `ArmAway` instead of `AlarmFeature::ArmAway`), but all internal usage already uses qualified names.
- Tracking docs (CODE-ROADMAP.md, REVIEW-FINDINGS.md) are NOT modified — they are historical records. Status updates happen separately.

## Review Notes

- Adversarial review completed
- Findings: 12 total, 2 fixed, 1 acknowledged, 9 noise
- Resolution approach: auto-fix
- F5 (Medium/Real): Added `rebootPending = false` before `HAL::restart()` to prevent infinite restart loop in mock environments
- F9 (Low/Real): Added explicit `inline` to `constexpr` operator overloads for C++14 ODR safety
- F10 (Low/Real): Acknowledged — no reboot test added per tech-spec (mock HAL may not track `restart()` calls)
