# DomoticsCore-MQTT -- Project Context (AI Agent Reference)

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

This document provides structured context for AI agents and developers working on or alongside the DomoticsCore-MQTT component.

---

## Component Identity

| Property | Value |
|----------|-------|
| **Name** | DomoticsCore-MQTT |
| **Registered as** | `"MQTT"` (via `metadata.name`) |
| **Version** | 1.4.1 (declared in both `library.json` and `metadata.version` in constructor) |
| **Role** | MQTT client component providing publish/subscribe messaging over TCP/IP for ESP32/ESP8266 IoT devices |
| **Architecture** | Header-only library; inline implementations in `MQTT_impl.h` included at the end of `MQTT.h` |
| **Namespace** | `DomoticsCore::Components::MQTTComponent` |
| **HAL Namespace** | `DomoticsCore::HAL::MQTT` |
| **Events Namespace** | `DomoticsCore::MQTTEvents` |
| **WebUI Namespace** | `DomoticsCore::Components::WebUI::MQTTWebUI` |
| **License** | MIT |
| **Platforms** | espressif32, espressif8266 |
| **Frameworks** | Arduino |

---

## File Inventory

All source files reside under `DomoticsCore-MQTT/include/DomoticsCore/`:

| File | Purpose |
|------|---------|
| `MQTT.h` | Main header. Declares `MQTTComponent`, `MQTTConfig`, `MQTTState`, `MQTTStatistics`, and EventBus event structures (`MQTTPublishEvent`, `MQTTSubscribeEvent`, `MQTTMessageEvent`). Includes `MQTT_impl.h` at the end. |
| `MQTT_impl.h` | Inline method implementations for `MQTTComponent`. Contains constructor, destructor, lifecycle (`begin`, `loop`, `shutdown`), connection management, publish/subscribe, configuration, reconnection logic, message queue processing, wildcard topic matching, and the static PubSubClient callback. |
| `MQTT_HAL.h` | HAL routing header. Declares the abstract `MQTTClient` interface and conditionally includes the correct platform implementation based on `#ifdef` directives. |
| `MQTT_ESP32.h` | ESP32 HAL implementation. Wraps PubSubClient with `WiFiClient` / `WiFiClientSecure`. Defines `MQTT_MAX_PACKET_SIZE = 2048`. |
| `MQTT_ESP8266.h` | ESP8266 HAL implementation. Wraps PubSubClient with ESP8266WiFi. Defines `MQTT_MAX_PACKET_SIZE = 768`. |
| `MQTT_Stub.h` | Native test stub. Simulates MQTT broker interaction without network. Provides `simulateMessage()` for injecting test messages. Defines `MQTT_MAX_PACKET_SIZE = 1024`. Tracks publish/subscribe/loop counts for assertions. |
| `MQTTEvents.h` | Event topic string constants: `mqtt/connected`, `mqtt/disconnected`, `mqtt/message`, `mqtt/publish`, `mqtt/subscribe`. |
| `MQTTWebUI.h` | WebUI provider class `MQTTWebUI`. Implements `CachingWebUIProvider` with three UI contexts: `mqtt_status` (header badge), `mqtt_settings` (configuration card), `mqtt_detail` (statistics card). |

### Project-Level Files

| File | Purpose |
|------|---------|
| `DomoticsCore-MQTT/library.json` | PlatformIO library manifest (version, dependencies, build config) |
| `DomoticsCore-MQTT/README.md` | Component-level README with usage examples and full API reference |
| `DomoticsCore-MQTT/SPECIFICATIONS.md` | Functional specification document |
| `DomoticsCore-MQTT/STATE_MACHINE.md` | Detailed state machine documentation with timing diagrams |

---

## Key Classes

### MQTTComponent

The primary class. Extends `IComponent`. Singleton-like due to a static `instance` pointer required by PubSubClient's C-style callback. Only one instance may exist at a time.

**Key responsibilities**:
- Manage MQTT connection lifecycle (connect, disconnect, reconnect with exponential backoff)
- Publish messages (string, JSON, binary) with offline queuing
- Subscribe to topics with wildcard support
- Forward incoming messages to EventBus
- Listen for publish/subscribe requests from other components via EventBus
- Track connection statistics

### MQTTClient (HAL)

Abstract interface for platform-agnostic MQTT operations. Concrete implementations: `MQTTClientImpl` in each platform HAL file.

### MQTTWebUI

Composition-based WebUI provider. Holds a non-owning pointer to `MQTTComponent`. Provides three UI contexts for the DomoticsCore WebUI dashboard.

---

## Dependencies

### Required (declared in `library.json`)

| Dependency | Version | What MQTT Uses From It |
|------------|---------|------------------------|
| **DomoticsCore-Core** | >= 1.0.0 | `IComponent`, `EventBus` (`emit`, `on`), `Logger` (`DLOG_I`, `DLOG_D`, etc.), `Timer` (`NonBlockingDelay`), `Platform_HAL` (`getChipId`, `getMillis`, `yield`), `ArduinoJsonString` |
| **DomoticsCore-Wifi** | >= 1.0.0 | `Wifi_HAL.h` for `HAL::WiFiHAL::isConnected()` check before MQTT connection |
| **PubSubClient** | ^2.8 | Underlying MQTT protocol implementation (wrapped by HAL) |
| **ArduinoJson** | ^7.0 | JSON serialization for `publishJSON()` and WebUI data responses |

### Optional

| Dependency | Purpose |
|------------|---------|
| **DomoticsCore-WebUI** | Required only if using `MQTTWebUI` provider (includes `IWebUIProvider`, `BaseWebUIComponents`, `WebUI.h`) |

---

## Dependents (Components That Depend on MQTT)

| Component | How It Uses MQTT |
|-----------|-----------------|
| **DomoticsCore-HomeAssistant** | Publishes HA discovery configs and state updates via MQTT. Subscribes to HA command topics. Uses EventBus events (`mqtt/connected`, `mqtt/message`) for lifecycle coordination. This is the primary downstream consumer. |

---

## HAL Files Structure

```
MQTT_HAL.h                    (routing header -- abstract interface + #ifdef dispatch)
    |
    +-- MQTT_ESP32.h           (ESP32: PubSubClient + WiFiClient/WiFiClientSecure, 2048-byte buffer)
    +-- MQTT_ESP8266.h         (ESP8266: PubSubClient + ESP8266WiFi, 768-byte buffer)
    +-- MQTT_Stub.h            (Native: mock with simulateMessage(), 1024-byte buffer)
```

Per Constitution Principle IX, `#ifdef` platform directives exist ONLY in these HAL files. All business logic in `MQTT.h` and `MQTT_impl.h` is platform-agnostic.

---

## Conventions and Pitfalls

### config.enabled Mutation Bug (FIXED)

**Historical issue**: A config reload from flash storage could set `config.enabled = false` while the component had an active broker connection. This caused `loop()` to skip `mqttClient->loop()`, silently dropping all incoming messages and breaking keep-alive.

**Fix (commit bc727bd)**: `setConfig()` now preserves `enabled = true` when the component is in `Connected` state and the incoming config has `enabled = false`. A warning is logged: `"setConfig: preserving enabled=true for active connection"`.

**Agent guidance**: When modifying `setConfig()` or the `loop()` method, always verify that an active connection is never silently disabled by a config reload.

### Message Ordering

The offline message queue is processed FIFO. If a publish fails during queue drain, processing stops immediately to preserve ordering. Messages are not retried individually -- they wait for the next `loop()` iteration.

### Singleton Instance Pointer

`MQTTComponent` uses a static `instance` pointer because PubSubClient requires a C-style function pointer callback (`void (*)(char*, byte*, unsigned int)`). The static `mqttCallback()` forwards to `instance->handleIncomingMessage()`. Creating a second `MQTTComponent` will overwrite the `instance` pointer, breaking the first.

### EventBus Listeners Registered Before Config Check

In `begin()`, EventBus listeners for `mqtt/publish` and `mqtt/subscribe` are registered BEFORE checking whether a broker is configured. This is intentional -- it allows other components to start emitting publish/subscribe requests immediately. When the broker eventually gets configured (e.g., via WebUI), the listeners are already in place.

### PubSubClient Server Pointer Lifetime

PubSubClient stores the `const char*` pointer passed to `setServer()` without copying it. If the `config.broker` String reallocates (e.g., after `setConfig()`), the old pointer becomes dangling. Both `setConfig()` and `connectInternal()` defensively re-call `mqttClient->setServer()` to refresh the pointer.

### Fixed-Size Event Buffers

EventBus event structures use `char[128]` for topics and `char[700]` for payloads. Messages exceeding these sizes are silently truncated. The 700-byte payload limit is sized for Home Assistant discovery payloads (~600 bytes) with headroom.

### WiFi Dependency Check

`connect()` checks `HAL::WiFiHAL::isConnected()` before attempting MQTT connection. If WiFi is not available, connection is not attempted and the error is logged at DEBUG level (not ERROR), since this is a normal condition during startup sequencing.

### publishBinary() Does Not Queue

Unlike `publish()` (string) and `publishJSON()`, `publishBinary()` returns `false` immediately when disconnected. It does not buffer binary data in the offline queue.

---

## Constitution Compliance Reminders

When working on DomoticsCore-MQTT, the following Constitution principles are especially relevant:

1. **Principle VI (EventBus Architecture)**: All inter-component communication goes through EventBus. MQTT does NOT hold direct references to HomeAssistant or other consumers. It emits events; consumers subscribe.

2. **Principle IX (HAL Isolation)**: Platform `#ifdef` directives are FORBIDDEN outside the HAL files. If you need platform-specific behavior, add it to `MQTT_ESP32.h`, `MQTT_ESP8266.h`, or `MQTT_Stub.h`.

3. **Principle X (Non-Blocking Timer)**: `delay()` is forbidden. Reconnection uses `NonBlockingDelay`. The `loop()` method must complete in under 10 ms. `connectInternal()` calls `HAL::Platform::yield()` before and after the blocking PubSubClient connect call to prevent watchdog resets.

4. **Principle XIV (Memory Leak Prevention)**: Monitor heap impact of the message queue. After processing queued messages, `std::vector::erase()` is used but `shrink_to_fit()` is not currently called -- this is a potential improvement area. Event payload buffers are stack-allocated (fixed-size), avoiding heap churn.

5. **Principle XV (Semantic Versioning)**: Version changes must use `tools/bump_version.py`. The version in `library.json` must match `metadata.version` in the constructor (`MQTT_impl.h` line 34).

6. **Principle VII (File Size Limits)**: `MQTT_impl.h` is the largest file at approximately 520 lines (including comments and blank lines). Monitor this if adding features -- it may need splitting if it approaches 800 lines of code.

7. **Principle II (TDD)**: All new features must have corresponding tests. The `MQTT_Stub.h` mock supports `simulateMessage()` for testing incoming message flows without a real broker.

8. **Principle XI (Centralized Storage)**: MQTT does not directly access ESP32 Preferences. Configuration persistence is handled externally (typically by `SystemPersistence` calling `setConfig()`).

---

## Quick Reference: Component Registration

```cpp
// Minimal registration
MQTTConfig cfg;
cfg.broker = "192.168.1.100";
core.addComponent(std::make_unique<MQTTComponent>(cfg));

// Retrieve at runtime
auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
```

The component registers itself with `metadata.name = "MQTT"`, `metadata.version = "1.4.1"`, `metadata.author = "DomoticsCore"`.
