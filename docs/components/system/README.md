# DomoticsCore-System

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

**Last verified against source:** 2026-03-10

## What is DomoticsCore-System?

DomoticsCore-System is the meta-orchestrator of the DomoticsCore IoT framework. It is the "batteries included" component that assembles all other DomoticsCore components into a cohesive, ready-to-use system. Instead of manually creating, configuring, and wiring dozens of components, you provide a single `SystemConfig` struct and call `begin()` -- System handles the rest.

The component manages automatic WiFi connection (with AP fallback), LED status visualization, remote console debugging, state lifecycle tracking, event orchestration between components, configuration persistence, and WebUI provider registration.

## Key Features

- **Automatic WiFi** -- STA connection with automatic AP fallback and auto-generated SSID
- **LED status patterns** -- visual feedback mapped to each system state (no LED code required)
- **State management** -- eight-state lifecycle enum (`SystemState`) with observer callbacks
- **Component orchestration** -- WiFi-to-MQTT, NTP sync, and Home Assistant discovery wiring
- **Configuration persistence** -- all settings saved/loaded via the Storage component (ESP32 NVS)
- **WebUI provider setup** -- automatic registration of all WebUI providers with persistence callbacks
- **Preset configurations** -- `minimal()`, `standard()`, and `fullStack()` factory methods
- **Custom console commands** -- `registerCommand()` delegates to RemoteConsole
- **Heap-guarded initialization** -- post-init steps are skipped when free heap falls below 3 KB

## SystemState Lifecycle

| State | Description |
|-------|-------------|
| `BOOTING` | Initial boot, system starting up |
| `WIFI_CONNECTING` | Connecting to WiFi network |
| `WIFI_CONNECTED` | WiFi STA established |
| `SERVICES_STARTING` | Initializing optional services |
| `READY` | All services operational, normal operation |
| `ERROR` | A critical error occurred |
| `OTA_UPDATE` | Firmware update in progress |
| `SHUTDOWN` | Graceful shutdown |

## Quick Start

### Minimal (WiFi + LED + Console)

```cpp
#include <DomoticsCore/System.h>
using namespace DomoticsCore;

SystemConfig config = SystemConfig::minimal();
config.deviceName = "MinimalDevice";
config.wifiSSID = "MyNetwork";
config.wifiPassword = "password";

System system(config);

void setup() {
    Serial.begin(115200);
    system.begin();  // WiFi, LED, Console -- all automatic
}

void loop() {
    system.loop();
}
```

### Standard (+ WebUI, NTP, Storage)

```cpp
SystemConfig config = SystemConfig::standard();
config.deviceName = "StandardDevice";
config.wifiSSID = "MyNetwork";
config.wifiPassword = "password";

System system(config);
```

### Full Stack (+ MQTT, Home Assistant, OTA, SystemInfo)

```cpp
SystemConfig config = SystemConfig::fullStack();
config.deviceName = "FullStackDevice";
config.wifiSSID = "MyNetwork";
config.wifiPassword = "password";
config.mqttBroker = "192.168.1.100";

System system(config);
```

## LED Status Patterns

The LED automatically reflects the current `SystemState`:

| State | LED Effect | Speed | Visual |
|-------|-----------|-------|--------|
| BOOTING | Fast Blink | 200 ms | Rapid on/off |
| WIFI_CONNECTING | Slow Blink | 1000 ms | Leisurely on/off |
| WIFI_CONNECTED | Pulse | 2000 ms | Heartbeat double-pulse |
| SERVICES_STARTING | Fade | 1500 ms | Smooth sine fade |
| READY | Breathing | 3000 ms | Slow cosine inhale/exhale |
| ERROR | Blink | 300 ms | Alert blink |
| OTA_UPDATE | Solid White | -- | Constant on |
| SHUTDOWN | Off | -- | LED off |

## Adding Custom Commands

```cpp
system.registerCommand("temp", [](const String& args) {
    return String("Temperature: ") + String(readTemperature(), 1) + " C\n";
});
```

Commands are accessible via telnet (`telnet <device-ip> 23`).

## Further Reading

- [Technical Reference](technical-reference.md) -- full API documentation, SystemConfig fields, state transitions, persistence keys, and WebUI setup details.
- [Project Context (AI Agent)](project-context.md) -- file inventory, dependencies, conventions, and constitution compliance notes.
- [Minimal Example](../../../DomoticsCore-System/examples/Minimal/) -- simplest possible working application.
- [Standard Example](../../../DomoticsCore-System/examples/Standard/) -- standalone device with WebUI and NTP.
- [FullStack Example](../../../DomoticsCore-System/examples/FullStack/) -- complete IoT solution with MQTT, Home Assistant, and OTA.

## License

MIT
