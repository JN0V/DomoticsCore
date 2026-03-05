# Deep-Dive: HA Alarm Control Panel

> **Generated:** 2026-03-05 | **Mode:** Deep-dive | **Scan Level:** Exhaustive
> **Files Analyzed:** 12 | **Lines of Code Scanned:** ~2,950

---

## Overview

The `HAAlarmControlPanel` entity adds native Home Assistant `alarm_control_panel` support to the DomoticsCore-HomeAssistant component. It renders as the HA native alarm panel Lovelace card with keypad, color-coded status, and multiple arm modes.

**Key characteristics:**
- Consumer-managed state transitions (no auto-publish)
- Optional PIN code passthrough
- Feature bitmask for supported arm modes
- First entity to use polymorphic `handleCommand()` dispatch via base pointer

---

## Library vs Consumer Responsibilities

The alarm panel entity is deliberately a **thin MQTT plumbing layer**. The library handles protocol concerns; all business logic belongs to the consumer.

### What the library does

| Concern | Detail |
|---------|--------|
| **Discovery payload** | Generates the JSON that tells Home Assistant the alarm panel exists, which arm modes are available, and whether a keypad should be shown. |
| **Command parsing** | Receives `"COMMAND"` or `"COMMAND CODE"` payloads from MQTT, splits them, and forwards `(command, codeValue)` to the consumer callback. |
| **Topic management** | Generates discovery, state, and command MQTT topics following the HA convention. |
| **Feature bitmask** | Translates `supportedFeatures` into the `supported_features` JSON array and conditionally emits `payload_arm_*` constants. |

### What the library does NOT do

| Concern | Detail |
|---------|--------|
| **Code validation** | The `code` field in discovery only tells the HA frontend to display a keypad. The library **never** checks whether the entered code is correct — it passes the raw value to the consumer callback. The consumer must validate. |
| **Command validation** | The library does not verify that the received command matches a supported feature. A consumer receiving `ARM_NIGHT` when only `ArmAway` is configured must handle the mismatch. |
| **State transitions** | The library has no state machine. It does not know the current state, does not enforce valid transitions, and does not auto-publish any state. The consumer calls `publishState()` explicitly. |
| **Timing / delays** | Arming delays, entry delays, and trigger durations are entirely consumer logic. |
| **Hardware control** | Siren activation, zone monitoring, sensor polling — all consumer domain. |

### Code flow illustration

```
HA frontend (shows keypad because discovery has "code": "1234")
  └─> User enters "5678" and taps "Disarm"
       └─> HA sends MQTT payload: "DISARM 5678"
            └─> Library parses → command="DISARM", codeValue="5678"
                 └─> Library calls: commandCallback("DISARM", "5678")
                      └─> Consumer responsibility:
                           - Is "5678" the correct code? (lib doesn't know)
                           - Is the panel in a state where disarm is valid? (lib doesn't track)
                           - If valid → publishState("alarm", AlarmPanelState::Disarmed)
                           - If invalid → log/ignore/increment failure counter
```

---

## File Inventory

### New Files

#### `HAAlarmControlPanel.h` (159 LOC)

**Path:** `DomoticsCore-HomeAssistant/include/DomoticsCore/HAAlarmControlPanel.h`
**Purpose:** Defines the alarm control panel entity class, feature flags enum, state/command constants, and command parsing logic.

**Exports:**

| Export | Type | Description |
|--------|------|-------------|
| `AlarmFeature` | `enum : uint8_t` | Bitmask flags: `ArmHome` (0x01), `ArmAway` (0x02), `ArmNight` (0x04), `ArmVacation` (0x08), `ArmCustomBypass` (0x10), `Trigger` (0x20) |
| `AlarmPanelState::*` | `constexpr const char*` | 10 state constants: `Disarmed`, `Arming`, `ArmedHome`, `ArmedAway`, `ArmedNight`, `ArmedVacation`, `ArmedCustomBypass`, `Pending`, `Triggered`, `Disarming` |
| `AlarmPanelCommand::*` | `constexpr const char*` | 7 command constants: `ARM_HOME`, `ARM_AWAY`, `ARM_NIGHT`, `ARM_VACATION`, `ARM_CUSTOM_BYPASS`, `DISARM`, `TRIGGER` |
| `HAAlarmControlPanel` | `class : HAEntity` | Main entity class |

**Class: `HAAlarmControlPanel`**

| Member | Type | Default | Description |
|--------|------|---------|-------------|
| `code` | `String` | `""` | PIN code sent to HA frontend for keypad display; the library does NOT validate it — passthrough only |
| `supportedFeatures` | `uint8_t` | `ArmAway` | Bitmask of supported arm modes |
| `codeArmRequired` | `bool` | `false` | Require code for arm operations |
| `codeDisarmRequired` | `bool` | `false` | Require code for disarm |
| `codeTriggerRequired` | `bool` | `false` | Require code for trigger |
| `commandCallback` | `std::function<void(const String&, const String&)>` | -- | Called with `(command, code)` |

**Methods:**

- `buildDiscoveryPayload()` -- Calls base, adds `command_topic`, conditional code config (`code`, `code_arm_required`, `code_disarm_required`, `code_trigger_required`, `command_template`), payload constants per supported feature, and `supported_features` JSON array.
- `handleCommand(payload)` -- Parses `"COMMAND"` or `"COMMAND CODE"` format with whitespace trimming. Delegates to `commandCallback(command, codeValue)`. Empty/whitespace-only payloads are rejected with a warning log.

**Side effects:** None. All state management is delegated to the consumer.

**Risks/Gotchas:**
- `AlarmFeature` is an unscoped enum (see CODE-ROADMAP R25). `Trigger` name may conflict.
- `code` field is a `String` -- contributes to heap fragmentation on ESP8266 (see CODE-ROADMAP R6).
- `handleCommand()` creates temporary `String` objects for command/code parsing.

---

#### `test_ha_alarm_panel.cpp` (497 LOC)

**Path:** `DomoticsCore-HomeAssistant/test/test_ha_alarm_panel/test_ha_alarm_panel.cpp`
**Purpose:** Comprehensive Unity test suite covering all alarm panel functionality.

**Test Categories:**

| # | Test | Type | Coverage |
|---|------|------|----------|
| 1 | `test_alarm_panel_discovery_payload` | Unit | Full discovery JSON, command_topic, state_topic, code fields, command_template, payload constants, supported_features array |
| 2 | `test_alarm_panel_discovery_supported_features` | Unit | Bitmask-to-array conversion: single flag, multiple flags, all flags |
| 3 | `test_alarm_panel_discovery_code_fields` | Unit | No code config = no fields; code set = command_template present |
| 4 | `test_alarm_panel_handle_command_basic` | Unit | Basic command parsing ("ARM_AWAY") |
| 5 | `test_alarm_panel_handle_command_with_code` | Unit | Command with code ("DISARM 1234") |
| 6 | `test_alarm_panel_handle_command_no_callback` | Unit | Null callback safety |
| 7 | `test_alarm_panel_handle_command_edge_cases` | Unit | Empty, whitespace-only, trailing space payloads |
| 8 | `test_alarm_panel_state_publish` | Integration | State publish via Core + HA component, correct topic verification |
| 9 | `test_alarm_panel_no_auto_publish` | Integration | Confirms no auto-publish on command (consumer responsibility) |
| 10 | `test_alarm_panel_add_method` | Integration | `addAlarmControlPanel()` registration with all code parameters |
| 11 | `test_alarm_panel_command_routing` | Integration | End-to-end command routing through HomeAssistantComponent |
| 12 | `test_alarm_panel_polymorphic_dispatch` | Unit | Virtual dispatch through `HAEntity*` base pointer |
| 13 | `test_alarm_panel_heap_stability` | Unit | 10 iterations of create/discover/command with HeapTracker (Constitution XIV) |

**Total: 13 tests (7 unit + 6 integration)**

---

### Modified Files

#### `HAEntity.h` -- Changes

- **Line 84:** Added TODO comment about progressive refactoring of `handleCommand()` virtual override.
- **Line 87:** Added `virtual void handleCommand(const String& payload) {}` base method. This enables polymorphic dispatch for alarm panel and future entity types.

**Impact:** All entity types can now be called via `HAEntity*` pointer. Existing entities (switch, light, button) added `override` to their `handleCommand()` methods.

#### `HomeAssistant.h` -- Changes

- **Line 22:** Added `#include "HAAlarmControlPanel.h"`.
- **Line 73:** Version bumped from `1.5.0` to `1.6.0`.
- **Lines 221-240:** New `addAlarmControlPanel()` method with parameters: id, name, commandCallback, icon, features, code, codeArmRequired, codeDisarmRequired, codeTriggerRequired.
- **Lines 584-586:** Added command routing for `"alarm_control_panel"` using `entity->handleCommand(payload)` (virtual dispatch, no `static_cast`). Comment: "No auto-publish: consumer manages alarm state."

#### `HAButton.h`, `HALight.h`, `HASwitch.h` -- Changes

- Added `override` keyword to `handleCommand()` method signatures. This ensures the compiler verifies virtual override correctness.

#### `library.json` -- Changes

- Version bumped from `1.5.0` to `1.6.0`.

---

## Dependency Graph

```
                    ┌───────────────────┐
                    │    HAEntity.h      │
                    │  (base class)      │
                    └──────┬────────────┘
                           │ inherits
          ┌────────────────┼────────────────┐──────────────────┐
          │                │                │                  │
    ┌─────┴─────┐   ┌─────┴─────┐   ┌─────┴─────┐   ┌───────┴────────┐
    │ HASwitch  │   │ HALight   │   │ HAButton  │   │HAAlarmControl  │
    │ (override)│   │ (override)│   │ (override)│   │ Panel (NEW)    │
    └───────────┘   └───────────┘   └───────────┘   └───────┬────────┘
                                                             │
                    ┌───────────────────┐                    │ includes
                    │ HomeAssistant.h   │ ───────────────────┘
                    │  (orchestrator)   │
                    └──────┬────────────┘
                           │ used by
          ┌────────────────┼────────────────┐
          │                │                │
    ┌─────┴──────┐  ┌─────┴──────┐  ┌──────┴─────────┐
    │test_ha_    │  │test_ha_    │  │BasicHA example │
    │alarm_panel │  │component   │  │(main.cpp)      │
    └────────────┘  └────────────┘  └────────────────┘
```

### Entry Points

- `addAlarmControlPanel()` in `HomeAssistantComponent` (registration)
- `handleCommand()` in `HomeAssistantComponent` (command routing)
- `publishState()` in `HomeAssistantComponent` (state publishing)

### Leaf Nodes

- `AlarmPanelState::*` constants (consumed by user code)
- `AlarmPanelCommand::*` constants (consumed by user code)
- `AlarmFeature` enum (consumed by user code)

---

## Data Flow

### 1. Entity Registration

```
User code
  └─> ha.addAlarmControlPanel("alarm", "Alarm", callback, ...)
       └─> Creates HAAlarmControlPanel with unique_ptr
       └─> Sets supportedFeatures, code, codeXxxRequired
       └─> Pushes to entities vector
       └─> Increments entityCount
       └─> If MQTT connected → republishEntity()
```

### 2. Discovery (MQTT connect)

```
MQTT broker → mqtt/connected event
  └─> HomeAssistantComponent.begin() handler
       └─> publishDiscovery()
            └─> For each entity:
                 └─> HAAlarmControlPanel::buildDiscoveryPayload()
                      ├─> Base: name, unique_id, state_topic, icon, device, availability
                      ├─> command_topic
                      ├─> [if code config active] code, code_*_required, command_template
                      ├─> payload_arm_* (per supportedFeatures bitmask)
                      ├─> payload_disarm (always)
                      └─> supported_features[] array
```

### 3. Command Handling

```
HA user action (arm/disarm)
  └─> MQTT: homeassistant/alarm_control_panel/{nodeId}/{entityId}/set
       └─> Payload: "ARM_AWAY" or "DISARM 1234"
            └─> mqtt/message event
                 └─> HomeAssistantComponent::handleCommand()
                      └─> Extract entityId from topic
                      └─> findEntity(entityId)
                      └─> component == "alarm_control_panel"
                           └─> entity->handleCommand(payload)  [virtual dispatch]
                                └─> HAAlarmControlPanel::handleCommand()
                                     └─> Parse command + optional code
                                     └─> commandCallback("ARM_AWAY", "")
                                         or commandCallback("DISARM", "1234")
```

### 4. State Publishing (Consumer-driven)

```
Consumer code (after validating state transition):
  └─> ha.publishState("alarm", AlarmPanelState::ArmedAway)
       └─> findEntity("alarm")
       └─> getStateTopic() → homeassistant/alarm_control_panel/{nodeId}/alarm/state
       └─> mqttPublish(topic, "armed_away", retained=true)
```

---

## Integration Points

| Integration | Direction | Mechanism |
|-------------|-----------|-----------|
| MQTT component | Indirect (EventBus) | `mqtt/publish`, `mqtt/subscribe`, `mqtt/message` events |
| HA Discovery | Outbound | JSON payload to `{prefix}/alarm_control_panel/{nodeId}/{id}/config` |
| HA Command | Inbound | Payload on `.../set` topic, parsed as "COMMAND" or "COMMAND CODE" |
| HA State | Outbound | String state to `.../state` topic (consumer-published) |
| Core EventBus | Both | Entity lifecycle events (`ha/entity_added`, `ha/discovery_published`) |

---

## Discovery Payload Example

Topic: `homeassistant/alarm_control_panel/esp32-demo/alarm/config`

```json
{
  "name": "Home Alarm",
  "unique_id": "esp32-demo_alarm",
  "state_topic": "homeassistant/alarm_control_panel/esp32-demo/alarm/state",
  "command_topic": "homeassistant/alarm_control_panel/esp32-demo/alarm/set",
  "icon": "mdi:shield-home",
  "device": {
    "identifiers": ["esp32-demo"],
    "name": "ESP32 Demo Device",
    "model": "ESP32",
    "manufacturer": "DomoticsCore",
    "sw_version": "1.6.0"
  },
  "availability_topic": "homeassistant/esp32-demo/availability",
  "payload_available": "online",
  "payload_not_available": "offline",
  "code": "1234",
  "code_arm_required": false,
  "code_disarm_required": true,
  "code_trigger_required": false,
  "command_template": "{{ action }}{% if code %} {{ code }}{% endif %}",
  "payload_arm_home": "ARM_HOME",
  "payload_arm_away": "ARM_AWAY",
  "payload_disarm": "DISARM",
  "payload_trigger": "TRIGGER",
  "supported_features": ["arm_home", "arm_away", "trigger"]
}
```

---

## Testing Analysis

### Test Coverage Summary

| Area | Tests | Status |
|------|-------|--------|
| Discovery payload (all fields) | 3 | Pass |
| Command parsing (basic, code, edge cases) | 4 | Pass |
| State publishing integration | 1 | Pass |
| No auto-publish verification | 1 | Pass |
| Registration + parameter passthrough | 1 | Pass |
| Command routing (end-to-end) | 1 | Pass |
| Polymorphic dispatch (AC 8) | 1 | Pass |
| Heap stability (Constitution XIV) | 1 | Pass |
| **Total** | **13** | **All pass** |

### Verification Steps

```bash
# Run alarm panel tests only
cd DomoticsCore-HomeAssistant
pio test -e native -f test_ha_alarm_panel

# Run all HA tests (including existing component tests)
pio test -e native
```

---

## Risks and Gotchas

1. **Unscoped `AlarmFeature` enum** -- `Trigger`, `ArmHome`, etc. leak into `HomeAssistant` namespace. Tracked as CODE-ROADMAP R25 for future `enum class` conversion.
2. **String temporaries in `handleCommand()`** -- Creates `String` objects for command/code parsing. Low impact (single allocation per command) but inconsistent with the zero-heap aspiration.
3. **`handleCommand()` shadow risk** -- While `override` was added to existing entities (HASwitch, HALight, HAButton), the command routing in `HomeAssistantComponent::handleCommand()` still uses `static_cast` for switch/light/button but virtual dispatch for alarm_control_panel. This inconsistency is tracked as CODE-ROADMAP R24.
4. **Missing `ha/entity_added` emission** -- `addAlarmControlPanel()` does not emit the `ha/entity_added` event (same as addBinarySensor, addSwitch, etc.). Tracked as CODE-ROADMAP M19.

---

## Related Code References

| File | Relevance |
|------|-----------|
| `HASensor.h`, `HABinarySensor.h` | Same entity pattern (read-only, no command handling) |
| `HASwitch.h` | Similar command handling pattern (simpler: ON/OFF only) |
| `HALight.h` | Similar command handling pattern (JSON parsing) |
| `HAButton.h` | Trigger-only pattern reference |
| `examples/BasicHA/src/main.cpp` | Example usage (may need alarm panel example added) |

---

## Implementation Guidance

### Adding Alarm Panel to Consumer Code

```cpp
#include <DomoticsCore/HomeAssistant.h>
using namespace DomoticsCore::Components::HomeAssistant;

// Register with multiple arm modes and PIN code
haPtr->addAlarmControlPanel("alarm", "Home Alarm",
    [](const String& command, const String& code) {
        if (command == AlarmPanelCommand::DISARM) {
            // Validate code, transition to disarmed
        } else if (command == AlarmPanelCommand::ARM_AWAY) {
            // Start arming sequence
        }
    },
    "mdi:shield-home",
    AlarmFeature::ArmAway | AlarmFeature::ArmHome | AlarmFeature::Trigger,
    "1234",           // PIN code
    false,            // codeArmRequired
    true,             // codeDisarmRequired
    false             // codeTriggerRequired
);

// Consumer manages state transitions:
haPtr->publishState("alarm", AlarmPanelState::Arming);
// ... after arming delay ...
haPtr->publishState("alarm", AlarmPanelState::ArmedAway);
```

### Valid State Transitions (typical)

```
Disarmed → Arming → ArmedHome/ArmedAway/ArmedNight/ArmedVacation/ArmedCustomBypass
Armed* → Pending → Triggered
Armed* → Disarming → Disarmed
Triggered → Disarming → Disarmed
```

*Note: These transitions are NOT enforced by the component. The consumer is fully responsible.*
