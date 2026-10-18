<!-- workline
sources: [DomoticsCore-MQTT/include/DomoticsCore/MQTT.h, DomoticsCore-MQTT/include/DomoticsCore/MQTT_impl.h, DomoticsCore-Core/include/DomoticsCore/EventBus.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h]
checked: 76c0609
verified: agent:documentalist
-->
# EventBus Implementation Details and Best Practices

Part of the [EventBus Architecture](eventbus-architecture.md) reference.

## Implementation Details

### Component Lifecycle and EventBus Registration

#### ✅ Correct Pattern

```cpp
ComponentStatus MQTTComponent::begin() {
    DLOG_I(LOG_MQTT, "Initializing");
    
    // ✅ STEP 1: Register EventBus listeners FIRST
    // This ensures listeners are ready even without runtime config
    on<MQTTPublishEvent>("mqtt/publish", [this](const MQTTPublishEvent& ev) {
        publish(String(ev.topic), String(ev.payload), ev.qos, ev.retain);
    });
    
    on<MQTTSubscribeEvent>("mqtt/subscribe", [this](const MQTTSubscribeEvent& ev) {
        subscribe(String(ev.topic), ev.qos);
    });
    
    DLOG_I(LOG_MQTT, "EventBus listeners registered");
    
    // ✅ STEP 2: Load runtime configuration
    loadConfiguration();
    
    // ✅ STEP 3: Check if component should be active
    if (config.broker.isEmpty()) {
        config.enabled = false;
        DLOG_W(LOG_MQTT, "No broker configured - component disabled");
        return ComponentStatus::Success;  // Inactive but listeners ready
    }
    
    // ✅ STEP 4: Initialize active resources
    mqttClient.setServer(config.broker.c_str(), config.port);
    // ...
    
    return ComponentStatus::Success;
}
```

#### ❌ Anti-Pattern (Bug)

```cpp
ComponentStatus MQTTComponent::begin() {
    loadConfiguration();
    
    if (config.broker.isEmpty()) {
        return ComponentStatus::Success;  // ❌ Early return!
    }
    
    // ❌ These listeners are NEVER registered without config
    on<MQTTPublishEvent>("mqtt/publish", ...);
    on<MQTTSubscribeEvent>("mqtt/subscribe", ...);
}
```

**Why this is a bug:**
1. Component starts without config
2. EventBus listeners never registered
3. User configures component via WebUI
4. Other components emit events
5. **No listener exists → events lost**
6. User must reboot for listeners to be registered

### PubSubClient Callback Bridge

The MQTT component uses `mqttClient.setCallback(mqttCallback)` to bridge between PubSubClient (C library) and C++ EventBus:

```cpp
// Static C-style callback required by PubSubClient
inline void MQTTComponent::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (instance) {
        instance->handleIncomingMessage(topic, payload, length);
    }
}

// C++ method that emits EventBus event
void MQTTComponent::handleIncomingMessage(char* topic, byte* payload, unsigned int length) {
    // Convert to String
    String topicStr = String(topic);
    String payloadStr = String((char*)payload).substring(0, length);
    
    // Emit EventBus event
    MQTTMessageEvent ev{};
    strncpy(ev.topic, topicStr.c_str(), sizeof(ev.topic) - 1);
    ev.topic[sizeof(ev.topic) - 1] = '\0';
    strncpy(ev.payload, payloadStr.c_str(), sizeof(ev.payload) - 1);
    ev.payload[sizeof(ev.payload) - 1] = '\0';
    emit("mqtt/message", ev);
}
```

**This callback is necessary** and is NOT the same as the application-level callbacks that were removed during EventBus migration.

---

## Best Practices

### 1. Always Register Listeners First

```cpp
// ✅ Good
begin() {
    registerEventBusListeners();
    loadConfiguration();
    if (!hasConfig()) return;
    initializeResources();
}

// ❌ Bad
begin() {
    loadConfiguration();
    if (!hasConfig()) return;  // Listeners never registered!
    registerEventBusListeners();
}
```

### 2. Use Typed Events

```cpp
// ✅ Good - Type safety
struct MyEvent {
    char data[128];
    int value;
};
on<MyEvent>("my/event", [](const MyEvent& ev) { ... });
emit("my/event", MyEvent{...});

// ❌ Bad - No type safety
on<void*>("my/event", [](void* data) { ... });
```

### 3. Emit Events, Don't Call Directly

```cpp
// ✅ Good - Loose coupling
emit("mqtt/publish", MQTTPublishEvent{...});

// ❌ Bad - Tight coupling
mqtt->publish(topic, payload);
```

### 4. Handle Event Absence Gracefully

```cpp
// ✅ Good - Defensive
if (!mqtt || !mqtt->isConnected()) {
    DLOG_W(LOG, "MQTT not available, skipping publish");
    return;
}
emit("mqtt/publish", event);

// ❌ Bad - Assumes MQTT exists
emit("mqtt/publish", event);  // Silently fails if no listener
```

