# DomoticsCore-HomeAssistant -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides the complete API surface, MQTT topic conventions, payload formats, and internal architecture of the DomoticsCore-HomeAssistant component (v1.6.0).

---

## Table of Contents

1. [HAConfig](#haconfig)
2. [HomeAssistantComponent](#homeassistantcomponent)
3. [HAEntity Base Class](#haentity-base-class)
4. [HASensor](#hasensor)
5. [HABinarySensor](#habinarysensor)
6. [HASwitch](#haswitch)
7. [HALight](#halight)
8. [HAButton](#habutton)
9. [HAAlarmControlPanel](#haalarmcontrolpanel)
10. [HAEvents](#haevents)
11. [HAStatistics](#hastatistics)
12. [HomeAssistantWebUI](#homeassistantwebui)
13. [MQTT Topic Structure](#mqtt-topic-structure)
14. [Discovery Payloads](#discovery-payloads)
15. [Device Registry](#device-registry)
16. [Command Handling](#command-handling)
17. [Availability](#availability)

---

## HAConfig

Configuration structure for the Home Assistant component. Passed to `HomeAssistantComponent` at construction.

**Namespace:** `DomoticsCore::Components::HomeAssistant`

```cpp
struct HAConfig {
    String nodeId = "myDeviceId";             // Unique device ID (used in MQTT topics)
    String deviceName = "My Device";          // Display name in HA device registry
    String manufacturer = "DomoticsCore";     // Manufacturer shown in HA
    String model = "MyDeviceModel";           // Hardware model (auto-detected via ESP.getChipModel())
    String swVersion = "1.0.0";               // Firmware version
    bool retainDiscovery = true;              // Retain discovery messages on the broker
    String discoveryPrefix = "homeassistant"; // MQTT discovery prefix (match HA config)
    String availabilityTopic = "";            // Auto-generated as "{prefix}/{nodeId}/availability" if empty
    String configUrl = "";                    // URL for "Configuration" link in HA device page
    String suggestedArea = "";                // Suggested area in HA (e.g., "Living Room")
};
```

| Field | Default | Description |
|-------|---------|-------------|
| `nodeId` | `"myDeviceId"` | Used in all MQTT topics and as the device identifier. Must be unique per device. |
| `deviceName` | `"My Device"` | Human-readable device name in the HA device registry. |
| `manufacturer` | `"DomoticsCore"` | Populated from `SystemConfig.manufacturer` when using the System component. |
| `model` | `"MyDeviceModel"` | Populated from `SystemConfig.model`; auto-detected via `ESP.getChipModel()`. |
| `swVersion` | `"1.0.0"` | Populated from `SystemConfig.firmwareVersion`. |
| `retainDiscovery` | `true` | When true, discovery payloads persist on the broker across broker restarts. |
| `discoveryPrefix` | `"homeassistant"` | Must match the MQTT discovery prefix configured in Home Assistant. |
| `availabilityTopic` | `""` (auto) | If left empty, auto-generated as `{discoveryPrefix}/{nodeId}/availability`. |
| `configUrl` | `""` | Optional. If set, HA shows a "Configuration" link on the device page. |
| `suggestedArea` | `""` | Optional. Suggests a room/area when the device first appears in HA. |

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
- `version`: `"1.6.0"`
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

Registers a read-only sensor entity. If `stateClass` is empty and `unit` is non-empty, `stateClass` defaults to `"measurement"` in the discovery payload.

#### addBinarySensor

```cpp
void addBinarySensor(const String& id, const String& name,
                     const String& deviceClass = "", const String& icon = "");
```

Registers a binary (on/off) sensor entity.

> **Note (M19):** Unlike `addSensor()`, this method does **not** emit the `ha/entity_added` event. This is a known inconsistency. See [HAEvents -- Known Issues](#known-issues) for details.

#### addSwitch

```cpp
void addSwitch(const String& id, const String& name,
               std::function<void(bool)> commandCallback,
               const String& icon = "",
               bool autoPublishState = true, bool optimistic = false);
```

Registers a controllable switch entity.

| Parameter | Description |
|-----------|-------------|
| `commandCallback` | Called with `true` (ON) or `false` (OFF) when HA sends a command. |
| `autoPublishState` | When `true`, the component auto-publishes the received state back to HA after the callback executes. |
| `optimistic` | When `true`, HA assumes the command succeeds without waiting for state confirmation. |

#### addLight

```cpp
void addLight(const String& id, const String& name,
              std::function<void(bool, uint8_t)> commandCallback);
```

Registers a light entity with brightness support. The callback receives `(state, brightness)` where brightness is 0-255.

#### addButton

```cpp
void addButton(const String& id, const String& name,
               std::function<void()> pressCallback,
               const String& icon = "");
```

Registers a trigger-only button entity. The callback fires when the user presses the button in HA.

#### addAlarmControlPanel

```cpp
void addAlarmControlPanel(
    const String& id, const String& name,
    const std::function<void(const String& command, const String& code)>& commandCallback,
    const String& icon = "mdi:shield-home",
    uint8_t features = AlarmFeature::ArmAway,
    const String& code = "",
    bool codeArmRequired = false,
    bool codeDisarmRequired = false,
    bool codeTriggerRequired = false);
```

Registers a native Home Assistant alarm control panel entity. Renders as the alarm panel Lovelace card with keypad and color-coded status.

| Parameter | Description |
|-----------|-------------|
| `commandCallback` | Called with `(command, code)` when HA sends an arm/disarm/trigger command. Command is one of `AlarmPanelCommand::*`. Code is the raw PIN entered by the user (empty if not provided). **The library does not validate the code — the consumer must.** |
| `features` | Bitmask of `AlarmFeature` flags defining which arm modes are available in the HA UI. |
| `code` | PIN code included in discovery so HA's frontend shows a keypad. The library does NOT validate entered codes — it passes them through to the callback. |
| `codeArmRequired` | If true, HA's frontend requires code entry for arm operations. |
| `codeDisarmRequired` | If true, HA's frontend requires code entry for disarm. |
| `codeTriggerRequired` | If true, HA's frontend requires code entry for trigger. |

> **Important — Library vs Consumer boundary:** The alarm panel entity is a thin MQTT plumbing layer. The library handles discovery, command parsing, and topic management. **All business logic is the consumer's responsibility:**
> - **Code validation** — The library passes the raw code to the callback; the consumer must verify it.
> - **State transitions** — The library has no state machine; the consumer calls `publishState()` with the appropriate `AlarmPanelState::*` value.
> - **Command validation** — The library does not check if a command matches the configured `supportedFeatures`.
> - **Timing** — Arming delays, entry delays, trigger durations are entirely consumer logic.

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
void setDeviceInfo(const String& name, const String& model,
                   const String& manufacturer, const String& swVersion);
```

### Status Methods

```cpp
bool isReady() const;           // True if MQTT connected AND availability published
bool isMQTTConnected() const;   // True if MQTT connection is active
const HAStatistics& getStatistics() const;
```

---

## HAEntity Base Class

Abstract base class for all entity types.

**Namespace:** `DomoticsCore::Components::HomeAssistant`
**Header:** `DomoticsCore/HAEntity.h`

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `id` | `String` | -- | Unique entity ID (e.g., `"temperature"`) |
| `name` | `String` | -- | Display name (e.g., `"Temperature"`) |
| `component` | `String` | -- | HA component type (`"sensor"`, `"switch"`, etc.) |
| `icon` | `String` | `""` | MDI icon name (e.g., `"mdi:thermometer"`) |
| `deviceClass` | `String` | `""` | HA device class (e.g., `"temperature"`, `"motion"`) |
| `retained` | `bool` | `true` | Whether state messages are retained on the broker |

### Topic Generation Methods

All topic methods take `nodeId` and an optional `discoveryPrefix` (default `"homeassistant"`).

```cpp
String getDiscoveryTopic(const String& nodeId, const String& discoveryPrefix) const;
// Returns: {prefix}/{component}/{nodeId}/{id}/config

String getStateTopic(const String& nodeId, const String& discoveryPrefix) const;
// Returns: {prefix}/{component}/{nodeId}/{id}/state

String getCommandTopic(const String& nodeId, const String& discoveryPrefix) const;
// Returns: {prefix}/{component}/{nodeId}/{id}/set

String getAttributesTopic(const String& nodeId, const String& discoveryPrefix) const;
// Returns: {prefix}/{component}/{nodeId}/{id}/attributes

String getUniqueId(const String& nodeId) const;
// Returns: {nodeId}_{id}
```

### Discovery Payload

```cpp
virtual void buildDiscoveryPayload(JsonDocument& doc, const String& nodeId,
                                   const String& discoveryPrefix,
                                   const JsonObject& device,
                                   const String& availabilityTopic) const;
```

The base implementation adds: `name`, `unique_id`, `state_topic`, `icon` (if set), `device_class` (if set), `device` (device registry object), and `availability_topic` with `payload_available`/`payload_not_available`.

Derived classes call the base implementation and then add type-specific fields.

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
| `commandCallback` | `std::function<void(bool)>` | `nullptr` | Called when HA sends ON/OFF command |

### Discovery Fields Added

- `command_topic`
- `payload_on`, `payload_off`
- `state_on`, `state_off`
- `optimistic` (only if `true`)

### Command Handling

```cpp
void handleCommand(const String& payload);
```

Compares `payload` to `payloadOn` and invokes `commandCallback(true)` or `commandCallback(false)`.

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
| `commandCallback` | `std::function<void(bool, uint8_t)>` | `nullptr` | Called with `(state, brightness)` |

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
void handleCommand(const String& payload);
```

Attempts to parse `payload` as JSON with `state` and `brightness` keys. Falls back to simple `"ON"`/`"OFF"` parsing if JSON deserialization fails.

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
| `pressCallback` | `std::function<void()>` | `nullptr` | Called when the button is pressed |

### Discovery Fields Added

Buttons override the base `buildDiscoveryPayload()` completely (no `state_topic` is added):

- `name`, `unique_id`, `icon`, `device_class`, `device`, `availability_topic`
- `command_topic`
- `payload_press`

### Command Handling

```cpp
void handleCommand(const String& payload);
```

Invokes `pressCallback()` only if `payload == payloadPress`.

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
| `code` | `String` | `""` | PIN code sent to HA frontend for keypad display; the library does NOT validate it — passthrough only |
| `supportedFeatures` | `uint8_t` | `ArmAway` | Bitmask of supported arm modes |
| `codeArmRequired` | `bool` | `false` | Require code for arm operations |
| `codeDisarmRequired` | `bool` | `false` | Require code for disarm |
| `codeTriggerRequired` | `bool` | `false` | Require code for trigger |
| `commandCallback` | `std::function<void(const String&, const String&)>` | -- | Called with `(command, code)` |

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
void handleCommand(const String& payload) override;
```

Parses payloads in `"COMMAND"` or `"COMMAND CODE"` format. Trims whitespace. Calls `commandCallback(command, code)` where `code` is empty if not provided. Empty or whitespace-only payloads are rejected with a warning log.

---

## HAEvents

Event constants published by the component via the EventBus.

**Namespace:** `DomoticsCore::HAEvents`
**Header:** `DomoticsCore/HAEvents.h`

| Constant | Topic String | Payload Type | Description |
|----------|-------------|--------------|-------------|
| `EVENT_DISCOVERY_PUBLISHED` | `"ha/discovery_published"` | `int` | Emitted after all discovery payloads are sent. Payload is entity count. |
| `EVENT_ENTITY_ADDED` | `"ha/entity_added"` | `String` | Emitted when a new entity is registered. Payload is the entity ID. |

### Known Issues

**C21 -- `HAEntityAddedEvent` struct/EventBus payload alignment:** The `HAEntityAddedEvent` struct is defined in `HomeAssistant.h` with `id[64]` and `component[32]` fields, but the EventBus `emit()` call for `EVENT_ENTITY_ADDED` currently passes a `String` (the entity ID only). The struct and the actual emission are not aligned. This is pending resolution -- either the emit should use the struct, or the struct should be removed.

```cpp
// Defined in HomeAssistant.h but NOT used by emit():
struct HAEntityAddedEvent {
    char id[64];           // Entity ID
    char component[32];    // Component type (sensor, switch, etc.)
};
```

**M19 -- Inconsistent `ha/entity_added` emission:** Only `addSensor()` emits the `ha/entity_added` event. The following methods do **not** emit it, which is inconsistent:
- `addBinarySensor()` -- missing `emit(EVENT_ENTITY_ADDED, id)`
- `addSwitch()` -- missing `emit(EVENT_ENTITY_ADDED, id)`
- `addLight()` -- missing `emit(EVENT_ENTITY_ADDED, id)`
- `addButton()` -- missing `emit(EVENT_ENTITY_ADDED, id)`

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

POST to `ha_settings` with parameters: `node_id`, `device_name`, `manufacturer`, `model`, `discovery_prefix`, `suggested_area`. The handler updates the config, invokes the save callback, and republishes discovery.

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
  "command_template": "{{ action }}{% if code %} {{ code }}{% endif }}",
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
5. The command is routed based on `entity->component`:
   - **`"switch"`**: Calls `HASwitch::handleCommand(payload)`. If `autoPublishState` is true and `optimistic` is false, the received payload is immediately published back as state.
   - **`"light"`**: Calls `HALight::handleCommand(payload)` which parses JSON or falls back to simple ON/OFF.
   - **`"button"`**: Calls `HAButton::handleCommand(payload)` which fires the callback only if payload matches `payloadPress`.
   - **`"alarm_control_panel"`**: Calls `entity->handleCommand(payload)` via virtual dispatch. Parses `"COMMAND"` or `"COMMAND CODE"` format and delegates to consumer callback. **No auto-publish** -- the consumer manages all state transitions.
6. `stats.commandsReceived` is incremented.

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
