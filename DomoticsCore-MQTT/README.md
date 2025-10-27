# DomoticsCore-MQTT

MQTT client component for DomoticsCore with auto-reconnection, QoS support, and WebUI integration.

## Features

### Core Functionality
- ✅ **Auto-connection** with exponential backoff reconnection
- ✅ **QoS levels 0, 1, 2** support
- ✅ **Wildcard subscriptions** (`+` and `#`)
- ✅ **Message queuing** for offline buffering
- ✅ **Last Will Testament** (LWT) support
- ✅ **TLS/SSL** support (optional)
- ✅ **JSON helpers** for structured data
- ✅ **Topic validation** and wildcard matching

### WebUI Integration
- ✅ **Header badge** - Connection status indicator
- ✅ **Settings card** - Full broker configuration
- ✅ **Component detail** - Statistics and diagnostics
- ✅ **Real-time updates** - Live connection monitoring

### Monitoring
- ✅ **Connection statistics** (publish/receive counts, uptime)
- ✅ **Error tracking** (publish failures, reconnections)
- ✅ **Active subscriptions** list
- ✅ **Message queue** size monitoring

## Installation

Add to your `platformio.ini`:

```ini
lib_deps =
    file://../DomoticsCore-Core
    file://../DomoticsCore-MQTT
```

Or use library.json in your project.

## Quick Start

### Basic Usage

```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>

using namespace DomoticsCore;
using namespace DomoticsCore::Components;

// Configure MQTT
MQTTConfig mqttConfig;
mqttConfig.broker = "mqtt.example.com";
mqttConfig.port = 1883;
mqttConfig.username = "user";
mqttConfig.password = "pass";
mqttConfig.clientId = "esp32-device-01";

// Create component
auto mqtt = std::make_unique<MQTTComponent>(mqttConfig);
auto* mqttPtr = mqtt.get();
core.addComponent(std::move(mqtt));

// Register callbacks
mqttPtr->onConnect([]() {
    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    mqtt->publish("home/status", "online", 1, true);
    mqtt->subscribe("home/commands/#", 1);
});

mqttPtr->onMessage("home/commands/led", [](const String& topic, const String& payload) {
    if (payload == "on") {
        // Turn LED on
    }
});

// Initialize
core.begin();

// In loop()
core.loop();
```

### With WebUI

```cpp
#include <DomoticsCore/WebUI.h>
#include <DomoticsCore/MQTTWebUI.h>

// ... configure and add components ...

// Register WebUI provider
auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
if (webui && mqtt) {
    webui->registerProviderWithComponent(new MQTTWebUI(mqtt), mqtt);
}
```

## Configuration

### MQTTConfig Structure

```cpp
struct MQTTConfig {
    // Server
    String broker;              // MQTT broker address
    uint16_t port;              // Port (1883 for plain, 8883 for TLS)
    bool useTLS;                // Use TLS/SSL encryption
    
    // Authentication
    String username;            // Optional username
    String password;            // Optional password
    String clientId;            // Auto-generated if empty
    
    // Session
    bool cleanSession;          // Start with clean session
    uint16_t keepAlive;         // Keep-alive interval (seconds)
    
    // Last Will Testament
    bool enableLWT;             // Enable LWT
    String lwtTopic;            // LWT topic (defaults to {clientId}/status)
    String lwtMessage;          // LWT message ("offline")
    uint8_t lwtQoS;             // LWT QoS level (0-2)
    bool lwtRetain;             // Retain LWT message
    
    // Reconnection
    bool autoReconnect;         // Auto-reconnect on disconnect
    uint32_t reconnectDelay;    // Initial delay (ms)
    uint32_t maxReconnectDelay; // Max delay (ms)
    
    // Publishing
    uint16_t maxQueueSize;      // Offline message queue size
    uint8_t publishRateLimit;   // Max messages/second (0 = unlimited)
    
    // Subscriptions
    uint8_t maxSubscriptions;   // Max number of subscriptions
    bool resubscribeOnConnect;  // Re-subscribe after reconnect
    
    // Timeouts
    uint32_t connectTimeout;    // Connection timeout (ms)
    uint32_t operationTimeout;  // Operation timeout (ms)
    
    // Component
    bool enabled;               // Enable/disable component
};
```

### Default Values

| Setting | Default | Description |
|---------|---------|-------------|
| `port` | 1883 | Standard MQTT port |
| `useTLS` | false | Plain connection |
| `cleanSession` | true | No session persistence |
| `keepAlive` | 60 | 60 second keep-alive |
| `enableLWT` | true | LWT enabled |
| `lwtQoS` | 1 | At-least-once delivery |
| `lwtRetain` | true | Retained message |
| `autoReconnect` | true | Auto-reconnection enabled |
| `reconnectDelay` | 1000 | 1 second initial delay |
| `maxReconnectDelay` | 30000 | 30 second max delay |
| `maxQueueSize` | 100 | 100 queued messages |
| `publishRateLimit` | 10 | 10 messages/second |
| `maxSubscriptions` | 50 | 50 subscriptions |

## API Reference

### Connection Management

```cpp
// Connect to broker
bool connect();

// Disconnect from broker
void disconnect();

// Check connection status
bool isConnected() const;

// Get current state
MQTTState getState() const;  // Disconnected, Connecting, Connected, Error
String getStateString() const;
```

### Publishing

```cpp
// Publish string message
bool publish(const String& topic, const String& payload, 
             uint8_t qos = 0, bool retain = false);

// Publish JSON document
bool publishJSON(const String& topic, const JsonDocument& doc, 
                 uint8_t qos = 0, bool retain = false);

// Publish binary data
bool publishBinary(const String& topic, const uint8_t* data, size_t length,
                   uint8_t qos = 0, bool retain = false);
```

### Subscribing

```cpp
// Subscribe to topic (supports wildcards)
bool subscribe(const String& topic, uint8_t qos = 0);

// Unsubscribe from topic
bool unsubscribe(const String& topic);

// Unsubscribe from all topics
void unsubscribeAll();

// Get active subscriptions
std::vector<String> getActiveSubscriptions() const;
```

### Callbacks

```cpp
// Message callback
using MessageCallback = std::function<void(const String& topic, const String& payload)>;
bool onMessage(const String& topicFilter, MessageCallback callback);

// Connection callbacks
using ConnectionCallback = std::function<void()>;
bool onConnect(ConnectionCallback callback);
bool onDisconnect(ConnectionCallback callback);
```

### Statistics

```cpp
struct MQTTStatistics {
    uint32_t connectCount;      // Total connections
    uint32_t reconnectCount;    // Reconnection attempts
    uint32_t publishCount;      // Messages published
    uint32_t publishErrors;     // Publish failures
    uint32_t receiveCount;      // Messages received
    uint32_t subscriptionCount; // Active subscriptions
    uint32_t uptime;            // Connected time (seconds)
    uint32_t lastLatency;       // Last latency (ms)
};

const MQTTStatistics& getStatistics() const;
size_t getQueuedMessageCount() const;
String getLastError() const;
```

### Helper Functions

```cpp
// Validate topic string
static bool isValidTopic(const String& topic, bool allowWildcards = false);

// Check if topic matches filter (with wildcards)
static bool topicMatches(const String& filter, const String& topic);
```

## Usage Examples

### Publishing Sensor Data

```cpp
auto* mqtt = core.getComponent<MQTTComponent>("MQTT");

// Simple publish
float temp = 23.5;
mqtt->publish("home/temperature", String(temp), 0, false);

// JSON publish
JsonDocument doc;
doc["temperature"] = 23.5;
doc["humidity"] = 65.2;
doc["timestamp"] = millis();
mqtt->publishJSON("home/sensors", doc, 1, true);
```

### Wildcard Subscriptions

```cpp
// Subscribe to all sensor topics
mqtt->subscribe("home/sensors/+/temperature", 1);

// Subscribe to all commands
mqtt->subscribe("home/commands/#", 1);

// Handle messages
mqtt->onMessage("home/sensors/+/temperature", [](const String& topic, const String& payload) {
    // Extracts device ID from topic
    int startIdx = topic.indexOf("sensors/") + 8;
    int endIdx = topic.indexOf("/temperature");
    String deviceId = topic.substring(startIdx, endIdx);
    
    float temp = payload.toFloat();
    Serial.printf("Device %s: %.1f°C\n", deviceId.c_str(), temp);
});
```

### Command Handling

```cpp
mqtt->subscribe("device/commands/#", 1);

mqtt->onMessage("device/commands/led", [](const String& topic, const String& payload) {
    if (payload == "on") {
        digitalWrite(LED_PIN, HIGH);
    } else if (payload == "off") {
        digitalWrite(LED_PIN, LOW);
    }
});

mqtt->onMessage("device/commands/restart", [](const String& topic, const String& payload) {
    ESP.restart();
});
```

### Connection Lifecycle

```cpp
mqtt->onConnect([]() {
    Serial.println("MQTT Connected!");
    
    auto* mqtt = core.getComponent<MQTTComponent>("MQTT");
    
    // Publish online status
    mqtt->publish("device/status", "online", 1, true);
    
    // Re-subscribe to topics
    mqtt->subscribe("device/commands/#", 1);
});

mqtt->onDisconnect([]() {
    Serial.println("MQTT Disconnected!");
    // Handle disconnection (e.g., stop relying on MQTT)
});
```

## WebUI Features

### Status Badge
Shows real-time connection status in header:
- 🔴 Disconnected (red)
- 🟡 Connecting (yellow)
- 🟢 Connected (green)
- 🔴 Error (red)

### Settings Card
Configure all MQTT parameters:
- Broker address and port
- Username and password
- Client ID
- TLS/SSL toggle
- Session settings
- Last Will Testament
- Connect/Disconnect buttons

### Component Detail
Monitor MQTT statistics:
- Connection state and uptime
- Messages published/received
- Active subscriptions list
- Queued messages count
- Reconnection attempts
- Error counts

## Examples

### MQTTNoWebUI
Minimal MQTT client setup with:
- WiFi connection
- Broker configuration
- Publishing sensor data
- Subscribing to commands
- Message handling

**Location:** `examples/MQTTNoWebUI/`

### MQTTWithWebUI
Full-featured example with:
- Web-based configuration
- Real-time monitoring
- Statistics dashboard
- Interactive testing

**Location:** `examples/MQTTWithWebUI/`

## Troubleshooting

### Connection Fails

**Check broker address:**
```cpp
Serial.printf("Broker: %s:%d\n", mqtt->getConfig().broker.c_str(), mqtt->getConfig().port);
```

**Check credentials:**
```cpp
if (!mqtt->getConfig().username.isEmpty()) {
    Serial.println("Using authentication");
}
```

**Check last error:**
```cpp
if (!mqtt->isConnected()) {
    Serial.printf("Error: %s\n", mqtt->getLastError().c_str());
}
```

### Messages Not Received

**Verify subscription:**
```cpp
auto subs = mqtt->getActiveSubscriptions();
for (const auto& topic : subs) {
    Serial.printf("Subscribed to: %s\n", topic.c_str());
}
```

**Check callback registration:**
```cpp
mqtt->onMessage("test/topic", [](const String& topic, const String& payload) {
    Serial.printf("Received: %s = %s\n", topic.c_str(), payload.c_str());
});
```

### Publish Failures

**Check connection:**
```cpp
if (mqtt->isConnected()) {
    mqtt->publish("test", "hello");
} else {
    Serial.println("Not connected!");
}
```

**Check queue:**
```cpp
Serial.printf("Queued messages: %d\n", mqtt->getQueuedMessageCount());
```

## Performance

### Memory Usage
- **RAM:** ~6KB (component instance + buffers)
- **Flash:** ~28KB (code)
- **Heap:** ~14KB (runtime, including message queues)

### Throughput
- **Publish rate:** Up to 100 msg/s (QoS 0)
- **Receive rate:** Up to 200 msg/s
- **Queue capacity:** 100 messages (configurable)

### Timing
- **Connect timeout:** 10 seconds
- **Publish latency:** < 100ms (QoS 0)
- **Loop iteration:** < 10ms

## Dependencies

- **DomoticsCore-Core** >= 0.2.5
- **PubSubClient** ^2.8
- **ArduinoJson** ^7.0
- **WiFi** (ESP32)
- **WiFiClientSecure** (for TLS)

Optional:
- **DomoticsCore-WebUI** (for web interface)

## License

MIT

## Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Add tests for new features
4. Submit a pull request

## Support

- 📖 [Documentation](./SPECIFICATIONS.md)
- 💬 [Issues](https://github.com/yourusername/DomoticsCore/issues)
- 📧 Email: info@domoticscore.dev

---

**Version:** 0.1.0  
**Last Updated:** 2025-10-01
