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
| **Command Handling** | Receives ON/OFF/brightness commands from HA and routes them to user callbacks |
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

Core core;
HomeAssistantComponent* haPtr;

void setup() {
    // 1. Configure and register MQTT
    MQTTConfig mqttCfg;
    mqttCfg.broker = "192.168.1.100";
    auto mqtt = std::make_unique<MQTTComponent>(mqttCfg);
    core.addComponent(std::move(mqtt));

    // 2. Configure and register Home Assistant
    HAConfig haCfg;
    haCfg.nodeId = "esp32-sensor";
    haCfg.deviceName = "Living Room Sensor";
    auto ha = std::make_unique<HomeAssistantComponent>(haCfg);
    haPtr = ha.get();

    // 3. Add entities
    haPtr->addSensor("temperature", "Temperature", "C", "temperature", "mdi:thermometer");
    haPtr->addBinarySensor("motion", "Motion", "motion");
    haPtr->addSwitch("relay", "Relay", [](bool state) {
        digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    });
    haPtr->addButton("restart", "Restart", []() { ESP.restart(); }, "mdi:restart");

    // 4. Add alarm panel with PIN code and multiple arm modes
    haPtr->addAlarmControlPanel("alarm", "Home Alarm",
        [](const String& command, const String& code) {
            // Handle arm/disarm commands with optional PIN code
        },
        "mdi:shield-home",
        AlarmFeature::ArmAway | AlarmFeature::ArmHome | AlarmFeature::Trigger,
        "1234", false, true, false);  // code, codeArmRequired, codeDisarmRequired, codeTriggerRequired

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

## Examples

- **BasicHA** -- Minimal WiFi + MQTT + HA integration with sensors, switch, and button.
- **HAWithWebUI** -- Full-featured example with web dashboard, real-time monitoring, and WebSocket updates.

## Further Reading

- [Technical Reference](./technical-reference.md) -- Full API documentation, MQTT topic details, and payload formats.
- [Project Context](./project-context.md) -- AI context file with class inventory, dependencies, and conventions.
- [Alarm Control Panel Deep-Dive](../../deep-dive-ha-alarm-control-panel.md) -- Exhaustive analysis of the alarm panel implementation.
- [Component Source](../../../DomoticsCore-HomeAssistant/) -- Header files and examples.
- [DomoticsCore Constitution](../../../.specify/memory/constitution.md) -- Governing development principles.
