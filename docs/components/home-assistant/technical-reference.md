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
| `availabilityTopic` | 128 chars | `""` (auto) | The topic every entity's `avty_t` points at. **Left empty it becomes the MQTT component's effective `lwtTopic`** (`{clientId}/status` by default), so the broker's Last Will lands where Home Assistant is watching. Set it, and the Last Will moves onto it instead. See [Availability](#availability). |
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
- Auto-generates `availabilityTopic` if not provided, then reconciles it with the MQTT component's Last Will — see [Availability](#availability).

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

#### entity

```cpp
HAEntity* entity(const String& id);
```

The registered entity with this id, or `nullptr`: the way to set the discovery fields `add*()` does not take (`entityCategory`, `stateTopicOverride`, `valueTemplate`, `jsonAttributesTopic`, `useAvailability`) before the connect that publishes discovery.

Registering an id twice is not refused: both entities publish a config for the same `uniq_id` and Home Assistant keeps the first. The component logs a warning at the second registration.

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

Publishes a string state value to the entity's state topic. While the broker is
unreachable the payload is **held** rather than dropped, and published once the
link returns — see [Holding a state through an outage](#holding-a-state-through-an-outage).

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

Publishes additional attributes as JSON to the entity's attributes topic. Always
retained, and held through an outage on the same terms as a state.

### Holding a state through an outage

A state is last-value-wins, and the broker keeps the last one it received. If the
link drops, a state published meanwhile would otherwise be lost for good: Home
Assistant marks the device unavailable through the Last Will, then reads the
*retained* pre-outage value when the device comes back and displays it until the
entity changes again.

So `publishState()`, `publishStateJson()` and `publishAttributes()` hand the
payload to a store when `isMQTTConnected()` is false:

- **one slot per entity and topic**, overwritten — ten changes during one outage
  cost one slot and republish one value, the last;
- **a byte budget as well as a slot count.** A slot may hold up to
  `MQTT_EVENT_PAYLOAD_SIZE - 1` characters, so the store also refuses — aloud,
  and counted in `HAStatistics::statesRefused` — a payload that would take it
  past 2 048 bytes. A payload over the event field is refused the same way,
  since it could not have been published either;
- the store is **empty while the link is up** and releases its buffer once
  drained, so it costs the component a single vector rather than a field on
  every entity. `shutdown()` releases it too;
- **a publish made over a live link supersedes anything held for the same
  topic**, so an application that republishes its own state on reconnection —
  which the examples do on `isReady()` — is never overwritten by an older value
  still waiting in the store;
- the store is drained **four entries per `loop()`**, behind the reconnection's
  discovery documents.

`getPendingPublishCount()` reports what is waiting. It is non-zero during an
outage *and* for the first loops after a reconnection, until the drain catches
up.

**Do not gate a publish on the link.** `if (mqtt connected) publishState(...)`
is the pattern this store exists to make unnecessary, and it defeats it: the
component never sees the value, so it cannot hold it.

The store takes a recursive lock, so publishing a state from a task other than
the loop — a web-server handler, say — is safe; the drain in `loop()` takes the
same lock.

#### What the pacing is for, and where it stops

`publishDiscovery()` emits one `mqtt/publish` per entity inside a single
EventBus handler, against a queue that holds about 32 of them and **evicts the
oldest**. Draining the store in the same pass would take that burst from
1 + N events to 1 + 2N. Pacing keeps the lossless connect at around thirty
entities instead of halving it to fifteen; **above that, the discovery burst
alone is what evicts**, with or without this store.

Two orderings the store relies on and does not enforce:

- a value that reached MQTT's own offline queue just before the component
  noticed the link was down is drained by MQTT before the store's flush is
  dispatched, so the older copy goes out first;
- the topic is resolved when the payload is sent, not when it is held, so a
  `nodeId` or `discoveryPrefix` changed during an outage republishes the held
  value on the topic the *new* discovery document advertises.

A state published with `retained = false` may reach the broker before Home
Assistant has resubscribed after the reconnection; only a retained state is
guaranteed to be read.

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

Every message this component sends crosses the EventBus as an `MQTTPublishEvent`, whose payload field holds 699 characters; on ESP8266 the MQTT client's packet buffer is 768 bytes, 7 of them header, the topic included. A discovery document longer than the field is not published — it would be cut mid-JSON, sit retained on the broker and be discarded by Home Assistant at every restart — and the component logs a warning naming the topic and the size, counts it in `discoveryRefused`, and does not announce it as queued. The keys are abbreviated so that ordinary configs stay well under: an alarm control panel with two arm modes, a code and the default device block is 617 characters, where the long spellings put a comparable panel at 774. The device block (name, model, manufacturer, version, configuration URL, area) and the node id, which appears six times, are what make a config long; a panel with all six arm modes, a 32-character node id, a configuration URL and an area is 974 characters and is refused.

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
size_t getPendingPublishCount() const;  // States and attributes an outage is holding
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
| `entityCategory` | `String` | `""` | `"diagnostic"` or `"config"`; emitted as `ent_cat` when set |
| `stateTopicOverride` | `String` | `""` | Replaces the generated state topic when set: several entities can read one JSON payload |
| `valueTemplate` | `String` | `""` | Emitted as `val_tpl`; the expression Home Assistant applies to the state topic's payload |
| `jsonAttributesTopic` | `String` | `""` | Emitted as `json_attr_t`; a JSON payload there becomes the entity's attributes |
| `useAvailability` | `bool` | `true` | `false` leaves the availability block out, so the entity stays readable while the device is offline |

The five discovery fields above are emitted only when set; an entity that sets none of them produces the same document as before they existed. `stateTopicOverride` and `jsonAttributesTopic` also redirect `getStateTopic()` and `getAttributesTopic()`, so `publishState()`, `publishStateJson()` and `publishAttributes()` land where the discovery config told Home Assistant to read. `entityCategory` accepts `"diagnostic"` and `"config"`; any other value is left out of the document with a warning, since Home Assistant would reject the whole config. Set the fields through `HomeAssistantComponent::entity(id)` after `add*()` and before the connect that publishes discovery; after it, `republishEntity(id)`.

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

The base implementation adds: `name`, `uniq_id`, `stat_t`, `ic` (if set), `dev_cla` (if set), `dev` (device registry object), and `avty_t` with `pl_avail`/`pl_not_avail`; then, only when set, `ent_cat`, `val_tpl` and `json_attr_t`.

Every key is written in the short form Home Assistant documents for MQTT discovery and expands on receipt (`stat_t` is `state_topic`, `cmd_t` is `command_topic`, `uniq_id` is `unique_id`, `dev` is `device` with `ids`, `mf`, `mdl`, `sw`, `cu`, `sa`; `pl_` is `payload_`, `_tpl` is `_template`, `avty` is `availability`, `cod_arm_req` is `code_arm_required`, `sup_feat` is `supported_features`). A config crosses the EventBus in a 699-character field and, on ESP8266, PubSubClient's 768-byte packet buffer; the long spellings put an alarm control panel over both. The full list is in Home Assistant's MQTT discovery documentation.

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

- `unit_of_meas` -- if `unit` is non-empty.
- `stat_cla` -- if explicitly set; otherwise defaults to `"measurement"` when `unit` is non-empty.
- `exp_aft` -- if `expireAfter > 0`.

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

- `pl_on`
- `pl_off`

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

- `cmd_t`
- `pl_on`, `pl_off`
- `stat_on`, `stat_off`
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

- `cmd_t`
- `pl_on`, `pl_off`
- `stat_val_tpl`: `{{ value_json.state }}`
- When `supportsBrightness` is true:
  - `brightness`: `true`
  - `bri_scl`: `255`
  - `bri_stat_t` (same as state topic)
  - `bri_cmd_t` (same as command topic)
  - `bri_val_tpl`: `{{ value_json.brightness }}`
  - `on_cmd_type`: `"brightness"`
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

Buttons override the base `buildDiscoveryPayload()` completely (no `stat_t` is added):

- `name`, `uniq_id`, `icon`, `device_class`, `device`, `avty_t`
- `cmd_t`
- `pl_prs`

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
| `codeArmRequired` | `bool` | `false` | Require code for arm operations; always published, since Home Assistant reads an absent key as `true` |
| `codeDisarmRequired` | `bool` | `false` | Require code for disarm; always published, for the same reason |
| `codeTriggerRequired` | `bool` | `false` | Require code for trigger; published on panels that declare the `Trigger` feature |
| `lastCommand` | `char[64]` | `""` | Parsed command from last `handleCommand()` (e.g., `"ARM_AWAY"`) |
| `lastCode` | `char[32]` | `""` | Parsed code from last `handleCommand()` (e.g., `"1234"`) |

### Discovery Fields Added

- `cmd_t`
- `cod_arm_req` and `cod_dis_req`, always. Home Assistant reads an **absent**
  `code_arm_required` or `code_disarm_required` as `true`, and both default to
  `false` here: an omitted key advertises the reverse of the entity's own
  configuration. A panel published without `cod_arm_req` is created, reports its
  state and disarms, and cannot be armed from the interface at all — Home
  Assistant demands a code, and with no `code` configured `code_format` is null,
  so the card cannot offer a keypad either.
- `cod_trig_req`, on a panel whose `supportedFeatures` includes `Trigger`. The
  trigger service requires that feature, so the key can change nothing on a panel
  that does not declare it and is left out.
- `code`, when one is configured.
- `cmd_tpl` (`{{ action }}{% if code %} {{ code }}{% endif %}`), when a code
  travels: one configured here, or one Home Assistant asks the user for.
- `sup_feat` JSON array built from bitmask

No `pl_*` key is published. Every command payload this panel accepts — `ARM_HOME`,
`DISARM`, the seven of `AlarmPanelCommand` — is the value Home Assistant assumes
for an absent key, so stating them spends the field to assert nothing: 188
characters on a panel offering every arm mode. The agreement rests on those
constants, which a native test holds equal to the payloads Home Assistant
documents. They exist for the consumer's state machine to compare against —
nothing inside the component reads them, and `handleCommand()` passes on
whatever arrives — so a consumer comparing against its own literals is on its
own.

A code-less panel with two arm modes (`arm_away`, `arm_night`), the default
device block, a 17-character node id and a 13-character entity id publishes a
541-character document; the two requirement keys are 40 of those characters,
against the 699-character field of the section above. The same panel with all six
arm modes fits as well, at 552 characters on a 9-character node id — it did not
while the payload constants were written out.

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
    uint32_t discoveryRefused = 0;  // Configs never handed to MQTT: over the event field
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
| Availability | the MQTT component's `lwtTopic` — `{clientId}/status` by default | Device -> Broker and Broker -> HA (Last Will) | Yes |

A topic also crosses the EventBus, in a 127-character field. A longer one is
**not published**: the component logs a warning naming the entity and the length
the topic needed, and counts a refused discovery in `discoveryRefused`. Published
cut, the config would land on a topic Home Assistant never reads, and two entity
ids sharing a prefix would cut to the same state topic and overwrite each other.
The budget is `len(prefix) + len(component) + len(nodeId) + len(entityId) + 4`
plus the suffix, so with a 32-character prefix and node id, `alarm_control_panel`
leaves 34 characters for the entity id — 30 if the entity publishes attributes,
`attributes` being the longest suffix.

### Command Subscription

The component subscribes to a single wildcard topic to receive all commands:

```
{prefix}/+/{nodeId}/+/set
```

Commands are routed internally to the appropriate entity based on the entity ID extracted from the topic.

The filter is built from the discovery prefix when the broker connects, and a
prefix changed afterwards — through the settings page or the persisted
configuration — re-subscribes immediately rather than at the next reconnection.

The MQTT client is shared, so **every** message the application subscribes to
also reaches this component. A topic that does not begin with the discovery
prefix followed by `/` belongs to the application: it is dropped before the
topic is parsed and produces no log line at any level. A malformed topic *under*
the prefix is a different matter and keeps its error line.

### Topic Generation (zero-heap)

All topic methods use caller-provided `char*` buffers (v2.0.0) and return the
length the topic needed, which is what a caller compares against the buffer to
tell a fit from a cut:

```cpp
char topic[HA_TOPIC_BUF_SIZE];  // HA_TOPIC_BUF_SIZE = 128, matching the event field
int needed = entity->getStateTopic(topic, sizeof(topic), nodeId, discoveryPrefix);
```

---

## Discovery Payloads

### Sensor Example

Topic: `homeassistant/sensor/esp32-demo/temperature/config`

```json
{
  "name": "Temperature",
  "uniq_id": "esp32-demo_temperature",
  "stat_t": "homeassistant/sensor/esp32-demo/temperature/state",
  "unit_of_meas": "C",
  "dev_cla": "temperature",
  "stat_cla": "measurement",
  "ic": "mdi:thermometer",
  "dev": {
    "ids": ["esp32-demo"],
    "name": "ESP32 Demo Device",
    "mdl": "ESP32",
    "mf": "DomoticsCore",
    "sw": "1.0.0"
  },
  "avty_t": "ESP32-0000a1b2c3d4/status",
  "pl_avail": "online",
  "pl_not_avail": "offline"
}
```

### Switch Example

Topic: `homeassistant/switch/esp32-demo/relay/config`

```json
{
  "name": "Relay",
  "uniq_id": "esp32-demo_relay",
  "stat_t": "homeassistant/switch/esp32-demo/relay/state",
  "cmd_t": "homeassistant/switch/esp32-demo/relay/set",
  "pl_on": "ON",
  "pl_off": "OFF",
  "stat_on": "ON",
  "stat_off": "OFF",
  "dev": { "..." : "..." },
  "avty_t": "ESP32-0000a1b2c3d4/status",
  "pl_avail": "online",
  "pl_not_avail": "offline"
}
```

### Light Example

Topic: `homeassistant/light/esp32-demo/led/config`

```json
{
  "name": "LED Strip",
  "uniq_id": "esp32-demo_led",
  "stat_t": "homeassistant/light/esp32-demo/led/state",
  "cmd_t": "homeassistant/light/esp32-demo/led/set",
  "pl_on": "ON",
  "pl_off": "OFF",
  "stat_val_tpl": "{{ value_json.state }}",
  "brightness": true,
  "bri_scl": 255,
  "bri_stat_t": "homeassistant/light/esp32-demo/led/state",
  "bri_cmd_t": "homeassistant/light/esp32-demo/led/set",
  "bri_val_tpl": "{{ value_json.brightness }}",
  "on_cmd_type": "brightness",
  "dev": { "..." : "..." },
  "avty_t": "ESP32-0000a1b2c3d4/status",
  "pl_avail": "online",
  "pl_not_avail": "offline"
}
```

### Button Example

Topic: `homeassistant/button/esp32-demo/restart/config`

```json
{
  "name": "Restart",
  "uniq_id": "esp32-demo_restart",
  "cmd_t": "homeassistant/button/esp32-demo/restart/set",
  "pl_prs": "PRESS",
  "ic": "mdi:restart",
  "dev": { "..." : "..." },
  "avty_t": "ESP32-0000a1b2c3d4/status",
  "pl_avail": "online",
  "pl_not_avail": "offline"
}
```

Note: Buttons do **not** include a `stat_t`.

### Alarm Control Panel Example

Topic: `homeassistant/alarm_control_panel/esp32-demo/alarm/config`

```json
{
  "name": "Home Alarm",
  "uniq_id": "esp32-demo_alarm",
  "stat_t": "homeassistant/alarm_control_panel/esp32-demo/alarm/state",
  "cmd_t": "homeassistant/alarm_control_panel/esp32-demo/alarm/set",
  "ic": "mdi:shield-home",
  "code": "1234",
  "cod_arm_req": false,
  "cod_dis_req": true,
  "cod_trig_req": false,
  "cmd_tpl": "{{ action }}{% if code %} {{ code }}{% endif %}",
  "sup_feat": ["arm_home", "arm_away", "trigger"],
  "dev": { "..." : "..." },
  "avty_t": "ESP32-0000a1b2c3d4/status",
  "pl_avail": "online",
  "pl_not_avail": "offline"
}
```

Note: `cod_arm_req` and `cod_dis_req` are **always** included — Home Assistant defaults an absent `code_*_required` to `true`, the reverse of this component's default, so silence would make every code-less panel unarmable. `cod_trig_req` is included on panels declaring the `Trigger` feature. `code` and `cmd_tpl` are included when a code travels. No `pl_*` key is published at all: each would restate the payload Home Assistant already assumes.

---

## Device Registry

All entities share a common `device` object in their discovery payloads, causing Home Assistant to group them under one device entry.

```json
{
  "ids": ["{nodeId}"],
  "name": "{deviceName}",
  "mdl": "{model}",
  "mf": "{manufacturer}",
  "sw": "{swVersion}",
  "cu": "{configUrl}",
  "sa": "{suggestedArea}"
}
```

`cu` and `sa` are only included when non-empty.

---

## Command Handling

When HA sends a command (e.g., turning a switch ON), the flow is:

1. MQTT broker delivers the message to the device on topic `{prefix}/{component}/{nodeId}/{entityId}/set`.
2. The MQTT component emits an `mqtt/message` event via the EventBus.
3. `HomeAssistantComponent` receives the event and extracts the `entityId` from the topic.
4. The entity is looked up by ID. If not found, the command is ignored with a warning log.
5. `stats.commandsReceived` is incremented (note: this happens before validation, so invalid commands are counted too).
6. `entity->handleCommand(payload)` is called via **virtual dispatch** (v2.0.0). Each entity type validates the command and stores state internally:
   - **`HASwitch`**: Sets `state = (payload == payloadOn)`. Returns `true`.
   - **`HALight`**: Parses JSON or simple ON/OFF. Sets `state` and `brightness`. Returns `true` for valid payloads, `false` for garbage.
   - **`HAButton`**: Returns `true` only if `payload == payloadPress`, `false` otherwise.
   - **`HAAlarmControlPanel`**: Parses `"COMMAND"` or `"COMMAND CODE"` format into `lastCommand`/`lastCode`. Returns `true`.
7. If `handleCommand()` returns `true`, an `HACommandEvent` is emitted on the `ha/command` EventBus topic.
8. If `handleCommand()` returns `false`, the event is suppressed (invalid command).
9. For switches with `autoPublishState == true` and `optimistic == false`, the received payload is immediately published back as state.

### Re-Entrancy Guard

A `volatile bool publishing` flag prevents re-entrant state publishing. This protects against callback loops where a command callback triggers a state publish that triggers another event.

---

## Availability

Home Assistant watches exactly **one** topic per device, the one `avty_t` names, and it is the only signal that tells it a device has gone. The device can publish `"online"` itself; it cannot publish `"offline"` when it has crashed or lost the link. Only the broker can, through the MQTT Last Will — so the topic Home Assistant is told to watch must be the topic the Last Will is set on, or entities stay available for ever after the device disappears.

The component enforces that at `begin()`, and again on every `setConfig()`:

- **`availabilityTopic` left empty** — it becomes the MQTT component's effective `lwtTopic`, `{clientId}/status` by default. Nothing else has to be configured.
- **`availabilityTopic` set by the application** — `MQTTConfig::lwtTopic` is moved onto it. A session already open is reopened, because the Last Will is sent in the CONNECT packet and cannot be changed afterwards.
- **`lwtRetain` false** — forced to `true`. Availability is published retained; a transient Last Will would leave that retained `"online"` standing after the device is gone.

Three configurations cannot be reconciled and are reported in the log instead of being advertised as if they worked: **no MQTT component**, **`enableLWT` false**, and **a `lwtTopic` set to an empty string**. In all three the generated topic is kept and no Last Will corrects it. A **`lwtMessage` other than `"offline"`** is also reported: entities declare `pl_not_avail: "offline"`, so a will saying anything else lands on the right topic and still never marks the device unavailable.

What the component publishes itself:

- **On MQTT connect**: `"online"` to the availability topic (retained).
- **On shutdown**: `"offline"` (retained).

Moving `MQTTConfig::lwtTopic` from the MQTT side afterwards — through the MQTT settings page, for instance — is not observed: the discovery documents keep pointing at the previous topic until the next reconciliation. Configure the pair from one side.

The `isReady()` method returns `true` only when both conditions are met:
1. MQTT is connected (`mqttConnected == true`)
2. Availability has been published (`availabilityPublished == true`)
