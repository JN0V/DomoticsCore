#pragma once

#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Logger.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <functional>
#include <vector>
#include <map>

namespace DomoticsCore {
namespace Components {

/**
 * @brief MQTT client configuration
 */
struct MQTTConfig {
    // Server
    String broker = "";                     ///< MQTT broker address
    uint16_t port = 1883;                   ///< MQTT broker port (1883 for plain, 8883 for TLS)
    bool useTLS = false;                    ///< Use TLS/SSL encryption
    
    // Authentication
    String username = "";                   ///< MQTT username (optional)
    String password = "";                   ///< MQTT password (optional)
    String clientId = "";                   ///< MQTT client ID (auto-generated if empty)
    
    // Session
    bool cleanSession = true;               ///< Start with clean session
    uint16_t keepAlive = 60;                ///< Keep-alive interval in seconds
    
    // Last Will and Testament
    bool enableLWT = true;                  ///< Enable Last Will Testament
    String lwtTopic = "";                   ///< LWT topic (defaults to {clientId}/status)
    String lwtMessage = "offline";          ///< LWT message payload
    uint8_t lwtQoS = 1;                     ///< LWT QoS level (0, 1, or 2)
    bool lwtRetain = true;                  ///< Retain LWT message
    
    // Reconnection
    bool autoReconnect = true;              ///< Automatically reconnect on disconnect
    uint32_t reconnectDelay = 1000;         ///< Initial reconnection delay (ms)
    uint32_t maxReconnectDelay = 30000;     ///< Maximum reconnection delay (ms)
    
    // Publishing
    uint16_t maxQueueSize = 100;            ///< Max queued messages when offline
    uint8_t publishRateLimit = 10;          ///< Max messages per second (0 = unlimited)
    
    // Subscriptions
    uint8_t maxSubscriptions = 50;          ///< Maximum number of subscriptions
    bool resubscribeOnConnect = true;       ///< Re-subscribe after reconnection
    
    // Timeouts
    uint32_t connectTimeout = 10000;        ///< Connection timeout (ms)
    uint32_t operationTimeout = 5000;       ///< Operation timeout (ms)
    
    // Component
    bool enabled = true;                    ///< Enable MQTT component
};

/**
 * @brief MQTT connection state
 */
enum class MQTTState {
    Disconnected,   ///< Not connected
    Connecting,     ///< Connection in progress
    Connected,      ///< Connected to broker
    Error          ///< Error state
};

/**
 * @brief MQTT statistics
 */
struct MQTTStatistics {
    uint32_t connectCount = 0;          ///< Total connection attempts
    uint32_t reconnectCount = 0;        ///< Total reconnection attempts
    uint32_t publishCount = 0;          ///< Total messages published
    uint32_t publishErrors = 0;         ///< Failed publish attempts
    uint32_t receiveCount = 0;          ///< Total messages received
    uint32_t subscriptionCount = 0;     ///< Active subscriptions
    uint32_t uptime = 0;                ///< Seconds connected
    uint32_t lastLatency = 0;           ///< Last measured latency (ms)
};

/**
 * @brief MQTT Client Component
 * 
 * Provides MQTT client functionality with auto-reconnection, QoS support,
 * topic management, and message callbacks. Uses PubSubClient library for
 * protocol implementation.
 * 
 * Features:
 * - Auto-connect and auto-reconnect with exponential backoff
 * - QoS levels 0, 1, 2
 * - Wildcard subscriptions (+ and #)
 * - Message queuing for offline buffering
 * - JSON helper methods
 * - Last Will Testament support
 * - TLS/SSL support
 * - Connection statistics
 * 
 * @example BasicMQTT
 * ```cpp
 * MQTTConfig cfg;
 * cfg.broker = "mqtt.example.com";
 * cfg.username = "user";
 * cfg.password = "pass";
 * 
 * auto mqtt = std::make_unique<MQTTComponent>(cfg);
 * core.addComponent(std::move(mqtt));
 * 
 * auto* mqttPtr = core.getComponent<MQTTComponent>("MQTT");
 * mqttPtr->subscribe("home/sensors/#", 1);
 * mqttPtr->onMessage("home/sensors/+/temperature", [](const String& topic, const String& payload) {
 *     DLOG_I(LOG_APP, "[App] Temperature: %s", payload.c_str());
 * });
 * ```
 */
class MQTTComponent : public IComponent {
public:
    /**
     * @brief Message callback type
     * @param topic MQTT topic
     * @param payload Message payload as string
     */
    using MessageCallback = std::function<void(const String& topic, const String& payload)>;
    
    /**
     * @brief Connection callback type
     */
    using ConnectionCallback = std::function<void()>;

    /**
     * @brief Construct MQTT component with configuration
     * @param config MQTT configuration
     */
    explicit MQTTComponent(const MQTTConfig& config = MQTTConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~MQTTComponent();

    // ========== IComponent Interface ==========
    
    String getName() const override { return "MQTT"; }
    String getVersion() const override { return "0.1.0"; }
    std::vector<String> getDependencies() const override { return {}; }
    
    ComponentStatus begin() override;
    void loop() override;
    ComponentStatus shutdown() override;

    // ========== Connection Management ==========
    
    /**
     * @brief Connect to MQTT broker
     * @return true if connection initiated successfully
     */
    bool connect();
    
    /**
     * @brief Disconnect from MQTT broker
     */
    void disconnect();
    
    /**
     * Check if connected to broker
     * @return true if connected
     */
    bool isConnected() const {
        // PubSubClient::connected() is not const, so we need const_cast
        return const_cast<PubSubClient&>(mqttClient).connected() && state == MQTTState::Connected;
    }
    
    /**
     * @brief Get current connection state
     * @return Current state
     */
    MQTTState getState() const { return state; }
    
    /**
     * @brief Get state as string
     * @return State name
     */
    String getStateString() const;

    // ========== Publishing ==========
    
    /**
     * @brief Publish message to topic
     * @param topic MQTT topic
     * @param payload Message payload
     * @param qos Quality of Service level (0, 1, 2)
     * @param retain Retain message flag
     * @return true if published successfully
     */
    bool publish(const String& topic, const String& payload, uint8_t qos = 0, bool retain = false);
    
    /**
     * @brief Publish JSON document to topic
     * @param topic MQTT topic
     * @param doc JSON document
     * @param qos Quality of Service level
     * @param retain Retain message flag
     * @return true if published successfully
     */
    bool publishJSON(const String& topic, const JsonDocument& doc, uint8_t qos = 0, bool retain = false);
    
    /**
     * @brief Publish binary data to topic
     * @param topic MQTT topic
     * @param data Binary data pointer
     * @param length Data length
     * @param qos Quality of Service level
     * @param retain Retain message flag
     * @return true if published successfully
     */
    bool publishBinary(const String& topic, const uint8_t* data, size_t length, uint8_t qos = 0, bool retain = false);

    // ========== Subscribing ==========
    
    /**
     * @brief Subscribe to topic
     * @param topic MQTT topic (supports wildcards + and #)
     * @param qos Quality of Service level
     * @return true if subscribed successfully
     */
    bool subscribe(const String& topic, uint8_t qos = 0);
    
    /**
     * @brief Unsubscribe from topic
     * @param topic MQTT topic
     * @return true if unsubscribed successfully
     */
    bool unsubscribe(const String& topic);
    
    /**
     * @brief Unsubscribe from all topics
     */
    void unsubscribeAll();
    
    /**
     * @brief Get list of active subscriptions
     * @return Vector of subscribed topics
     */
    std::vector<String> getActiveSubscriptions() const;

    // ========== Callbacks ==========
    
    /**
     * @brief Register message callback for topic filter
     * @param topicFilter Topic filter (supports wildcards)
     * @param callback Callback function
     * @return true if registered successfully
     */
    bool onMessage(const String& topicFilter, MessageCallback callback);
    
    /**
     * @brief Register connection callback
     * @param callback Callback function called on connection
     * @return true if registered successfully
     */
    bool onConnect(ConnectionCallback callback);
    
    /**
     * @brief Register disconnection callback
     * @param callback Callback function called on disconnection
     * @return true if registered successfully
     */
    bool onDisconnect(ConnectionCallback callback);

    // ========== Configuration ==========
    
    /**
     * @brief Set configuration
     * @param cfg New configuration
     */
    void setConfig(const MQTTConfig& cfg);
    
    /**
     * Get current MQTT-specific configuration
     * @return MQTT configuration reference
     */
    const MQTTConfig& getMQTTConfig() const { return config; }
    
    /**
     * @brief Set broker address and port
     * @param broker Broker hostname or IP
     * @param port Broker port
     */
    void setBroker(const String& broker, uint16_t port = 1883);
    
    /**
     * @brief Set authentication credentials
     * @param username Username
     * @param password Password
     */
    void setCredentials(const String& username, const String& password);

    // ========== Statistics & Diagnostics ==========
    
    /**
     * @brief Get connection statistics
     * @return Statistics structure
     */
    const MQTTStatistics& getStatistics() const { return stats; }
    
    /**
     * @brief Get number of queued messages
     * @return Queue size
     */
    size_t getQueuedMessageCount() const { return messageQueue.size(); }
    
    /**
     * @brief Get last error message
     * @return Error string
     */
    String getLastError() const { return lastError; }

    // ========== Helper Functions ==========
    
    /**
     * @brief Check if topic is valid
     * @param topic Topic string
     * @param allowWildcards Allow + and # wildcards
     * @return true if valid
     */
    static bool isValidTopic(const String& topic, bool allowWildcards = false);
    
    /**
     * @brief Check if topic matches filter (with wildcards)
     * @param filter Topic filter (can contain + and #)
     * @param topic Actual topic
     * @return true if matches
     */
    static bool topicMatches(const String& filter, const String& topic);

private:
    // Configuration
    MQTTConfig config;
    
    // Network clients
    WiFiClient wifiClient;
    WiFiClientSecure wifiClientSecure;
    PubSubClient mqttClient;
    
    // State management
    MQTTState state;
    String lastError;
    unsigned long lastConnectAttempt;
    unsigned long currentReconnectDelay;
    unsigned long stateChangeTime;
    
    // Statistics
    MQTTStatistics stats;
    
    // Subscriptions
    struct Subscription {
        String topic;
        uint8_t qos;
    };
    std::vector<Subscription> subscriptions;
    
    // Message callbacks
    struct CallbackEntry {
        String topicFilter;
        MessageCallback callback;
    };
    std::vector<CallbackEntry> messageCallbacks;
    
    // Connection callbacks
    std::vector<ConnectionCallback> connectCallbacks;
    std::vector<ConnectionCallback> disconnectCallbacks;
    
    // Message queue (for offline buffering)
    struct QueuedMessage {
        String topic;
        String payload;
        uint8_t qos;
        bool retain;
    };
    std::vector<QueuedMessage> messageQueue;
    
    // Rate limiting
    unsigned long lastPublishTime;
    uint8_t publishCountThisSecond;
    
    // Internal methods
    bool connectInternal();
    void handleReconnection();
    void processMessageQueue();
    void handleIncomingMessage(char* topic, byte* payload, unsigned int length);
    void updateStatistics();
    void loadConfiguration();
    void saveConfiguration();
    String generateClientId();
    
    // Static callback for PubSubClient
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    static MQTTComponent* instance;  // For static callback
};

} // namespace Components
} // namespace DomoticsCore

// Include inline implementations
#include "MQTT_impl.h"
