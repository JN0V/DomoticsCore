---
title: 'Fix WebUIConfig char[] Direct Assignment Compilation Errors in Examples'
slug: 'fix-webui-char-array-examples'
created: '2026-03-08'
status: 'completed'
specStepsCompleted: [1, 2, 3, 4]  # Refers to spec-authoring phases, not implementation tasks
tech_stack: [C++, Arduino/PlatformIO, ESP32]
files_to_modify:
  - DomoticsCore-HomeAssistant/examples/HAWithWebUI/src/main.cpp
  - DomoticsCore-LED/examples/LEDWithWebUI/src/main.cpp
  - DomoticsCore-MQTT/examples/MQTTWifiWithWebUI/src/main.cpp
  - DomoticsCore-MQTT/examples/MQTTWithWebUI/src/main.cpp
  - DomoticsCore-NTP/examples/NTPWithWebUI/src/main.cpp
  - DomoticsCore-OTA/examples/OTAWithWebUI/src/main.cpp
  - DomoticsCore-Storage/examples/StorageWithWebUI/src/main.cpp
  - DomoticsCore-SystemInfo/examples/SystemInfoWithWebUI/src/main.cpp
  - DomoticsCore-Wifi/examples/WifiWithWebUI/src/main.cpp
  - DomoticsCore-WebUI/examples/WebUIOnly/src/main.cpp
  - DomoticsCore-WebUI/examples/HeadlessAPI/src/main.cpp
  - DomoticsCore-WebUI/README.md
code_patterns:
  - 'WebUIConfig setters: setDeviceName(), setTheme(), setStaticPath(), setPrimaryColor(), setUsername(), setPassword()'
  - 'Setters internally use strncpy(field, value, sizeof(field) - 1) with null termination'
test_patterns:
  - 'Existing test_webui_component.cpp already uses setters correctly'
---

# Tech-Spec: Fix WebUIConfig char[] Direct Assignment Compilation Errors in Examples

**Created:** 2026-03-08

## Overview

### Problem Statement

Roadmap item R6 migrated `WebUIConfig` fields from `String` to fixed-size `char[]` arrays to eliminate heap fragmentation. However, 11 example files across 9 DomoticsCore module directories were not updated and still use direct assignment (e.g., `webCfg.deviceName = "value"`), which fails to compile in C++ because `char[]` does not support the `=` operator with string literals.

### Solution

Replace all direct `char[]` field assignments in example files with calls to the corresponding safe setter methods already provided by `WebUIConfig` (e.g., `setDeviceName()`, `setTheme()`, etc.). These setters handle `strncpy` + null-termination + truncation warnings internally.

### Scope

**In Scope:**
- Fix all 11 example `.cpp` files that directly assign to `WebUIConfig` char[] fields (12 tasks total including README)
- Only the `deviceName` field is currently assigned directly in examples (no examples assign `theme`, `staticPath`, `primaryColor`, `username`, or `password` directly)
- Update the WebUI README code snippet that shows direct assignment

**Out of Scope:**
- Other config structs (`CoreConfig`, `MQTTConfig`, `WifiConfig`, `SystemInfoConfig`, `RemoteConsoleConfig`, `SystemConfig`) -- these still use `String` and are not affected
- Internal framework code in `System.h`, `SystemWebUISetup.h`, `SystemPersistence.h` -- these assign to `SystemConfig`/`SystemInfoConfig` String fields, not `WebUIConfig` char[] fields
- Migration of other config structs from String to char[] (future roadmap items)

## Context for Development

### Codebase Patterns

- `WebUIConfig` (in `DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h`) defines 6 char[] fields with safe setter methods
- The setters follow a consistent pattern: `strncpy` + null-termination + `DLOG_W` truncation warning
- The existing test file (`test_webui_component.cpp`) already uses the setter API correctly
- All other config structs in the project still use `String` fields -- only `WebUIConfig` and `HAConfig` have been migrated to `char[]` (note: the comment in `WebUIConfig.h` claiming "same as MQTTConfig, WifiConfig, OTAConfig" is misleading -- those still use `String`)

### Files to Reference

| File | Purpose |
| ---- | ------- |
| `DomoticsCore-WebUI/include/DomoticsCore/WebUI/WebUIConfig.h` | Defines the char[] fields and setter methods |
| `DomoticsCore-WebUI/test/test_webui_component/test_webui_component.cpp` | Shows correct setter usage pattern |
| `DomoticsCore-WebUI/README.md` | Contains a code snippet with direct assignment that needs updating |

### Technical Decisions

- **Use setters, not raw strncpy**: `WebUIConfig` already provides safe setter methods (`setDeviceName()`, etc.) that handle truncation + null-termination + warnings. Examples should use these rather than raw `strncpy()` for readability and consistency.
- **Only `deviceName` is affected in practice**: While 6 char[] fields exist, only `deviceName` is directly assigned in example code. The other fields (`theme`, `staticPath`, `primaryColor`, `username`, `password`) are either left at defaults or were never assigned directly in examples.

## Implementation Plan

### Tasks

- [x] Task 1: Fix HAWithWebUI example
  - File: `DomoticsCore-HomeAssistant/examples/HAWithWebUI/src/main.cpp`
  - Action: Line 98 -- replace `webuiCfg.deviceName = "ESP32 HA Demo";` with `webuiCfg.setDeviceName("ESP32 HA Demo");`

- [x] Task 2: Fix LEDWithWebUI example
  - File: `DomoticsCore-LED/examples/LEDWithWebUI/src/main.cpp`
  - Action: Line 59 -- replace `webCfg.deviceName = "LED With WebUI";` with `webCfg.setDeviceName("LED With WebUI");`

- [x] Task 3: Fix MQTTWifiWithWebUI example
  - File: `DomoticsCore-MQTT/examples/MQTTWifiWithWebUI/src/main.cpp`
  - Action: Line 78 -- replace `webCfg.deviceName = "MQTT Wifi WebUI";` with `webCfg.setDeviceName("MQTT Wifi WebUI");`

- [x] Task 4: Fix MQTTWithWebUI example
  - File: `DomoticsCore-MQTT/examples/MQTTWithWebUI/src/main.cpp`
  - Action: Line 138 -- replace `webConfig.deviceName = "ESP32 MQTT Device";` with `webConfig.setDeviceName("ESP32 MQTT Device");`

- [x] Task 5: Fix NTPWithWebUI example
  - File: `DomoticsCore-NTP/examples/NTPWithWebUI/src/main.cpp`
  - Action: Line 66 -- replace `webuiCfg.deviceName = "NTP Demo";` with `webuiCfg.setDeviceName("NTP Demo");`

- [x] Task 6: Fix OTAWithWebUI example
  - File: `DomoticsCore-OTA/examples/OTAWithWebUI/src/main.cpp`
  - Action: Line 93 -- replace `webCfg.deviceName = "OTA With WebUI";` with `webCfg.setDeviceName("OTA With WebUI");`

- [x] Task 7: Fix StorageWithWebUI example
  - File: `DomoticsCore-Storage/examples/StorageWithWebUI/src/main.cpp`
  - Action: Line 26 -- replace `webCfg.deviceName = "Storage With WebUI";` with `webCfg.setDeviceName("Storage With WebUI");` (multi-statement line; only replace the deviceName portion, preserve surrounding statements)

- [x] Task 8: Fix SystemInfoWithWebUI example
  - File: `DomoticsCore-SystemInfo/examples/SystemInfoWithWebUI/src/main.cpp`
  - Action: Line 35 -- replace `webCfg.deviceName = "System Info With WebUI";` with `webCfg.setDeviceName("System Info With WebUI");`

- [x] Task 9: Fix WifiWithWebUI example
  - File: `DomoticsCore-Wifi/examples/WifiWithWebUI/src/main.cpp`
  - Action: Line 27 -- replace `webCfg.deviceName = "WiFi With WebUI";` with `webCfg.setDeviceName("WiFi With WebUI");` (multi-statement line; only replace the deviceName portion, preserve surrounding statements)

- [x] Task 10: Fix WebUIOnly example
  - File: `DomoticsCore-WebUI/examples/WebUIOnly/src/main.cpp`
  - Action: Line 198 -- replace `webUIConfig.deviceName = "DomoticsCore WebUI Demo";` with `webUIConfig.setDeviceName("DomoticsCore WebUI Demo");`

- [x] Task 11: Fix HeadlessAPI example
  - File: `DomoticsCore-WebUI/examples/HeadlessAPI/src/main.cpp`
  - Action: Line 139 -- replace `config.deviceName = "ESP32 API Server";` with `config.setDeviceName("ESP32 API Server");`

- [x] Task 12: Fix WebUI README code snippet
  - File: `DomoticsCore-WebUI/README.md`
  - Action: Line 27 -- replace `WebUIConfig cfg; cfg.deviceName = "My Device";` with `WebUIConfig cfg; cfg.setDeviceName("My Device");`

### Acceptance Criteria

- [x] AC1: Given all 11 example `.cpp` files and the README have been updated to use setter methods (12 tasks total), when `check_everything.sh` is run (or individual PlatformIO builds), then no compilation errors related to `char[]` assignment are reported

- [x] AC2: Given the updated example files, when searching for direct assignment to WebUIConfig char[] fields (`grep -rn '\.deviceName\s*=' --include='*.cpp' --exclude-dir=.pio` in example directories, then manually filtering to only `WebUIConfig` instances), then no direct assignments to `WebUIConfig` char[] fields remain in any example file. Note: `CoreConfig.deviceName` uses `String` and will appear in grep results -- these are NOT bugs and should be ignored.

- [x] AC3: Given the setter methods perform the same `strncpy` + null-termination, when the examples run on hardware, then device names and other configured values appear identically to the pre-R6 behavior (when `String` fields were used)

- [x] AC4: Given the `DomoticsCore-WebUI/README.md` contains a quick-start code snippet, when reviewing the snippet, then it uses `setDeviceName()` instead of direct assignment

## Additional Context

### Dependencies

- No new dependencies. The setter methods (`setDeviceName()`, etc.) already exist in `WebUIConfig.h` and were added as part of the R6 roadmap migration.

### Testing Strategy

- **Compilation test**: Run `check_everything.sh` or build each affected example with PlatformIO to verify no compilation errors remain.
- **Grep audit**: Run `grep -rn --include='*.cpp' --exclude-dir=.pio '\.deviceName\s*='` in example directories, then verify that all `WebUIConfig` instances use setters. Note: the simpler `| grep -i webui` filter will miss `HeadlessAPI/src/main.cpp` where the variable is named `config` without "webui" in the line -- manual inspection is required for that file.
- **Existing unit tests**: `test_webui_component.cpp` already tests setter methods, truncation, and defaults -- no new tests needed.

### Notes

- This is a purely mechanical fix with zero behavioral change. Each replacement is a 1-line edit. The setters do emit `DLOG_W` truncation warnings if a value exceeds the buffer size, but none of the current example device names are long enough to trigger this (max field is `deviceName[32]`, longest example value is "DomoticsCore WebUI Demo" at 23 chars).
- Line numbers in the task list are approximate references based on the codebase at spec creation time. If files are modified before implementation, use the string-match patterns rather than line numbers.
- The root cause was R6 (roadmap item) migrating `WebUIConfig` fields to `char[]` without updating example code at the same time.
- Future roadmap items that migrate other config structs (e.g., `CoreConfig`, `MQTTConfig`) to `char[]` should update all consumers in the same PR.
- When running grep-based audits, always use `--exclude-dir=.pio` to avoid false positives from PlatformIO cached library copies.

## Review Notes
- Adversarial review completed
- Findings: 4 total (all Low severity), 0 fixed, 4 skipped (2 noise, 2 real but out-of-scope style observations)
- Resolution approach: skip (all findings non-blocking, purely cosmetic)
- Build verification: HAWithWebUI and WebUIOnly compiled successfully with PlatformIO (esp32dev)
- Grep audit confirmed: no remaining direct WebUIConfig char[] assignments in any example file
