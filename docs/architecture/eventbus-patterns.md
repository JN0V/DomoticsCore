# EventBus Patterns

This document describes common patterns for using the DomoticsCore EventBus.

## Overview

The EventBus provides publish/subscribe messaging between components without direct coupling.

## Basic Usage

### Publishing Events

```cpp
// Simple event
eventBus->publish("sensor/temperature", "25.5");

// Sticky event (persists for late subscribers)
eventBus->publish("system/status", "ready", true);
```

### Subscribing to Events

```cpp
// Exact topic match
eventBus->subscribe("sensor/temperature", [](const String& topic, const String& payload) {
    float temp = payload.toFloat();
    Serial.printf("Temperature: %.1f\n", temp);
});

// Wildcard subscription
eventBus->subscribe("sensor/*", [](const String& topic, const String& payload) {
    Serial.printf("Sensor event: %s = %s\n", topic.c_str(), payload.c_str());
});
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
        __dc_eventBus->subscribe(EVENT_NETWORK_READY, [this](const String&, const String&) {
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
// Requester
eventBus->subscribe("sensor/response", [](const String&, const String& value) {
    handleResponse(value);
});
eventBus->publish("sensor/request", "temperature");

// Responder
eventBus->subscribe("sensor/request", [this](const String&, const String& param) {
    String value = getSensorValue(param);
    eventBus->publish("sensor/response", value);
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
┌─────────────┐     publish()     ┌───────────┐     callback()    ┌─────────────┐
│  Publisher  │ ─────────────────▶│  EventBus │ ─────────────────▶│  Subscriber │
└─────────────┘                   └───────────┘                   └─────────────┘
                                       │
                                       │ (if sticky)
                                       ▼
                                  ┌─────────┐
                                  │  Cache  │
                                  └─────────┘
                                       │
                                       │ (late subscriber)
                                       ▼
                                  ┌─────────────┐
                                  │ New Sub gets│
                                  │ cached value│
                                  └─────────────┘
```
