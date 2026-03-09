# EventBus Patterns

This document describes common patterns for using the DomoticsCore EventBus.

## Overview

The EventBus provides publish/subscribe messaging between components without direct coupling.

## Dispatch Model

The EventBus is **queue-based** with polling dispatch, NOT immediate:

- Events are queued on `publish()` (not dispatched immediately)
- `poll(maxPerPoll)` processes up to `maxPerPoll` events per call (default: 8)
- **Backpressure**: Queue capped at 32 events; oldest dropped on overflow
- `Core::loop()` calls `eventBus.poll()` automatically each cycle

## Basic Usage

### Publishing Events

```cpp
// Simple event (queued, dispatched on next poll())
eventBus().publish("sensor/temperature", tempData);

// Sticky event (persists for late subscribers)
eventBus().publishSticky("system/status", readyFlag);
```

### Subscribing to Events

The raw EventBus API uses `std::function<void(const void*)>` callbacks:

```cpp
// Raw API — exact topic match
eventBus().subscribe("sensor/temperature", [](const void* payload) {
    const float* temp = static_cast<const float*>(payload);
    if (temp) Serial.printf("Temperature: %.1f\n", *temp);
});

// Wildcard subscription
eventBus().subscribe("sensor/*", [](const void* payload) {
    // Handle any sensor event
});
```

The typed helper API (available on IComponent) uses `std::function<void(const T&)>`:

```cpp
// Typed API — from within a component
on<float>("sensor/temperature", [](const float& temp) {
    Serial.printf("Temperature: %.1f\n", temp);
}, true);  // replayLast = true for sticky events
```

## Common Patterns

### 1. Component Ready Notification

```cpp
ComponentStatus begin() override {
    // Initialization logic...
    
    if (__dc_eventBus) {
        __dc_eventBus->publish(EVENT_COMPONENT_READY, metadata.name, true);
    }
    return ComponentStatus::Success;
}
```

### 2. Waiting for Dependencies

```cpp
ComponentStatus begin() override {
    if (__dc_eventBus) {
        __dc_eventBus->subscribe(EVENT_NETWORK_READY, [this](const void*) {
            onNetworkReady();
        });
    }
    return ComponentStatus::Success;
}
```

### 3. State Change Broadcasting

```cpp
void setMode(const String& mode) {
    currentMode = mode;
    if (__dc_eventBus) {
        __dc_eventBus->publish("mycomponent/mode", mode);
    }
}
```

### 4. Request/Response Pattern

```cpp
// Requester — using typed helper (from within a component)
on<String>("sensor/response", [](const String& value) {
    handleResponse(value);
});
emit<String>("sensor/request", String("temperature"));

// Responder — using typed helper (from within a component)
on<String>("sensor/request", [this](const String& param) {
    String value = getSensorValue(param);
    emit<String>("sensor/response", value);
});
```

## Lifecycle Events

| Event Constant | Topic | Payload | Sticky |
|---------------|-------|---------|--------|
| `EVENT_COMPONENT_READY` | `system/component/ready` | Component name | Yes |
| `EVENT_SYSTEM_READY` | `system/ready` | - | Yes |
| `EVENT_STORAGE_READY` | `system/storage/ready` | - | Yes |
| `EVENT_NETWORK_READY` | `system/network/ready` | IP address | Yes |
| `EVENT_SHUTDOWN_START` | `system/shutdown` | - | No |

## Best Practices

1. **Use constants** for event topics to avoid typos
2. **Sticky events** for state that late subscribers need
3. **Keep handlers fast** - offload heavy work to loop()
4. **Unsubscribe** when component shuts down
5. **Namespace topics** by component: `led/effect`, `wifi/status`

## Event Flow Diagram

```
┌─────────────┐     publish()     ┌───────────┐     poll()        ┌─────────────┐
│  Publisher  │ ─────────────────▶│   Queue   │ ─────────────────▶│  Subscriber │
└─────────────┘                   │ (max 32)  │   (up to 8/call)  └─────────────┘
                                  └───────────┘
                                       │
                                       │ (if sticky)
                                       ▼
                                  ┌─────────┐
                                  │  Cache  │
                                  └─────────┘
                                       │
                                       │ (late subscriber with replayLast)
                                       ▼
                                  ┌─────────────┐
                                  │ New Sub gets│
                                  │ cached value│
                                  └─────────────┘
```
