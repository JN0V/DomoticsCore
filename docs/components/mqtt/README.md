# DomoticsCore-MQTT

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

## What is DomoticsCore-MQTT?

DomoticsCore-MQTT is a header-only MQTT client component for the DomoticsCore IoT framework. It provides reliable, production-grade MQTT connectivity for ESP32 and ESP8266 devices with automatic reconnection, offline message buffering, and seamless integration with the DomoticsCore ecosystem.

The component wraps the PubSubClient library behind a Hardware Abstraction Layer (HAL), enabling full native testing without hardware and multi-platform support from a single codebase.

## Key Features

- **Auto-reconnection** with exponential backoff (1s to 30s, configurable)
- **QoS 0, 1, 2** support for publish and subscribe operations
- **Offline message queuing** -- messages published while disconnected are buffered and sent on reconnect (up to 100 messages by default)
- **Wildcard subscriptions** using single-level (`+`) and multi-level (`#`) MQTT wildcards
- **Last Will and Testament (LWT)** -- broker publishes a configurable offline message when the device disconnects unexpectedly
- **TLS/SSL** optional encrypted connections (port 8883)
- **EventBus communication** -- fully decoupled inter-component messaging via `mqtt/connected`, `mqtt/disconnected`, `mqtt/message`, `mqtt/publish`, and `mqtt/subscribe` events
- **JSON helpers** -- `publishJSON()` serializes ArduinoJson documents directly
- **WebUI provider** -- real-time status badge, settings card, and statistics dashboard
- **Platform HAL** -- ESP32, ESP8266, and native stub implementations

## Quick Start

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// 1. Configure
MQTTConfig cfg;
cfg.broker = "mqtt.example.com";
cfg.port = 1883;
cfg.username = "user";
cfg.password = "pass";

// 2. Register component
auto mqtt = std::make_unique<MQTTComponent>(cfg);
auto* mqttPtr = mqtt.get();
core.addComponent(std::move(mqtt));

// 3. Subscribe (can be called before connection -- queued internally)
mqttPtr->subscribe("home/sensors/#", 1);

// 4. React to incoming messages via EventBus
mqttPtr->on<MQTTMessageEvent>("mqtt/message", [](const MQTTMessageEvent& ev) {
    DLOG_I("APP", "Received on %s: %s", ev.topic, ev.payload);
});

// 5. Start the system
core.begin();

// 6. In your main loop
void loop() {
    core.loop();
}
```

### Publishing from Another Component (via EventBus)

```cpp
MQTTPublishEvent ev{};
strncpy(ev.topic, "home/status", sizeof(ev.topic) - 1);
strncpy(ev.payload, "online", sizeof(ev.payload) - 1);
ev.qos = 1;
ev.retain = true;
emit("mqtt/publish", ev);
```

## Platform Support

| Platform | Buffer Size | Status |
|----------|-------------|--------|
| ESP32    | 2048 bytes  | Primary target |
| ESP8266  | 768 bytes   | Secondary target |
| Native   | 1024 bytes  | Test stub |

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| DomoticsCore-Core | >= 1.0.0 | IComponent, EventBus, Logger, Timer |
| DomoticsCore-Wifi | >= 1.0.0 | WiFi connectivity check via HAL |
| PubSubClient | ^2.8 | MQTT protocol implementation |
| ArduinoJson | ^7.0 | JSON serialization helpers |

Optional: **DomoticsCore-WebUI** for the web configuration interface.

## Detailed Documentation

- **[Technical Reference](./technical-reference.md)** -- full API, state machine, configuration details, event system, and WebUI provider
- **[Project Context](./project-context.md)** -- AI-agent context with file inventory, dependency graph, conventions, and pitfalls

## Version

Current version: **1.4.1** (as declared in `library.json` and `metadata.version`).

## License

MIT
