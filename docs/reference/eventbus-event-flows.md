<!-- workline
sources: [DomoticsCore-System/include/DomoticsCore/System.h, DomoticsCore-Wifi/include/DomoticsCore/Wifi.h, DomoticsCore-Wifi/include/DomoticsCore/WifiEvents.h, DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h, DomoticsCore-MQTT/include/DomoticsCore/MQTTEvents.h, DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h, DomoticsCore-HomeAssistant/include/DomoticsCore/HASwitch.h]
checked: 6687dc4
verified: agent:claude-code
-->
# EventBus Event Flow Diagram

Part of the [EventBus Architecture](eventbus-architecture.md) reference.

## System Boot Sequence

```mermaid
sequenceDiagram
    participant System
    participant WiFi
    participant MQTT
    participant HA as HomeAssistant
    participant Broker as MQTT Broker
    
    System->>WiFi: begin()
    System->>MQTT: begin()
    Note over MQTT: Register EventBus listeners<br/>(ALWAYS, even without config)
    System->>HA: begin()
    Note over HA: Register EventBus listeners
    
    Note over System: User configures WiFi via WebUI
    
    WiFi->>WiFi: connect()
    WiFi-->>System: emit("wifi/sta/connected")
    System->>MQTT: connect()
    
    MQTT->>Broker: TCP connect
    Broker-->>MQTT: Connected
    MQTT-->>System: emit("mqtt/connected", true)
    MQTT-->>HA: emit("mqtt/connected", true)
    
    HA->>HA: setAvailable(true)
    HA->>HA: subscribeToCommands()
    HA-->>MQTT: emit("mqtt/subscribe", event)
    MQTT->>Broker: SUBSCRIBE homeassistant/+/.../set
    
    HA->>HA: publishDiscovery()
    loop For each entity
        HA-->>MQTT: emit("mqtt/publish", discovery_config)
        MQTT->>Broker: PUBLISH config topic
    end
```

## Command Handling Flow

```mermaid
sequenceDiagram
    participant User as HA UI
    participant Broker as MQTT Broker
    participant MQTT
    participant HA as HomeAssistant
    participant App
    
    User->>Broker: switch.relay = ON
    Broker->>MQTT: homeassistant/switch/.../relay/set = "ON"
    MQTT->>MQTT: handleIncomingMessage()
    MQTT-->>HA: emit("mqtt/message", {topic, payload})
    
    HA->>HA: handleCommand()
    HA->>HA: Extract entityId = "relay"
    HA->>App: sw->handleCommand(payload)<br/>Execute user callback
    App->>App: setRelay(true)
    
    Note over HA: Auto-publish state
    HA->>HA: publishState("relay", "ON")
    HA-->>MQTT: emit("mqtt/publish", {state_topic, "ON"})
    MQTT->>Broker: PUBLISH state topic
    Broker->>User: State updated in UI
```
