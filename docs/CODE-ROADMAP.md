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

### R7 — Multiple: String concatenation in logging hot paths

- **Files**: `RemoteConsole.h`, `Storage.h`, `System.h`
- **Problem**: Log formatting uses String concatenation in hot paths (e.g., `String msg = "[" + tag + "] " + message`). Each concatenation allocates a new String on the heap.
- **Fix**: Use `snprintf()` with static thread-local buffers:
  ```cpp
  static char buf[200];
  snprintf(buf, sizeof(buf), "[%s] %s", tag, message);
  ```
- **Test**: HeapTracker around 100 log operations — assert zero growth.

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

### M11 — Core::emit() missing sticky parameter

- **File**: `DomoticsCore-Core/include/DomoticsCore/Core.h` (or `Core.cpp`)
- **Problem**: `Core::emit()` does not have a `sticky` parameter, unlike `IComponent::emit()`. This means code using `core.emit()` directly cannot create sticky events.
- **Fix**: Add `bool sticky = false` parameter to `Core::emit()` and forward to `eventBus.publishSticky()` when true. Or document the difference explicitly.

### M12 — LED metadata.name inconsistency

- **File**: `DomoticsCore-LED/include/DomoticsCore/LED.h`
- **Problem**: `metadata.name = "LEDComponent"` — all other components use short names (`"MQTT"`, `"Wifi"`, `"NTP"`, etc.). This means `core.getComponent<LEDComponent>("LED")` fails; you must use `"LEDComponent"`.
- **Fix**: Change to `metadata.name = "LED"`.
- **Impact**: Breaking change for anyone using `getComponent("LEDComponent")`. Requires minor version bump.

### M15 — Storage: no change events

- **File**: `DomoticsCore-Storage/include/DomoticsCore/Storage.h`
- **Problem**: Constitution XI requires storage changes to emit events, but only `storage/ready` is emitted. `putString()`, `putInt()`, `remove()`, `clear()` never emit change events.
- **Fix**: Emit `storage/changed` event after successful write/remove/clear operations. Define a `StorageChangedEvent` struct with key + operation type.
- **Note**: Design the event payload carefully — avoid emitting on every write in high-frequency scenarios.

### M16 — System: direct millis() call

- **File**: `DomoticsCore-System/include/DomoticsCore/System.h` (~line 530)
- **Problem**: `getSystemStatus()` calls `millis()` directly instead of `HAL::Platform::getMillis()`. Violates constitution IX (HAL isolation).
- **Fix**: Replace `millis()` with `HAL::Platform::getMillis()`.

### M19 — HomeAssistant: addBinarySensor() missing event

- **File**: `DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h`
- **Problem**: `addBinarySensor()` does not emit `ha/entity_added`, while `addSensor()`, `addSwitch()`, `addLight()`, and `addButton()` do.
- **Fix**: Add `emit<String>(HAEvents::EVENT_ENTITY_ADDED, entityId)` to `addBinarySensor()`.

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

## Tracking

| Priority | Items | Constitution | Status |
|----------|-------|-------------|--------|
| 1. Memory Safety | R1-R7, M9-M10 | XIV (ABSOLUTE) | TODO |
| 2. Code Bugs | M11-M12, M15-M16, M19 | Multiple | TODO |
| 3. HAL Isolation | R8-R10 | IX (NON-NEGOTIABLE) | TODO |
| 4. File Splits | R11-R13 | VII (800 lines) | TODO |
| 5. Dead Code | R17-R23 | IV (YAGNI) | TODO |
| 6. Anti-Patterns | R14-R16 | XIII | Document exceptions |

---

*Generated from adversarial review findings. Each item should be addressed in a separate commit.*
