# DomoticsCore-HomeAssistant -- Project Context

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides AI-oriented context for the DomoticsCore-HomeAssistant component. It summarizes identity, file inventory, class hierarchy, dependencies, coding conventions, and constitution compliance.

---

## Component Identity

| Field | Value |
|-------|-------|
| **Library Name** | `DomoticsCore-HomeAssistant` |
| **Version** | `1.5.0` |
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
  library.json                                  # PlatformIO metadata (v1.5.0)
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
      HAEvents.h                                # EventBus event constants
      HomeAssistantWebUI.h                      # WebUI provider (optional)
  examples/
    BasicHA/src/main.cpp                        # Minimal HA integration example
    HAWithWebUI/src/main.cpp                    # Full-featured example with WebUI
```

Total header files: 9 (all under `include/DomoticsCore/`).

---

## Key Classes

### HomeAssistantComponent

- **File:** `HomeAssistant.h`
- **Inherits:** `IComponent`
- **Role:** Top-level orchestrator. Manages entity vector, publishes discovery, routes commands, tracks statistics.
- **EventBus subscriptions:** `mqtt/connected`, `mqtt/disconnected`, `mqtt/message`
- **EventBus emissions:** `ha/discovery_published`, `ha/entity_added`
- **MQTT communication:** All MQTT operations go through EventBus (`mqtt/publish`, `mqtt/subscribe`) -- never through direct MQTT client references.

### HAEntity (base)

- **File:** `HAEntity.h`
- **Role:** Provides topic generation, unique ID, and base discovery payload. All entity types derive from this.
- **Properties:** `id`, `name`, `component`, `icon`, `deviceClass`, `retained`
- **Virtual method:** `buildDiscoveryPayload()` -- overridden by each entity type.

### Entity Type Classes

| Class | File | HA Component | Callback Signature | Has State | Receives Commands |
|-------|------|-------------|-------------------|-----------|-------------------|
| `HASensor` | `HASensor.h` | `sensor` | None | Yes | No |
| `HABinarySensor` | `HABinarySensor.h` | `binary_sensor` | None | Yes | No |
| `HASwitch` | `HASwitch.h` | `switch` | `void(bool)` | Yes | Yes |
| `HALight` | `HALight.h` | `light` | `void(bool, uint8_t)` | Yes (JSON) | Yes (JSON) |
| `HAButton` | `HAButton.h` | `button` | `void()` | No | Yes |

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

## Entity Type Patterns

All entity types follow the same pattern:

1. **Class inherits `HAEntity`** with component type set in constructor (e.g., `"sensor"`, `"switch"`).
2. **Constructor** accepts `id`, `name`, and type-specific parameters.
3. **`buildDiscoveryPayload()` override** calls `HAEntity::buildDiscoveryPayload()` first, then appends type-specific JSON fields.
4. **Controllable entities** (`HASwitch`, `HALight`, `HAButton`) have:
   - A `commandCallback` (or `pressCallback` for buttons)
   - A `handleCommand(const String& payload)` method
   - A `command_topic` in their discovery payload
5. **Entity storage:** `std::vector<std::unique_ptr<HAEntity>>` in `HomeAssistantComponent`.
6. **Entity lookup:** Linear scan by `id` string via `findEntity()`.

### Adding a New Entity Type

To add a new entity type (e.g., `HANumber`):

1. Create `HANumber.h` inheriting `HAEntity` with `component = "number"`.
2. Override `buildDiscoveryPayload()` to add type-specific fields (`min`, `max`, `step`).
3. Add `handleCommand()` if the entity receives commands.
4. Add `addNumber()` method to `HomeAssistantComponent`.
5. Add command routing in `handleCommand()` for `entity->component == "number"`.
6. Include the new header in `HomeAssistant.h`.

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

---

## EventBus Integration

### Subscribed Events

| Event | Source | Payload Type | Handler |
|-------|--------|-------------|---------|
| `mqtt/connected` | MQTT component | `bool` | Publishes availability, subscribes to commands, publishes discovery |
| `mqtt/disconnected` | MQTT component | `bool` | Sets `mqttConnected = false` |
| `mqtt/message` | MQTT component | `MQTTMessageEvent` | Routes to `handleCommand()` |

### Emitted Events

| Event | Payload Type | When |
|-------|-------------|------|
| `ha/discovery_published` | `int` (entity count) | After `publishDiscovery()` completes |
| `ha/entity_added` | `String` (entity ID) | When `addSensor()` is called (**only** -- see known issues below) |
| `mqtt/publish` | `MQTTPublishEvent` | Every state/discovery/availability publish |
| `mqtt/subscribe` | `MQTTSubscribeEvent` | On MQTT connect (command wildcard) |

### Known Issues

- **C21 -- HAEntityAddedEvent struct alignment:** The `HAEntityAddedEvent` struct is defined in `HomeAssistant.h` with `id[64]` and `component[32]` fields, but `emit()` passes a `String` (entity ID only) for `ha/entity_added`. The struct and actual emission are not aligned; resolution is pending.
- **M19 -- Inconsistent `ha/entity_added` emission:** Only `addSensor()` emits the `ha/entity_added` event. `addBinarySensor()`, `addSwitch()`, `addLight()`, and `addButton()` do not emit this event, which is an inconsistency to be addressed in a future release.

---

## Constitution Compliance

This section maps component behavior to the [DomoticsCore Constitution](../../../.specify/memory/constitution.md) principles.

| Principle | Compliance |
|-----------|------------|
| **I. SOLID** | SRP: Each entity type is a separate class. DIP: Depends on `IComponent` abstraction, communicates via EventBus. ISP: Entity classes expose only relevant methods. |
| **III. KISS** | Header-only library. Simple linear entity lookup. No complex inheritance hierarchies. |
| **IV. YAGNI** | Only 5 entity types implemented (the ones actively used). Future types marked in specs but not implemented. |
| **V. Performance** | Reuses device JSON across all entity discovery. Volatile publishing guard prevents re-entrancy. |
| **VI. EventBus Architecture** | All MQTT communication via EventBus events. No direct component references. Topic-based messaging with type-safe payload structs. |
| **VII. File Size** | All header files are well under 800 lines. Largest file (`HomeAssistant.h`) is ~565 lines. |
| **IX. HAL Isolation** | No `#ifdef` platform directives in any file. Fully platform-agnostic. |
| **X. Non-Blocking** | `loop()` is a no-op. All operations are event-driven callbacks. |
| **XII. Multi-Registry** | `library.json` present with proper PlatformIO structure. |
| **XIII. Anti-Patterns** | No singletons. No circular dependencies. Event constants centralized in `HAEvents.h` and `MQTTEvents.h`. |
| **XIV. Memory Leak Prevention** | Entities stored in `std::unique_ptr`. No raw `new`/`delete`. Fixed-size char buffers for MQTT events (`MQTT_EVENT_TOPIC_SIZE`, `MQTT_EVENT_PAYLOAD_SIZE`). |
| **XV. Semantic Versioning** | `library.json` version (`1.5.0`) matches `metadata.version` in constructor. |
