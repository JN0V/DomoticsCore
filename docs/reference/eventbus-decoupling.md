<!-- workline
sources: [DomoticsCore-HomeAssistant/include/DomoticsCore/HomeAssistant.h, DomoticsCore-MQTT/include/DomoticsCore/MQTT.h, DomoticsCore-MQTT/include/DomoticsCore/MQTTEvents.h]
checked: 76c0609
verified: agent:documentalist
-->
# EventBus Decoupling

Part of the [EventBus Architecture](eventbus-architecture.md) reference.

## Before (Direct Callbacks)

```cpp
// ❌ Tight coupling - components directly reference each other
class HomeAssistant {
    void begin() {
        mqtt->onConnect([this]() { publishDiscovery(); });
        mqtt->onMessage([this](String topic, String payload) {
            handleCommand(topic, payload);
        });
    }
};
```

**Problems:**
- HomeAssistant depends on MQTT interface
- Can't test HomeAssistant without MQTT
- Hard to add new components that react to MQTT events

## After (EventBus)

```cpp
// ✅ Loose coupling - components communicate via events
class HomeAssistant {
    void begin() {
        // Listen to events
        on<bool>("mqtt/connected", [this](const bool&) {
            publishDiscovery();
        });
        
        on<MQTTMessageEvent>("mqtt/message", [this](const MQTTMessageEvent& ev) {
            handleCommand(String(ev.topic), String(ev.payload));
        });
    }
};
```

**Benefits:**
- HomeAssistant only depends on event types
- Can test with mock events
- Other components can also listen to `mqtt/connected`

