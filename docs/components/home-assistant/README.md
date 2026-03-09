# DomoticsCore-HomeAssistant

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## What is DomoticsCore-HomeAssistant?

DomoticsCore-HomeAssistant is the Home Assistant MQTT Discovery integration component for the DomoticsCore IoT framework. It enables ESP32 and ESP8266 devices to register themselves and their entities with Home Assistant automatically -- no manual YAML configuration required. Devices, sensors, switches, lights, and buttons appear in Home Assistant the moment the firmware connects to the MQTT broker.

The component is **header-only** and communicates with the MQTT component exclusively through the DomoticsCore EventBus, following the framework's decoupled architecture.

## Key Features

| Feature | Description |
|---------|-------------|
| **Auto-Discovery** | Publishes MQTT discovery payloads so entities appear in HA without configuration |
| **Device Registry** | Groups all entities under a single device with name, model, manufacturer, and firmware version |
| **Availability Tracking** | Publishes online/offline status; works with MQTT LWT for crash detection |
| **State Management** | Publish sensor values, switch states, and light brightness via typed helpers |
| **Virtual Dispatch + EventBus Commands** | Entity commands are handled via virtual `handleCommand()` and emitted as `ha/command` EventBus events (v2.0.0) |
| **Zero-Heap Configuration** | `HAConfig` uses fixed-size `char[]` arrays with `HA::setField()` helper -- no heap-allocated Strings |
| **WebUI Integration** | Optional web interface for status, dashboard, settings, and statistics |

## Supported Entity Types

| Entity Type | HA Component | Direction | Use Case |
|-------------|--------------|-----------|----------|
| **Sensor** | `sensor` | Device to HA | Temperature, humidity, voltage, power |
| **BinarySensor** | `binary_sensor` | Device to HA | Motion, door, window, smoke |
| **Switch** | `switch` | Bidirectional | Relay, socket, fan |
| **Light** | `light` | Bidirectional | LED strip, dimmer (with brightness) |
| **Button** | `button` | HA to device | Restart, calibrate, trigger actions |
| **AlarmControlPanel** | `alarm_control_panel` | Bidirectional | Alarm system with arm/disarm modes, PIN code, keypad |

## Dependencies

| Dependency | Version | Required |
|------------|---------|----------|
| DomoticsCore-Core | >= 1.4.0 | Yes |
| DomoticsCore-MQTT | >= 1.4.0 | Yes |
| ArduinoJson | ^7.0.0 | Yes |
| DomoticsCore-WebUI | >= 0.1.0 | Optional |

## Quick Start

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/HomeAssistant.h>

using namespace DomoticsCore::Components;
using namespace DomoticsCore::Components::HomeAssistant;
using namespace DomoticsCore;

Core core;
HomeAssistantComponent* haPtr;

void setup() {
    // 1. Configure and register MQTT
    MQTTConfig mqttCfg;
    mqttCfg.broker = "192.168.1.100";
    auto mqtt = std::make_unique<MQTTComponent>(mqttCfg);
    core.addComponent(std::move(mqtt));

    // 2. Configure Home Assistant (v2.0.0: char[] fields + HA::setField())
    HAConfig haCfg;
    HA::setField(haCfg.nodeId, "esp32-sensor", HA::MAX_NODE_ID);
    HA::setField(haCfg.deviceName, "Living Room Sensor", HA::MAX_DEVICE_NAME);
    auto ha = std::make_unique<HomeAssistantComponent>(haCfg);
    haPtr = ha.get();

    // 3. Add entities (v2.0.0: NO callback parameters)
    haPtr->addSensor("temperature", "Temperature", "C", "temperature", "mdi:thermometer");
    haPtr->addBinarySensor("motion", "Motion", "motion");
    haPtr->addSwitch("relay", "Relay");
    haPtr->addButton("restart", "Restart", "mdi:restart");

    // 4. Add alarm panel with PIN code and multiple arm modes
    haPtr->addAlarmControlPanel("alarm", "Home Alarm",
        "mdi:shield-home",
        AlarmFeature::ArmAway | AlarmFeature::ArmHome | AlarmFeature::Trigger,
        "1234", false, true, false);  // code, codeArmRequired, codeDisarmRequired, codeTriggerRequired

    // 5. Subscribe to ha/command EventBus event to react to HA commands
    haPtr->on<HAEvents::HACommandEvent>(HAEvents::EVENT_COMMAND,
        [](const HAEvents::HACommandEvent& ev) {
            if (strcmp(ev.component, "switch") == 0 && strcmp(ev.entityId, "relay") == 0) {
                bool on = (strcmp(ev.command, "ON") == 0);
                digitalWrite(RELAY_PIN, on ? HIGH : LOW);
            }
            if (strcmp(ev.component, "button") == 0 && strcmp(ev.entityId, "restart") == 0) {
                ESP.restart();
            }
            if (strcmp(ev.component, "alarm_control_panel") == 0) {
                // ev.command = "ARM_AWAY", "DISARM", etc.
                // ev.code = PIN code entered by user (may be empty)
            }
        });

    core.addComponent(std::move(ha));
    core.begin();
}

void loop() {
    core.loop();

    // Publish sensor values periodically
    if (haPtr->isReady()) {
        haPtr->publishState("temperature", 22.5f);
        haPtr->publishState("motion", true);
        // Alarm state is consumer-managed:
        // haPtr->publishState("alarm", AlarmPanelState::ArmedAway);
    }
}
```

When MQTT connects, the component automatically publishes discovery payloads and availability. Entities appear in Home Assistant under the device "Living Room Sensor" without any manual setup.

## MQTT Topic Overview

```
homeassistant/{component}/{nodeId}/{entityId}/config       # Discovery
homeassistant/{component}/{nodeId}/{entityId}/state        # State
homeassistant/{component}/{nodeId}/{entityId}/set          # Commands
homeassistant/{component}/{nodeId}/{entityId}/attributes   # Attributes
homeassistant/{nodeId}/availability                        # Online/offline
```

## Migration from v1.x

Version 2.0.0 introduces **breaking changes**. This section covers how to update existing v1.x code.

### 1. HAConfig: String fields replaced with char[] arrays

**Before (v1.x):**
```cpp
HAConfig cfg;
cfg.nodeId = "my-device";
cfg.deviceName = "My Device";
```

**After (v2.0.0):**
```cpp
HAConfig cfg;
HA::setField(cfg.nodeId, "my-device", HA::MAX_NODE_ID);
HA::setField(cfg.deviceName, "My Device", HA::MAX_DEVICE_NAME);
```

All `HAConfig` fields are now fixed-size `char[]` arrays. Use `HA::setField(dest, src, maxLen)` to safely populate them. Available size constants: `HA::MAX_NODE_ID` (33), `HA::MAX_DEVICE_NAME` (65), `HA::MAX_MANUFACTURER` (33), `HA::MAX_MODEL` (33), `HA::MAX_SW_VERSION` (17), `HA::MAX_DISCOVERY_PREFIX` (33), `HA::MAX_AVAIL_TOPIC` (129), `HA::MAX_CONFIG_URL` (129), `HA::MAX_SUGGESTED_AREA` (33).

### 2. Callbacks removed from add*() methods

**Before (v1.x):**
```cpp
haPtr->addSwitch("relay", "Relay", [](bool state) { /* ... */ });
haPtr->addLight("led", "LED", [](bool on, uint8_t bri) { /* ... */ });
haPtr->addButton("restart", "Restart", []() { ESP.restart(); });
haPtr->addAlarmControlPanel("alarm", "Alarm",
    [](const String& cmd, const String& code) { /* ... */ }, ...);
```

**After (v2.0.0):**
```cpp
// No callbacks in add*() calls
haPtr->addSwitch("relay", "Relay");
haPtr->addLight("led", "LED");
haPtr->addButton("restart", "Restart");
haPtr->addAlarmControlPanel("alarm", "Alarm");

// Subscribe to ha/command EventBus event instead
haPtr->on<HAEvents::HACommandEvent>(HAEvents::EVENT_COMMAND,
    [](const HAEvents::HACommandEvent& ev) {
        // ev.entityId, ev.component, ev.command, ev.code
        if (strcmp(ev.component, "switch") == 0) {
            bool on = (strcmp(ev.command, "ON") == 0);
            // handle switch command
        }
        if (strcmp(ev.component, "button") == 0) {
            // handle button press
        }
        if (strcmp(ev.component, "alarm_control_panel") == 0) {
            // ev.command = "ARM_AWAY", "DISARM", etc.
            // ev.code = PIN entered by user
        }
    });
```

### 3. handleCommand() returns bool

In v1.x, `handleCommand()` returned `void`. In v2.0.0, it returns `bool`:
- `true` -- command was valid, `ha/command` event is emitted
- `false` -- command was invalid (e.g., bad JSON for light, wrong payload for button), event is suppressed

Each entity type now stores its state internally (e.g., `HASwitch::state`, `HALight::state`, `HALight::brightness`).

### 4. setDeviceInfo() takes const char*

**Before (v1.x):**
```cpp
ha->setDeviceInfo(String("name"), String("model"), String("mfg"), String("1.0.0"));
```

**After (v2.0.0):**
```cpp
ha->setDeviceInfo("name", "model", "mfg", "1.0.0");
```

### 5. Topic methods use zero-heap char* buffer API

**Before (v1.x):**
```cpp
String topic = entity->getStateTopic(nodeId, prefix);
```

**After (v2.0.0):**
```cpp
char topic[HA_TOPIC_BUF_SIZE];
entity->getStateTopic(topic, sizeof(topic), nodeId, prefix);
```

### 6. publishState(const char*) overload

A new `publishState(const String& id, const char* state)` overload exists to prevent implicit `bool` conversion when passing string literals:

```cpp
haPtr->publishState("relay", "ON");  // Calls const char* overload, not bool
```

## Examples

- **BasicHA** -- Minimal WiFi + MQTT + HA integration with sensors, switch, and button.
- **HAWithWebUI** -- Full-featured example with web dashboard, real-time monitoring, and WebSocket updates.

## Further Reading

- [Technical Reference](./technical-reference.md) -- Full API documentation, MQTT topic details, and payload formats.
- [Project Context](./project-context.md) -- AI context file with class inventory, dependencies, and conventions.
- [Alarm Control Panel Deep-Dive](../../deep-dive-ha-alarm-control-panel.md) -- Exhaustive analysis of the alarm panel implementation.
- [Component Source](../../../DomoticsCore-HomeAssistant/) -- Header files and examples.
- [DomoticsCore Constitution](../../../.specify/memory/constitution.md) -- Governing development principles.
