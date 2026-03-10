---
title: 'Remove Dead Config Fields C1-C3'
slug: 'remove-dead-config-fields-c1-c3'
created: '2026-03-09'
status: 'ready-for-dev'
stepsCompleted: [1, 2, 3, 4]
tech_stack: ['C++17', 'PlatformIO', 'Arduino Framework', 'ESP32/ESP8266', 'Unity Test Framework', 'ArduinoJson']
files_to_modify:  # Only lists files that will be edited. Reference-only files (e.g., MQTT_impl.h) are listed in the "Files to Reference" table below.
  - 'DomoticsCore-MQTT/include/DomoticsCore/MQTT.h'
  - 'DomoticsCore-MQTT/include/DomoticsCore/MQTTWebUI.h'
  - 'DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp'
  - 'DomoticsCore-MQTT/README.md'
  - 'DomoticsCore-MQTT/SPECIFICATIONS.md'
  - 'DomoticsCore-MQTT/STATE_MACHINE.md'
  - 'DomoticsCore-System/include/DomoticsCore/SystemConfig.h'
  - 'DomoticsCore-System/examples/FullStack/test/test_fullstack.cpp'
  - 'DomoticsCore-System/README.md'
  - 'DomoticsCore-Storage/include/DomoticsCore/Storage.h'
  - 'DomoticsCore-Storage/examples/BasicStorage/src/main.cpp'
  - 'DomoticsCore-Storage/examples/BasicStorage/README.md'
  - 'DomoticsCore-Storage/examples/StorageWithWebUI/src/main.cpp'
  - 'DomoticsCore-Storage/README.md'
  - 'docs/components/mqtt/technical-reference.md'
  - 'docs/components/system/technical-reference.md'
  - 'docs/components/storage/technical-reference.md'
  - 'docs/components/storage/README.md'
  - 'docs/CODE-ROADMAP.md'
code_patterns:
  - 'Config structs are plain POD-like structs with default member initializers'
  - 'WebUI providers use buildContexts() for form fields and handleWebUIRequest() for POST handling'
  - 'WebUI data serialization uses ArduinoJson in getWebUIData()'
  - 'Documentation uses markdown tables for config field references'
  - 'Examples set config fields inline before passing to constructors'
test_patterns:
  - 'Unity test framework with TEST_ASSERT_* macros'
  - 'Config default value tests verify each field default in dedicated test functions'
  - 'Tests are in component-level test/ directories'
---

# Tech-Spec: Remove Dead Config Fields C1-C3

**Created:** 2026-03-09

## Overview

### Problem Statement

The codebase contains 8 configuration fields across 3 config structs (`MQTTConfig`, `SystemConfig`, `StorageConfig`) that are declared and exposed to users via WebUI forms and documentation, but are never read by any runtime code path. Per `CODE-ROADMAP.md` Priority 10 and Constitution IV (YAGNI), these dead fields waste RAM on constrained devices (ESP32 ~320KB, ESP8266 ~80KB), mislead users into thinking the settings have effect, and increase maintenance surface.

**Dead fields inventory:**

| ID | Struct | Field | RAM Cost | Why Dead |
|----|--------|-------|----------|----------|
| C1a | `MQTTConfig` | `resubscribeOnConnect` (bool) | 1 byte | `connect()` unconditionally resubscribes all topics (MQTT_impl.h:158-161) without checking this flag |
| C1b | `MQTTConfig` | `cleanSession` (bool) | 1 byte | Never passed to PubSubClient `connect()`. PubSubClient library handles clean session at protocol level but this config field is never read |
| C1c | `MQTTConfig` | `connectTimeout` (uint32_t) | 4 bytes | `connectInternal()` has no timeout logic; it calls PubSubClient `connect()` which blocks until success/failure |
| C1d | `MQTTConfig` | `operationTimeout` (uint32_t) | 4 bytes | No operation timeout logic exists anywhere in MQTT_impl.h |
| C2a | `SystemConfig` | `haDiscoveryPrefix` (String) | ~28 bytes (String overhead + "homeassistant" heap) | `System.h` never reads this field. HA discovery prefix comes from `HAConfig.discoveryPrefix` which is loaded from storage by `SystemPersistence.h` |
| C2b | `SystemConfig` | `webUIEnableAPI` (bool) | 1 byte | Never consumed. WebUI always enables API endpoints. |
| C2c | `SystemConfig` | `wifiTimeout` (uint32_t) | 4 bytes | Never passed to WifiComponent. Wifi uses its own `WifiConfig.connectionTimeout`. |
| C3 | `StorageConfig` | `autoCommit` (bool) | 1 byte | Always `true`, never checked. HAL backends commit immediately on every write. |

**RAM savings:** The most significant saving comes from removing the `String haDiscoveryPrefix` field in `SystemConfig` (~28 bytes of `String` object overhead plus the heap-allocated `"homeassistant"` default). The remaining 7 fields are `bool`/`uint32_t` types totaling ~16 bytes before padding; actual savings for those depend on compiler struct alignment and padding rules, which vary across ESP32 and ESP8266 toolchains. On ESP8266 (only ~80KB SRAM), every byte matters.

### Solution

Remove all 8 dead config fields from their respective structs, strip them from WebUI form definitions and data serialization, remove from documentation tables and examples, and update tests that assert on removed fields. This is a pure deletion task with no new functionality and zero runtime behavior change.

### Scope

**In Scope:**
- C1: Remove `resubscribeOnConnect`, `cleanSession`, `connectTimeout`, `operationTimeout` from `MQTTConfig`
- C2: Remove `haDiscoveryPrefix`, `webUIEnableAPI`, `wifiTimeout` from `SystemConfig`
- C3: Remove `autoCommit` from `StorageConfig`
- Remove corresponding WebUI form fields, data serialization, and POST handler branches
- Remove from documentation (markdown docs only, not generated Doxygen HTML)
- Remove from examples and tests
- Update CODE-ROADMAP.md status to DONE

**Out of Scope:**
- Implementing any behavior for these fields (decision already made: remove per YAGNI)
- Changing any runtime behavior (these fields were never consumed)
- Regenerating Doxygen HTML docs (tracked separately)
- Version bumping (deferred to release process). **SemVer note:** Removing public config struct fields is a breaking API change. Per Constitution XV (SemVer), the next release containing this change MUST be a MAJOR version bump. **Tracking action:** Upon completion of this task, add an entry to `docs/CODE-ROADMAP.md` under a `## Breaking Changes Pending Release` section (create if absent) with text: `- Removed 8 dead config fields from MQTTConfig, SystemConfig, StorageConfig — requires MAJOR version bump`. If the project uses GitHub Issues, also create an issue titled "Release: Major version bump required (dead config field removal)" and link it in the roadmap entry.
- CHANGELOG update (tracked in release process; the release CHANGELOG entry must list all 8 removed fields)

## Context for Development

### Codebase Patterns

- **Config structs** are plain C++ structs with default member initializers (e.g., `bool enabled = true;`). No constructors, no serialization methods.
- **WebUI providers** inherit from `CachingWebUIProvider`. Form fields are defined in `buildContexts()` using `WebUIField(id, label, type, default)`. Data is serialized in `getWebUIData()` using ArduinoJson. POST handling is in `handleWebUIRequest()` with field-by-field `if/else if` chains.
- **Documentation** uses markdown tables with columns `| Field | Type | Default | Description |`.
- **Examples** set config fields inline (e.g., `config.autoCommit = true;`).
- **Tests** use Unity `TEST_ASSERT_TRUE(config.fieldName)` for default value verification.

### Files to Reference

| File | Purpose |
| ---- | ------- |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h` | MQTTConfig struct definition (lines 84-125) |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h` | MQTT runtime — confirms fields are never read |
| `DomoticsCore-MQTT/include/DomoticsCore/MQTTWebUI.h` | WebUI form for MQTT: buildContexts (line 78), getWebUIData (line 135), handleWebUIRequest (lines 241-242) |
| `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp` | Test asserting `cleanSession` default (line 84) |
| `DomoticsCore-MQTT/README.md` | Doc: config struct listing and default value table |
| `DomoticsCore-MQTT/SPECIFICATIONS.md` | Doc: config struct listing (lines 75, 96, 99-100) and WebUI form field list (line 236) |
| `DomoticsCore-MQTT/STATE_MACHINE.md` | Doc: references to `connectTimeout`, `resubscribeOnConnect`, `cleanSession` |
| `DomoticsCore-System/include/DomoticsCore/SystemConfig.h` | SystemConfig struct definition (lines 67, 82, 94) |
| `DomoticsCore-System/examples/FullStack/test/test_fullstack.cpp` | Test asserting `haDiscoveryPrefix` default (line 81) |
| `DomoticsCore-System/README.md` | Doc: SystemConfig listing (line 233) |
| `DomoticsCore-Storage/include/DomoticsCore/Storage.h` | StorageConfig struct definition (line 58) |
| `DomoticsCore-Storage/examples/BasicStorage/src/main.cpp` | Example setting `autoCommit` (line 44) |
| `DomoticsCore-Storage/examples/BasicStorage/README.md` | Doc: config example and troubleshooting (lines 60, 214) |
| `DomoticsCore-Storage/examples/StorageWithWebUI/src/main.cpp` | Example setting `autoCommit` (line 30) |
| `DomoticsCore-Storage/README.md` | Doc: mentions `autoCommit` flag (line 47) |
| `docs/components/mqtt/technical-reference.md` | Doc: config table rows (lines 191, 204-206) and WebUI editable fields list (line 416) |
| `docs/components/system/technical-reference.md` | Doc: config table rows (lines 105, 129, 147). Note: line 377 (`ha_disc_prefix`) documents the LIVE `HAConfig.discoveryPrefix` storage key — do NOT delete |
| `docs/components/storage/technical-reference.md` | Doc: config struct code block (line 38) and Auto-Commit section (lines 235-237) |
| `docs/components/storage/README.md` | Doc: config example (line 39) |
| `docs/CODE-ROADMAP.md` | Tracking table for C1-C3 status |

### Technical Decisions

- **Remove over implement**: Per Constitution IV (YAGNI) and CODE-ROADMAP.md recommendation, these fields are removed rather than wired to behavior.
- **No runtime behavior change**: The `connect()` function in `MQTT_impl.h` already unconditionally resubscribes topics (line 158-161) without checking `resubscribeOnConnect`. PubSubClient's `connect()` call does not accept a `cleanSession` parameter from our side. No timeout logic exists. `autoCommit` is never checked — writes always commit immediately. Therefore, removing these fields changes zero runtime behavior.
- **Compilation breakage for users**: Users who explicitly set `config.cleanSession = false` or `config.autoCommit = true` in their sketches will get a compile error. This is intentional and desired — the setting never had any effect, and the compile error alerts them.
- **WebUI `clean_session` form field**: This field is actively rendered in the MQTT settings form and serialized/deserialized in `MQTTWebUI.h`. It must be removed from `buildContexts()`, `getWebUIData()`, and `handleWebUIRequest()` — 3 locations.

## Implementation Plan

### Tasks

Tasks are ordered by dependency: pre-flight checks first, then struct changes, then consumers.

- [ ] **Task 1: Pre-flight — verify no binary serialization of affected structs**
  - Action: Before making ANY code changes, verify that no code uses `sizeof()`, `memcpy()`, `reinterpret_cast`, or binary read/write on `MQTTConfig`, `SystemConfig`, or `StorageConfig`. Struct field removal changes struct layout and would corrupt any binary serialization.
  - Run: `grep -rn 'sizeof(MQTTConfig)\|sizeof(SystemConfig)\|sizeof(StorageConfig)\|memcpy.*MQTTConfig\|memcpy.*SystemConfig\|memcpy.*StorageConfig\|reinterpret_cast.*MQTTConfig\|reinterpret_cast.*SystemConfig\|reinterpret_cast.*StorageConfig\|fwrite.*MQTTConfig\|fwrite.*SystemConfig\|fwrite.*StorageConfig\|fread.*MQTTConfig\|fread.*SystemConfig\|fread.*StorageConfig' --include='*.h' --include='*.cpp'`
  - Expected result: Zero matches. (Note: `MQTTConfig` and `SystemConfig` contain `String` members, which already invalidate binary serialization via `memcpy`/`reinterpret_cast`/`fwrite`/`fread`. `StorageConfig` is POD-like but still should not be binary-serialized. This step is a safety verification covering all common binary serialization patterns.)
  - If any matches are found: assess impact and update the serialization code as part of this task.
  - Files: No file changes — verification step only.

- [ ] **Task 2: Pre-flight — green-to-green baseline test run**
  - Action: Before making ANY code changes, run the full native test suite for all 3 affected components to establish a green baseline. If any test fails at this stage, the failure is pre-existing and must be investigated/resolved before proceeding with field removal.
  - Commands:
    ```bash
    pio test -e native -d DomoticsCore-MQTT
    pio test -e native -d DomoticsCore-System
    pio test -e native -d DomoticsCore-Storage
    ```
  - Expected result: All tests pass. If not, fix pre-existing failures first.
  - Files: No file changes — verification step only.

- [ ] **Task 3: Remove C1 fields from MQTTConfig struct**
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h`
  - Action: Delete the following 4 field declarations and their Doxygen comments. Search by content (line numbers are approximate):
    - ~Line 96, content: `bool cleanSession = true;` with comment `///< Start with clean session`
    - ~Line 117, content: `bool resubscribeOnConnect = true;` with comment `///< Re-subscribe after reconnection`
    - ~Line 120, content: `uint32_t connectTimeout = 10000;` with comment `///< Connection timeout (ms)`
    - ~Line 121, content: `uint32_t operationTimeout = 5000;` with comment `///< Operation timeout (ms)`
  - Also remove the section comments `// Session` (since `cleanSession` is removed, `keepAlive` is the only remaining field — move `keepAlive` up into the `// Server` section), and remove the `// Timeouts` section header entirely (all fields in that section are deleted). **Keep the `// Subscriptions` section comment** — `maxSubscriptions` remains as a live field under that header.
  - **Decision:** After removal, move `uint16_t keepAlive = 60;` (with its existing Doxygen comment) from the now-empty `// Session` section into the `// Server` section (the section containing `server`, `port`, etc.). Place it immediately after the last field in that section (e.g., after `port`). Delete the `// Session` comment line entirely since no fields remain in that section. The `keepAlive` field is a connection-level parameter (MQTT PINGREQ interval) so this placement is semantically correct. **Note:** MQTT.h uses `// Server` as the section header for connection fields — there is no `// Connection` section.

- [ ] **Task 4: Remove C1 WebUI form field for `clean_session`**
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTTWebUI.h`
  - Action (3 locations):
    1. **Line 78**: Delete `.withField(WebUIField("clean_session", "Clean Session", WebUIFieldType::Boolean, "true"))`
    2. **Line 135**: Delete `doc["clean_session"] = cfg.cleanSession;`
    3. **Lines 241-242**: Delete the `else if (field == "clean_session")` block:
       ```cpp
       } else if (field == "clean_session") {
           cfg.cleanSession = (value == "true" || value == "1");
       ```

- [ ] **Task 5: Remove C1 test assertion**
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Delete ~line 84 (content: `TEST_ASSERT_TRUE(config.cleanSession);`)

- [ ] **Task 6: Remove C2 fields from SystemConfig struct**
  - File: `DomoticsCore-System/include/DomoticsCore/SystemConfig.h`
  - Action: Delete the following 3 field declarations. Search by content (line numbers are approximate):
    - ~Line 67, content: `uint32_t wifiTimeout = 30000;   // 30 seconds`
    - ~Line 82, content: `bool webUIEnableAPI = true;`
    - ~Line 94, content: `String haDiscoveryPrefix = "homeassistant";`
  - Notes: After removing `wifiTimeout`, the WiFi section has `wifiSSID`, `wifiPassword`, `wifiAutoConfig`, `wifiAPSSID`, `wifiAPPassword` — all still valid. After removing `webUIEnableAPI`, the WebUI section has `enableWebUI` and `webUIPort`. After removing `haDiscoveryPrefix`, the HA section has only `enableHomeAssistant`.

- [ ] **Task 7: Remove C2 test assertion**
  - File: `DomoticsCore-System/examples/FullStack/test/test_fullstack.cpp`
  - Action: Modify the `test_home_assistant_configuration` test function (lines 78-83) to remove the `haDiscoveryPrefix` assertion while keeping the `enableHomeAssistant` assertion. The resulting function should be:
    ```cpp
    void test_home_assistant_configuration() {
        TEST_ASSERT_TRUE_MESSAGE(testConfig.enableHomeAssistant,
                                 "Home Assistant should be enabled");
    }
    ```
  - Notes: The `enableHomeAssistant` assertion is still valid and should be kept. Keep any existing French Doxygen comment above the test function unchanged.

- [ ] **Task 8: Remove C3 field from StorageConfig struct**
  - File: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
  - Action: Delete ~line 58 (content: `bool autoCommit = true;`)

- [ ] **Task 9: Remove C3 from examples**
  - File: `DomoticsCore-Storage/examples/BasicStorage/src/main.cpp`
    - Action: Delete line 44: `config.autoCommit = true;`
  - File: `DomoticsCore-Storage/examples/StorageWithWebUI/src/main.cpp`
    - Action: Remove `scfg.autoCommit = true;` from line 30. The line currently reads:
      `StorageConfig scfg; scfg.namespace_name = "domotics"; scfg.maxEntries = 100; scfg.autoCommit = true;`
      Change to:
      `StorageConfig scfg; scfg.namespace_name = "domotics"; scfg.maxEntries = 100;`

- [ ] **Task 10: Update MQTT documentation**
  - **Note:** Line numbers in this task are approximate. Always match by content, not line number. If the content is not found at the stated line, search the file for the exact text.
  - File: `DomoticsCore-MQTT/README.md`
    - Delete lines 117-118 (content: `// Session` + `bool cleanSession;          // Start with clean session`). After deletion, move `keepAlive` (which was listed under `// Session`) into the `// Connection` section to mirror the MQTT.h reorganization from Task 3. Delete the `// Session` comment line entirely since no fields remain in that section.
    - Delete only line 139 (content: `bool resubscribeOnConnect;  // Re-subscribe after reconnect`). **Keep the `// Subscriptions` section header** (line ~137-138) — `maxSubscriptions` remains as a live field in that section.
    - Delete lines 141-143 (content: `// Timeouts` section header + `uint32_t connectTimeout;    // Connection timeout (ms)` + `uint32_t operationTimeout;  // Operation timeout (ms)`)
    - Delete row from Default Values table (line 156, content: `| \`cleanSession\` | true | No session persistence |`)
  - File: `DomoticsCore-MQTT/SPECIFICATIONS.md`
    - Delete lines 74-75 (content: `// Session` + `bool cleanSession;          // Default: true`). After deletion, move `keepAlive` (which was listed under `// Session`) into the `// Server` section (or whichever section contains `server`/`port` fields) to mirror the MQTT.h reorganization from Task 3. Delete the `// Session` comment line.
    - Delete line 96 (content: `bool resubscribeOnConnect;  // Default: true`)
    - Delete lines 98-100 (content: `// Timeouts` header + `uint32_t connectTimeout;    // Milliseconds, default: 10000` + `uint32_t operationTimeout;  // Milliseconds, default: 5000`)
    - Delete line 236 (content: `- \`clean_session\`: Boolean toggle - "Clean Session"`) — this is a WebUI form field listing that also references the removed field
  - File: `DomoticsCore-MQTT/STATE_MACHINE.md`
    - Line 76: Replace `- Timeout after connectTimeout ms (default: 10s)` with `- Timeout is handled internally by PubSubClient`
    - Line 97: Replace `- **Re-subscribe** to topics (if resubscribeOnConnect = true)` with `- **Re-subscribe** to all tracked topics`
    - Lines 211-218: Delete the entire fenced code block (` ``` ` to ` ``` `) that contains the `If resubscribeOnConnect = true:` conditional flow diagram. Replace it with a single prose line: `On every successful connect/reconnect, all tracked topics are unconditionally resubscribed.` This means the fenced code block and all its contents are removed, not just the conditional lines within it.
    - Line 373: Replace `- ✅ Use cleanSession = false for persistent subscriptions` with `- ✅ Clean session behavior is managed at the PubSubClient/broker level`
  - File: `docs/components/mqtt/technical-reference.md`
    - Line 191: Delete row (content: `` | `cleanSession` | `bool` | `true` | Start with a clean MQTT session | ``)
    - Lines 204-206: Delete rows:
      - (content: `` | `resubscribeOnConnect` | `bool` | `true` | Re-subscribe to all topics after reconnect | ``)
      - (content: `` | `connectTimeout` | `uint32_t` | `10000` | Connection attempt timeout in ms | ``)
      - (content: `` | `operationTimeout` | `uint32_t` | `5000` | Generic operation timeout in ms | ``)
    - Line 416: Remove `clean_session` from the editable fields comma-separated list (content: `- **Editable fields**: ..., \`clean_session\`, ...`)

- [ ] **Task 11: Update System documentation**
  - **Note:** Line numbers in this task are approximate. Always match by content, not line number. If the content is not found at the stated line, search the file for the exact text.
  - File: `DomoticsCore-System/README.md`
    - Line 233: Delete `uint32_t wifiTimeout = 30000;  // 30 seconds`
  - File: `docs/components/system/technical-reference.md`
    - Line 105: Delete row `| wifiTimeout | uint32_t | 30000 | WiFi connection timeout in milliseconds |`
    - Line 129: Delete row `| webUIEnableAPI | bool | true | Enable REST API endpoints |`
    - Line 147: Delete row `| haDiscoveryPrefix | String | "homeassistant" | MQTT discovery prefix |`
    - **DO NOT** delete line 377 (content: `` | | `ha_disc_prefix` | `s` | Discovery prefix | ``). This documents the LIVE storage key for `HAConfig.discoveryPrefix`, which is loaded by `SystemPersistence.h` and actively used at runtime. It is NOT related to the dead `SystemConfig.haDiscoveryPrefix` field being removed.

- [ ] **Task 12: Update Storage documentation**
  - **Note:** Line numbers in this task are approximate. Always match by content, not line number. If the content is not found at the stated line, search the file for the exact text.
  - File: `docs/components/storage/technical-reference.md`
    - Line 38: Delete `bool   autoCommit     = true;        // Flush writes immediately` from the code block
    - Lines 235-237: Delete the entire "Auto-Commit" section (heading + paragraph). The behavior (immediate commit) is inherent to the HAL and needs no configuration.
  - File: `docs/components/storage/README.md`
    - Line 39: Delete `cfg.autoCommit = true;`
  - File: `DomoticsCore-Storage/README.md`
    - Line 47: Delete or rewrite `- Respects autoCommit flag for automatic flush.` → `- Writes are committed immediately to persistent storage.`
  - File: `DomoticsCore-Storage/examples/BasicStorage/README.md`
    - Line 60: Delete `config.autoCommit = true;              // Auto-commit changes`
    - Line 214: Rewrite `1. Ensure autoCommit is enabled or call commit manually` → `1. Writes are committed immediately — no manual commit needed`

- [ ] **Task 13: Update CODE-ROADMAP.md tracking**
  - File: `docs/CODE-ROADMAP.md`
  - Action 1: In the Priority 10 tracking table row, change status from `Open — see below` to `DONE — all dead fields removed`
  - Action 2: Add a `## Breaking Changes Pending Release` section (create if absent) **immediately after the Priority 10 section** with the entry: `- Removed 8 dead config fields from MQTTConfig, SystemConfig, StorageConfig (C1-C3) — requires MAJOR version bump per Constitution XV (SemVer)`. This ensures the deferred major version bump is tracked with a concrete artifact.

- [ ] **Task 14: Verify compilation and run tests**
  - Action: Build and test all 3 affected components to verify:
    1. No compilation errors from removed fields
    2. All remaining tests pass
  - Commands (all run from the repository root directory):
    ```bash
    # Run unit tests for affected components (using -d to specify project directory)
    pio test -e native -d DomoticsCore-MQTT
    pio test -e native -d DomoticsCore-System
    pio test -e native -d DomoticsCore-Storage

    # Build affected examples to verify compilation
    pio run -d DomoticsCore-Storage/examples/BasicStorage
    pio run -d DomoticsCore-Storage/examples/StorageWithWebUI
    pio run -d DomoticsCore-System/examples/FullStack
    pio run -d DomoticsCore-MQTT/examples/BasicMQTT
    ```
  - **Note on pre-existing failures:** The `BasicMQTT` example build may fail for reasons unrelated to this task (e.g., missing network dependencies in the PlatformIO environment). If any example build fails and the error does not reference any of the 8 removed fields, the failure is pre-existing and out of scope — document it and move on. Only test/build failures caused by incomplete field removal are blockers for this task.
  - Files: No file changes — verification step only.

- [ ] **Task 15: Add migration guidance for downstream users**
  - Action: Add a brief migration note to the following component READMEs, in a `## Migration Notes` section (create if absent, place before any `## License` or at end of file). **Important:** Use prose descriptions only — do NOT use the exact camelCase field names (e.g., write "the clean-session flag" not `cleanSession`) to avoid false positives in AC 8's grep verification.
    - `DomoticsCore-MQTT/README.md`: Note that four dead fields have been removed from `MQTTConfig`: the clean-session flag, the resubscribe-on-connect flag, and both timeout fields (connect and operation). Users who set these fields in their sketches should simply delete those lines — the settings never had any runtime effect.
    - `DomoticsCore-System/README.md`: Note that three dead fields have been removed from `SystemConfig`: the HA discovery-prefix string, the WebUI enable-API flag, and the WiFi timeout field. Users who set these fields should delete those lines. HA discovery prefix is managed via `HAConfig.discoveryPrefix`. WiFi timeout is managed via `WifiConfig.connectionTimeout`.
    - `DomoticsCore-Storage/README.md`: Note that the auto-commit flag has been removed from `StorageConfig`. Writes are always committed immediately — no configuration needed.
  - Notes: This provides compile-error recovery guidance for downstream users. Full CHANGELOG details are deferred to the release process (see Out of Scope).

### Acceptance Criteria

- [ ] **AC 1**: Given the `MQTTConfig` struct, when I inspect `MQTT.h`, then the fields `resubscribeOnConnect`, `cleanSession`, `connectTimeout`, and `operationTimeout` do not exist.
- [ ] **AC 2**: Given the MQTT WebUI settings form, when the form is rendered, then there is no "Clean Session" toggle field. When the MQTT settings data is serialized to JSON, then `clean_session` key is absent.
- [ ] **AC 3**: Given the `SystemConfig` struct, when I inspect `SystemConfig.h`, then the fields `haDiscoveryPrefix`, `webUIEnableAPI`, and `wifiTimeout` do not exist.
- [ ] **AC 4**: Given the `StorageConfig` struct, when I inspect `Storage.h`, then the field `autoCommit` does not exist.
- [ ] **AC 5**: Given the test suite for MQTT component, when `test_mqtt_config_defaults` runs, then it passes without referencing `cleanSession`.
- [ ] **AC 6**: Given the test suite for FullStack example, when `test_home_assistant_configuration` runs, then it passes without referencing `haDiscoveryPrefix`.
- [ ] **AC 7**: Given the Storage examples (`BasicStorage`, `StorageWithWebUI`), when they are compiled, then they compile successfully without `autoCommit` references.
- [ ] **AC 8**: Given all markdown documentation files, when searched with the following exact command, then zero matches are found:
    ```bash
    grep -rn --include='*.md' \
      --exclude-dir='docs/html' \
      --exclude='CODE-ROADMAP.md' \
      --exclude='tech-spec-remove-dead-config-fields-c1-c3.md' \
      -E 'resubscribeOnConnect|cleanSession|clean_session|connectTimeout|operationTimeout|haDiscoveryPrefix|webUIEnableAPI|wifiTimeout|autoCommit' \
      docs/ DomoticsCore-MQTT/ DomoticsCore-System/ DomoticsCore-Storage/
    ```
    Exclusions: `docs/CODE-ROADMAP.md` (historical tracking), the tech-spec itself, and `docs/html/` (auto-generated Doxygen). **Note:** `ha_disc_prefix` is NOT included in this grep because it legitimately exists as the storage key for the LIVE `HAConfig.discoveryPrefix` field in `docs/components/system/technical-reference.md`.
- [ ] **AC 9**: Given the native test suite, when all tests are run, then all tests pass with zero failures.
- [ ] **AC 10**: Given `CODE-ROADMAP.md`, when I inspect the Priority 10 tracking row, then the status reads `DONE`.

## Additional Context

### Dependencies

- No external dependencies. This is a pure deletion task.
- No dependency on other roadmap items — C1-C3 are independent.
- Must be done AFTER all P1-P9 items (already complete per CODE-ROADMAP.md).

### Testing Strategy

- **Unit tests**: Run existing MQTT and FullStack test suites after field removal to verify no regressions. The only test modifications are:
  - Remove `TEST_ASSERT_TRUE(config.cleanSession)` from MQTT config defaults test
  - Remove/simplify `test_home_assistant_configuration` in FullStack test
- **Compilation verification**: Build all examples that reference the removed fields:
  - `DomoticsCore-Storage/examples/BasicStorage`
  - `DomoticsCore-Storage/examples/StorageWithWebUI`
  - `DomoticsCore-System/examples/FullStack`
  - `DomoticsCore-MQTT/examples/BasicMQTT` (should be unaffected but verify)
- **Manual WebUI check** (optional): Open MQTT settings page and verify "Clean Session" toggle is gone.
- **No new tests needed**: This is pure dead code removal. Existing tests cover the remaining fields.
- **Fields with no test references**: `webUIEnableAPI` and `wifiTimeout` have no existing test assertions, so no test modifications are needed for those fields. Their removal from `SystemConfig` only affects the struct definition and documentation.

### Notes

- **Risk: User code breakage**. Users who explicitly set `config.cleanSession`, `config.autoCommit`, `config.wifiTimeout`, `config.haDiscoveryPrefix`, or `config.webUIEnableAPI` will get compile errors. This is intentional — these settings never had any effect, and the compile error alerts users to remove dead code from their sketches. CHANGELOG updates are out of scope for this task (see Out of Scope) and will be handled during the release process.
- **Constitution compliance**: This task directly implements Constitution IV (YAGNI — no dead code) and Constitution V (Performance First — reduce RAM usage on constrained devices).
- **File size impact**: All modified files remain well within the 800-line limit (Constitution VII).
- **`cleanSession` is an MQTT protocol feature**: PubSubClient manages clean session at the protocol level. Our config field was never passed to the library. Users who need persistent sessions should configure this at the PubSubClient level or wait for a future implementation that wires this properly.
- **`resubscribeOnConnect` behavior is hardcoded**: The `connect()` method in `MQTT_impl.h` always resubscribes. If conditional resubscription is ever needed, a new field should be added with proper runtime wiring at that time.
