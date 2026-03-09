# DomoticsCore-HomeAssistant -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides the complete API surface, MQTT topic conventions, payload formats, and internal architecture of the DomoticsCore-HomeAssistant component (v2.0.0).

---

## Table of Contents

1. [HAConfig](#haconfig)
2. [HA Namespace Utilities](#ha-namespace-utilities)
3. [HomeAssistantComponent](#homeassistantcomponent)
4. [HAEntity Base Class](#haentity-base-class)
5. [HASensor](#hasensor)
6. [HABinarySensor](#habinarysensor)
7. [HASwitch](#haswitch)
8. [HALight](#halight)
9. [HAButton](#habutton)
10. [HAAlarmControlPanel](#haalarmcontrolpanel)
11. [HAEvents](#haevents)
12. [HAStatistics](#hastatistics)
13. [HomeAssistantWebUI](#homeassistantwebui)
14. [MQTT Topic Structure](#mqtt-topic-structure)
15. [Discovery Payloads](#discovery-payloads)
16. [Device Registry](#device-registry)
17. [Command Handling](#command-handling)
18. [Availability](#availability)

---

## HAConfig

Configuration structure for the Home Assistant component. Passed to `HomeAssistantComponent` at construction. All fields are fixed-size `char[]` arrays (v2.0.0 -- zero heap allocation).

**Namespace:** `DomoticsCore::Components::HomeAssistant`

```cpp
struct HAConfig {
    char nodeId[HA::MAX_NODE_ID];                 // 33 bytes -- unique device ID (used in MQTT topics)
    char deviceName[HA::MAX_DEVICE_NAME];         // 65 bytes -- display name in HA device registry
    char manufacturer[HA::MAX_MANUFACTURER];       // 33 bytes -- manufacturer shown in HA
    char model[HA::MAX_MODEL];                     // 33 bytes -- hardware model
    char swVersion[HA::MAX_SW_VERSION];            // 17 bytes -- firmware version
    bool retainDiscovery = true;                   // Retain discovery messages on the broker
    char discoveryPrefix[HA::MAX_DISCOVERY_PREFIX]; // 33 bytes -- MQTT discovery prefix
    char availabilityTopic[HA::MAX_AVAIL_TOPIC];   // 129 bytes -- auto-generated if empty
    char configUrl[HA::MAX_CONFIG_URL];            // 129 bytes -- "Configuration" link in HA device page
    char suggestedArea[HA::MAX_SUGGESTED_AREA];    // 33 bytes -- suggested area in HA
};
```

| Field | Max Size | Default | Description |
|-------|----------|---------|-------------|
| `nodeId` | 32 chars | `"myDeviceId"` | Used in all MQTT topics and as the device identifier. Must be unique per device. |
| `deviceName` | 64 chars | `"My Device"` | Human-readable device name in the HA device registry. |
| `manufacturer` | 32 chars | `"DomoticsCore"` | Populated from `SystemConfig.manufacturer` when using the System component. |
| `model` | 32 chars | `"MyDeviceModel"` | Populated from `SystemConfig.model`; auto-detected via `ESP.getChipModel()`. |
| `swVersion` | 16 chars | `"1.0.0"` | Populated from `SystemConfig.firmwareVersion`. |
| `retainDiscovery` | -- | `true` | When true, discovery payloads persist on the broker across broker restarts. |
| `discoveryPrefix` | 32 chars | `"homeassistant"` | Must match the MQTT discovery prefix configured in Home Assistant. |
| `availabilityTopic` | 128 chars | `""` (auto) | If left empty, auto-generated as `{discoveryPrefix}/{nodeId}/availability`. |
| `configUrl` | 128 chars | `""` | Optional. If set, HA shows a "Configuration" link on the device page. |
| `suggestedArea` | 32 chars | `""` | Optional. Suggests a room/area when the device first appears in HA. |

### Setting HAConfig Fields

Use `HA::setField()` to safely populate fields with truncation protection:

```cpp
HAConfig cfg;
HA::setField(cfg.nodeId, "esp32-sensor", HA::MAX_NODE_ID);
HA::setField(cfg.deviceName, "Living Room Sensor", HA::MAX_DEVICE_NAME);
HA::setField(cfg.manufacturer, "Acme Corp", HA::MAX_MANUFACTURER);
```

If the source string exceeds the maximum length, it is truncated and a warning is logged.

---

## HA Namespace Utilities

**Namespace:** `DomoticsCore::Components::HomeAssistant::HA`

### Size Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_NODE_ID` | 33 | 32 chars + null |
| `MAX_DEVICE_NAME` | 65 | 64 chars + null |
| `MAX_MANUFACTURER` | 33 | 32 chars + null |
| `MAX_MODEL` | 33 | 32 chars + null |
| `MAX_SW_VERSION` | 17 | 16 chars + null |
| `MAX_DISCOVERY_PREFIX` | 33 | 32 chars + null |
| `MAX_AVAIL_TOPIC` | 129 | 128 chars + null |
| `MAX_CONFIG_URL` | 129 | 128 chars + null |
| `MAX_SUGGESTED_AREA` | 33 | 32 chars + null |

### setField()

```cpp
inline void setField(char* dest, const char* src, size_t maxLen);
```

Safely copies `src` into `dest` with truncation. If `src` is `nullptr`, sets `dest` to empty string. Logs a warning if truncation occurs.

---

## HomeAssistantComponent

The main component class. Implements `IComponent` and manages entity registration, discovery, state publishing, and command routing.

**Namespace:** `DomoticsCore::Components::HomeAssistant`
**Header:** `DomoticsCore/HomeAssistant.h`
**Inherits:** `IComponent`

### Constructor

```cpp
HomeAssistantComponent(const HAConfig& config = HAConfig());
```

Sets component metadata:
- `name`: `"HomeAssistant"`
- `version`: `"2.0.0"`
- Auto-generates `availabilityTopic` if not provided.

### IComponent Lifecycle

```cpp
ComponentStatus begin() override;
void loop() override;
ComponentStatus shutdown() override;
```

- **`begin()`** -- Subscribes to EventBus events: `mqtt/connected`, `mqtt/disconnected`, and `mqtt/message`. On MQTT connect, publishes availability and discovery. Returns `ComponentStatus::Success`.
- **`loop()`** -- No-op. All communication is event-driven via the EventBus.
- **`shutdown()`** -- Publishes `"offline"` availability and removes all discovery payloads (empty retained messages).

### Entity Management Methods

#### addSensor

```cpp
void addSensor(const String& id, const String& name,
               const String& unit = "", const String& deviceClass = "",
               const String& icon = "", const String& stateClass = "");
```

Registers a read-only sensor entity. If `stateClass` is empty and `unit` is non-empty, `stateClass` defaults to `"measurement"` in the discovery payload. Emits `ha/entity_added` with `HAEntityAddedEvent{id, "sensor"}`.

#### addBinarySensor

```cpp
void addBinarySensor(const String& id, const String& name,
                     const String& deviceClass = "", const String& icon = "");
```

Registers a binary (on/off) sensor entity. Emits `ha/entity_added` with `HAEntityAddedEvent{id, "binary_sensor"}`.

#### addSwitch

```cpp
void addSwitch(const String& id, const String& name, const String& icon = "",
               bool autoPublishState = true, bool optimistic = false);
```

Registers a controllable switch entity. **No callback parameter** (v2.0.0). Subscribe to `ha/command` event to handle switch commands.

| Parameter | Description |
|-----------|-------------|
| `autoPublishState` | When `true`, the component auto-publishes the received state back to HA after command processing. |
| `optimistic` | When `true`, HA assumes the command succeeds without waiting for state confirmation. |

Emits `ha/entity_added` with `HAEntityAddedEvent{id, "switch"}`.

#### addLight

```cpp
void addLight(const String& id, const String& name);
```

Registers a light entity with brightness support. **No callback parameter** (v2.0.0). Subscribe to `ha/command` event to handle light commands. Emits `ha/entity_added` with `HAEntityAddedEvent{id, "light"}`.

#### addButton

```cpp
void addButton(const String& id, const String& name, const String& icon = "");
```

Registers a trigger-only button entity. **No callback parameter** (v2.0.0). Subscribe to `ha/command` event to handle button presses. Emits `ha/entity_added` with `HAEntityAddedEvent{id, "button"}`.

#### addAlarmControlPanel

```cpp
void addAlarmControlPanel(
    const String& id, const String& name,
    const String& icon = "mdi:shield-home",
    AlarmFeature features = AlarmFeature::ArmAway,
    const String& code = "",
    bool codeArmRequired = false,
    bool codeDisarmRequired = false,
    bool codeTriggerRequired = false);
```

Registers a native Home Assistant alarm control panel entity. Renders as the alarm panel Lovelace card with keypad and color-coded status. **No callback parameter** (v2.0.0). Subscribe to `ha/command` event to handle alarm commands.

| Parameter | Description |
|-----------|-------------|
| `features` | Bitmask of `AlarmFeature` flags defining which arm modes are available in the HA UI. |
| `code` | PIN code included in discovery so HA's frontend shows a keypad. The library does NOT validate entered codes -- it passes them through via the `ha/command` event. |
| `codeArmRequired` | If true, HA's frontend requires code entry for arm operations. |
| `codeDisarmRequired` | If true, HA's frontend requires code entry for disarm. |
| `codeTriggerRequired` | If true, HA's frontend requires code entry for trigger. |

Emits `ha/entity_added` with `HAEntityAddedEvent{id, "alarm_control_panel"}`.

> **Important -- Library vs Consumer boundary:** The alarm panel entity is a thin MQTT plumbing layer. The library handles discovery, command parsing, and topic management. **All business logic is the consumer's responsibility:**
> - **Code validation** -- The library passes the raw code via `HACommandEvent.code`; the consumer must verify it.
> - **State transitions** -- The library has no state machine; the consumer calls `publishState()` with the appropriate `AlarmPanelState::*` value.
> - **Command validation** -- The library does not check if a command matches the configured `supportedFeatures`.
> - **Timing** -- Arming delays, entry delays, trigger durations are entirely consumer logic.

### State Publishing Methods

#### publishState (String)

```cpp
void publishState(const String& id, const String& state);
```

Publishes a string state value to the entity's state topic. Skips silently if MQTT is not connected.

#### publishState (float)

```cpp
void publishState(const String& id, float value);
```

Publishes a numeric value with 2 decimal places (e.g., `"22.50"`).

#### publishState (bool)

```cpp
void publishState(const String& id, bool state);
```

Publishes `"ON"` or `"OFF"`.

#### publishState (const char*)

```cpp
void publishState(const String& id, const char* state);
```

Publishes a C-string state. This overload exists to **prevent implicit `bool` conversion** when passing string literals like `"ON"`.

#### publishStateJson

```cpp
void publishStateJson(const String& id, const JsonDocument& doc);
```

Publishes a JSON document as state. Used primarily for lights that need `{"state":"ON","brightness":128}`.

#### publishAttributes

```cpp
void publishAttributes(const String& id, const JsonDocument& attributes);
```

Publishes additional attributes as JSON to the entity's attributes topic. Always retained.

### Availability

```cpp
void setAvailable(bool available);
```

Publishes `"online"` or `"offline"` to the device availability topic. Called automatically on MQTT connect/disconnect.

### Discovery Methods

```cpp
void publishDiscovery();        // Publish discovery for all registered entities
void removeDiscovery();         // Remove all discovery payloads (empty retained messages)
void republishEntity(const String& id);  // Republish discovery for a single entity
```

- `publishDiscovery()` builds the device info JSON once and publishes a discovery payload for each entity. Emits the `ha/discovery_published` event with the entity count.
- `removeDiscovery()` publishes empty payloads to each entity's config topic, causing HA to remove them.
- `republishEntity()` publishes discovery for a single entity. Called automatically when an entity is added while MQTT is already connected.

### Configuration

```cpp
void setConfig(const HAConfig& cfg);
const HAConfig& getConfig() const;
void setDeviceInfo(const char* name, const char* model,
                   const char* manufacturer, const char* swVersion);
```

Note: `setDeviceInfo()` takes `const char*` parameters (v2.0.0), not `const String&`.

### Status Methods

```cpp
bool isReady() const;           // True if MQTT connected AND availability published
bool isMQTTConnected() const;   // True if MQTT connection is active
const HAStatistics& getStatistics() const;
```

---

## HAEntity Base Class

Base class for all entity types.

**Namespace:** `DomoticsCore::Components::HomeAssistant`
**Header:** `DomoticsCore/HAEntity.h`

### Constants

```cpp
static constexpr size_t HA_TOPIC_BUF_SIZE = 128;
```

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `id` | `String` | -- | Unique entity ID (e.g., `"temperature"`) |
| `name` | `String` | -- | Display name (e.g., `"Temperature"`) |
| `component` | `String` | -- | HA component type (`"sensor"`, `"switch"`, etc.) |
| `icon` | `String` | `""` | MDI icon name (e.g., `"mdi:thermometer"`) |
| `deviceClass` | `String` | `""` | HA device class (e.g., `"temperature"`, `"motion"`) |
| `retained` | `bool` | `true` | Whether state messages are retained on the broker |

### Topic Generation Methods (zero-heap)

All topic methods write into a caller-provided buffer. No heap allocation.

```cpp
void getDiscoveryTopic(char* buf, size_t len, const char* nodeId,
                       const char* discoveryPrefix = "homeassistant") const;
// Writes: {prefix}/{component}/{nodeId}/{id}/config

void getStateTopic(char* buf, size_t len, const char* nodeId,
                   const char* discoveryPrefix = "homeassistant") const;
// Writes: {prefix}/{component}/{nodeId}/{id}/state

void getCommandTopic(char* buf, size_t len, const char* nodeId,
                     const char* discoveryPrefix = "homeassistant") const;
// Writes: {prefix}/{component}/{nodeId}/{id}/set

void getAttributesTopic(char* buf, size_t len, const char* nodeId,
                        const char* discoveryPrefix = "homeassistant") const;
// Writes: {prefix}/{component}/{nodeId}/{id}/attributes

void getUniqueId(char* buf, size_t len, const char* nodeId) const;
// Writes: {nodeId}_{id}
```

### Virtual Methods

#### buildDiscoveryPayload

```cpp
virtual void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                                   const String& discoveryPrefix,
                                   const JsonObject& device,
                                   const String& availabilityTopic) const;
```

The base implementation adds: `name`, `unique_id`, `state_topic`, `icon` (if set), `device_class` (if set), `device` (device registry object), and `availability_topic` with `payload_available`/`payload_not_available`.

Derived classes call the base implementation and then add type-specific fields.

#### handleCommand

```cpp
virtual bool handleCommand(const String& payload);
```

Virtual dispatch for command handling (v2.0.0). Each controllable entity overrides this to validate the command and store state internally.

- Returns `true` if the command was valid and should be emitted as `ha/command`.
- Returns `false` if the command was invalid (e.g., garbage JSON for lights, wrong payload for buttons). The `ha/command` event is suppressed.
- Base implementation returns `true` (pass-through).

---

## HASensor

Read-only numeric or text sensor.

**Header:** `DomoticsCore/HASensor.h`
**HA Component:** `sensor`

### Additional Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `unit` | `String` | `""` | Unit of measurement (`"C"`, `"%"`, `"W"`, etc.) |
| `stateClass` | `String` | `""` | State class: `"measurement"`, `"total"`, `"total_increasing"` |
| `expireAfter` | `float` | `0` | Seconds after which HA marks the value as unavailable (0 = never) |

### Discovery Fields Added

- `unit_of_measurement` -- if `unit` is non-empty.
- `state_class` -- if explicitly set; otherwise defaults to `"measurement"` when `unit` is non-empty.
- `expire_after` -- if `expireAfter > 0`.

---

## HABinarySensor

Read-only on/off sensor (motion, door, etc.).

**Header:** `DomoticsCore/HABinarySensor.h`
**HA Component:** `binary_sensor`

### Additional Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `payloadOn` | `String` | `"ON"` | Payload representing the "on" state |
| `payloadOff` | `String` | `"OFF"` | Payload representing the "off" state |

### Discovery Fields Added

- `payload_on`
- `payload_off`

---

## HASwitch

Controllable on/off device (relay, socket).

**Header:** `DomoticsCore/HASwitch.h`
**HA Component:** `switch`

### Additional Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `payloadOn` | `String` | `"ON"` | Payload for the ON command/state |
| `payloadOff` | `String` | `"OFF"` | Payload for the OFF command/state |
| `optimistic` | `bool` | `false` | If true, HA assumes commands succeed immediately |
| `autoPublishState` | `bool` | `true` | If true, state is auto-published after command handling |
| `state` | `bool` | `false` | Current switch state (updated by `handleCommand()`) |

### Discovery Fields Added

- `command_topic`
- `payload_on`, `payload_off`
- `state_on`, `state_off`
- `optimistic` (only if `true`)

### Command Handling

```cpp
bool handleCommand(const String& payload) override;
```

Sets `state = (payload == payloadOn)`. Always returns `true` (switch commands are always valid).

After `handleCommand()`, if `autoPublishState` is `true` and `optimistic` is `false`, `HomeAssistantComponent` auto-publishes the received payload back as state.

---

## HALight

Controllable light with optional brightness.

**Header:** `DomoticsCore/HALight.h`
**HA Component:** `light`

### Additional Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `supportsBrightness` | `bool` | `true` | Enable brightness control |
| `optimistic` | `bool` | `false` | If true, HA assumes commands succeed immediately |
| `state` | `bool` | `false` | Current light state (updated by `handleCommand()`) |
| `brightness` | `uint8_t` | `0` | Current brightness 0-255 (updated by `handleCommand()`) |

### Discovery Fields Added

- `command_topic`
- `payload_on`, `payload_off`
- `state_value_template`: `{{ value_json.state }}`
- When `supportsBrightness` is true:
  - `brightness`: `true`
  - `brightness_scale`: `255`
  - `brightness_state_topic` (same as state topic)
  - `brightness_command_topic` (same as command topic)
  - `brightness_value_template`: `{{ value_json.brightness }}`
  - `on_command_type`: `"brightness"`
- `optimistic` (only if `true`)

### Command Handling

```cpp
bool handleCommand(const String& payload) override;
```

Attempts to parse `payload` as JSON with `state` and `brightness` keys. Falls back to simple `"ON"`/`"OFF"` parsing if JSON deserialization fails. Returns `false` for payloads that are neither valid JSON nor `"ON"`/`"OFF"` (invalid commands are suppressed from the `ha/command` event).

Updates `state` and `brightness` internally.

### State Payload Format

Light state must be published as JSON using `publishStateJson()`:

```json
{
  "state": "ON",
  "brightness": 128
}
```

---

## HAButton

Trigger-only action (restart, calibrate, etc.).

**Header:** `DomoticsCore/HAButton.h`
**HA Component:** `button`

### Additional Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `payloadPress` | `String` | `"PRESS"` | Expected payload to trigger the action |

### Discovery Fields Added

Buttons override the base `buildDiscoveryPayload()` completely (no `state_topic` is added):

- `name`, `unique_id`, `icon`, `device_class`, `device`, `availability_topic`
- `command_topic`
- `payload_press`

### Command Handling

```cpp
bool handleCommand(const String& payload) override;
```

Returns `true` only if `payload == payloadPress`. Returns `false` for any other payload, suppressing the `ha/command` event.

---

## HAAlarmControlPanel

Native Home Assistant alarm control panel with multiple arm modes, intermediate states, and optional PIN code passthrough.

**Header:** `DomoticsCore/HAAlarmControlPanel.h`
**HA Component:** `alarm_control_panel`

### AlarmFeature Flags

Bitmask enum (`uint8_t`) defining supported arm modes:

| Flag | Value | Description |
|------|-------|-------------|
| `ArmHome` | `0x01` | Arm home mode |
| `ArmAway` | `0x02` | Arm away mode (default) |
| `ArmNight` | `0x04` | Arm night mode |
| `ArmVacation` | `0x08` | Arm vacation mode |
| `ArmCustomBypass` | `0x10` | Arm custom bypass mode |
| `Trigger` | `0x20` | Manual trigger capability |

Combine with bitwise OR: `AlarmFeature::ArmAway | AlarmFeature::ArmHome | AlarmFeature::Trigger`

### AlarmPanelState Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `Disarmed` | `"disarmed"` | System disarmed |
| `Arming` | `"arming"` | Arming in progress (exit delay) |
| `ArmedHome` | `"armed_home"` | Armed in home mode |
| `ArmedAway` | `"armed_away"` | Armed in away mode |
| `ArmedNight` | `"armed_night"` | Armed in night mode |
| `ArmedVacation` | `"armed_vacation"` | Armed in vacation mode |
| `ArmedCustomBypass` | `"armed_custom_bypass"` | Armed with custom bypass |
| `Pending` | `"pending"` | Entry delay active |
| `Triggered` | `"triggered"` | Alarm triggered |
| `Disarming` | `"disarming"` | Consumer convenience; not triggered by HA commands |

### AlarmPanelCommand Constants

| Constant | Value |
|----------|-------|
| `ARM_HOME` | `"ARM_HOME"` |
| `ARM_AWAY` | `"ARM_AWAY"` |
| `ARM_NIGHT` | `"ARM_NIGHT"` |
| `ARM_VACATION` | `"ARM_VACATION"` |
| `ARM_CUSTOM_BYPASS` | `"ARM_CUSTOM_BYPASS"` |
| `DISARM` | `"DISARM"` |
| `TRIGGER` | `"TRIGGER"` |

### Additional Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `code` | `String` | `""` | PIN code sent to HA frontend for keypad display; the library does NOT validate it -- passthrough only |
| `supportedFeatures` | `AlarmFeature` | `ArmAway` | Bitmask of supported arm modes |
| `codeArmRequired` | `bool` | `false` | Require code for arm operations |
| `codeDisarmRequired` | `bool` | `false` | Require code for disarm |
| `codeTriggerRequired` | `bool` | `false` | Require code for trigger |
| `lastCommand` | `char[64]` | `""` | Parsed command from last `handleCommand()` (e.g., `"ARM_AWAY"`) |
| `lastCode` | `char[32]` | `""` | Parsed code from last `handleCommand()` (e.g., `"1234"`) |

### Discovery Fields Added

- `command_topic`
- When code configuration is active (any of `code`, `codeArmRequired`, `codeDisarmRequired`, `codeTriggerRequired` is set):
  - `code` (if non-empty)
  - `code_arm_required`, `code_disarm_required`, `code_trigger_required`
  - `command_template`: `{{ action }}{% if code %} {{ code }}{% endif %}`
- Payload constants per supported feature: `payload_arm_home`, `payload_arm_away`, `payload_arm_night`, `payload_arm_vacation`, `payload_arm_custom_bypass`, `payload_trigger`
- `payload_disarm` (always present)
- `supported_features` JSON array built from bitmask

### Command Handling

```cpp
bool handleCommand(const String& payload) override;
```

Parses payloads in `"COMMAND"` or `"COMMAND CODE"` format. Trims whitespace. Stores the parsed command in `lastCommand` and the code in `lastCode`. Always returns `true` (alarm commands are always valid). Empty or whitespace-only payloads are accepted with a warning log.

The `HomeAssistantComponent` reads `lastCommand` and `lastCode` to populate the `HACommandEvent.command` and `HACommandEvent.code` fields before emitting `ha/command`.

---

## HAEvents

Event constants and structures published by the component via the EventBus.

**Namespace:** `DomoticsCore::HAEvents`
**Header:** `DomoticsCore/HAEvents.h`

### Event Constants

| Constant | Topic String | Payload Type | Description |
|----------|-------------|--------------|-------------|
| `EVENT_DISCOVERY_PUBLISHED` | `"ha/discovery_published"` | `int` | Emitted after all discovery payloads are sent. Payload is entity count. |
| `EVENT_ENTITY_ADDED` | `"ha/entity_added"` | `HAEntityAddedEvent` | Emitted when a new entity is registered via any `add*()` method. |
| `EVENT_COMMAND` | `"ha/command"` | `HACommandEvent` | Emitted when an entity processes a valid command from Home Assistant (v2.0.0). |

### HACommandEvent Struct

```cpp
struct HACommandEvent {
    char entityId[64];    // Entity that received the command
    char component[32];   // HA component type (switch, light, button, alarm_control_panel)
    char command[128];    // Raw MQTT payload (or parsed command for alarm_control_panel)
    char code[32];        // Alarm PIN code (empty for non-alarm entities)
};
```

Fixed-size POD struct (~256 bytes), zero heap allocation. This is the **primary mechanism** for consumers to react to commands from Home Assistant in v2.0.0.

- `entityId` -- the entity that received the command (e.g., `"relay"`, `"alarm"`)
- `component` -- HA component type (e.g., `"switch"`, `"alarm_control_panel"`)
- `command` -- the raw MQTT payload for most entities; for `alarm_control_panel`, the parsed command (e.g., `"ARM_AWAY"` instead of `"ARM_AWAY 1234"`)
- `code` -- alarm PIN code entered by the user (empty for non-alarm entities)

### HAEntityAddedEvent Struct

```cpp
struct HAEntityAddedEvent {
    char id[64];           // Entity ID
    char component[32];    // Component type (sensor, switch, etc.)
};
```

Emitted by all `add*()` methods with both `id` and `component` populated.

### Consumer Usage Example

```cpp
haPtr->on<HAEvents::HACommandEvent>(HAEvents::EVENT_COMMAND,
    [](const HAEvents::HACommandEvent& ev) {
        if (strcmp(ev.component, "switch") == 0) {
            bool on = (strcmp(ev.command, "ON") == 0);
            // Handle switch command
        }
        if (strcmp(ev.component, "light") == 0) {
            // ev.command contains JSON or ON/OFF
        }
        if (strcmp(ev.component, "button") == 0) {
            // Button was pressed (only fires for valid payloads)
        }
        if (strcmp(ev.component, "alarm_control_panel") == 0) {
            // ev.command = "ARM_AWAY", "DISARM", etc.
            // ev.code = PIN entered by user (may be empty)
        }
    });
```

---

## HAStatistics

Runtime statistics counters.

```cpp
struct HAStatistics {
    uint32_t entityCount = 0;       // Total registered entities
    uint32_t discoveryCount = 0;    // Number of full discovery publishes
    uint32_t stateUpdates = 0;      // Total state messages sent
    uint32_t commandsReceived = 0;  // Total commands received from HA
};
```

Access via `component.getStatistics()`.

---

## HomeAssistantWebUI

Optional WebUI provider that exposes configuration, status, and statistics through the DomoticsCore web interface.

**Namespace:** `DomoticsCore::Components::WebUI`
**Header:** `DomoticsCore/HomeAssistantWebUI.h`
**Inherits:** `CachingWebUIProvider`

### Constructor

```cpp
explicit HomeAssistantWebUI(HomeAssistant::HomeAssistantComponent* ha);
```

Takes a non-owning pointer to the HomeAssistant component.

### Configuration Persistence

```cpp
void setConfigSaveCallback(std::function<void(const HomeAssistant::HAConfig&)> callback);
```

Set a callback that fires when the user saves HA settings via the web interface. Use this to persist configuration to Storage.

### UI Contexts

| Context ID | Location | Description | Refresh |
|------------|----------|-------------|---------|
| `ha_status` | Status Badge | Entity count and connection status | 5s |
| `ha_dashboard` | Dashboard Card | Node ID, device name, entity count, stats | 5s |
| `ha_settings` | Settings Card | Editable: node ID, device name, manufacturer, model, prefix, area | On demand |
| `ha_detail` | Component Detail | Full statistics, availability topic, config URL | 5s |

### Settings API

POST to `ha_settings` with parameters: `node_id`, `device_name`, `manufacturer`, `model`, `discovery_prefix`, `suggested_area`. The handler updates the config using `HA::setField()`, invokes the save callback, and republishes discovery.

### Registration

```cpp
#include <DomoticsCore/HomeAssistantWebUI.h>

webui->registerProviderWithComponent(new HomeAssistantWebUI(haPtr), haPtr);
```

---

## MQTT Topic Structure

All topics follow the Home Assistant MQTT Discovery convention.

### Template Variables

- `{prefix}` -- Discovery prefix (default: `homeassistant`)
- `{component}` -- HA component type: `sensor`, `binary_sensor`, `switch`, `light`, `button`, `alarm_control_panel`
- `{nodeId}` -- Unique device identifier from `HAConfig.nodeId`
- `{entityId}` -- Entity ID from `HAEntity.id`

### Topic Patterns

| Purpose | Topic Pattern | Direction | Retained |
|---------|--------------|-----------|----------|
| Discovery | `{prefix}/{component}/{nodeId}/{entityId}/config` | Device -> Broker | Yes (configurable) |
| State | `{prefix}/{component}/{nodeId}/{entityId}/state` | Device -> Broker | Yes (per entity) |
| Command | `{prefix}/{component}/{nodeId}/{entityId}/set` | HA -> Device | No |
| Attributes | `{prefix}/{component}/{nodeId}/{entityId}/attributes` | Device -> Broker | Yes |
| Availability | `{prefix}/{nodeId}/availability` | Device -> Broker | Yes |

### Command Subscription

The component subscribes to a single wildcard topic to receive all commands:

```
{prefix}/+/{nodeId}/+/set
```

Commands are routed internally to the appropriate entity based on the entity ID extracted from the topic.

### Topic Generation (zero-heap)

All topic methods use caller-provided `char*` buffers (v2.0.0):

```cpp
char topic[HA_TOPIC_BUF_SIZE];  // HA_TOPIC_BUF_SIZE = 128
entity->getStateTopic(topic, sizeof(topic), nodeId, discoveryPrefix);
```

---

## Discovery Payloads

### Sensor Example

Topic: `homeassistant/sensor/esp32-demo/temperature/config`

```json
{
  "name": "Temperature",
  "unique_id": "esp32-demo_temperature",
  "state_topic": "homeassistant/sensor/esp32-demo/temperature/state",
  "unit_of_measurement": "C",
  "device_class": "temperature",
  "state_class": "measurement",
  "icon": "mdi:thermometer",
  "device": {
    "identifiers": ["esp32-demo"],
    "name": "ESP32 Demo Device",
    "model": "ESP32",
    "manufacturer": "DomoticsCore",
    "sw_version": "1.0.0"
  },
  "availability_topic": "homeassistant/esp32-demo/availability",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

### Switch Example

Topic: `homeassistant/switch/esp32-demo/relay/config`

```json
{
  "name": "Relay",
  "unique_id": "esp32-demo_relay",
  "state_topic": "homeassistant/switch/esp32-demo/relay/state",
  "command_topic": "homeassistant/switch/esp32-demo/relay/set",
  "payload_on": "ON",
  "payload_off": "OFF",
  "state_on": "ON",
  "state_off": "OFF",
  "device": { "..." : "..." },
  "availability_topic": "homeassistant/esp32-demo/availability",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

### Light Example

Topic: `homeassistant/light/esp32-demo/led/config`

```json
{
  "name": "LED Strip",
  "unique_id": "esp32-demo_led",
  "state_topic": "homeassistant/light/esp32-demo/led/state",
  "command_topic": "homeassistant/light/esp32-demo/led/set",
  "payload_on": "ON",
  "payload_off": "OFF",
  "state_value_template": "{{ value_json.state }}",
  "brightness": true,
  "brightness_scale": 255,
  "brightness_state_topic": "homeassistant/light/esp32-demo/led/state",
  "brightness_command_topic": "homeassistant/light/esp32-demo/led/set",
  "brightness_value_template": "{{ value_json.brightness }}",
  "on_command_type": "brightness",
  "device": { "..." : "..." },
  "availability_topic": "homeassistant/esp32-demo/availability",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

### Button Example

Topic: `homeassistant/button/esp32-demo/restart/config`

```json
{
  "name": "Restart",
  "unique_id": "esp32-demo_restart",
  "command_topic": "homeassistant/button/esp32-demo/restart/set",
  "payload_press": "PRESS",
  "icon": "mdi:restart",
  "device": { "..." : "..." },
  "availability_topic": "homeassistant/esp32-demo/availability",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

Note: Buttons do **not** include a `state_topic`.

### Alarm Control Panel Example

Topic: `homeassistant/alarm_control_panel/esp32-demo/alarm/config`

```json
{
  "name": "Home Alarm",
  "unique_id": "esp32-demo_alarm",
  "state_topic": "homeassistant/alarm_control_panel/esp32-demo/alarm/state",
  "command_topic": "homeassistant/alarm_control_panel/esp32-demo/alarm/set",
  "icon": "mdi:shield-home",
  "code": "1234",
  "code_arm_required": false,
  "code_disarm_required": true,
  "code_trigger_required": false,
  "command_template": "{{ action }}{% if code %} {{ code }}{% endif %}",
  "payload_arm_home": "ARM_HOME",
  "payload_arm_away": "ARM_AWAY",
  "payload_disarm": "DISARM",
  "payload_trigger": "TRIGGER",
  "supported_features": ["arm_home", "arm_away", "trigger"],
  "device": { "..." : "..." },
  "availability_topic": "homeassistant/esp32-demo/availability",
  "payload_available": "online",
  "payload_not_available": "offline"
}
```

Note: `code`, `code_*_required`, and `command_template` fields are **only included** when code configuration is active. `payload_arm_*` and `payload_trigger` fields are only included for features present in the `supportedFeatures` bitmask. `payload_disarm` is always included.

---

## Device Registry

All entities share a common `device` object in their discovery payloads, causing Home Assistant to group them under one device entry.

```json
{
  "identifiers": ["{nodeId}"],
  "name": "{deviceName}",
  "model": "{model}",
  "manufacturer": "{manufacturer}",
  "sw_version": "{swVersion}",
  "configuration_url": "{configUrl}",
  "suggested_area": "{suggestedArea}"
}
```

`configuration_url` and `suggested_area` are only included when non-empty.

---

## Command Handling

When HA sends a command (e.g., turning a switch ON), the flow is:

1. MQTT broker delivers the message to the device on topic `{prefix}/{component}/{nodeId}/{entityId}/set`.
2. The MQTT component emits an `mqtt/message` event via the EventBus.
3. `HomeAssistantComponent` receives the event and extracts the `entityId` from the topic.
4. The entity is looked up by ID. If not found, the command is ignored with a warning log.
5. `entity->handleCommand(payload)` is called via **virtual dispatch** (v2.0.0). Each entity type validates the command and stores state internally:
   - **`HASwitch`**: Sets `state = (payload == payloadOn)`. Returns `true`.
   - **`HALight`**: Parses JSON or simple ON/OFF. Sets `state` and `brightness`. Returns `true` for valid payloads, `false` for garbage.
   - **`HAButton`**: Returns `true` only if `payload == payloadPress`, `false` otherwise.
   - **`HAAlarmControlPanel`**: Parses `"COMMAND"` or `"COMMAND CODE"` format into `lastCommand`/`lastCode`. Returns `true`.
6. If `handleCommand()` returns `true`, an `HACommandEvent` is emitted on the `ha/command` EventBus topic.
7. If `handleCommand()` returns `false`, the event is suppressed (invalid command).
8. For switches with `autoPublishState == true` and `optimistic == false`, the received payload is immediately published back as state.
9. `stats.commandsReceived` is incremented.

### Re-Entrancy Guard

A `volatile bool publishing` flag prevents re-entrant state publishing. This protects against callback loops where a command callback triggers a state publish that triggers another event.

---

## Availability

The component manages device-level availability:

- **On MQTT connect**: Publishes `"online"` to `{prefix}/{nodeId}/availability` (retained).
- **On shutdown**: Publishes `"offline"` (retained).
- **LWT integration**: Configure the MQTT component's Last Will Testament to publish `"offline"` to the same availability topic for crash detection.

The `isReady()` method returns `true` only when both conditions are met:
1. MQTT is connected (`mqttConnected == true`)
2. Availability has been published (`availabilityPublished == true`)
