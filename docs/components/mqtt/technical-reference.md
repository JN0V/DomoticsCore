# DomoticsCore-MQTT -- Technical Reference

> **All development MUST comply with the [DomoticsCore Constitution](../../../.specify/memory/constitution.md).**

---

## Table of Contents

1. [MQTTComponent API](#mqttcomponent-api)
2. [MQTTConfig Struct](#mqttconfig-struct)
3. [Connection State Machine](#connection-state-machine)
4. [QoS Levels and Behavior](#qos-levels-and-behavior)
5. [Message Queuing and Offline Buffering](#message-queuing-and-offline-buffering)
6. [Wildcard Subscription Patterns](#wildcard-subscription-patterns)
7. [TLS/SSL Configuration](#tlsssl-configuration)
8. [JSON Helpers](#json-helpers)
9. [WebUI Provider](#webui-provider)
10. [Event System (MQTTEvents)](#event-system-mqttevents)
11. [EventBus Event Structures](#eventbus-event-structures)
12. [Static Utility Methods](#static-utility-methods)
13. [Statistics and Diagnostics](#statistics-and-diagnostics)
14. [HAL Architecture](#hal-architecture)
15. [Thread Safety](#thread-safety)

---

## MQTTComponent API

`MQTTComponent` extends `IComponent` and lives in namespace `DomoticsCore::Components`.

### Constructor

```cpp
explicit MQTTComponent(const MQTTConfig& config = MQTTConfig());
```

Creates the component. If `config.clientId` is empty, a client ID is auto-generated from the chip ID in the format `{platform}-{chipId}`. If LWT is enabled and `config.lwtTopic` is empty, it defaults to `{clientId}/status`.

### Destructor

```cpp
virtual ~MQTTComponent();
```

Disconnects from the broker (if connected) and releases the underlying HAL client.

### IComponent Lifecycle

```cpp
std::vector<Dependency> getDependencies() const override;  // Returns empty -- MQTT has no hard component deps
ComponentStatus begin() override;
void loop() override;
ComponentStatus shutdown() override;
```

**`begin()`** performs the following steps in order:

1. Registers EventBus listeners for `mqtt/publish` and `mqtt/subscribe` (always, even if broker is unconfigured).
2. Sets the PubSubClient message callback.
3. If `config.broker` is empty or `config.enabled` is false, returns `ComponentStatus::Success` without connecting (inactive but ready).
4. Configures the HAL client (server, keep-alive).
5. If `config.autoReconnect` is true, calls `connect()`.

**`loop()`**:

- If `config.broker` is empty, returns immediately.
- If connected: calls `mqttClient->loop()`, updates statistics, and processes the offline message queue.
- If disconnected and `config.enabled && config.autoReconnect`: calls `handleReconnection()`.

**`shutdown()`**: Disconnects from the broker if connected.

### Connection Management

```cpp
bool connect();
```

Initiates connection to the MQTT broker. Checks WiFi connectivity first (via `HAL::WiFiHAL::isConnected()`). On success, transitions to `Connected`, resubscribes to all tracked topics, and emits `mqtt/connected`. Returns `false` if WiFi is down or broker is unconfigured.

```cpp
void disconnect();
```

Disconnects from broker, transitions to `Disconnected`, emits `mqtt/disconnected`.

```cpp
void resetReconnect();
```

Resets the reconnection counter, timer, and error state. Call this after changing broker configuration at runtime (e.g., via WebUI) to re-enable auto-retry from the initial delay.

```cpp
bool isConnected() const;
```

Returns `true` only when the HAL client reports `connected()` AND internal state is `MQTTState::Connected`.

```cpp
MQTTState getState() const;
String getStateString() const;
```

Returns the current state as an enum or human-readable string (`"Disconnected"`, `"Connecting"`, `"Connected"`, `"Error"`).

### Publishing

```cpp
bool publish(const String& topic, const String& payload, uint8_t qos = 0, bool retain = false);
```

Publishes a string message. If disconnected, the message is pushed to the offline queue and the method returns `true` (queued). If connected, publishes directly via the HAL client.

```cpp
bool publishJSON(const String& topic, const JsonDocument& doc, uint8_t qos = 0, bool retain = false);
```

Serializes an ArduinoJson document to string and delegates to `publish()`.

```cpp
bool publishBinary(const String& topic, const uint8_t* data, size_t length, uint8_t qos = 0, bool retain = false);
```

Publishes raw binary data. Does NOT queue when offline -- returns `false` immediately if disconnected.

### Subscribing

```cpp
bool subscribe(const String& topic, uint8_t qos = 0);
```

Subscribes to a topic. If already subscribed to the exact same topic string, returns `true` immediately (idempotent). If disconnected, the subscription is stored internally and will be registered with the broker on next connect. Supports MQTT wildcards (`+`, `#`).

```cpp
bool unsubscribe(const String& topic);
```

Unsubscribes from a specific topic and removes it from the internal tracking list.

```cpp
void unsubscribeAll();
```

Unsubscribes from all tracked topics and clears the subscription list.

```cpp
std::vector<String> getActiveSubscriptions() const;
```

Returns a list of all currently tracked subscription topic strings.

### Configuration

```cpp
void setConfig(const MQTTConfig& cfg);
```

Replaces the current configuration. **Critical behavior**: if the component is currently `Connected` and the new config has `enabled = false`, the `enabled` flag is forcibly preserved to `true` to prevent silently dropping messages on an active connection. Also refreshes the PubSubClient server pointer after config change.

```cpp
const MQTTConfig& getConfig() const;
```

Returns a const reference to the current configuration.

```cpp
void setBroker(const String& broker, uint16_t port = 1883);
```

Updates broker address and port in both config and HAL client.

```cpp
void setCredentials(const String& username, const String& password);
```

Updates authentication credentials in config.

---

## MQTTConfig Struct

All fields with their types, defaults, and descriptions:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `broker` | `String` | `""` | Broker hostname or IP address |
| `port` | `uint16_t` | `1883` | Broker port (1883 plain, 8883 TLS) |
| `useTLS` | `bool` | `false` | Enable TLS/SSL encryption |
| `username` | `String` | `""` | Authentication username (optional) |
| `password` | `String` | `""` | Authentication password (optional) |
| `clientId` | `String` | `""` | Client identifier (auto-generated if empty) |
| `cleanSession` | `bool` | `true` | Start with a clean MQTT session |
| `keepAlive` | `uint16_t` | `60` | Keep-alive interval in seconds |
| `enableLWT` | `bool` | `true` | Enable Last Will and Testament |
| `lwtTopic` | `String` | `""` | LWT topic (defaults to `{clientId}/status`) |
| `lwtMessage` | `String` | `"offline"` | LWT payload |
| `lwtQoS` | `uint8_t` | `1` | LWT QoS level |
| `lwtRetain` | `bool` | `true` | Retain the LWT message |
| `autoReconnect` | `bool` | `true` | Auto-reconnect on disconnect |
| `reconnectDelay` | `uint32_t` | `1000` | Initial reconnection delay in ms |
| `maxReconnectDelay` | `uint32_t` | `30000` | Maximum reconnection delay in ms |
| `maxQueueSize` | `uint16_t` | `100` | Maximum offline message queue size **(not enforced -- see warning below)** |
| `publishRateLimit` | `uint8_t` | `10` | Max messages per second (0 = unlimited) **(not enforced -- see warning below)** |
| `maxSubscriptions` | `uint8_t` | `50` | Maximum number of subscriptions **(not enforced -- see warning below)** |
| `resubscribeOnConnect` | `bool` | `true` | Re-subscribe to all topics after reconnect |
| `connectTimeout` | `uint32_t` | `10000` | Connection attempt timeout in ms |
| `operationTimeout` | `uint32_t` | `5000` | Generic operation timeout in ms |
| `enabled` | `bool` | `true` | Master enable/disable flag |

> **Warning -- Unenforced config fields**: The `maxQueueSize`, `publishRateLimit`, and `maxSubscriptions` fields are declared in `MQTTConfig` but are **not enforced at runtime** in the current implementation. Specifically:
> - `maxQueueSize`: `publish()` appends to the offline queue unconditionally with no size check. The queue can grow beyond this value.
> - `publishRateLimit`: The `lastPublishTime` and `publishCountThisSecond` members exist in the class but are never checked before publishing. Rate limiting is not applied.
> - `maxSubscriptions`: `subscribe()` adds subscriptions with no count check against this limit.
>
> Do not rely on these fields for resource protection until enforcement logic is implemented.

---

## Connection State Machine

### States

```
enum class MQTTState {
    Disconnected,   // Not connected -- initial state
    Connecting,     // TCP + MQTT handshake in progress
    Connected,      // Active broker session
    Error           // Connection failed, awaiting retry
};
```

### Transition Diagram

```
                        +----------------+
              -------->| Disconnected   |<--------
             |         +----------------+         |
             |           |            ^            |
             |  connect()|            | disconnect()
             |           v            |            |
             |         +----------------+         |
             |         | Connecting     |         |
             |         +----------------+         |
             |           |            |            |
             |    success|            |failure     |
             |           v            v            |
             |         +----------+ +-------+     |
             |         | Connected| | Error |-----+
             |         +----------+ +-------+   autoReconnect=false
             |                        |
             |                        | timer expired
             |                        | (exponential backoff)
             +------------------------+
                   reconnect attempt
```

### Transition Details

| From | To | Trigger |
|------|----|---------|
| Disconnected | Connecting | `connect()` called (WiFi available, broker configured) |
| Connecting | Connected | Broker sends CONNACK success |
| Connecting | Error | Connection timeout or broker rejection |
| Connected | Disconnected | `disconnect()` called or network loss detected |
| Error | Connecting | Reconnection timer fires (if `autoReconnect = true`) |
| Error | Disconnected | `autoReconnect` disabled or component shutdown |

### Exponential Backoff

The reconnection delay doubles after each failure, capped at `maxReconnectDelay`:

```
Attempt 1: 1000 ms
Attempt 2: 2000 ms
Attempt 3: 4000 ms
Attempt 4: 8000 ms
Attempt 5: 16000 ms
Attempt 6+: 30000 ms (capped)
```

On successful connection, the delay resets to `reconnectDelay` (initial value) and `stats.reconnectCount` resets to 0.

---

## QoS Levels and Behavior

| QoS | Name | Delivery Guarantee | Use Case |
|-----|------|--------------------|----------|
| 0 | At most once | Fire-and-forget, no acknowledgment | Telemetry, sensor readings (acceptable loss) |
| 1 | At least once | Broker acknowledges receipt; may deliver duplicates | Commands, state changes |
| 2 | Exactly once | Four-step handshake ensures single delivery | Critical operations (rarely needed on ESP) |

**Note**: The underlying PubSubClient library handles QoS 0 natively. QoS 1 and 2 behavior depends on broker support. The component passes the QoS parameter through to PubSubClient for both publish and subscribe operations.

---

## Message Queuing and Offline Buffering

When the component is **disconnected**, calls to `publish()` do not fail. Instead, messages are pushed into an internal `std::vector<QueuedMessage>`:

```cpp
struct QueuedMessage {
    String topic;
    String payload;
    uint8_t qos;
    bool retain;
};
```

**Queue processing** happens in `loop()` when the connection is active. The component iterates through the queue, publishing each message. Successfully published messages are removed; if a publish fails, processing stops (preserving message order).

**Limits**: The `config.maxQueueSize` field (default 100) is intended to bound queue size, but **this limit is not currently enforced** in the code. `publish()` appends to the queue unconditionally. Monitor `getQueuedMessageCount()` and implement your own bounds checking if unbounded growth is a concern.

**Important**: `publishBinary()` does NOT queue when offline -- it returns `false` immediately.

---

## Wildcard Subscription Patterns

MQTT wildcards are supported in `subscribe()` and matched by `topicMatches()`:

| Pattern | Meaning | Example Filter | Matches | Does Not Match |
|---------|---------|---------------|---------|----------------|
| `+` | Single level | `home/+/temp` | `home/living/temp` | `home/living/room/temp` |
| `#` | Multi level (must be last) | `home/sensors/#` | `home/sensors/temp`, `home/sensors/a/b` | `home/other` |
| Literal | Exact match | `home/light` | `home/light` | `home/lights` |

The `topicMatches(filter, topic)` static method splits both strings on `/` and compares segment-by-segment. `+` matches any single segment; `#` matches all remaining segments.

---

## TLS/SSL Configuration

TLS is enabled by setting `config.useTLS = true` and (typically) `config.port = 8883`.

The HAL implementations select the transport at construction time:

- **ESP32**: Uses `WiFiClientSecure` when TLS is enabled, `WiFiClient` otherwise.
- **ESP8266**: Uses `WiFiClientSecure` (BearSSL-based) when TLS is enabled.
- **Native stub**: Ignores TLS flag (no real network).

Certificate validation, pinning, and custom CA configuration must be done at the HAL level before connection. The component constructor passes `config.useTLS` to the `MQTTClientImpl` constructor.

---

## JSON Helpers

```cpp
bool publishJSON(const String& topic, const JsonDocument& doc, uint8_t qos = 0, bool retain = false);
```

Serializes the ArduinoJson document to a `String` using `serializeJson()`, then delegates to the standard `publish()` method. This means JSON publishes benefit from the same offline queuing behavior as string publishes.

For incoming JSON messages, parse the payload from `MQTTMessageEvent`:

```cpp
on<MQTTMessageEvent>("mqtt/message", [](const MQTTMessageEvent& ev) {
    JsonDocument doc;
    if (deserializeJson(doc, ev.payload) == DeserializationError::Ok) {
        float temp = doc["temperature"];
    }
});
```

---

## WebUI Provider

### Class: MQTTWebUI

```cpp
namespace DomoticsCore::Components::WebUI {

class MQTTWebUI : public CachingWebUIProvider {
public:
    explicit MQTTWebUI(MQTTComponent* component);
    void setConfigSaveCallback(std::function<void(const MQTTConfig&)> callback);

    String getWebUIName() const override;   // Returns "MQTT"
    String getWebUIVersion() const override; // Returns "1.4.0"

    String getWebUIData(const String& contextId) override;
    bool hasDataChanged(const String& contextId) override;
    String handleWebUIRequest(const String& contextId, const String& endpoint,
                              const String& method, const std::map<String, String>& params) override;
};

}
```

### Registration

```cpp
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
if (webui && mqtt) {
    auto* provider = new MQTTWebUI(mqtt);
    provider->setConfigSaveCallback([](const MQTTConfig& cfg) {
        // Persist to Storage
    });
    webui->registerProviderWithComponent(provider, mqtt);
}
```

### UI Contexts

#### `mqtt_status` -- Header Status Badge

- **Location**: Dashboard header
- **Real-time interval**: 2000 ms
- **API endpoint**: `/api/mqtt/status`
- **Data fields**: `state` (bool string "true"/"false"), `state_label`, `connected`, `state_code`
- **Change detection**: Only pushes updates when `getStateString()` changes (via `LazyState<String>`)

#### `mqtt_settings` -- Settings Card

- **Location**: Settings tab
- **API endpoint**: `/api/mqtt/settings`
- **Editable fields**: `enabled`, `broker`, `port`, `username`, `password`, `client_id`, `use_tls`, `clean_session`, `lwt_enabled`, `lwt_topic`, `lwt_message`
- **POST handling**: Updates config field-by-field; calls `setConfig()`, optionally invokes `onConfigSaved` callback. Toggling `enabled` triggers `connect()` or `disconnect()`.

#### `mqtt_detail` -- Component Detail Card

- **Location**: Component detail view
- **Real-time interval**: 1000 ms
- **API endpoint**: `/api/mqtt/detail`
- **Data fields**: `broker_addr`, `state`, `uptime`, `client_id`, `publish_count`, `receive_count`, `subscription_count`, `queue_size`, `reconnect_count`, `error_count`, `subscriptions` (JSON array)

---

## Event System (MQTTEvents)

Defined in `MQTTEvents.h` under namespace `DomoticsCore::MQTTEvents`:

| Constant | Value | Direction | Description |
|----------|-------|-----------|-------------|
| `EVENT_CONNECTED` | `"mqtt/connected"` | Emitted | Fired after successful broker connection |
| `EVENT_DISCONNECTED` | `"mqtt/disconnected"` | Emitted | Fired after disconnection |
| `EVENT_MESSAGE` | `"mqtt/message"` | Emitted | Fired for each incoming message |
| `EVENT_PUBLISH` | `"mqtt/publish"` | Listened | Other components request a publish |
| `EVENT_SUBSCRIBE` | `"mqtt/subscribe"` | Listened | Other components request a subscription |

### Event Flow

```
  Other Component               MQTTComponent                Broker
       |                             |                          |
       |-- emit("mqtt/publish") ---->|                          |
       |                             |-- MQTT PUBLISH --------->|
       |                             |                          |
       |                             |<-- MQTT message ---------|
       |<-- emit("mqtt/message") ----|                          |
       |                             |                          |
       |                             |-- connection established-|
       |<-- emit("mqtt/connected") --|                          |
```

---

## EventBus Event Structures

Defined in `MQTT.h` under namespace `DomoticsCore::Components`:

### Buffer Size Constants

```cpp
constexpr size_t MQTT_EVENT_TOPIC_SIZE = 128;    // Max topic length
constexpr size_t MQTT_EVENT_PAYLOAD_SIZE = 700;   // Max payload (~600 bytes for HA discovery + headroom)
```

### MQTTPublishEvent

```cpp
struct MQTTPublishEvent {
    char topic[128];      // Null-terminated topic
    char payload[700];    // Null-terminated payload
    uint8_t qos = 0;
    bool retain = false;
};
```

Used to request a publish via EventBus. Caller must `strncpy()` strings into the fixed buffers before emitting.

### MQTTSubscribeEvent

```cpp
struct MQTTSubscribeEvent {
    char topic[128];      // Topic filter (supports wildcards)
    uint8_t qos = 0;
};
```

Used to request a subscription via EventBus.

### MQTTMessageEvent

```cpp
struct MQTTMessageEvent {
    char topic[128];      // Received message topic
    char payload[700];    // Received message payload
};
```

Emitted for every incoming MQTT message. Data is copied into these fixed-size buffers and remains valid for the entire event dispatch cycle.

**Why fixed-size buffers?** EventBus uses `memcpy` for value-based event dispatch. Fixed-size char arrays avoid heap allocation and dangling pointer issues.

---

## Static Utility Methods

```cpp
static bool isValidTopic(const String& topic, bool allowWildcards = false);
```

> **Warning**: `isValidTopic()` is declared in the header but **not yet implemented** in `MQTT_impl.h`. Calling it will cause a **linker error**. Do not rely on this method until an implementation is provided.

Intended behavior: validates an MQTT topic string. When `allowWildcards` is false, rejects topics containing `+` or `#`.

```cpp
static bool topicMatches(const String& filter, const String& topic);
```

Checks whether a concrete topic matches a filter pattern containing MQTT wildcards. Implementation splits both strings on `/` separators and compares segment-by-segment:

- `+` matches any single segment.
- `#` matches all remaining segments (must be the last segment in the filter).
- Literal segments must match exactly.

Returns `true` for an exact string match or if `filter == "#"`.

---

## Statistics and Diagnostics

### MQTTStatistics Struct

```cpp
struct MQTTStatistics {
    uint32_t connectCount = 0;      // Total successful connections
    uint32_t reconnectCount = 0;    // Consecutive reconnection attempts (resets on success)
    uint32_t publishCount = 0;      // Total messages published
    uint32_t publishErrors = 0;     // Failed publish attempts
    uint32_t receiveCount = 0;      // Total messages received
    uint32_t subscriptionCount = 0; // Current active subscription count
    uint32_t uptime = 0;            // Seconds since last connection
    uint32_t lastLatency = 0;       // Last measured latency in ms
};
```

### Diagnostic Methods

```cpp
const MQTTStatistics& getStatistics() const;
size_t getQueuedMessageCount() const;
String getLastError() const;
uint32_t debugLoopCount() const;  // HAL client loop() call count
```

---

## HAL Architecture

The MQTT HAL follows Constitution Principle IX -- all `#ifdef` platform directives are isolated in HAL files.

### Routing Header: `MQTT_HAL.h`

Declares the abstract `MQTTClient` interface and conditionally includes the platform implementation:

```cpp
#if defined(ARDUINO_ARCH_ESP32)
    #include "MQTT_ESP32.h"        // PubSubClient + WiFiClient/WiFiClientSecure
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
    #include "MQTT_ESP8266.h"      // PubSubClient + ESP8266WiFi
#else
    #include "MQTT_Stub.h"         // Mock for native tests
#endif
```

### MQTTClient Interface

```cpp
class MQTTClient {
public:
    virtual bool connect(const char* id, const char* user, const char* pass,
                         const char* willTopic, uint8_t willQoS,
                         bool willRetain, const char* willMessage) = 0;
    virtual void disconnect() = 0;
    virtual bool loop() = 0;
    virtual bool publish(const char* topic, const uint8_t* payload,
                         unsigned int length, bool retained) = 0;
    virtual bool subscribe(const char* topic, uint8_t qos) = 0;
    virtual bool unsubscribe(const char* topic) = 0;
    virtual void setServer(const char* domain, uint16_t port) = 0;
    virtual void setCallback(void (*callback)(char*, uint8_t*, unsigned int)) = 0;
    virtual void setKeepAlive(uint16_t keepAlive) = 0;
    virtual bool setBufferSize(uint16_t size) = 0;
    virtual uint16_t getBufferSize() = 0;
    virtual int state() = 0;
    virtual bool connected() = 0;
    virtual uint32_t getLoopCallCount() const { return 0; }
};
```

### Platform Buffer Sizes

Each platform defines `MQTT_MAX_PACKET_SIZE` before including PubSubClient:

| Platform | `MQTT_MAX_PACKET_SIZE` | Rationale |
|----------|------------------------|-----------|
| ESP32 | 2048 | ~520KB RAM allows larger buffers |
| ESP8266 | 768 | ~80KB RAM; sized for HA discovery payloads (~600 bytes) |
| Native | 1024 | Moderate size for testing |

---

## Thread Safety

The MQTT component is **NOT thread-safe**. All methods must be called from the same task/thread (typically the Arduino `loop()` task). If multi-task access is required, protect calls with FreeRTOS mutexes externally.

The static `instance` pointer used for the PubSubClient callback is set in the constructor and cleared in the destructor. Only one `MQTTComponent` instance may exist at a time.
