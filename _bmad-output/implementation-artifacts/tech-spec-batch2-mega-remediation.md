---
title: 'Batch 2 Mega Remediation — R6/R14-R16/R18/R20/R21/R25 + tracking fixes'
slug: 'batch2-mega-remediation'
created: '2026-03-07'
status: 'implementation-complete'
stepsCompleted: [1, 2, 3, 4]
tech_stack: [C++17 (gnu++17 native/ESP8266, gnu++14 ESP32), Arduino, ESP8266, ESP32, PlatformIO, Unity]
files_to_modify:
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistantWebUI.h
  - DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h
  - DomoticsCore-MQTT/include/DomoticsCore/MQTT.h
  - DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h
  - DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h
  - DomoticsCore-System/include/DomoticsCore/System.h
  - DomoticsCore-System/include/DomoticsCore/SystemPersistence.h
  - DomoticsCore-System/include/DomoticsCore/SystemWebUISetup.h
  - DomoticsCore-OTA/include/DomoticsCore/OTA.h
  - DomoticsCore-Core/include/DomoticsCore/MemoryManager.h
  - DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp
  - DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp
  - DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp
  - DomoticsCore-RemoteConsole/test/test_remoteconsole_component/test_remoteconsole_component.cpp
  - DomoticsCore-HomeAssistant/examples/BasicHA/src/main.cpp
  - DomoticsCore-HomeAssistant/examples/HAWithWebUI/src/main.cpp
  - docs/CODE-ROADMAP.md
code_patterns: [header-only, HAL abstraction, EventBus, IComponent lifecycle, snprintf over String, .c_str() for all String reads]
test_patterns: [Unity framework, HeapTracker, mock-based, native platform]
---

# Tech-Spec: Batch 2 Mega Remediation — R6/R14-R16/R18/R20/R21/R25 + tracking fixes

**Created:** 2026-03-07

## Overview

### Problem Statement

The DomoticsCore codebase has 7 remaining remediation items from the adversarial review that span memory safety (R6), anti-pattern documentation (R14-R16), dead code enforcement (R18, R20, R21), and type safety (R25). Additionally, the CODE-ROADMAP.md tracking table has stale statuses for M12, R9, and R10 that were already fixed in previous batches.

### Solution

A single batch addressing all remaining quick-to-medium remediation items:
- **R6**: Replace 9 `String` fields in `HAConfig` with fixed `char[]` arrays using named constants, with WebUI validation of max lengths.
- **R14-R16**: Add code comments documenting accepted exceptions for MemoryManager singleton, MQTT static instance, and System `__has_include()`.
- **R18**: Enforce `maxQueueSize`, `publishRateLimit`, `maxSubscriptions` in MQTT component.
- **R20**: Wire `SystemConfig.otaPassword` to `OTAConfig` during System assembly.
- **R21**: Implement basic Telnet authentication in RemoteConsole using existing `requireAuth`/`password`/`allowCommands` config fields.
- **R25**: Fix `AlarmFeature` operator return types (already `enum class`, but `operator|` returns `uint8_t` instead of `AlarmFeature`, and `operator&` returns `uint8_t` instead of `bool`).
- **Tracking**: Fix stale statuses in CODE-ROADMAP.md.

### Scope

**In Scope:**
- R6: HAConfig String→char[] with named size constants + WebUI input validation
- R14: Document MemoryManager singleton as accepted exception
- R15: Document MQTT static instance as accepted exception
- R16: Document System `__has_include()` as intentional deviation
- R18: Enforce MQTT config limits (maxQueueSize, publishRateLimit, maxSubscriptions)
- R20: Wire otaPassword from SystemConfig to OTAConfig
- R21: Implement basic Telnet auth handshake in RemoteConsole
- R25: AlarmFeature operator return types fix (`operator|` → `AlarmFeature`, `operator&` → `bool`)
- Tracking: Update CODE-ROADMAP.md statuses for M12, R9, R10

**Out of Scope:**
- R11-R13 (file splits — separate batch)
- R24 completion (virtual dispatch routing — depends on R26)
- R26 (EventBus command architecture — separate feature)
- WebUI provider changes for HAConfig fields (existing WebUI providers handle display; we add validation only)

## Context for Development

### Codebase Patterns

- **Header-only**: All component logic lives in `.h` files under `include/DomoticsCore/`
- **HAL abstraction**: Hardware calls go through `HAL::Platform::*` (Platform_Arduino.h / Platform_Stub.h)
- **snprintf over String**: Constitution XIV mandates `snprintf()` + stack buffers instead of String concatenation in hot paths
- **IComponent lifecycle**: Components implement `onInit()`, `onStart()`, `onLoop()`, `onShutdown()`
- **EventBus communication**: Components communicate via `emit()`/`subscribe()` with POD event structs
- **Config structs**: Each component has a public config struct (e.g., `HAConfig`, `MQTTConfig`)

### Files to Reference

| File | Purpose | Key Lines |
| ---- | ------- | --------- |
| `HomeAssistant/HomeAssistant.h` | HAConfig struct (R6), HA component, constructor L69-80 | L41-55 (struct), L76-78 (availabilityTopic gen), L297/343/413/546/566 (snprintf topics) |
| `HomeAssistant/HomeAssistantWebUI.h` | WebUI fields for HAConfig (R6) | L49-96 (field defs), L145-177 (POST handler) |
| `HomeAssistant/HAAlarmControlPanel.h` | AlarmFeature enum class (R25) | L12-19 (enum), L21-30 (operators), L82 (supportedFeatures), L113-128 (bitwise checks) |
| `MQTT/MQTT.h` | MQTTConfig struct (R18), static instance (R15) | L112-116 (limit fields), L398 (subscriptions vec), L409 (messageQueue vec), L424-425 (static callback+instance) |
| `MQTT/MQTT_impl.h` | MQTT implementation (R18 enforcement) | L209-232 (publish), L212 (queue push, NO size check), L223 (publishCountThisSecond++ — exists but no guard), L255-278 (subscribe, NO count check), L430-444 (processQueue) |
| `RemoteConsole/RemoteConsole.h` | RC auth fields (R21) | L34-45 (config), L37-40 (auth fields), L78 (dead `authenticated` bool), L188-211 (client accept), L478-495 (sendWelcome), L497-583 (handleClient), L549 (command exec) |
| `System/System.h` | otaPassword wiring (R20), __has_include (R16) | L328-341 (HAConfig population), L331-337 (String ops on fields), L347-355 (OTA registration — NO config passed!), L32 (first __has_include, 20 total) |
| `System/SystemPersistence.h` | Storage load/save for HAConfig (R6) | L126-133 (key registration), L298-318 (loadHomeAssistantConfig) |
| `System/SystemWebUISetup.h` | WebUI save callback for HAConfig (R6) | L381-386 (persistence callback — only 3 of 9 fields saved: `nodeId`, `deviceName`, `discoveryPrefix`) |
| `OTA/OTA.h` | OTAConfig target for password (R20) | L20-34 (struct: bearerToken, basicAuthUser, basicAuthPassword — all String) |
| `Core/MemoryManager.h` | Singleton exception (R14) | L112-115 (instance() method) |
| `docs/CODE-ROADMAP.md` | Tracking table updates | Tracking tables at L263 and L432 |

### Technical Decisions

- **R6 buffer sizes**: Named constants defined in `HAConfig` struct scope (e.g., `HA::MAX_NODE_ID = 33`). Sizes based on MQTT topic length limits and HA device registry field conventions. Constants visible to both `HomeAssistant.h` and `System.h` (System populates HAConfig from SystemConfig). Constants displayed in WebUI input fields as `maxlength` attributes.
- **R6 helper**: Provide an inline `HA::setField(char* dest, const char* src, size_t maxLen)` utility defined inside `namespace HA` in `HomeAssistant.h`. Truncates with null-termination guarantee. Logs a warning via `DLOG_W` when truncation occurs.
- **R6 constructor**: `HAConfig` needs a default constructor that `strncpy`s the default values into the char[] fields (replacing the previous `String field = "default"` pattern). **Struct size**: ~506 bytes inline (vs ~72 bytes + heap before). `HomeAssistantComponent` constructor already takes `const HAConfig&` — no by-value copy. Verify no other code path passes `HAConfig` by value. On ESP8266 (4KB stack), a local `HAConfig` variable is safe but should not be nested inside deep call chains.
- **R6 availabilityTopic**: Constructor auto-generates `availabilityTopic` via `snprintf(availabilityTopic, ..., "%s/%s/availability", discoveryPrefix, nodeId)` — replaces String concatenation at L76-78.
- **R6 System.h nodeId processing**: L335-337 does `substring(0,32)`, `toLowerCase()`, `replace(" ", "_")` on nodeId. With char[], this becomes manual: `strncpy` + loop for tolower + loop for space→underscore.
- **R6 cascade updates**: SystemPersistence.h L308-313 (`getString` → `HA::setField`; note: `getString(key, default)` with char[] default triggers implicit `const char*→String` conversion — temporary heap allocation per field, acceptable at boot time), SystemWebUISetup.h L381-386 (save callback `putString` from char[] — works directly), HomeAssistantWebUI.h L145-177 (POST handler `params["field"]` returns String → use `HA::setField(field, params["key"].c_str(), MAX)`; also add `maxlength` attributes to field definitions at L49-96 for client-side validation), examples (String assignment → `HA::setField`). **Persistence gap**: Only 3 of 9 HAConfig fields are persisted (`nodeId`, `deviceName`, `discoveryPrefix`). The remaining 6 (`manufacturer`, `model`, `swVersion`, `availabilityTopic`, `configUrl`, `suggestedArea`) are re-derived from SystemConfig at boot or left as defaults. This is pre-existing behavior — extending persistence is out of scope for this batch but should be documented in the WebUI so users understand which fields survive reboot.
- **R6 setDeviceInfo() API**: L449-452 public method `setDeviceInfo(const String&, ...)` — change signature to `const char*` parameters. **Arduino `String` has implicit `operator const char*()` on most platforms** (ESP8266, ESP32), so existing callers passing `String` will compile without `.c_str()`. However, this implicit conversion is platform-dependent — add a note in the API docstring recommending `.c_str()` for portability. Callers using string literals (`"..."`) work directly.
- **R6 isEmpty() replacement**: Fields like `configUrl` and `suggestedArea` use `.isEmpty()` checks (L527, L531). With char[], replace with `configUrl[0] == '\0'`.
- **R18 enforcement**: Check limits at the point of use (publish, subscribe, queue push), log warning on rejection. **0 = unlimited** (no limit enforced) — preserves current default behavior and avoids breaking change. Rate limit vars `lastPublishTime`/`publishCountThisSecond` already exist (MQTT.h L412-413) and `publishCountThisSecond` is incremented on successful publish (MQTT_impl.h L223), but no guard checks `publishRateLimit` — add the guard. Note: current defaults are `maxQueueSize=100`, `publishRateLimit=10`, `maxSubscriptions=50` (not 0), so enforcement will immediately apply with existing defaults. These defaults are reasonable for IoT and were already documented — enforcement makes them real rather than aspirational. No default change needed.
- **R20 discovery**: `OTAComponent` is instantiated with **default constructor** (System.h L350) — no OTAConfig is passed at all. Fix requires: (1) create `OTAConfig`, (2) wire `config.otaPassword` → `otaConfig.bearerToken`, (3) pass `otaConfig` to `OTAComponent` constructor. Also check that `OTAComponent` accepts config in its constructor.
- **R21 auth**: Simple password prompt on Telnet connect. No encryption (Telnet is plaintext by nature). Per-client auth state via `std::map<uint32_t, bool> clientAuthenticated` (replaces dead `bool authenticated` at L78). New `auth <password>` built-in command. **Security caveat documented clearly**: this is a "speed bump", not real security — Telnet is cleartext by nature. **Client identification**: Use an incrementing `uint32_t nextClientId` counter (not `remoteIP()`) to generate unique client IDs — IP-based IDs collide behind NAT or with multiple connections from the same host. Existing `clientBuffers` map key type (`uint32_t`) is preserved but the value source changes from `(uint32_t)remoteIP()` to `nextClientId++`.
- **R21 auth timeout**: Client that connects but never sends password is disconnected after a configurable timeout (default 10s). Track connect time per client via `std::map<uint32_t, unsigned long> clientConnectTime`.
- **R21 client ID migration**: All maps keyed by client ID (`clientBuffers`, `clientAuthenticated`, `clientConnectTime`) use the same `nextClientId`-assigned value. Each client object needs a way to carry its ID — add a parallel `std::vector<uint32_t> clientIds` alongside the existing `std::vector<WiFiClient> clients`, or use `std::vector<std::pair<uint32_t, WiFiClient>>`. The latter is cleaner. This replaces `(uint32_t)it->remoteIP()` lookups throughout the file.
- **R21 allowCommands**: Check before command execution at L549. When `false`, only `auth`, `help`, and `quit` are allowed.
- **R25 correction**: `AlarmFeature` is already `enum class` (L12-19). Operators already exist (L21-30) but return `uint8_t`. Fix: `operator|` returns `AlarmFeature` (for chaining), `operator&` returns `bool` (for flag testing). Remove `operator|(uint8_t, AlarmFeature)` overload — no longer needed with proper typing. **Breaking change**: any external code using `uint8_t result = features & AlarmFeature::X` will get a `bool` (0/1) instead of the masked value. Since this batch also changes `supportedFeatures` from `uint8_t` to `AlarmFeature` (Task 13), ensure all internal usage sites are covered. For external API safety, add an `operator&(AlarmFeature, AlarmFeature)` overload (both args typed) so callers must use `AlarmFeature` on both sides — compile errors surface misuse at build time rather than silent semantic changes.
- **R25 type cascade**: Change `supportedFeatures` member (HAAlarmControlPanel.h L82) from `uint8_t` to `AlarmFeature`. Change `features` parameter in `addAlarmControlPanel()` (HomeAssistant.h L254) from `uint8_t` to `AlarmFeature`. Default value becomes `AlarmFeature::ArmAway` (no more `static_cast`). Bitwise checks at L113-128 work directly since `operator&` returns `bool`. Test casts like `static_cast<uint8_t>(AlarmFeature::ArmNight)` at test L120 become unnecessary — clean up.
- **R20 confirmed**: `OTAComponent` already has a parametric constructor `OTAComponent(const OTAConfig& cfg)`. Fix is 3 lines in System.h: create `OTAConfig`, set `bearerToken`, pass to constructor. OTAConfig remains `String`-based (not in scope for char[] conversion — intentional inconsistency documented). **Caveat**: `bearerToken` is declared in `OTAConfig` but may not be consumed by `OTAComponent`'s implementation. Wiring without verification is a no-op. Task 22 includes a prerequisite audit step.
- **R14 comment**: Add above `instance()` at MemoryManager.h L112: "Accepted deviation from Constitution XIII — MemoryManager requires global access for buffer sizing decisions across all components."
- **R15 comment**: Add above static declarations at MQTT.h L424: "Accepted deviation from Constitution XIII — PubSubClient's C-style callback requires static routing. Consider std::function if library is replaced."
- **R16 comment**: Add above first `__has_include` at System.h L32: "Intentional deviation from Constitution IX — __has_include() enables zero-config optional component detection. Not platform-specific, not feature-flagging."

## Implementation Plan

### Tasks

#### R6 — HAConfig String→char[] (Breaking Change)

- [x] Task 1: Define size constants and helper function
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Add named constants and helper **after** the existing `#include` directives (DLOG_W must be available from Logger.h). Place before `HAConfig` struct but after all includes. Project uses C++17 (`gnu++17`) on native/ESP8266 but **C++14 (`gnu++14`) on ESP32**. In C++17, namespace-scope `constexpr` variables are implicitly inline (no ODR issue). In C++14, they have internal linkage (per-TU copies — acceptable for small integral constants). The namespace approach works for both standards:
    ```cpp
    namespace HA {
    constexpr size_t MAX_NODE_ID         = 33;   // 32 chars + null (MQTT client ID limit)
    constexpr size_t MAX_DEVICE_NAME     = 65;   // 64 chars + null (HA device registry)
    constexpr size_t MAX_MANUFACTURER    = 33;   // 32 chars + null
    constexpr size_t MAX_MODEL           = 33;   // 32 chars + null
    constexpr size_t MAX_SW_VERSION      = 17;   // 16 chars + null (semver with pre-release)
    constexpr size_t MAX_DISCOVERY_PREFIX = 33;   // 32 chars + null
    constexpr size_t MAX_AVAIL_TOPIC     = 129;  // 128 chars + null (generated topic)
    constexpr size_t MAX_CONFIG_URL      = 129;  // 128 chars + null (http://IP:port)
    constexpr size_t MAX_SUGGESTED_AREA  = 33;   // 32 chars + null
    } // namespace HA
    ```
  - Action: Add inline helper **inside** `namespace HA` (after the constants, before closing brace):
    ```cpp
    namespace HA {
    // ... constants above ...
    inline void setField(char* dest, const char* src, size_t maxLen) {
        if (!src) { dest[0] = '\0'; return; }
        size_t srcLen = strlen(src);
        if (srcLen >= maxLen) {
            DLOG_W("HA", "Field truncated: '%.*s...' (max %zu)", (int)(maxLen-1), src, maxLen-1);
        }
        strncpy(dest, src, maxLen - 1);
        dest[maxLen - 1] = '\0';
    }
    } // namespace HA
    ```

- [x] Task 2: Convert HAConfig struct from String to char[]
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` (L41-55)
  - Action: Replace all 9 `String` fields with `char[]` using the constants. Add default constructor:
    ```cpp
    struct HAConfig {
        char nodeId[HA::MAX_NODE_ID];
        char deviceName[HA::MAX_DEVICE_NAME];
        char manufacturer[HA::MAX_MANUFACTURER];
        char model[HA::MAX_MODEL];
        char swVersion[HA::MAX_SW_VERSION];
        bool retainDiscovery = true;
        char discoveryPrefix[HA::MAX_DISCOVERY_PREFIX];
        char availabilityTopic[HA::MAX_AVAIL_TOPIC];
        char configUrl[HA::MAX_CONFIG_URL];
        char suggestedArea[HA::MAX_SUGGESTED_AREA];

        HAConfig() : retainDiscovery(true) {
            HA::setField(nodeId, "myDeviceId", sizeof(nodeId));
            HA::setField(deviceName, "My Device", sizeof(deviceName));
            HA::setField(manufacturer, "DomoticsCore", sizeof(manufacturer));
            HA::setField(model, "MyDeviceModel", sizeof(model));
            HA::setField(swVersion, "1.0.0", sizeof(swVersion));
            HA::setField(discoveryPrefix, "homeassistant", sizeof(discoveryPrefix));
            availabilityTopic[0] = '\0';
            configUrl[0] = '\0';
            suggestedArea[0] = '\0';
        }
    };
    ```

- [x] Task 3: Update HomeAssistantComponent constructor
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` (L69-80)
  - Action: Replace `availabilityTopic` generation (L76-78) from String concatenation to:
    ```cpp
    if (config.availabilityTopic[0] == '\0') {
        int written = snprintf(config.availabilityTopic, HA::MAX_AVAIL_TOPIC,
                               "%s/%s/availability", config.discoveryPrefix, config.nodeId);
        if (written >= (int)HA::MAX_AVAIL_TOPIC) {
            DLOG_W("HA", "availabilityTopic truncated (%d chars, max %zu)",
                   written, HA::MAX_AVAIL_TOPIC - 1);
        }
    }
    ```

- [x] Task 4: Update all `.isEmpty()` checks to `[0] == '\0'`
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
  - Action: Replace `config.configUrl.isEmpty()` (L527) with `config.configUrl[0] == '\0'`. Same for `suggestedArea` (L531) and `availabilityTopic` (L76).

- [x] Task 5: Update `setDeviceInfo()` signature
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` (L449-452)
  - Action: Change parameter types from `const String&` to `const char*`. Use `HA::setField()` internally:
    ```cpp
    void setDeviceInfo(const char* name, const char* model, const char* manufacturer, const char* swVersion) {
        HA::setField(config.deviceName, name, HA::MAX_DEVICE_NAME);
        HA::setField(config.model, model, HA::MAX_MODEL);
        HA::setField(config.manufacturer, manufacturer, HA::MAX_MANUFACTURER);
        HA::setField(config.swVersion, swVersion, HA::MAX_SW_VERSION);
    }
    ```

- [x] Task 6: Update System.h HAConfig population
  - File: `DomoticsCore-System/include/DomoticsCore/System.h` (L328-341)
  - Action: Replace String assignments (L331-337) with `HA::setField()` calls. Replace `substring(0,32)` + `toLowerCase()` + `replace(" ", "_")` with manual char[] processing:
    ```cpp
    HA::setField(haConfig.deviceName, config.deviceName.c_str(), HA::MAX_DEVICE_NAME);
    HA::setField(haConfig.swVersion, config.firmwareVersion.c_str(), HA::MAX_SW_VERSION);
    HA::setField(haConfig.manufacturer, config.manufacturer.c_str(), HA::MAX_MANUFACTURER);
    HA::setField(haConfig.model, config.model.c_str(), HA::MAX_MODEL);
    // nodeId: copy, lowercase, replace spaces
    HA::setField(haConfig.nodeId, config.deviceName.c_str(), HA::MAX_NODE_ID);
    for (size_t i = 0; haConfig.nodeId[i]; i++) {
        if (haConfig.nodeId[i] == ' ') haConfig.nodeId[i] = '_';
        else haConfig.nodeId[i] = tolower(haConfig.nodeId[i]);
    }
    ```

- [x] Task 7: Update SystemPersistence.h load
  - File: `DomoticsCore-System/include/DomoticsCore/SystemPersistence.h` (L298-318)
  - Action: Replace `haConfig.field = storage->getString(key, haConfig.field)` with:
    ```cpp
    HA::setField(haConfig.nodeId, storage->getString("ha_nodeid", haConfig.nodeId).c_str(), HA::MAX_NODE_ID);
    ```
  - Notes: Repeat for all 6 persisted fields. Temporary String allocation from `getString()` is acceptable at boot time. **Load order caveat**: `loadHomeAssistantConfig()` runs after `System.h` populates HAConfig defaults from SystemConfig (Task 6). The `getString(key, haConfig.nodeId)` fallback uses the *already-processed* nodeId (lowercased, underscored) as the default — not the original `"myDeviceId"`. This is correct behavior (persistence overrides System defaults), but must be tested (see AC-32).

- [x] Task 8: Update SystemWebUISetup.h save callback
  - File: `DomoticsCore-System/include/DomoticsCore/SystemWebUISetup.h` (L381-386)
  - Action: `putString("ha_nodeid", cfg.nodeId)` — char[] to `const char*` is implicit, no change needed for save. Verify compilation.

- [x] Task 9: Update HomeAssistantWebUI.h POST handler + field defs
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistantWebUI.h`
  - Action (L145-177): Replace `newCfg.field = params["key"]` with `HA::setField(newCfg.field, params["key"].c_str(), HA::MAX_*)`.
  - Action (L49-96): Add `maxlength` attribute to Text input field definitions for client-side validation. Example: `{"node_id", "Node ID", FieldType::Text, HA::MAX_NODE_ID - 1}`.

- [x] Task 10: Update examples
  - Files: `DomoticsCore-HomeAssistant/examples/BasicHA/src/main.cpp`, `examples/HAWithWebUI/src/main.cpp`
  - Action: Replace `haCfg.nodeId = "MyDeviceId"` with `HA::setField(haCfg.nodeId, "MyDeviceId", HA::MAX_NODE_ID)`. Repeat for all HAConfig field assignments. Replace `configUrl` String concatenation with `snprintf`.

- [x] Task 11: Update HA tests for char[] API
  - File: `DomoticsCore-HomeAssistant/test/test_ha_component/test_ha_component.cpp`
  - Action: Replace all `config.field = "value"` with `HA::setField()`. Replace `TEST_ASSERT_EQUAL_STRING(config.field.c_str(), ...)` with `TEST_ASSERT_EQUAL_STRING(config.field, ...)` (char[] works directly with strcmp).
  - Action: Add new tests:
    - `test_ha_config_defaults()` — verify all default values after default construction
    - `test_ha_config_availability_topic_generation()` — verify auto-generated topic format
    - `test_ha_set_field_truncation()` — verify truncation + null termination for oversized input
    - `test_ha_set_field_null_input()` — verify null src produces empty string
    - `test_ha_node_id_processing()` — `"My Device Name"` → `"my_device_name"`

#### R25 — AlarmFeature Operator Return Types

- [x] Task 12: Fix operator overloads
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h` (L21-30)
  - Action: Replace all 3 existing operator overloads with 2 type-safe versions. Remove `operator|(uint8_t, AlarmFeature)` and `operator&(uint8_t, AlarmFeature)`:
    ```cpp
    inline constexpr AlarmFeature operator|(AlarmFeature a, AlarmFeature b) {
        return static_cast<AlarmFeature>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
    inline AlarmFeature& operator|=(AlarmFeature& a, AlarmFeature b) {
        return a = a | b;
    }
    inline constexpr bool operator&(AlarmFeature a, AlarmFeature b) {
        return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
    }
    ```
  - Notes: Both operands are now `AlarmFeature`. `operator|=` is required for compound assignment with `enum class` (e.g., `features |= AlarmFeature::ArmHome`). Any external code using `uint8_t` as left operand will get a compile error (intentional — forces migration to `AlarmFeature` type). This is a source-breaking change for callers mixing `uint8_t` and `AlarmFeature`.

- [x] Task 13: Change `supportedFeatures` type to `AlarmFeature`
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h` (L82)
  - Action: Change `uint8_t supportedFeatures = static_cast<uint8_t>(AlarmFeature::ArmAway)` to `AlarmFeature supportedFeatures = AlarmFeature::ArmAway`.
  - Notes: Bitwise checks at L113-128 (`if (supportedFeatures & AlarmFeature::ArmHome)`) work directly since `operator&` now returns `bool`.

- [x] Task 14: Update `addAlarmControlPanel()` signature
  - File: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h` (L254)
  - Action: Change `uint8_t features` parameter to `AlarmFeature features`. Default becomes `AlarmFeature::ArmAway` (remove `static_cast`). Update assignment at L260.

- [x] Task 15: Update alarm panel tests
  - File: `DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp`
  - Action: Remove unnecessary `static_cast<uint8_t>()` calls (e.g., L120). Verify `panel.supportedFeatures = AlarmFeature::ArmAway | AlarmFeature::ArmHome` compiles without cast. Add `test_alarm_feature_type_safety()` — verify `|` returns `AlarmFeature` and `&` is usable in conditionals.

#### R18 — MQTT Enforce Config Limits

- [x] Task 16: Add queue size check in `publish()`
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h` (L209-213)
  - Action: Before `messageQueue.push_back()` at L212, add:
    ```cpp
    if (mqttConfig.maxQueueSize > 0 && messageQueue.size() >= mqttConfig.maxQueueSize) {
        DLOG_W(LOG_MQTT, "Message queue full (%u/%u), dropping message for '%s'",
               (unsigned)messageQueue.size(), mqttConfig.maxQueueSize, topic.c_str());
        stats.publishErrors++;
        return false;
    }
    ```

- [x] Task 17: Add rate limit guard in `publish()` — BEFORE Task 16's queue check
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h` (L209-232)
  - Action: At method entry (after L209), add rate limit guard **before** the queue size check from Task 16. Order: rate limit → queue size → existing publish logic. This ensures rate-limited messages don't trigger the queue check or increment `publishCountThisSecond` (L223). Note: `publishCountThisSecond++` already exists at L223 after successful publish — do NOT add a duplicate increment. Only add the guard check:
    ```cpp
    if (mqttConfig.publishRateLimit > 0) {
        unsigned long now = HAL::getMillis();
        if (now - lastPublishTime >= 1000) {
            publishCountThisSecond = 0;
            lastPublishTime = now;
        }
        if (publishCountThisSecond >= mqttConfig.publishRateLimit) {
            DLOG_W(LOG_MQTT, "Publish rate limit reached (%u/s), dropping message for '%s'",
                   mqttConfig.publishRateLimit, topic.c_str());
            stats.publishErrors++;
            return false;
        }
    }
    ```

- [x] Task 18: Add subscription count check in `subscribe()`
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h` (L255-278)
  - Action: After the duplicate check loop (L256-261), before adding subscription, add:
    ```cpp
    if (mqttConfig.maxSubscriptions > 0 && subscriptions.size() >= mqttConfig.maxSubscriptions) {
        DLOG_W(LOG_MQTT, "Max subscriptions reached (%u/%u), rejecting '%s'",
               (unsigned)subscriptions.size(), mqttConfig.maxSubscriptions, topic.c_str());
        return false;
    }
    ```

- [x] Task 19: Remove `@warning Not enforced` comments from MQTTConfig
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h` (L112-116)
  - Action: Remove `@warning Not enforced at runtime` comments from `maxQueueSize`, `publishRateLimit`, `maxSubscriptions` fields. Update docstrings to document the 0=unlimited behavior.

- [x] Task 20: Add MQTT limit enforcement tests
  - File: `DomoticsCore-MQTT/test/test_mqtt_component/test_mqtt_component.cpp`
  - Action: Add tests:
    - `test_mqtt_queue_rejects_when_full()` — set `maxQueueSize=5`, queue 5 messages (disconnected), verify 6th returns false
    - `test_mqtt_queue_unlimited_when_zero()` — set `maxQueueSize=0`, queue 100 messages, all succeed
    - `test_mqtt_subscribe_rejects_at_limit()` — set `maxSubscriptions=3`, subscribe 3, verify 4th returns false
    - `test_mqtt_subscribe_unlimited_when_zero()` — set `maxSubscriptions=0`, subscribe 100, all succeed
    - `test_mqtt_rate_limit_enforced()` — set `publishRateLimit=2`, publish 3 in same millisecond, verify 3rd returns false
    - `test_mqtt_rate_limit_unlimited_when_zero()` — set `publishRateLimit=0`, rapid publishes all succeed

#### R20 — Wire otaPassword to OTAConfig

- [x] Task 21: Wire SystemConfig.otaPassword to OTAComponent
  - File: `DomoticsCore-System/include/DomoticsCore/System.h` (L347-355)
  - Action: Replace `core.addComponent(std::make_unique<Components::OTAComponent>())` with:
    ```cpp
    Components::OTAConfig otaConfig;
    otaConfig.bearerToken = config.otaPassword;
    core.addComponent(std::make_unique<Components::OTAComponent>(otaConfig));
    ```

- [x] Task 22: Verify OTAComponent actually uses bearerToken, then add wiring test
  - File: `DomoticsCore-OTA/include/DomoticsCore/OTA.h` (and any .cpp implementation)
  - Action (prerequisite): **Audit `OTAComponent::onInit()`/`onStart()` to confirm `bearerToken` is used** for HTTP OTA authentication. Based on header review, `bearerToken` is declared in `OTAConfig` but may not be consumed by the implementation. If unused, add the authentication logic (e.g., `httpUpdate.setAuthorization(config.bearerToken.c_str())`) before wiring is meaningful. If the OTA library doesn't support bearer auth, use `basicAuthUser`/`basicAuthPassword` fields instead and wire `otaPassword` to `basicAuthPassword`.
  - Action (test): Verify that creating a System with `config.otaPassword = "secret"` results in OTAComponent receiving that value. If direct test is not feasible (System creates components internally), at minimum test that `OTAComponent(OTAConfig{.bearerToken="secret"})` stores the token correctly.

#### R21 — RemoteConsole Telnet Authentication

- [x] Task 23: Add per-client auth state tracking with unique client IDs
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` (L64-82)
  - Action: Replace dead `bool authenticated` (L78) and refactor client storage for unique IDs:
    ```cpp
    uint32_t nextClientId = 1;  // Incrementing counter — NOT remoteIP(). Wraps after 4B connections (accepted risk for IoT)
    std::vector<std::pair<uint32_t, WiFiClient>> clients;  // replaces std::vector<WiFiClient>
    std::map<uint32_t, bool> clientAuthenticated;
    std::map<uint32_t, unsigned long> clientConnectTime;
    std::map<uint32_t, String> clientBuffers;  // already exists, key changes from IP to counter
    ```
  - Action: Add config field for auth timeout:
    ```cpp
    uint32_t authTimeoutMs = 10000;  // 10s default
    ```
  - Notes: Using `remoteIP()` as client key is unsafe — multiple clients behind NAT or from the same host share the same IP, causing auth state collisions. An incrementing counter guarantees uniqueness. All loops iterating `clients` must be updated from `WiFiClient&` to `auto& [cid, client]` structured bindings (or `.first`/`.second`).

- [x] Task 24: Add `auth` to help text (NOT to commands map)
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` (L361-466)
  - Action: Do NOT register `auth` in the `commands` map — it is handled directly in `handleClient()` (Task 25). Instead, add `auth` to the `help` command output text manually:
    ```cpp
    // In the help command handler, add line:
    result += "  auth <password>  - Authenticate (if auth required)\n";
    ```
  - Notes: This avoids a split-brain design where auth lives in two places. The `auth` command is fully handled in `handleClient()` where client ID context is available.

- [x] Task 25: Add auth check in `handleClient()`
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` (L549)
  - Notes: `cmd` and `args` are both `String` type (parsed via `line.substring()`). `cmd == "auth"` and `config.password == args` are `String::operator==` comparisons — safe.
  - Action: Before command execution at L549, add:
    ```cpp
    // Handle auth command specially (needs client context)
    if (cmd == "auth") {
        if (!config.requireAuth) {
            client.println("Authentication not required.");
        } else if (config.password == args) {
            clientAuthenticated[clientId] = true;
            client.println("Authentication successful!");
        } else {
            client.println("Authentication failed.");
        }
        client.print("> ");
        continue;
    }

    // Block commands if not authenticated (allow help + quit for discoverability)
    if (config.requireAuth && !clientAuthenticated[clientId]
        && cmd != "help" && cmd != "quit") {
        client.println("Authentication required. Use: auth <password>");
        client.print("> ");
        continue;
    }

    // Block commands if allowCommands is false (except auth, help, quit)
    if (!config.allowCommands && cmd != "help" && cmd != "quit") {
        client.println("Commands are disabled.");
        client.print("> ");
        continue;
    }
    ```

- [x] Task 26: Modify `sendWelcome()` for auth prompt
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` (L478-495)
  - Action: If `config.requireAuth`, show auth prompt instead of logs:
    ```cpp
    if (config.requireAuth) {
        client.println("Authentication required. Use: auth <password>\n");
    } else {
        client.println("Type 'help' for available commands\n");
        // Show recent logs only if not requiring auth
        auto recent = getRecentLogs(10);
        // ... existing log display code ...
    }
    ```

- [x] Task 27: Add auth timeout check in `onLoop()`
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` (client loop section)
  - Action: In the client iteration loop, check timeout for unauthenticated clients:
    ```cpp
    if (config.requireAuth) {
        unsigned long now = HAL::getMillis();
        for (auto it = clients.begin(); it != clients.end(); ) {
            uint32_t cid = it->first;
            WiFiClient& client = it->second;
            if (!clientAuthenticated[cid] && (now - clientConnectTime[cid]) > config.authTimeoutMs) {
                client.println("Authentication timeout. Disconnecting.");
                client.stop();
                clientAuthenticated.erase(cid);
                clientConnectTime.erase(cid);
                clientBuffers.erase(cid);
                it = clients.erase(it);
            } else {
                ++it;
            }
        }
    }
    ```

- [x] Task 28: Initialize auth state on client connect
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h` (L188-211)
  - Action: Replace `clients.push_back(newClient)` with ID-based insertion:
    ```cpp
    uint32_t clientId = nextClientId++;
    clients.push_back({clientId, newClient});
    clientAuthenticated[clientId] = !config.requireAuth;  // Auto-auth if no auth required
    clientConnectTime[clientId] = HAL::getMillis();
    clientBuffers[clientId] = "";
    ```
  - Notes: Remove old `uint32_t clientId = (uint32_t)newClient.remoteIP()` assignment.

- [x] Task 29: Clean up auth state on client disconnect
  - File: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
  - Action: Wherever clients are removed (disconnect handling, quit command), also erase from `clientAuthenticated` and `clientConnectTime` maps.

- [x] Task 30: Add RemoteConsole auth tests
  - File: `DomoticsCore-RemoteConsole/test/test_remoteconsole_component/test_remoteconsole_component.cpp`
  - Action: Add tests:
    - `test_remoteconsole_auth_blocks_commands()` — `requireAuth=true`, verify commands rejected before auth
    - `test_remoteconsole_auth_correct_password()` — verify `auth <password>` grants access
    - `test_remoteconsole_auth_wrong_password()` — verify wrong password rejected
    - `test_remoteconsole_auth_not_required()` — `requireAuth=false`, commands work immediately
    - `test_remoteconsole_allow_commands_false()` — verify only help/quit/auth work
    - `test_remoteconsole_auth_timeout()` — verify client disconnected after timeout
    - `test_remoteconsole_timeout_during_active_clients()` — connect 3 clients, authenticate client 1, let client 2 timeout, verify client 1 and client 3 remain connected and functional after timeout pass

#### R14-R16 — Document Accepted Exceptions

- [x] Task 31: Add MemoryManager singleton exception comment (R14)
  - File: `DomoticsCore-Core/include/DomoticsCore/MemoryManager.h` (L112)
  - Action: Add comment above `instance()`:
    ```cpp
    // Accepted deviation from Constitution XIII (no singleton abuse):
    // MemoryManager requires global access for buffer sizing decisions across all components.
    // No viable alternative without passing MemoryManager& through every component constructor.
    ```

- [x] Task 32: Add MQTT static instance exception comment (R15)
  - File: `DomoticsCore-MQTT/include/DomoticsCore/MQTT.h` (L424)
  - Action: Add comment above static callback declarations:
    ```cpp
    // Accepted deviation from Constitution XIII (no singleton abuse):
    // PubSubClient's C-style callback (void(*)(char*, byte*, unsigned int)) does not support
    // user-data pointers. Static instance is required for callback routing.
    // Consider std::function wrapper if PubSubClient is ever replaced.
    ```

- [x] Task 33: Add System __has_include deviation comment (R16)
  - File: `DomoticsCore-System/include/DomoticsCore/System.h` (L32)
  - Action: Add comment above first `__has_include`:
    ```cpp
    // Intentional deviation from Constitution IX (no #ifdef outside HAL):
    // __has_include() enables the zero-config "just add components" developer experience.
    // This is compile-time optional dependency detection, not platform-specific feature-flagging.
    // 20 directives across: initial includes, component registration, event orchestration, console commands.
    ```

#### Tracking — Fix Stale Roadmap Statuses

- [x] Task 34: Update CODE-ROADMAP.md tracking tables
  - File: `docs/CODE-ROADMAP.md`
  - Action: In both tracking tables (L263 and L432):
    - M12: Change from `TODO` to `DONE` (metadata.name already "LED" since batch 1)
    - R9: Change from `TODO` to `DONE` (fixed in commit ab026e2)
    - R10: Change from `TODO` to `DONE` (fixed via R23 in commit ab026e2)
    - R6: Change from `DONE` to `IN PROGRESS` (erroneously marked DONE — code still has String fields). Change to `DONE` after this batch is implemented and verified.
    - R14-R16: Update to `DONE — documented as accepted exceptions`
    - R18: Update to `DONE — limits enforced`
    - R20: Update to `DONE — wired to OTAConfig`
    - R21: Update to `DONE — auth implemented`
    - R25: Update to `DONE — operator types fixed`
  - Action: Add M12 status note: "Already fixed in code (metadata.name = 'LED'). Tracking was stale."

### Acceptance Criteria

#### R6 — HAConfig char[]

- [x] AC-1: Given a default-constructed `HAConfig`, when inspecting fields, then `nodeId` == `"myDeviceId"`, `deviceName` == `"My Device"`, `manufacturer` == `"DomoticsCore"`, `model` == `"MyDeviceModel"`, `swVersion` == `"1.0.0"`, `discoveryPrefix` == `"homeassistant"`, `availabilityTopic` == `""`, `configUrl` == `""`, `suggestedArea` == `""`.
- [x] AC-2: Given a `HAConfig` with `nodeId="test"` and `discoveryPrefix="homeassistant"`, when the `HomeAssistantComponent` constructor runs, then `availabilityTopic` == `"homeassistant/test/availability"`.
- [x] AC-3: Given `HA::setField(buf, "a_very_long_string_exceeding_buffer", 10)`, when called, then `buf` contains `"a_very_lo"` (9 chars + null) and a `DLOG_W` warning is emitted.
- [x] AC-4: Given `HA::setField(buf, NULL, 10)`, when called, then `buf[0] == '\0'`.
- [x] AC-5: Given `System.h` with `config.deviceName = "My Device Name"`, when HAConfig nodeId is generated, then `haConfig.nodeId` == `"my_device_name"` (lowercase, spaces replaced with underscores, truncated to `HA::MAX_NODE_ID - 1`).
- [x] AC-6: Given the WebUI POST handler receives a `node_id` longer than `HA::MAX_NODE_ID - 1`, when processed, then the value is truncated via `HA::setField` and no buffer overflow occurs.
- [x] AC-7: Given a HeapTracker checkpoint before and after creating 10 `HAConfig` instances, when measured, then heap delta is 0 (all storage is stack/struct, not heap).
- [x] AC-8: Given all existing HA tests updated for char[] API, when running the full test suite, then all tests pass.
- [x] AC-32: Given HAConfig with `nodeId="custom_device"` saved via `putString("ha_nodeid", ...)`, when loaded via `loadHomeAssistantConfig()` with `HA::setField`, then `haConfig.nodeId` == `"custom_device"` (persistence round-trip).

#### R25 — AlarmFeature Type Safety

- [x] AC-9: Given `AlarmFeature::ArmAway | AlarmFeature::ArmHome`, when evaluated, then the result type is `AlarmFeature` (not `uint8_t`).
- [x] AC-10: Given `AlarmFeature features = AlarmFeature::ArmAway | AlarmFeature::ArmHome`, when `if (features & AlarmFeature::ArmAway)` is evaluated, then it compiles and returns `true`.
- [x] AC-11: Given `AlarmFeature features = AlarmFeature::ArmAway`, when `features & AlarmFeature::ArmHome` is evaluated, then it returns `false`.
- [x] AC-12: Given all existing alarm panel tests updated, when running the test suite, then all tests pass without `static_cast<uint8_t>` on operator results.

#### R18 — MQTT Config Limits

- [x] AC-13: Given `maxQueueSize=5` and MQTT disconnected, when publishing 6 messages, then the first 5 succeed (return true) and the 6th returns false with a `DLOG_W` warning.
- [x] AC-14: Given `maxQueueSize=0` and MQTT disconnected, when publishing 100 messages, then all 100 succeed (0 = unlimited).
- [x] AC-15: Given `maxSubscriptions=3`, when subscribing to 4 topics, then the first 3 succeed and the 4th returns false.
- [x] AC-16: Given `maxSubscriptions=0`, when subscribing to 100 topics, then all succeed (0 = unlimited).
- [x] AC-17: Given `publishRateLimit=2` and MQTT connected, when publishing 3 messages within the same second, then the first 2 succeed and the 3rd returns false.
- [x] AC-18: Given `publishRateLimit=0` and MQTT connected, when publishing rapidly, then all succeed (0 = unlimited).
- [x] AC-33: Given `maxQueueSize=5` and queue full (5 messages), when `processMessageQueue()` drains 3 messages, then publishing 3 new messages succeeds (queue accepts new messages after drain).
- [x] AC-36: Given default `MQTTConfig` (maxQueueSize=100, publishRateLimit=10, maxSubscriptions=50), when enforcement is active, then limits are applied at the existing default values (100 queue / 10 msg/s / 50 subs). These defaults are reasonable for IoT and were already documented in the config — enforcement makes them real. No default change needed.

#### R20 — OTA Password Wiring

- [x] AC-19: Given `SystemConfig` with `otaPassword = "mysecret"`, when `registerOTAComponent()` runs, then `OTAComponent` is created with `otaConfig.bearerToken == "mysecret"`.
- [x] AC-20: Given `SystemConfig` with `otaPassword = ""` (default), when `registerOTAComponent()` runs, then `OTAComponent` is created with empty `bearerToken` (no auth required).

#### R21 — RemoteConsole Authentication

- [x] AC-21: Given `requireAuth=true` and `password="secret"`, when a client connects and sends a command without authenticating, then the command is rejected with "Authentication required" message.
- [x] AC-22: Given `requireAuth=true` and `password="secret"`, when a client sends `auth secret`, then the client is authenticated and subsequent commands execute normally.
- [x] AC-23: Given `requireAuth=true` and `password="secret"`, when a client sends `auth wrongpassword`, then authentication fails and the client remains unauthenticated.
- [x] AC-24: Given `requireAuth=false`, when a client connects and sends a command, then the command executes immediately (no auth prompt).
- [x] AC-25: Given `allowCommands=false`, when an authenticated client sends a command other than `help`/`quit`/`auth`, then the command is rejected with "Commands are disabled" message.
- [x] AC-26: Given `requireAuth=true` and `authTimeoutMs=10000`, when a client connects but does not authenticate within 10 seconds, then the client is disconnected with a timeout message.
- [x] AC-27: Given `requireAuth=true`, when a client connects, then the welcome message shows "Authentication required" instead of recent logs and help prompt.
- [x] AC-34: Given `requireAuth=true`, when client A authenticates successfully and client B connects from the **same IP** without authenticating, then client B's commands are still rejected (per-client auth isolation — client IDs are unique counters, not IP-based).
- [x] AC-35: Given `requireAuth=true` and 3 connected clients, when client 2 times out and is disconnected, then clients 1 and 3 remain connected and functional (no iterator/map corruption from mid-list erasure).

#### R14-R16 — Exception Documentation

- [x] AC-28: Given `MemoryManager.h`, when inspecting `instance()` method, then an accepted deviation comment explains why singleton is justified.
- [x] AC-29: Given `MQTT.h`, when inspecting static callback/instance, then an accepted deviation comment explains the PubSubClient constraint.
- [x] AC-30: Given `System.h`, when inspecting `__has_include` directives, then an intentional deviation comment explains the zero-config design choice.

#### Tracking

- [x] AC-31: Given `CODE-ROADMAP.md`, when inspecting tracking tables, then M12, R9, R10 are marked DONE with notes. R6, R14-R16, R18, R20, R21, R25 are updated to reflect completion.

## Additional Context

### Dependencies

- No external dependencies. All changes are internal to existing components.
- R20 requires both System.h and OTA.h to be modified in the same batch.

### Testing Strategy

- Unity test framework on native platform
- HeapTracker for R6 memory validation
- **R6 truncation tests**: verify `HA::setField` truncates correctly and logs warning when input exceeds buffer
- **R6 constructor tests**: verify default values populated correctly; verify `availabilityTopic` auto-generated as `"homeassistant/mydeviceid/availability"`; verify with truncated nodeId that availabilityTopic uses the truncated value
- **R6 nodeId processing test**: `"My Device Name"` → `"my_device_name"` (lowercase + space→underscore)
- **R18 per-limit tests**: queue full → message rejected; rate limit → publish throttled; max subscriptions → subscribe rejected. Edge case: limit=0 → unlimited (no rejection)
- **R21 auth tests**: no password → rejected; wrong password → rejected; correct password → access granted; auth timeout → disconnected; `allowCommands=false` → commands blocked after successful auth
- **R20 wiring test**: `SystemConfig.otaPassword = "secret"` → verify `OTAConfig` receives value after assembly
- **R25 type safety test**: verify `AlarmFeature::ArmAway | AlarmFeature::ArmHome` returns `AlarmFeature` type; verify `(features & AlarmFeature::ArmHome)` usable in `if` (returns `bool`)
- Existing test suites must continue passing

### Batch Scope Rationale

This batch bundles 7 remediation items into a single spec. Rationale:
- **R6 is the only high-risk item** — the others (R14-R16 comments, R18 guards, R20 3-line wiring, R25 operator fix) are individually trivial and don't justify separate spec overhead.
- **Task order is fail-fast**: R6 is implemented first. If it causes regressions, the remaining items (R18, R20, R21, R25) are independent and can be cherry-picked or reverted independently.
- **R21 (Telnet auth) is medium complexity** but fully isolated to RemoteConsole with no cross-component dependencies.
- If the batch proves too large during implementation, split after R6 (commit point) and defer R21 to a follow-up.

### Task Order

Per dev review, implementation order is: R6 (biggest, API-breaking — fail fast) → R25 (small, isolated) → R18 (MQTT isolated) → R20 (small wiring) → R21 (medium, RemoteConsole) → R14-R16 (trivial comments) → tracking fixes (docs only). **Commit after R6 to create a safe rollback point before continuing.**

### Notes

- R6 is a breaking API change (String→char[]). Requires minor version bump.
- **R6 migration risk**: Existing persisted String values from pre-migration firmware may exceed the new `char[]` buffer limits. `HA::setField` will silently truncate at load time with a `DLOG_W`. A truncated `nodeId` produces a *different* `availabilityTopic`, causing the device to appear as a new entity in Home Assistant. Document this in release notes and recommend users re-save configuration via WebUI after upgrade. Consider adding a one-time migration log at boot: `DLOG_W("HA", "Config migrated from String to char[] — verify HA entity mapping")`.
- R6 constants and `HA::setField` helper must be placed **after** includes in HomeAssistant.h (DLOG_W dependency on Logger.h).
- **R6 size idiom**: Use `sizeof(field)` inside the HAConfig struct (constructor, member functions). Use `HA::MAX_*` constants outside the struct (System.h, SystemPersistence.h, WebUI, examples). Both resolve to the same value — `sizeof` is preferred inside the struct for self-documentation; named constants are preferred outside where the field isn't directly accessible.
- R18 rate limit uses a **tumbling window** (reset counter every 1000ms), not a sliding window. This is burst-friendly — acceptable for IoT use case. Document in MQTTConfig docstring.
- R21 auth is basic cleartext — document security limitations clearly.
- R21 `auth` command handled exclusively in `handleClient()` (not in commands map) — added to help text manually.
- R21 auth timeout check must execute as a **separate pass** before the handleClient loop in `onLoop()`. Although the erase-while-iterating pattern (`it = clients.erase(it)`) is itself safe, mixing timeout disconnection with the handleClient data-processing loop would risk processing a partially-disconnected client. The timeout pass runs first, then the handleClient pass iterates only live clients. Both passes use the same `std::vector<std::pair<uint32_t, WiFiClient>>` container.
- R25 compile alarm panel tests first after Tasks 12-13 to catch any missed usage sites before proceeding.
- Roadmap tracking fixes are documentation-only, no code changes.
