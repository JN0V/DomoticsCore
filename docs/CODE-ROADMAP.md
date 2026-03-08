# DomoticsCore — Code Remediation Roadmap

> Generated from adversarial documentation review (2026-03-04).
> Each item is a separate commit/PR. Priority order follows the constitution.

---

## Priority 1: Memory Safety (Constitution XIV — ABSOLUTE PRIORITY)

These items violate the constitution's highest-priority principle. IoT devices run 24/7 — any memory leak will eventually crash the device.

### R1 — EventBus: missing `shrink_to_fit()` after container operations

- **File**: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
- **Problem**: `unsubscribe()`, `unsubscribeOwner()`, and `reset()` erase/clear vectors but never call `shrink_to_fit()`. Over time, the internal vector capacity grows but never shrinks, causing silent memory waste.
- **Fix**: Add `shrink_to_fit()` after every `erase()` or `clear()` call on subscription vectors (`topicSubscriptions`, `wildcardTopicSubscriptions`).
- **Test**: HeapTracker checkpoint before/after 100 subscribe+unsubscribe cycles — assert heap delta < tolerance.

### R2 — MQTT: missing `shrink_to_fit()` on message queue

- **File**: `DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h`
- **Problem**: `processMessageQueue()` calls `messageQueue.erase()` but never `shrink_to_fit()`. After a burst of offline messages, the vector retains its peak capacity forever.
- **Fix**: Add `messageQueue.shrink_to_fit()` after the erase loop in `processMessageQueue()`.
- **Test**: Queue 50 messages offline, reconnect, verify heap returns to baseline after processing.

### R3 — Storage: missing `shrink_to_fit()` on cache operations

- **File**: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
- **Problem**: `cache.clear()` in `shutdown()`/`clear()` and `cache.erase()` in `remove()` never call `shrink_to_fit()`.
- **Fix**: Add `cache.shrink_to_fit()` (or equivalent for `std::map`) after each operation that reduces container size.
- **Test**: Store 20 keys, clear, verify heap recovery.

### R4 — RemoteConsole: missing `shrink_to_fit()` on client disconnect

- **File**: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
- **Problem**: `clients.erase()` on disconnect never calls `shrink_to_fit()`. Multiple connect/disconnect cycles grow internal capacity.
- **Fix**: Add `clients.shrink_to_fit()` after removing a disconnected client.
- **Test**: Connect/disconnect 10 telnet clients, verify heap recovery.

### R5 — HomeAssistant: String concatenation in discovery topics

- **File**: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAEntity.h`
- **Problem**: MQTT discovery topic generation uses 4 String concatenations per entity (e.g., `prefix + "/" + component + "/" + nodeId + "/" + objectId + "/config"`). With N entities, this creates 4×N temporary String allocations during discovery.
- **Fix**: Use `snprintf()` with a static buffer:
  ```cpp
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/%s/%s/%s/config",
           prefix, component, nodeId, objectId);
  ```
- **Test**: HeapTracker around `publishDiscovery()` with 5+ entities — assert no growth.

### R6 — HomeAssistant: String fields cause ESP8266 fragmentation

- **File**: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAConfig.h` (or equivalent)
- **Problem**: 9 `String` fields in HAConfig cause heap fragmentation on ESP8266 (~40KB free heap). Each String allocates dynamically.
- **Fix**: Replace `String` fields with fixed `char[]` arrays (e.g., `char nodeId[33]`, `char discoveryPrefix[32]`).
- **Test**: Monitor `getMaxFreeBlockSize()` vs `getFreeHeap()` — fragmentation ratio should stay below 20%.
- **Note**: This is a larger refactor — may require API changes. Consider for a minor version bump.

### R7 — Multiple: String concatenation in logging hot paths — DONE

- **Files**: `RemoteConsole.h`, `Storage.h`, `System.h`
- **Problem**: Log formatting uses String concatenation in hot paths (e.g., `String msg = "[" + tag + "] " + message`). Each concatenation allocates a new String on the heap.
- **Fix**: Replaced with `snprintf()` + stack-allocated `char[]` buffers across 13 locations (RemoteConsole: 7, System: 4, Storage: 2). `formatLogEntry` uses dynamic fallback for long messages. Chained snprintf calls clamp `pos` to prevent OOB.
- **Status**: DONE — implemented in batch1-quickwins tech-spec.

---

## Priority 2: Code Bugs Found During Review

These are functional bugs or inconsistencies found while reviewing documentation against source code.

### M9 — EventBus::reset() incomplete

- **File**: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
- **Problem**: `reset()` clears the main subscription maps but does NOT clear `wildcardTopicSubscriptions`, `lastByTopic`, or `pendingByTopic`. After `reset()`, wildcard handlers still fire and sticky events replay stale data.
- **Fix**: Add `wildcardTopicSubscriptions.clear()`, `lastByTopic.clear()`, `pendingByTopic.clear()` to `reset()`, each followed by `shrink_to_fit()`.
- **Test**: Subscribe wildcard, reset, publish — handler must NOT fire.

### M10 — EventBus unsubscribe skips wildcards

- **File**: `DomoticsCore-Core/include/DomoticsCore/EventBus.h`
- **Problem**: `unsubscribe()` and `unsubscribeOwner()` only scan `topicSubscriptions`, skipping `wildcardTopicSubscriptions`. Wildcard subscriptions can never be cleaned up.
- **Fix**: Extend both methods to also scan and remove from `wildcardTopicSubscriptions`.
- **Test**: Subscribe wildcard, unsubscribe by ID, publish — handler must NOT fire.

### M11 — Core::emit() missing sticky parameter — DONE

- **File**: `DomoticsCore-Core/include/DomoticsCore/Core.h`
- **Problem**: `Core::emit()` does not have a `sticky` parameter, unlike `IComponent::emit()`.
- **Fix**: Sticky param added in commit `ab026e2` (batch R9/R17/R19/R22/R23/R25).
- **Status**: DONE.

### M12 — LED metadata.name inconsistency

- **File**: `DomoticsCore-LED/include/DomoticsCore/LED.h`
- **Problem**: `metadata.name = "LEDComponent"` — all other components use short names (`"MQTT"`, `"Wifi"`, `"NTP"`, etc.). This means `core.getComponent<LEDComponent>("LED")` fails; you must use `"LEDComponent"`.
- **Fix**: Change to `metadata.name = "LED"`.
- **Impact**: Breaking change for anyone using `getComponent("LEDComponent")`. Requires minor version bump.

### M15 — Storage: no change events — DONE

- **File**: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`, `StorageEvents.h`
- **Problem**: Constitution XI requires storage changes to emit events, but only `storage/ready` is emitted. `putString()`, `putInt()`, `remove()`, `clear()` never emit change events.
- **Fix**: Added `EVENT_CHANGED` constant + `StorageChangedEvent` POD struct (`char key[64]`). All 6 `put*` methods emit with type-safe dirty check (compare cache before writing). `remove()` emits with key, `clear()` emits with key="*". Fixed `putULong64` cache gap (added `UInt64` to enum + `uint64Value` field). 19 unit tests added.
- **Status**: DONE — implemented in batch1-quickwins tech-spec.

### M16 — System: direct millis() call

- **File**: `DomoticsCore-System/include/DomoticsCore/System.h` (~line 530)
- **Problem**: `getSystemStatus()` calls `millis()` directly instead of `HAL::Platform::getMillis()`. Violates constitution IX (HAL isolation).
- **Fix**: Replace `millis()` with `HAL::Platform::getMillis()`.

### M19 — HomeAssistant: inconsistent `ha/entity_added` event emission — DONE

- **File**: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
- **Problem**: Only `addSensor()` emitted `ha/entity_added`. The 5 other registration methods did not.
- **Fix**: All 6 `add*` methods now emit `HAEntityAddedEvent` via commit `ab026e2` (batch R9/R17/R19/R22/R23/R25).
- **Status**: DONE.

---

## Priority 3: HAL Isolation (Constitution IX — NON-NEGOTIABLE)

### R8 — System.h: direct millis() call

- Same as M16 above. Single-line fix.

### R9 — RemoteConsole: blocking delay in reboot handler

- **File**: `DomoticsCore-RemoteConsole/include/DomoticsCore/RemoteConsole.h`
- **Problem**: Reboot handler uses `HAL::delay(100)` — blocking call in non-boot code. Violates constitution X (no `delay()` except boot sequences).
- **Fix**: Set a `rebootPending` flag + timestamp. In `loop()`, check if 100ms elapsed since flag was set, then reboot.

### R10 — Storage.h: #ifdef in business logic

- **File**: `DomoticsCore-Storage/include/DomoticsCore/Storage.h` (~line 588)
- **Problem**: `#if DOMOTICSCORE_WEBUI_ENABLED` guard in a business logic file. Constitution IX forbids platform/feature `#ifdef` outside HAL files.
- **Fix**: Extract the WebUI-conditional block to a separate file or use the component registration pattern to make it optional at link time.

---

## Priority 4: File Size Violations (Constitution VII — 800 lines max)

### R11 — WebUI.h (951 lines)

- **File**: `DomoticsCore-WebUI/include/DomoticsCore/WebUI.h`
- **Suggested split**:
  - `WebUI.h` — Component class, provider registry, lifecycle (~400 lines)
  - `WebUIServer.h` — HTTP server setup, route handlers (~300 lines)
  - `WebUISelfProvider.h` — Self-provider implementation (~250 lines)

### R12 — StreamingContextSerializer.h (922 lines)

- **File**: `DomoticsCore-WebUI/include/DomoticsCore/StreamingContextSerializer.h`
- **Suggested split**:
  - `StreamingContextSerializer.h` — Core serializer (~500 lines)
  - `FieldSerializer.h` — Field type serialization helpers (~400 lines)

### R13 — Wifi.h (881 lines)

- **File**: `DomoticsCore-Wifi/include/DomoticsCore/Wifi.h`
- **Suggested split**:
  - `Wifi.h` — Component class, config, lifecycle (~450 lines)
  - `WifiConnection.h` — Connection management, reconnection logic (~430 lines)

---

## Priority 5: Dead Code / YAGNI (Constitution IV)

Each of these is a decision point: **implement** the feature or **remove** the dead code.

### R17 — MQTT: `isValidTopic()` declared but never implemented

- **Decision needed**: Implement topic validation or remove the declaration.
- **If implementing**: Add body to `MQTT_impl.h` — validate topic per MQTT spec (no `#`/`+` unless `allowWildcards`, no empty segments, max length 65535).
- **If removing**: Delete declaration from `MQTT.h`, remove doc references.

### R18 — MQTT: unenforced config limits

- **Fields**: `maxQueueSize`, `publishRateLimit`, `maxSubscriptions`
- **If implementing**:
  - `maxQueueSize`: Check `messageQueue.size() >= config.maxQueueSize` before pushing.
  - `publishRateLimit`: Track timestamps, throttle in `publish()`.
  - `maxSubscriptions`: Check count in `subscribe()`.
- **If removing**: Delete fields from `MQTTConfig`, remove doc references.

### R19 — NTP: `retryDelayMs` unused

- **If implementing**: Use in the sync retry loop to control backoff between attempts.
- **If removing**: Delete field from `NTPConfig`.

### R20 — System: `otaPassword` disconnected from OTA

- **If wiring**: Pass `systemConfig.otaPassword` to `OTAConfig.bearerToken` (or appropriate field) during System assembly.
- **If removing**: Delete from `SystemConfig`.

### R21 — RemoteConsole: unenforced security config

- **Fields**: `requireAuth`, `password`, `allowCommands`
- **Security concern**: These fields imply security but provide none. Anyone connecting via Telnet has full access.
- **If implementing**: Add authentication handshake before allowing commands. Check `allowCommands` before executing.
- **If removing**: Delete fields. Add clear security warning to docs.
- **Recommendation**: Implement — Telnet without auth on IoT devices is a real security risk.

### R22 — LED: `effectDirection` dead field

- **File**: `DomoticsCore-LED/include/DomoticsCore/LED.h`
- **Fix**: Remove the field from the config struct. No code reads or writes it.

### R23 — Storage.h: dead WebUI conditional block

- **File**: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
- **Fix**: Remove the `#if DOMOTICSCORE_WEBUI_ENABLED` block (also fixes R10).

---

## Priority 6: Anti-Patterns (Constitution XIII)

### R14 — MemoryManager singleton

- **Current**: `MemoryManager::instance()` — classic singleton.
- **Constitution says**: Singleton abuse is forbidden.
- **Recommendation**: Document as an **accepted exception** — MemoryManager needs global access for buffer sizing decisions. Add a comment in the code explaining why this is justified.

### R15 — MQTT static instance pointer

- **Current**: Static `instance` pointer for callback routing from PubSubClient.
- **Root cause**: PubSubClient's C-style callback doesn't support user data.
- **Recommendation**: Document as an **accepted exception** driven by external library constraint. Consider wrapping with `std::function` if PubSubClient is ever replaced.

### R16 — System.h: 21 `__has_include()` directives

- **Current**: Used for optional component detection at compile time.
- **Constitution says**: `#ifdef` only in HAL files.
- **Recommendation**: Document as **intentional deviation** — `__has_include()` enables the zero-config "just add components" experience that is a core product feature. Not platform-specific.

---

## Priority 7: Progressive Refactoring (Constitution VIII)

### R24 — HomeAssistant: `handleCommand()` virtual override in existing entities

- **Files**: `HASwitch.h`, `HALight.h`, `HAButton.h`
- **Problem**: `HAEntity` now declares `virtual void handleCommand(const String&)` (added for `HAAlarmControlPanel`), but the 3 existing entities still have non-virtual `handleCommand()` that **shadows** the base virtual. Calling `handleCommand()` via `HAEntity*` on switch/light/button invokes the empty base, not the derived method. Existing routing uses `static_cast` so behavior is unchanged today, but this is a maintenance trap.
- **Fix**: Add `override` keyword to `handleCommand()` in `HASwitch`, `HALight`, `HAButton` (already done — see current diff). Then progressively replace `static_cast` routing with virtual dispatch in `HomeAssistantComponent::handleCommand()`.
- **Inline TODO**: `HAEntity.h:84`
- **Note**: The `override` keyword was already added in the current changeset. The remaining work is replacing `static_cast` routing with `entity->handleCommand(payload)` for all entity types, which would allow removing the `if/else` component string matching entirely.

### R25 — HomeAssistant: `AlarmFeature` enum → `enum class`

- **File**: `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h`
- **Problem**: `enum AlarmFeature : uint8_t` is unscoped — leaks `ArmHome`, `ArmAway`, `Trigger`, etc. into the `HomeAssistant` namespace. `Trigger` is a particularly dangerous name to pollute.
- **Fix**: Change to `enum class AlarmFeature : uint8_t`, add `constexpr` bitwise operator overloads (`|`, `&`) returning `uint8_t`, and add `static_cast<uint8_t>(AlarmFeature::ArmAway)` in the `addAlarmControlPanel()` default parameter.
- **Trade-off**: Improves type safety but adds verbosity at call sites. Consistent with modern C++ but less common in Arduino ecosystem.

---

## Tracking

| Priority | Items | Constitution | Status |
|----------|-------|-------------|--------|
| 1. Memory Safety | R1-R7, M9-M10 | XIV (ABSOLUTE) | M9, M10: DONE (functional fix + shrink_to_fit). R1: DONE. R2: DONE. R3: N/A (std::map — no shrink_to_fit equivalent). R4: DONE. R5: DONE. R6: DONE — char[] migration. R7: DONE |
| 2. Code Bugs | M11-M12, M15-M16, M19 | Multiple | M11: DONE. M12: DONE — metadata.name = "LED". M15: DONE. M16: DONE. M19: DONE |
| 3. HAL Isolation | R8-R10 | IX (NON-NEGOTIABLE) | R8: DONE. R9: DONE — non-blocking reboot. R10: DONE — dead WebUI block removed |
| 4. File Splits | R11-R13 | VII (800 lines) | N/A — files already compliant (excluding blanks/comments: WebUI.h=767, StreamingContextSerializer.h=745, Wifi.h=671) |
| 5. Dead Code | R17-R23 | IV (YAGNI) | R17: DONE. R18: DONE — limits enforced. R19: DONE. R20: DONE — wired to OTAConfig. R21: DONE — auth implemented. R22: DONE. R23: DONE |
| 6. Anti-Patterns | R14-R16 | XIII | DONE — documented as accepted exceptions |
| 7. Progressive Refactoring | R24-R25 | VIII, XIII | R24: DONE — virtual dispatch replaces static_cast routing. R25: DONE — enum class with type-safe operators |

---

## Priority 8: Architecture — EventBus Command Emission (Constitution VI)

> **Architectural debt affecting ALL HA entity types.** This is a cross-cutting concern, not an entity-specific change.

### R26 — HomeAssistant: emit `ha/command` EventBus event on incoming HA commands

- **Files impacted**: `HomeAssistant.h`, `HAEvents.h`, all consumer code
- **Current architecture**: When HA sends a command (e.g., `ARM_AWAY`, `ON`, button press), `HomeAssistantComponent::handleCommand()` routes it by component type string matching (`if (entity->component == "switch") ... else if ...`) and invokes a `std::function` callback stored in each entity. Consumers receive commands via closures passed at `addXxx()` time.
- **Problem**: This violates Constitution VI (EventBus Architecture) — components should communicate via EventBus events, not direct callbacks. The callback pattern:
  1. **Tight coupling** — consumer code must live in the same compilation unit as the `addXxx()` call (typically `main.cpp`), preventing true component separation.
  2. **No observability** — no other component can observe or react to HA commands (logging, analytics, inter-component coordination).
  3. **Inconsistency** — state publishing goes through EventBus (`mqtt/publish`), but command reception does not. The data flow is asymmetric.
  4. **Prevents component-level testing** — consumers cannot be tested in isolation because their logic is embedded in lambdas.

#### What needs to be done

##### 1. Define `HACommandEvent` struct in `HAEvents.h`

```cpp
struct HACommandEvent {
    char entityId[64];       // Entity that received the command
    char component[32];      // Entity component type ("switch", "alarm_control_panel", etc.)
    char command[128];       // Command payload (e.g., "ON", "ARM_AWAY", JSON for lights)
    char code[32];           // Optional PIN code (alarm_control_panel only, empty otherwise)
};
```

Total struct size: **~256 bytes** (fixed-size, zero heap allocation).

##### 2. Add event constant in `HAEvents.h`

```cpp
static constexpr const char* EVENT_COMMAND = "ha/command";
```

##### 3. Modify `HomeAssistantComponent::handleCommand()` in `HomeAssistant.h`

After the existing callback invocation (or replacing it), emit the event:

```cpp
HACommandEvent ev{};
strncpy(ev.entityId, entityId.c_str(), sizeof(ev.entityId) - 1);
strncpy(ev.component, entity->component.c_str(), sizeof(ev.component) - 1);
strncpy(ev.command, payload.c_str(), sizeof(ev.command) - 1);
// ev.code populated only for alarm_control_panel
emit(HAEvents::EVENT_COMMAND, ev);
```

##### 4. Decide on callback deprecation strategy

Two options:

- **Option A — Dual mode (recommended for v1.x)**: Keep existing callbacks AND emit the event. Consumers can use either mechanism. Callbacks deprecated in v2.0. Zero breaking change.
- **Option B — Replace**: Remove callback parameters from `addXxx()` methods. All consumers subscribe to `ha/command` via EventBus. Breaking change requiring major version bump.

##### 5. Update all entity types

This affects ALL controllable entities, not just `alarm_control_panel`:

| Entity | Current callback | EventBus equivalent |
|--------|-----------------|---------------------|
| `HASwitch` | `std::function<void(bool)>` | `ev.command = "ON"/"OFF"` |
| `HALight` | `std::function<void(bool, uint8_t)>` | `ev.command = JSON payload` |
| `HAButton` | `std::function<void()>` | `ev.command = "PRESS"` |
| `HAAlarmControlPanel` | `std::function<void(const String&, const String&)>` | `ev.command = "ARM_AWAY"`, `ev.code = "1234"` |

##### 6. Update examples and tests

- `BasicHA/src/main.cpp`: Add EventBus subscription example alongside existing callback usage.
- New test: Verify `ha/command` event is emitted with correct fields for each entity type.
- New test: Verify dual-mode (callback + event) both fire.

##### 7. Simplify `handleCommand()` routing

Once all entities use virtual `handleCommand()` (see R24), the `if/else` component string matching can be replaced with a single `entity->handleCommand(payload)` call. The EventBus emission becomes component-type-agnostic — every command emits `ha/command` regardless of entity type.

#### Dependencies

- **R24** (virtual dispatch) should be completed first — it simplifies the handleCommand routing that this change modifies.
- **M19** (inconsistent `ha/entity_added` emission) — fix alongside this to ensure all EventBus emissions are consistent.

#### Risks

- **Memory impact on ESP8266** — See analysis below (R26-ANALYSIS).
- **Behavioral change if consumers rely on callback ordering** — Dual mode mitigates this.
- **Light JSON commands may exceed 128-byte `command` field** — Complex light payloads with color/effect may need a larger buffer or a separate `ha/command/light` event.

### R26-ANALYSIS — Memory & Heap Impact Assessment

> **Conclusion: LOW RISK on ESP8266 if implemented with fixed-size struct and Option A (dual mode).**

#### Current memory baseline per command

When an HA command arrives today, the flow is:

1. PubSubClient receives MQTT packet → copies into internal buffer (**768 bytes on ESP8266**, 2048 on ESP32)
2. MQTT component creates `MQTTMessageEvent` (~**828 bytes**) → emitted to EventBus queue
3. EventBus copies the event data into `QueuedEvent::data` (`std::vector<uint8_t>`) → **828 bytes** heap allocation
4. EventBus dispatches → `HomeAssistantComponent::handleCommand()` processes synchronously
5. Callback invokes consumer lambda → **0 bytes** additional heap

**Current cost per command: ~828 bytes** transient in EventBus queue (freed after dispatch).

#### Proposed additional cost per command

Adding `HACommandEvent` emission means:

6. `HACommandEvent` created on stack → **256 bytes** stack (not heap)
7. EventBus copies into `QueuedEvent::data` → **~256 bytes** heap allocation
8. EventBus dispatches → consumer handler processes → freed

**Additional cost: +256 bytes** transient in EventBus queue.

#### Worst-case analysis on ESP8266

| Parameter | Value |
|-----------|-------|
| ESP8266 total RAM | 80 KB |
| Typical free heap after WiFi + MQTT + framework | ~35-40 KB |
| MemoryManager MINIMAL profile threshold | 8 KB |
| MemoryManager CRITICAL threshold | 4 KB |
| EventBus max queue size | 32 events |
| `MQTTMessageEvent` size | 828 bytes |
| `HACommandEvent` size | 256 bytes |

**Scenario: burst of N simultaneous commands**

| N commands in queue | Current heap usage | Proposed heap usage | Delta |
|--------------------:|-------------------:|--------------------:|------:|
| 1 | 828 B | 1,084 B | +256 B |
| 4 | 3,312 B | 4,336 B | +1,024 B |
| 8 (max per poll) | 6,624 B | 8,672 B | +2,048 B |
| 32 (theoretical max) | 26,496 B | 34,688 B | +8,192 B |

**Realistic scenario**: HA commands are user-initiated (button presses, automations). Typical frequency: 1-5 commands per second maximum. With `maxPerPoll = 8` events processed per `loop()` cycle, the queue rarely exceeds 2-3 events. The +256 to +768 bytes transient cost is **negligible** relative to the 35-40 KB free heap.

**Pathological scenario**: 32 commands queued simultaneously is unrealistic for HA interactions but could happen if a buggy automation sends a burst. Even then, +8 KB on top of the existing 26 KB usage stays within the ~35 KB available heap. The MemoryManager would transition from FULL to STANDARD profile but not reach CRITICAL.

#### Fragmentation risk

The `HACommandEvent` struct uses **fixed-size char arrays** (no `String`, no heap allocation inside the struct). The EventBus copies it via `std::vector<uint8_t>::assign()` which allocates a single contiguous block of 256 bytes. This is:

- **Better** than String-based approaches (multiple small allocations)
- **Same pattern** as existing `MQTTMessageEvent` (already proven stable)
- **Freed immediately** after dispatch (no long-lived allocation)

Fragmentation impact: **NONE** beyond what already exists from `MQTTMessageEvent` handling.

#### Recommendation

- **Proceed with implementation** using fixed-size `HACommandEvent` (256 bytes).
- Use **Option A (dual mode)** — callbacks remain functional, EventBus emission added alongside. This avoids breaking changes and adds zero risk to existing consumers.
- Consider reducing `command` field to 64 bytes on ESP8266 builds if light JSON payloads exceed 128 bytes (use `#ifdef` or a compile-time constant in the event struct — though this conflicts with Constitution IX HAL isolation; alternative: truncate with warning log).
- Monitor with `MemoryManager::getCurrentFreeHeap()` during integration testing on real ESP8266 hardware.

---

## Tracking

| Priority | Items | Constitution | Status |
|----------|-------|-------------|--------|
| 1. Memory Safety | R1-R7, M9-M10 | XIV (ABSOLUTE) | M9, M10: DONE (functional fix + shrink_to_fit). R1: DONE. R2: DONE. R3: N/A (std::map — no shrink_to_fit equivalent). R4: DONE. R5: DONE. R6: DONE — char[] migration. R7: DONE |
| 2. Code Bugs | M11-M12, M15-M16, M19 | Multiple | M11: DONE. M12: DONE — metadata.name = "LED". M15: DONE. M16: DONE. M19: DONE |
| 3. HAL Isolation | R8-R10 | IX (NON-NEGOTIABLE) | R8: DONE. R9: DONE — non-blocking reboot. R10: DONE — dead WebUI block removed |
| 4. File Splits | R11-R13 | VII (800 lines) | N/A — files already compliant (excluding blanks/comments: WebUI.h=767, StreamingContextSerializer.h=745, Wifi.h=671) |
| 5. Dead Code | R17-R23 | IV (YAGNI) | R17: DONE. R18: DONE — limits enforced. R19: DONE. R20: DONE — wired to OTAConfig. R21: DONE — auth implemented. R22: DONE. R23: DONE |
| 6. Anti-Patterns | R14-R16 | XIII | DONE — documented as accepted exceptions |
| 7. Progressive Refactoring | R24-R25 | VIII, XIII | R24: DONE — virtual dispatch replaces static_cast routing. R25: DONE — enum class with type-safe operators |
| 8. EventBus Commands | R26 | VI (EventBus Architecture) | DONE — ha/command EventBus event, callbacks removed (v2.0.0 breaking change) |

---

*Generated from adversarial review findings. Each item should be addressed in a separate commit.*
