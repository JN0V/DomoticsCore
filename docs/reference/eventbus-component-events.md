<!-- workline
sources: [DomoticsCore-MQTT/include/DomoticsCore/MQTT.h, DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h, DomoticsCore-MQTT/include/DomoticsCore/MQTTEvents.h, DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h, DomoticsCore-Wifi/include/DomoticsCore/Wifi.h, DomoticsCore-Wifi/include/DomoticsCore/WifiEvents.h, DomoticsCore-System/include/DomoticsCore/System.h]
checked: 76c0609
verified: agent:documentalist
-->
# EventBus Component Events

Part of the [EventBus Architecture](eventbus-architecture.md) reference.

## MQTT Component

### Emitted Events

| Event Name | Type | Payload | When |
|------------|------|---------|------|
| `mqtt/connected` | `bool` | `true` | After successful connection to broker |
| `mqtt/disconnected` | `bool` | `true` | After a connection is lost, deliberately or not |
| `mqtt/message` | `MQTTMessageEvent` | `{topic, payload}` | When message received from broker |

### Listened Events

| Event Name | Type | Handler |
|------------|------|---------|
| `mqtt/publish` | `MQTTPublishEvent` | Publishes message to broker |
| `mqtt/subscribe` | `MQTTSubscribeEvent` | Subscribes to topic (with wildcards) |

### Event Structures

```cpp
struct MQTTPublishEvent {
    char topic[160];      // MQTT topic
    char payload[2048];   // Message payload
    uint8_t qos;          // QoS level (0, 1, 2)
    bool retain;          // Retain flag
};

struct MQTTSubscribeEvent {
    char topic[160];      // Topic filter (supports +, # wildcards)
    uint8_t qos;          // QoS level
};

struct MQTTMessageEvent {
    char topic[160];      // Message topic
    char payload[2048];   // Message payload
};
```

## HomeAssistant Component

### Emitted Events

| Event Name | Type | Payload | When |
|------------|------|---------|------|
| `mqtt/publish` | `MQTTPublishEvent` | Discovery/state data | Publishing to MQTT |
| `mqtt/subscribe` | `MQTTSubscribeEvent` | Command topics | Subscribing to commands |

### Listened Events

| Event Name | Type | Handler |
|------------|------|---------|
| `mqtt/connected` | `bool` | Publishes availability and discovery |
| `mqtt/disconnected` | `bool` | Logs disconnection |
| `mqtt/message` | `MQTTMessageEvent` | Routes commands to entities |

## WiFi Component

### Emitted Events

| Event Name | Type | Payload | When |
|------------|------|---------|------|
| `wifi/sta/connected` | `bool` | `true` | After STA connection established |
| `wifi/ap/enabled` | `bool` | `true` | After Access Point enabled |

## System Orchestration

The `System.h` component orchestrates events between components:

```cpp
// WiFi → MQTT orchestration
on<bool>("wifi/sta/connected", [this](const bool&) {
    if (mqtt && mqtt->isEnabled()) {
        mqtt->connect();
    }
});

// MQTT → HomeAssistant orchestration
// NOTE: NOT needed in System.h because HomeAssistant component 
// listens to "mqtt/connected" internally!
//
// on<bool>("mqtt/connected", ...); // ❌ Don't do this in System.h
```
