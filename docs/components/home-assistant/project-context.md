# DomoticsCore-HomeAssistant -- Project Context

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides AI-oriented context for the DomoticsCore-HomeAssistant component. It summarizes identity, file inventory, class hierarchy, dependencies, coding conventions, and constitution compliance.

---

## Component Identity

| Field | Value |
|-------|-------|
| **Library Name** | `DomoticsCore-HomeAssistant` |
| **Version** | `2.0.0` |
| **Component Name** | `HomeAssistant` (registered as `metadata.name`) |
| **Namespace** | `DomoticsCore::Components::HomeAssistant` |
| **Architecture** | Header-only (no `.cpp` source files) |
| **Platforms** | `espressif32`, `espressif8266` |
| **Framework** | Arduino |
| **License** | MIT |
| **Author** | JN0V |
| **Repository** | `https://github.com/JN0V/DomoticsCore.git` |

---

## File Inventory

```
DomoticsCore-HomeAssistant/
  library.json                                  # PlatformIO metadata (v2.0.0)
  README.md                                     # Component README
  SPECIFICATIONS.md                             # Design specifications
  include/
    DomoticsCore/
      HomeAssistant.h                           # Main component: HAConfig, HomeAssistantComponent
      HAEntity.h                                # Base entity class
      HASensor.h                                # Sensor entity (read-only numeric/text)
      HABinarySensor.h                          # Binary sensor entity (on/off)
      HASwitch.h                                # Switch entity (controllable on/off)
      HALight.h                                 # Light entity (on/off + brightness)
      HAButton.h                                # Button entity (trigger-only)
      HAAlarmControlPanel.h                     # Alarm control panel entity (arm/disarm + code)
      HAEvents.h                                # EventBus event constants + HACommandEvent struct
      HomeAssistantWebUI.h                      # WebUI provider (optional)
  examples/
    BasicHA/src/main.cpp                        # Minimal HA integration example
    HAWithWebUI/src/main.cpp                    # Full-featured example with WebUI
```

Total header files: 10 (all under `include/DomoticsCore/`).

---

## Key Classes

### HomeAssistantComponent

- **File:** `HomeAssistant.h`
- **Inherits:** `IComponent`
- **Role:** Top-level orchestrator. Manages entity vector, publishes discovery, routes commands via virtual dispatch, emits `ha/command` EventBus events, tracks statistics.
- **EventBus subscriptions:** `mqtt/connected`, `mqtt/disconnected`, `mqtt/message`
- **EventBus emissions:** `ha/discovery_published`, `ha/entity_added`, `ha/command`

  The `ha/command` event (v2.0.0) is the primary mechanism for consumers to react to commands from Home Assistant. It carries an `HACommandEvent` struct with `entityId`, `component`, `command`, and `code` fields.

- **MQTT communication:** All MQTT operations go through EventBus (`mqtt/publish`, `mqtt/subscribe`) -- never through direct MQTT client references.

### HAEntity (base)

- **File:** `HAEntity.h`
- **Role:** Provides topic generation (zero-heap `char* buf` API), unique ID, and base discovery payload. All entity types derive from this.
- **Properties:** `id`, `name`, `component`, `icon`, `deviceClass`, `retained`
- **Virtual methods:**
  - `buildDiscoveryPayload()` -- overridden by each entity type.
  - `handleCommand(const String& payload) -> bool` -- virtual dispatch for command handling (v2.0.0). Returns `true` if the command was valid, `false` to suppress `ha/command` emission.

### Entity Type Classes

| Class | File | HA Component | Internal State Fields | Has State | Receives Commands |
|-------|------|-------------|----------------------|-----------|-------------------|
| `HASensor` | `HASensor.h` | `sensor` | None | Yes | No |
| `HABinarySensor` | `HABinarySensor.h` | `binary_sensor` | None | Yes | No |
| `HASwitch` | `HASwitch.h` | `switch` | `bool state` | Yes | Yes |
| `HALight` | `HALight.h` | `light` | `bool state`, `uint8_t brightness` | Yes (JSON) | Yes (JSON) |
| `HAButton` | `HAButton.h` | `button` | None | No | Yes |
| `HAAlarmControlPanel` | `HAAlarmControlPanel.h` | `alarm_control_panel` | `char lastCommand[64]`, `char lastCode[32]` | Yes (consumer-managed) | Yes |

**v2.0.0 change:** No entity type has a `commandCallback` or `pressCallback` field. All command handling is via virtual `handleCommand()` which stores state internally, followed by the `ha/command` EventBus event for consumer logic.

### HomeAssistantWebUI

- **File:** `HomeAssistantWebUI.h`
- **Namespace:** `DomoticsCore::Components::WebUI`
- **Inherits:** `CachingWebUIProvider`
- **Role:** Exposes 4 UI contexts (`ha_status`, `ha_dashboard`, `ha_settings`, `ha_detail`) for the WebUI component.

---

## Dependencies

### Required (declared in `library.json`)

| Dependency | Version | Purpose |
|------------|---------|---------|
| `DomoticsCore-Core` | `>= 1.4.0` | `IComponent`, EventBus, Logger |
| `DomoticsCore-MQTT` | `>= 1.4.0` | MQTT event structures (`MQTTPublishEvent`, `MQTTSubscribeEvent`, `MQTTMessageEvent`) and event constants |
| `ArduinoJson` | `^7.0.0` | JSON serialization for discovery payloads and light command parsing |

### Optional (runtime, not declared)

| Dependency | Purpose |
|------------|---------|
| `DomoticsCore-WebUI` | Required only if `HomeAssistantWebUI` is used |

### Dependency Direction

```
Core  <--  MQTT  <--  HomeAssistant  -->  WebUI (optional)
                         |
                         v
                     ArduinoJson
```

HomeAssistant depends on Core (IComponent, EventBus, Logger) and MQTT (event structures only). It does **not** hold a direct pointer to the MQTT component -- all communication is via EventBus events.

---

## HAConfig: Fixed-Size Char Arrays (v2.0.0)

All `HAConfig` fields are fixed-size `char[]` arrays. Use `HA::setField(dest, src, maxLen)` to safely populate them. Fields that exceed `maxLen` are truncated with a warning log.

| Field | Type | Max Size Constant |
|-------|------|-------------------|
| `nodeId` | `char[33]` | `HA::MAX_NODE_ID` |
| `deviceName` | `char[65]` | `HA::MAX_DEVICE_NAME` |
| `manufacturer` | `char[33]` | `HA::MAX_MANUFACTURER` |
| `model` | `char[33]` | `HA::MAX_MODEL` |
| `swVersion` | `char[17]` | `HA::MAX_SW_VERSION` |
| `discoveryPrefix` | `char[33]` | `HA::MAX_DISCOVERY_PREFIX` |
| `availabilityTopic` | `char[129]` | `HA::MAX_AVAIL_TOPIC` |
| `configUrl` | `char[129]` | `HA::MAX_CONFIG_URL` |
| `suggestedArea` | `char[33]` | `HA::MAX_SUGGESTED_AREA` |

---

## Entity Type Patterns

All entity types follow the same pattern:

1. **Class inherits `HAEntity`** with component type set in constructor (e.g., `"sensor"`, `"switch"`).
2. **Constructor** accepts `id`, `name`, and type-specific parameters. **No callback parameters** (v2.0.0).
3. **`buildDiscoveryPayload()` override** calls `HAEntity::buildDiscoveryPayload()` first, then appends type-specific JSON fields.
4. **Controllable entities** (`HASwitch`, `HALight`, `HAButton`, `HAAlarmControlPanel`) have:
   - A `handleCommand(const String& payload) -> bool` override (virtual dispatch)
   - Internal state fields updated by `handleCommand()` (e.g., `HASwitch::state`, `HALight::brightness`)
   - A `command_topic` in their discovery payload
5. **Entity storage:** `std::vector<std::unique_ptr<HAEntity>>` in `HomeAssistantComponent`.
6. **Entity lookup:** Linear scan by `id` string via `findEntity()`.

### Command Flow (v2.0.0)

1. MQTT message arrives on `{prefix}/+/{nodeId}/+/set`.
2. `HomeAssistantComponent::handleCommand()` extracts entity ID from topic.
3. `stats.commandsReceived` is incremented (before validation -- invalid commands are counted too).
4. `entity->handleCommand(payload)` is called via virtual dispatch -- each entity type validates and stores state internally.
5. If `handleCommand()` returns `true`, an `HACommandEvent` is emitted on `ha/command`.
6. If `handleCommand()` returns `false` (invalid payload), no event is emitted.
7. For switches with `autoPublishState && !optimistic`, state is auto-published back.
8. Consumers subscribe to `ha/command` to react to commands.

### Adding a New Entity Type

To add a new entity type (e.g., `HANumber`), follow the pattern established by `HAAlarmControlPanel`:

1. Create `HANumber.h` inheriting `HAEntity` with `component = "number"`.
2. Override `buildDiscoveryPayload()` to add type-specific fields (`min`, `max`, `step`).
3. Override `handleCommand()` to validate and store the command, returning `bool`.
4. Add `addNumber()` method to `HomeAssistantComponent` (no callback parameter).
5. Include the new header in `HomeAssistant.h`.

---

## MQTT Topic Conventions

### Pattern

```
{discoveryPrefix}/{component}/{nodeId}/{entityId}/{suffix}
```

### Suffixes

| Suffix | Purpose |
|--------|---------|
| `config` | Discovery payload (JSON, retained) |
| `state` | Current state value (retained by default) |
| `set` | Command from HA to device (not retained) |
| `attributes` | Additional attributes JSON (retained) |

### Availability Topic (separate pattern)

```
{discoveryPrefix}/{nodeId}/availability
```

Payload: `"online"` or `"offline"` (always retained).

### Wildcard Subscription

The component subscribes to `{prefix}/+/{nodeId}/+/set` to receive commands for all entity types in a single subscription.

### Topic Generation (v2.0.0 -- zero-heap)

All topic methods use a caller-provided `char* buf` buffer instead of returning `String`:

```cpp
char topic[HA_TOPIC_BUF_SIZE];  // HA_TOPIC_BUF_SIZE = 128
entity->getStateTopic(topic, sizeof(topic), nodeId, discoveryPrefix);
```

---

## EventBus Integration

### Subscribed Events

| Event | Source | Payload Type | Handler |
|-------|--------|-------------|---------|
| `mqtt/connected` | MQTT component | `bool` | Publishes availability, subscribes to commands, publishes discovery |
| `mqtt/disconnected` | MQTT component | `bool` | Sets `mqttConnected = false` |
| `mqtt/message` | MQTT component | `MQTTMessageEvent` | Routes to `handleCommand()` via virtual dispatch |

### Emitted Events

| Event | Payload Type | When |
|-------|-------------|------|
| `ha/discovery_published` | `int` (entity count) | After `publishDiscovery()` completes |
| `ha/entity_added` | `HAEntityAddedEvent` | When any `add*()` method is called (all entity types) |
| `ha/command` | `HACommandEvent` | When an entity processes a valid command from HA |
| `mqtt/publish` | `MQTTPublishEvent` | Every state/discovery/availability publish |
| `mqtt/subscribe` | `MQTTSubscribeEvent` | On MQTT connect (command wildcard) |

### HACommandEvent Struct (v2.0.0)

```cpp
struct HACommandEvent {
    char entityId[64];    // Entity that received the command
    char component[32];   // HA component type (switch, light, button, alarm_control_panel)
    char command[128];    // Raw MQTT payload (or parsed command for alarm_control_panel)
    char code[32];        // Alarm PIN code (empty for non-alarm entities)
};
```

### HAEntityAddedEvent Struct

```cpp
struct HAEntityAddedEvent {
    char id[64];           // Entity ID
    char component[32];    // Component type (sensor, switch, etc.)
};
```

All `add*()` methods properly emit this event with both `id` and `component` populated.

---

## Constitution Compliance

This section maps component behavior to the [DomoticsCore Constitution](../../../.specify/memory/constitution.md) principles.

| Principle | Compliance |
|-----------|------------|
| **I. SOLID** | SRP: Each entity type is a separate class. OCP: Virtual `handleCommand()` allows extension without modifying base. DIP: Depends on `IComponent` abstraction, communicates via EventBus. ISP: Entity classes expose only relevant methods. |
| **III. KISS** | Header-only library. Simple linear entity lookup. No complex inheritance hierarchies. |
| **IV. YAGNI** | 6 entity types implemented (sensor, binary_sensor, switch, light, button, alarm_control_panel). Future types added as needed. |
| **V. Performance** | Fixed-size `char[]` config fields (zero heap). Zero-heap topic generation. Reuses device JSON across all entity discovery. Volatile publishing guard prevents re-entrancy. |
| **VI. EventBus Architecture** | All MQTT communication via EventBus events. `ha/command` event replaces per-entity callbacks. No direct component references. Topic-based messaging with type-safe payload structs. |
| **VII. File Size** | All header files are well under 800 lines. Largest file (`HomeAssistant.h`) is ~695 lines. |
| **IX. HAL Isolation** | No `#ifdef` platform directives in any file. Fully platform-agnostic. |
| **X. Non-Blocking** | `loop()` is a no-op. All operations are event-driven callbacks. |
| **XII. Multi-Registry** | `library.json` present with proper PlatformIO structure. |
| **XIII. Anti-Patterns** | No singletons. No circular dependencies. Event constants centralized in `HAEvents.h` and `MQTTEvents.h`. |
| **XIV. Memory Leak Prevention** | Entities stored in `std::unique_ptr`. No raw `new`/`delete`. Fixed-size char buffers for MQTT events (`MQTT_EVENT_TOPIC_SIZE`, `MQTT_EVENT_PAYLOAD_SIZE`) and HAConfig. |
| **XV. Semantic Versioning** | `library.json` version (`2.0.0`) matches `metadata.version` in constructor. |
