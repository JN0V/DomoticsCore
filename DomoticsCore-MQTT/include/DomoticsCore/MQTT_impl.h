// MQTT Component Inline Implementations
// This file is included at the end of MQTT.h

namespace DomoticsCore {
namespace Components {

// Static members
MQTTComponent* MQTTComponent::instance = nullptr;

// Constructor
inline MQTTComponent::MQTTComponent(const MQTTConfig& cfg)
    : config(cfg)
    , mqttClient(config.useTLS ? (Client&)wifiClientSecure : (Client&)wifiClient)
    , state(MQTTState::Disconnected)
    , reconnectTimer(cfg.reconnectDelay)
    , stateChangeTime(0)
    , lastPublishTime(0)
    , publishCountThisSecond(0)
{
    instance = this;
    
    // Generate client ID if not provided
    if (config.clientId.isEmpty()) {
        config.clientId = generateClientId();
    }
    
    // Set default LWT topic if not provided
    if (config.enableLWT && config.lwtTopic.isEmpty()) {
        config.lwtTopic = config.clientId + "/status";
    }
    
    // Initialize metadata
    metadata.name = "MQTT";
    metadata.version = "0.1.0";
    metadata.author = "DomoticsCore";
    metadata.description = "MQTT client with auto-reconnection";
}

// Destructor
inline MQTTComponent::~MQTTComponent() {
    if (isConnected()) {
        disconnect();
    }
    instance = nullptr;
}

// Lifecycle methods
inline ComponentStatus MQTTComponent::begin() {
    DLOG_I(LOG_MQTT, "Initializing");
    
    loadConfiguration();
    
    // Auto-disable if no broker configured (similar to WiFi auto-switching to AP)
    if (config.broker.isEmpty()) {
        config.enabled = false;
        DLOG_W(LOG_MQTT, "No broker configured - component disabled (can be configured via WebUI)");
        return ComponentStatus::Success;  // Success but inactive
    }
    
    if (!config.enabled) {
        DLOG_I(LOG_MQTT, "Component disabled in configuration");
        return ComponentStatus::Success;  // Success but inactive
    }
    
    mqttClient.setServer(config.broker.c_str(), config.port);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(config.keepAlive);
    mqttClient.setBufferSize(1024);  // Increase buffer for large payloads (HA discovery)
    
    if (config.autoReconnect) {
        connect();
    }
    
    DLOG_I(LOG_MQTT, "Initialized with broker %s:%d, client ID: %s", 
           config.broker.c_str(), config.port, config.clientId.c_str());
    
    return ComponentStatus::Success;
}

inline void MQTTComponent::loop() {
    if (!config.enabled || config.broker.isEmpty()) return;
    
    if (isConnected()) {
        mqttClient.loop();
        updateStatistics();
        processMessageQueue();
    } else if (config.autoReconnect) {
        handleReconnection();
    }
}

inline ComponentStatus MQTTComponent::shutdown() {
    DLOG_I(LOG_MQTT, "Shutting down");
    
    if (isConnected()) {
        disconnect();
    }
    
    return ComponentStatus::Success;
}

// Connection management
inline bool MQTTComponent::connect() {
    if (isConnected()) {
        DLOG_W(LOG_MQTT, "Already connected");
        return true;
    }
    
    if (config.broker.isEmpty()) {
        lastError = "No broker configured";
        return false;
    }
    
    state = MQTTState::Connecting;
    stateChangeTime = millis();
    
    bool success = connectInternal();
    
    if (success) {
        state = MQTTState::Connected;
        stateChangeTime = millis();
        reconnectTimer.setInterval(config.reconnectDelay);  // Reset to initial delay
        stats.connectCount++;
        stats.reconnectCount = 0;  // Reset failure counter on successful connection
        
        DLOG_I(LOG_MQTT, "Connected to %s:%d", config.broker.c_str(), config.port);
        
        // Resubscribe to all topics
        for (const auto& sub : subscriptions) {
            mqttClient.subscribe(sub.topic.c_str(), sub.qos);
        }
        
        // Call connect callbacks
        for (const auto& callback : connectCallbacks) {
            callback();
        }
    } else {
        state = MQTTState::Error;
        stateChangeTime = millis();
        lastError = "Connection failed";
        DLOG_E(LOG_MQTT, "Connection failed");
    }
    
    return success;
}

inline void MQTTComponent::disconnect() {
    if (!isConnected()) return;
    
    mqttClient.disconnect();
    state = MQTTState::Disconnected;
    stateChangeTime = millis();
    
    DLOG_I(LOG_MQTT, "Disconnected from broker");
    
    // Call disconnect callbacks
    for (const auto& callback : disconnectCallbacks) {
        callback();
    }
}

inline void MQTTComponent::resetReconnect() {
    stats.reconnectCount = 0;
    reconnectTimer.setInterval(config.reconnectDelay);
    reconnectTimer.enable();
    reconnectTimer.reset();
    state = MQTTState::Disconnected;
    lastError = "";
    DLOG_I(LOG_MQTT, "Reconnection reset - auto-retry re-enabled");
}

inline String MQTTComponent::getStateString() const {
    switch (state) {
        case MQTTState::Disconnected: return "Disconnected";
        case MQTTState::Connecting: return "Connecting";
        case MQTTState::Connected: return "Connected";
        case MQTTState::Error: return "Error";
        default: return "Unknown";
    }
}

// Publishing
inline bool MQTTComponent::publish(const String& topic, const String& payload, uint8_t qos, bool retain) {
    if (!isConnected()) {
        // Queue message for later if offline
        messageQueue.push_back({topic, payload, qos, retain});
        return true;
    }
    
    bool success = mqttClient.publish(topic.c_str(), payload.c_str(), retain);
    
    if (success) {
        stats.publishCount++;
        publishCountThisSecond++;
    } else {
        stats.publishErrors++;
    }
    
    return success;
}

inline bool MQTTComponent::publishJSON(const String& topic, const JsonDocument& doc, uint8_t qos, bool retain) {
    String payload;
    serializeJson(doc, payload);
    return publish(topic, payload, qos, retain);
}

inline bool MQTTComponent::publishBinary(const String& topic, const uint8_t* data, size_t length, uint8_t qos, bool retain) {
    if (!isConnected()) return false;
    
    bool success = mqttClient.publish(topic.c_str(), data, length, retain);
    
    if (success) {
        stats.publishCount++;
    } else {
        stats.publishErrors++;
    }
    
    return success;
}

// Subscribing
inline bool MQTTComponent::subscribe(const String& topic, uint8_t qos) {
    // Check if already subscribed
    for (const auto& sub : subscriptions) {
        if (sub.topic == topic) {
            return true;
        }
    }
    
    if (!isConnected()) {
        subscriptions.push_back({topic, qos});
        stats.subscriptionCount = subscriptions.size();
        return true;
    }
    
    bool success = mqttClient.subscribe(topic.c_str(), qos);
    
    if (success) {
        subscriptions.push_back({topic, qos});
        stats.subscriptionCount = subscriptions.size();
        DLOG_I(LOG_MQTT, "Subscribed to: %s (QoS %d)", topic.c_str(), qos);
    }
    
    return success;
}

inline bool MQTTComponent::unsubscribe(const String& topic) {
    bool success = mqttClient.unsubscribe(topic.c_str());
    
    if (success) {
        auto it = subscriptions.begin();
        while (it != subscriptions.end()) {
            if (it->topic == topic) {
                it = subscriptions.erase(it);
                break;
            } else {
                ++it;
            }
        }
        stats.subscriptionCount = subscriptions.size();
        DLOG_I(LOG_MQTT, "Unsubscribed from: %s", topic.c_str());
    }
    
    return success;
}

inline void MQTTComponent::unsubscribeAll() {
    for (const auto& sub : subscriptions) {
        mqttClient.unsubscribe(sub.topic.c_str());
    }
    subscriptions.clear();
    stats.subscriptionCount = 0;
}

inline std::vector<String> MQTTComponent::getActiveSubscriptions() const {
    std::vector<String> result;
    for (const auto& sub : subscriptions) {
        result.push_back(sub.topic);
    }
    return result;
}

// Callbacks
inline bool MQTTComponent::onMessage(const String& topicFilter, MessageCallback callback) {
    CallbackEntry entry;
    entry.topicFilter = topicFilter;
    entry.callback = callback;
    messageCallbacks.push_back(entry);
    return true;
}

inline bool MQTTComponent::onConnect(ConnectionCallback callback) {
    connectCallbacks.push_back(callback);
    return true;
}

inline bool MQTTComponent::onDisconnect(ConnectionCallback callback) {
    disconnectCallbacks.push_back(callback);
    return true;
}

// Configuration
inline void MQTTComponent::setConfig(const MQTTConfig& cfg) {
    config = cfg;
    // Ensure PubSubClient stores a valid pointer to the current broker string.
    // PubSubClient retains the const char* pointer passed to setServer without copying,
    // so if our String storage changes (e.g., via WebUI updates), the old pointer becomes invalid.
    if (!config.broker.isEmpty()) {
        mqttClient.setServer(config.broker.c_str(), config.port);
    }
    saveConfiguration();
}

inline void MQTTComponent::setBroker(const String& broker, uint16_t port) {
    config.broker = broker;
    config.port = port;
    mqttClient.setServer(broker.c_str(), port);
    saveConfiguration();
}

inline void MQTTComponent::setCredentials(const String& username, const String& password) {
    config.username = username;
    config.password = password;
    saveConfiguration();
}

// Note: getName() already defined inline in MQTT.h

// Private methods
inline bool MQTTComponent::connectInternal() {
    bool success = false;
    // Defensive: refresh server pointer before attempting connection in case config changed recently
    if (!config.broker.isEmpty()) {
        mqttClient.setServer(config.broker.c_str(), config.port);
    }
    
    if (config.enableLWT) {
        if (!config.username.isEmpty()) {
            success = mqttClient.connect(
                config.clientId.c_str(),
                config.username.c_str(),
                config.password.c_str(),
                config.lwtTopic.c_str(),
                config.lwtQoS,
                config.lwtRetain,
                config.lwtMessage.c_str(),
                config.cleanSession
            );
        } else {
            success = mqttClient.connect(
                config.clientId.c_str(),
                config.lwtTopic.c_str(),
                config.lwtQoS,
                config.lwtRetain,
                config.lwtMessage.c_str()
            );
        }
    } else {
        if (!config.username.isEmpty()) {
            success = mqttClient.connect(
                config.clientId.c_str(),
                config.username.c_str(),
                config.password.c_str()
            );
        } else {
            success = mqttClient.connect(config.clientId.c_str());
        }
    }
    
    return success;
}

inline void MQTTComponent::handleReconnection() {
    if (state == MQTTState::Connecting) return;
    
    // Use NonBlockingDelay timer - check if reconnect interval has elapsed
    if (!reconnectTimer.isReady()) {
        return;
    }
    
    // Exponential backoff with max delay
    unsigned long currentDelay = reconnectTimer.getInterval();
    if (currentDelay < config.maxReconnectDelay) {
        unsigned long newDelay = currentDelay * 2;
        if (newDelay > config.maxReconnectDelay) {
            newDelay = config.maxReconnectDelay;
        }
        reconnectTimer.setInterval(newDelay);
    }
    
    DLOG_I(LOG_MQTT, "Attempting reconnection (delay: %lu ms)", reconnectTimer.getInterval());
    stats.reconnectCount++;
    connect();
}

inline void MQTTComponent::processMessageQueue() {
    if (messageQueue.empty()) return;
    
    auto it = messageQueue.begin();
    while (it != messageQueue.end() && isConnected()) {
        if (publish(it->topic, it->payload, it->qos, it->retain)) {
            it = messageQueue.erase(it);
        } else {
            break;
        }
    }
}

inline void MQTTComponent::handleIncomingMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }
    
    stats.receiveCount++;
    
    // Match against registered callbacks
    for (const auto& cb : messageCallbacks) {
        if (topicMatches(cb.topicFilter, topicStr)) {
            cb.callback(topicStr, payloadStr);
        }
    }
}

inline void MQTTComponent::updateStatistics() {
    if (isConnected()) {
        stats.uptime = (millis() - stateChangeTime) / 1000;
    }
}

inline void MQTTComponent::loadConfiguration() {
    Preferences prefs;
    if (prefs.begin("mqtt", true)) {
        config.enabled = prefs.getBool("enabled", config.enabled);
        config.broker = prefs.getString("broker", config.broker);
        config.port = prefs.getUShort("port", config.port);
        config.username = prefs.getString("username", config.username);
        config.password = prefs.getString("password", config.password);
        prefs.end();
    }
}

inline void MQTTComponent::saveConfiguration() {
    Preferences prefs;
    if (prefs.begin("mqtt", false)) {
        prefs.putBool("enabled", config.enabled);
        prefs.putString("broker", config.broker);
        prefs.putUShort("port", config.port);
        prefs.putString("username", config.username);
        prefs.putString("password", config.password);
        prefs.end();
    }
}

inline String MQTTComponent::generateClientId() {
    uint64_t mac = ESP.getEfuseMac();
    char clientId[32];
    snprintf(clientId, sizeof(clientId), "esp32-%04x%08x", 
             (uint16_t)(mac >> 32), (uint32_t)mac);
    return String(clientId);
}

// Static callback
inline void MQTTComponent::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (instance) {
        instance->handleIncomingMessage(topic, payload, length);
    }
}

// Topic matching with wildcards (static)
inline bool MQTTComponent::topicMatches(const String& filter, const String& topic) {
    if (filter == topic) return true;
    if (filter == "#") return true;
    
    std::vector<String> filterParts;
    std::vector<String> topicParts;
    
    int start = 0;
    int end = filter.indexOf('/');
    while (end >= 0) {
        filterParts.push_back(filter.substring(start, end));
        start = end + 1;
        end = filter.indexOf('/', start);
    }
    filterParts.push_back(filter.substring(start));
    
    start = 0;
    end = topic.indexOf('/');
    while (end >= 0) {
        topicParts.push_back(topic.substring(start, end));
        start = end + 1;
        end = topic.indexOf('/', start);
    }
    topicParts.push_back(topic.substring(start));
    
    size_t fi = 0, ti = 0;
    while (fi < filterParts.size() && ti < topicParts.size()) {
        if (filterParts[fi] == "#") {
            return true;
        }
        if (filterParts[fi] != "+" && filterParts[fi] != topicParts[ti]) {
            return false;
        }
        fi++;
        ti++;
    }
    
    return fi == filterParts.size() && ti == topicParts.size();
}

} // namespace Components
} // namespace DomoticsCore
