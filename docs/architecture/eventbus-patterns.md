<!-- workline
sources: [DomoticsCore-Core/include/DomoticsCore/EventBus.h, DomoticsCore-Core/include/DomoticsCore/IComponent.h, DomoticsCore-Core/include/DomoticsCore/ComponentRegistry.h, DomoticsCore-Core/include/DomoticsCore/Core.h, DomoticsCore-Core/include/DomoticsCore/Events.h, DomoticsCore-Wifi/include/DomoticsCore/WifiEvents.h, DomoticsCore-Wifi/include/DomoticsCore/Wifi.h, DomoticsCore-MQTT/include/DomoticsCore/MQTTEvents.h, DomoticsCore-NTP/include/DomoticsCore/NTPEvents.h, DomoticsCore-OTA/include/DomoticsCore/OTAEvents.h, DomoticsCore-HomeAssistant/include/DomoticsCore/HAEvents.h, DomoticsCore-Storage/include/DomoticsCore/StorageEvents.h]
checked: cff4019
verified: agent:documentalist
judged-in-parts: cff4019
-->
# EventBus Patterns

This document describes common patterns for using the DomoticsCore EventBus.

## Overview

The EventBus provides publish/subscribe messaging between components without direct coupling.

## Dispatch Model

The EventBus is **queue-based** with polling dispatch, NOT immediate:

- Events are queued on `publish()` (not dispatched immediately)
- `poll(maxPerPoll)` processes up to `maxPerPoll` events per call (default: 8)
- **Backpressure**: Queue bounded by the bytes it holds (`QueueCost::kBudgetBytes`); oldest dropped until the new one fits
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

The `EVENT_COMPONENT_READY` event is published **automatically** by the `ComponentRegistry` after each successful `begin()` call. You do NOT need to publish it manually:

```cpp
ComponentStatus begin() override {
    // Just return Success -- ComponentRegistry publishes EVENT_COMPONENT_READY for you
    return ComponentStatus::Success;
}

// To react to another component becoming ready:
void afterAllComponentsReady() override {
    on<const char*>("component/ready", [](const char* name) {
        DLOG_I("APP", "Component ready: %s", name);
    });
}
```

### 2. Waiting for Dependencies

```cpp
ComponentStatus begin() override {
    // Subscribe to WiFi connected event (defined in WifiEvents.h)
    on<bool>("wifi/sta/connected", [this](const bool& connected) {
        if (connected) onNetworkReady();  // Wifi publishes false on timeout
    });
    return ComponentStatus::Success;
}
```

Wifi publishes `wifi/sta/connected` without `sticky`, so `replayLast` has
nothing to replay: a component that subscribes after the connection should
also ask the Wifi component (`isSTAConnected()`).

### 3. State Change Broadcasting

```cpp
void setMode(const String& mode) {
    currentMode = mode;
    if (__dc_eventBus) {
        __dc_eventBus->publish("mycomponent/mode", mode.c_str(), mode.length() + 1);
    }
}
```

### 4. Request/Response Pattern

A string travels as its bytes, NUL included, through the sized
`emit(topic, data, size, sticky)`; all four arguments are needed, since
`emit(topic, x, n)` resolves to the typed `emit` with `n` as `sticky`. The
handler receives a pointer to the queued copy, so subscribe with the raw API:
`on<const char*>` would read those bytes as a pointer.

```cpp
// Requester (from within a component)
eventBus().subscribe("sensor/response", [this](const void* p) {
    const char* value = static_cast<const char*>(p);
    if (value) handleResponse(value);
}, this);
static const char kParam[] = "temperature";
emit("sensor/request", kParam, sizeof(kParam), false);

// Responder (from within a component)
eventBus().subscribe("sensor/request", [this](const void* p) {
    const char* param = static_cast<const char*>(p);
    if (!param) return;
    String value = getSensorValue(param);
    emit("sensor/response", value.c_str(), value.length() + 1, false);
}, this);
```

## Lifecycle Events

Core lifecycle events are defined in `DomoticsCore-Core/include/DomoticsCore/Events.h`:

| Event Constant | Topic | Payload | Source |
|---------------|-------|---------|--------|
| `EVENT_COMPONENT_READY` | `component/ready` | `const char*` (component name) | Core (ComponentRegistry) |
| `EVENT_COMPONENT_ERROR` | `component/error` | - | Defined only: nothing publishes it |
| `EVENT_SYSTEM_READY` | `system/ready` | - | Core (ComponentRegistry) |
| `EVENT_SYSTEM_REBOOT` | `system/reboot` | - | Defined only: nothing publishes it |
| `EVENT_SHUTDOWN_START` | `shutdown/start` | - | Core (ComponentRegistry) |

Component-specific events are defined in their respective `*Events.h` files:

| Event | Topic | Defined In |
|-------|-------|-----------|
| WiFi connected | `wifi/sta/connected` | `WifiEvents.h` |
| WiFi AP enabled | `wifi/ap/enabled` | `WifiEvents.h` |
| Network ready | `network/ready` | `WifiEvents.h` |
| MQTT connected | `mqtt/connected` | `MQTTEvents.h` |
| MQTT message | `mqtt/message` | `MQTTEvents.h` |
| NTP synced | `ntp/synced` | `NTPEvents.h` |
| OTA events | `ota/start`, `ota/progress`, etc. | `OTAEvents.h` |
| HA discovery | `ha/discovery_published` | `HAEvents.h` |
| Storage ready | `storage/ready` | `StorageEvents.h` |

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
└─────────────┘                   │ (bytes)   │   (up to 8/call)  └─────────────┘
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
