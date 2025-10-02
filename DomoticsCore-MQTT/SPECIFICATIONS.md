# DomoticsCore-MQTT - Functional Specifications

## 1. Overview

**Component Name:** `DomoticsCore-MQTT`  
**Purpose:** Provides MQTT client functionality with automatic reconnection, topic management, QoS support, and WebUI configuration.  
**Architecture Pattern:** Modular component following DomoticsCore composition pattern with separate WebUI provider.

---

## 2. Core Features

### 2.1 Connection Management
- **Auto-connect** on component initialization
- **Auto-reconnect** with exponential backoff (1s, 2s, 5s, 10s, 30s)
- **Connection state monitoring** (Disconnected, Connecting, Connected, Error)
- **Persistent credentials** stored in ESP32 Preferences
- **TLS/SSL support** (optional, for secure connections)
- **Clean session** vs **persistent session** options
- **Last Will and Testament (LWT)** support
- **Keep-alive** interval configuration (default: 60s)

### 2.2 Publishing
- **Publish with QoS** levels (0, 1, 2)
- **Retained messages** support
- **JSON helper** for structured data
- **Batch publishing** (queue multiple messages)
- **Publish rate limiting** (configurable max messages/second)
- **Topic validation** before publishing
- **Publish confirmation** callbacks (for QoS > 0)

### 2.3 Subscribing
- **Topic subscription** with QoS levels
- **Wildcard support** (`+` and `#`)
- **Topic unsubscription**
- **Subscription callbacks** with topic routing
- **Message filtering** by topic pattern
- **Subscription persistence** (re-subscribe on reconnect)
- **Maximum subscriptions** limit (configurable, default: 50)

### 2.4 Message Handling
- **Callback registration** per topic/pattern
- **Message queue** for offline buffering (configurable size)
- **Message deduplication** (for QoS > 0)
- **Payload parsing** (string, JSON, binary)
- **Message size limits** (configurable, default: 4KB)
- **UTF-8 validation** for text payloads

### 2.5 Monitoring & Diagnostics
- **Connection statistics** (uptime, reconnect count, messages sent/received)
- **Message metrics** (publish/subscribe counters, errors)
- **Network quality** indicators (latency, packet loss)
- **Topic list** (active subscriptions)
- **Error logging** with categorization
- **Health status** reporting

---

## 3. Configuration

### 3.1 Connection Configuration
```cpp
struct MQTTConfig {
    // Server
    String broker;              // "mqtt.example.com" or IP
    uint16_t port;              // Default: 1883 (non-TLS), 8883 (TLS)
    bool useTLS;                // Default: false
    
    // Authentication
    String username;            // Optional
    String password;            // Optional
    String clientId;            // Auto-generated if empty
    
    // Session
    bool cleanSession;          // Default: true
    uint16_t keepAlive;         // Seconds, default: 60
    
    // Last Will
    bool enableLWT;             // Default: true
    String lwtTopic;            // Default: "{clientId}/status"
    String lwtMessage;          // Default: "offline"
    uint8_t lwtQoS;             // Default: 1
    bool lwtRetain;             // Default: true
    
    // Reconnection
    bool autoReconnect;         // Default: true
    uint32_t reconnectDelay;    // Initial delay (ms), default: 1000
    uint32_t maxReconnectDelay; // Max delay (ms), default: 30000
    
    // Publishing
    uint16_t maxQueueSize;      // Offline queue, default: 100
    uint8_t publishRateLimit;   // Messages/sec, 0 = unlimited, default: 10
    
    // Subscriptions
    uint8_t maxSubscriptions;   // Default: 50
    bool resubscribeOnConnect;  // Default: true
    
    // Timeouts
    uint32_t connectTimeout;    // Milliseconds, default: 10000
    uint32_t operationTimeout;  // Milliseconds, default: 5000
};
```

### 3.2 Runtime Configuration
- All settings modifiable via WebUI
- Preferences namespace: `"mqtt"`
- Preference keys (15 chars max):
  - `mqtt_broker`
  - `mqtt_port`
  - `mqtt_use_tls`
  - `mqtt_username`
  - `mqtt_password`
  - `mqtt_client_id`
  - `mqtt_clean_ses`
  - `mqtt_keepalive`
  - `mqtt_lwt_topic`
  - `mqtt_lwt_msg`
  - `mqtt_enabled`

---

## 4. Public API

### 4.1 Component Interface
```cpp
class MQTTComponent : public IComponent {
public:
    // Lifecycle
    ComponentStatus begin() override;
    void loop() override;
    ComponentStatus shutdown() override;
    
    // Connection
    bool connect();
    void disconnect();
    bool isConnected() const;
    ConnectionState getState() const;
    
    // Publishing
    bool publish(const String& topic, const String& payload, 
                 uint8_t qos = 0, bool retain = false);
    bool publishJSON(const String& topic, const JsonDocument& doc, 
                     uint8_t qos = 0, bool retain = false);
    bool publishBinary(const String& topic, const uint8_t* data, 
                       size_t len, uint8_t qos = 0, bool retain = false);
    
    // Subscribing
    bool subscribe(const String& topic, uint8_t qos = 0);
    bool unsubscribe(const String& topic);
    void unsubscribeAll();
    
    // Callbacks
    using MessageCallback = std::function<void(const String& topic, 
                                               const String& payload)>;
    bool onMessage(const String& topicFilter, MessageCallback callback);
    bool onConnect(std::function<void()> callback);
    bool onDisconnect(std::function<void()> callback);
    
    // Configuration
    void setConfig(const MQTTConfig& cfg);
    const MQTTConfig& getConfig() const;
    void setBroker(const String& broker, uint16_t port = 1883);
    void setCredentials(const String& username, const String& password);
    
    // Statistics
    struct Statistics {
        uint32_t connectCount;
        uint32_t reconnectCount;
        uint32_t publishCount;
        uint32_t publishErrors;
        uint32_t receiveCount;
        uint32_t subscriptionCount;
        uint32_t uptime;           // Seconds connected
        uint32_t lastLatency;      // Milliseconds
    };
    const Statistics& getStatistics() const;
    
    // Diagnostics
    std::vector<String> getActiveSubscriptions() const;
    size_t getQueuedMessageCount() const;
    String getLastError() const;
};
```

### 4.2 Helper Functions
```cpp
// Topic validation
bool isValidTopic(const String& topic, bool allowWildcards = false);

// Topic matching
bool topicMatches(const String& filter, const String& topic);

// Message helpers
String createJSONPayload(const JsonDocument& doc);
bool parseJSONPayload(const String& payload, JsonDocument& doc);
```

---

## 5. WebUI Provider

### 5.1 Composition Pattern
```cpp
class MQTTWebUI : public IWebUIProvider {
    MQTTComponent* mqtt; // non-owning pointer
    
    // Dashboard badge showing connection status
    // Settings card for configuration
    // Component detail for diagnostics
};
```

### 5.2 UI Contexts

#### 5.2.1 Header Badge - `mqtt_status`
- **Location:** Dashboard header
- **Icon:** `dc-mqtt` (SVG sprite - three dots representing message broker)
- **States:**
  - 🔴 Disconnected (red)
  - 🟡 Connecting (yellow)
  - 🟢 Connected (green)
  - 🔴 Error (red)
- **Fields:**
  - `state`: Display (Connected/Disconnected)
- **Real-time:** 2s update interval
- **Note:** All icons use local SVG sprites embedded in HTML, no external dependencies

#### 5.2.2 Settings Card - `mqtt_settings`
- **Location:** Settings tab
- **Title:** "MQTT Configuration"
- **Fields:**
  - `enabled`: Boolean toggle - "MQTT Enabled"
  - `broker`: Text input - "Broker Address"
  - `port`: Number input - "Port" (1-65535)
  - `username`: Text input - "Username" (optional)
  - `password`: Password input - "Password" (optional)
  - `client_id`: Text input - "Client ID" (auto-generated if empty)
  - `use_tls`: Boolean toggle - "Use TLS/SSL"
  - `clean_session`: Boolean toggle - "Clean Session"
  - `lwt_enabled`: Boolean toggle - "Last Will Enabled"
  - `lwt_topic`: Text input - "LWT Topic"
  - `lwt_message`: Text input - "LWT Message"
- **Actions:**
  - `connect`: Button - "Connect Now"
  - `disconnect`: Button - "Disconnect"
  - `test`: Button - "Test Connection"

#### 5.2.3 Component Detail - `mqtt_detail`
- **Location:** Components tab
- **Title:** "MQTT Client"
- **Sections:**
  1. **Connection Info**
     - Broker address
     - Connection state
     - Uptime
     - Client ID
  
  2. **Statistics** (Chart + Values)
     - Messages published (chart)
     - Messages received (chart)
     - Reconnection count
     - Error count
  
  3. **Active Subscriptions** (Table)
     - Topic filter
     - QoS level
     - Message count
  
  4. **Publish Test**
     - Topic input
     - Payload input (JSON editor)
     - QoS selector
     - Retain checkbox
     - "Publish" button

---

## 6. Event Integration

### 6.1 Published Events

**Topic:** `mqtt.status`
- `state`: String (disconnected, connecting, connected, error)
- `broker`: String
- `uptime`: Number (seconds)

**Topic:** `mqtt.message.received`
- `topic`: String
- `payload`: String
- `qos`: Number
- `retain`: Boolean

**Topic:** `mqtt.message.published`
- `topic`: String
- `qos`: Number
- `success`: Boolean

**Topic:** `mqtt.error`
- `code`: Number
- `message`: String
- `context`: String

### 6.2 Subscribed Events

**Topic:** `system.ready`
- Auto-connect to broker if enabled

**Topic:** `wifi.connected`
- Attempt MQTT connection (if auto-connect enabled)

**Topic:** `wifi.disconnected`
- Gracefully disconnect MQTT

---

## 7. Dependencies

### 7.1 Required Components
- **DomoticsCore-Core** (>= 0.2.5)
  - IComponent interface
  - ComponentRegistry
  - Preferences API

### 7.2 Required Libraries
- **PubSubClient** @ ^2.8 (MQTT protocol implementation)
- **ArduinoJson** @ ^7.0 (JSON parsing)
- **WiFi** @ 2.0.0 (network connectivity)
- **WiFiClientSecure** @ 2.0.0 (TLS support, optional)

### 7.3 Optional Components
- **DomoticsCore-WebUI** (for web interface)
- **DomoticsCore-Wifi** (for WiFi management)

---

## 8. Usage Examples

### 8.1 Basic Setup
```cpp
#include <DomoticsCore/Core.h>
#include <DomoticsCore/MQTT.h>

MQTTConfig mqttConfig;
mqttConfig.broker = "mqtt.example.com";
mqttConfig.port = 1883;
mqttConfig.username = "user";
mqttConfig.password = "pass";
mqttConfig.clientId = "esp32-device-01";

core.addComponent(std::make_unique<MQTTComponent>(mqttConfig));
```

### 8.2 Publishing Messages
```cpp
auto* mqtt = core.getComponent<MQTTComponent>("MQTT");

// Simple publish
mqtt->publish("home/temperature", "23.5", 0, false);

// JSON publish
JsonDocument doc;
doc["temp"] = 23.5;
doc["humidity"] = 65.2;
mqtt->publishJSON("home/sensors", doc, 1, true);
```

### 8.3 Subscribing to Topics
```cpp
auto* mqtt = core.getComponent<MQTTComponent>("MQTT");

// Subscribe with callback
mqtt->subscribe("home/commands/#", 1);
mqtt->onMessage("home/commands/led", [](const String& topic, const String& payload) {
    if (payload == "on") {
        digitalWrite(LED_PIN, HIGH);
    } else {
        digitalWrite(LED_PIN, LOW);
    }
});

// Wildcard subscription
mqtt->onMessage("home/sensors/+/temperature", [](const String& topic, const String& payload) {
    float temp = payload.toFloat();
    DLOG_I(LOG_CORE, "[App] Temperature from %s: %.1f", topic.c_str(), temp);
});
```

### 8.4 With WebUI
```cpp
#include <DomoticsCore/MQTTWebUI.h>

auto* webui = core.getComponent<WebUIComponent>("WebUI");
auto* mqtt = core.getComponent<MQTTComponent>("MQTT");

if (webui && mqtt) {
    webui->registerProviderWithComponent(new MQTTWebUI(mqtt), mqtt);
}
```

---

## 9. Error Handling

### 9.1 Error Codes
```cpp
enum class MQTTError {
    None = 0,
    ConnectionRefused = -1,
    ConnectionTimeout = -2,
    NetworkError = -3,
    ProtocolError = -4,
    InvalidTopic = -5,
    PayloadTooLarge = -6,
    QueueFull = -7,
    SubscriptionLimitReached = -8,
    NotConnected = -9,
    TLSError = -10
};
```

### 9.2 Error Handling Strategy
- Log errors with severity levels
- Publish error events to EventBus
- Update WebUI with error messages
- Auto-recovery for network errors
- User notification for configuration errors

---

## 10. Security Considerations

### 10.1 Credentials Storage
- ✅ Passwords stored encrypted in Preferences
- ✅ Never log passwords in plaintext
- ✅ WebUI password field masked
- ✅ Optional certificate validation for TLS

### 10.2 Topic Security
- ✅ Validate topic strings before publish/subscribe
- ✅ Sanitize user input from WebUI
- ✅ Limit topic length (max 256 chars)
- ✅ Block system topics (e.g., `$SYS/#`)

### 10.3 Network Security
- ✅ TLS/SSL support for encrypted communication
- ✅ Certificate pinning option
- ✅ Timeout protection against hanging connections

---

## 11. Performance Requirements

### 11.1 Memory Usage
- **RAM:** < 8KB (component instance)
- **Flash:** < 30KB (code)
- **Heap:** < 16KB (runtime, including message queues)

### 11.2 Timing
- **Connect timeout:** 10 seconds
- **Publish latency:** < 100ms (QoS 0)
- **Message processing:** < 50ms per message
- **Loop iteration:** < 10ms

### 11.3 Throughput
- **Publish rate:** Up to 100 messages/second (QoS 0)
- **Receive rate:** Up to 200 messages/second
- **Queue capacity:** 100 messages (configurable)

---

## 12. Testing Requirements

### 12.1 Unit Tests
- Connection state machine
- Topic validation and matching
- Message queue operations
- Subscription management
- Error handling

### 12.2 Integration Tests
- Connect to public MQTT broker
- Publish/subscribe round-trip
- Reconnection after network drop
- TLS connection
- WebUI configuration changes

### 12.3 Examples
- **MQTTNoWebUI:** Minimal setup, publish sensor data
- **MQTTWithWebUI:** Full setup with web configuration
- **MQTTCommands:** Subscribe to command topics (future)
- **MQTTHomeAssistant:** Home Assistant MQTT discovery integration (future)

---

## 13. Documentation Requirements

### 13.1 API Documentation
- Doxygen comments for all public methods
- Code examples in header comments
- Parameter descriptions and return values

### 13.2 User Guide (README.md)
- Quick start guide
- Configuration options
- Common usage patterns
- Troubleshooting section
- FAQ

### 13.3 Architecture Document
- Component design overview
- State machine diagrams
- Sequence diagrams for key operations
- WebUI integration pattern

---

## 14. Future Enhancements

### Phase 2
- **MQTT v5** protocol support
- **Message compression** (gzip)
- **Offline message persistence** (SPIFFS/LittleFS)
- **Multi-broker** support (failover)

### Phase 3
- **Home Assistant discovery** auto-configuration
- **Topic templates** with variable substitution
- **Message transformation** rules
- **Custom authentication** mechanisms

---

## 15. Success Criteria

✅ Component compiles without warnings  
✅ Connects to public MQTT brokers (Eclipse Mosquitto, HiveMQ)  
✅ All unit tests pass  
✅ Example applications run successfully  
✅ WebUI configuration works end-to-end  
✅ Documentation complete and accurate  
✅ Memory usage within targets  
✅ No memory leaks detected  
✅ Stable operation for 24+ hours  

---

## 16. Timeline Estimate

- **Phase 1 - Core Component:** 3 days
  - MQTT client wrapper
  - Connection management
  - Pub/sub functionality
  
- **Phase 2 - WebUI Provider:** 2 days
  - UI contexts
  - Configuration interface
  - Statistics display
  
- **Phase 3 - Testing & Examples:** 2 days
  - Unit tests
  - Example applications
  - Integration testing
  
- **Phase 4 - Documentation:** 1 day
  - API docs
  - README
  - User guide

**Total:** ~8 days

---

**Version:** 1.0  
**Date:** 2025-10-01  
**Author:** DomoticsCore Development Team
