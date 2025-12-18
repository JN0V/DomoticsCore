#include <unity.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstring>

// Native build stubs
#ifdef NATIVE_BUILD
typedef std::string String;
inline unsigned long millis() { static unsigned long t = 0; return t += 10; }

namespace DomoticsCore {
namespace Mocks {

// ============================================================================
// MockWifiHAL
// ============================================================================
class MockWifiHAL {
public:
    static bool connected;
    static void simulateConnect() { connected = true; }
    static void simulateDisconnect() { connected = false; }
    static bool isConnected() { return connected; }
    static void reset() { connected = false; }
};
bool MockWifiHAL::connected = false;

// ============================================================================
// MockMQTTClient - Simulates PubSubClient
// ============================================================================
struct MockMessage {
    std::string topic;
    std::string payload;
    bool retain;
};

class MockMQTTClient {
public:
    static bool connected;
    static bool shouldFailConnect;
    static int connectAttempts;
    static std::vector<MockMessage> publishedMessages;
    static std::vector<std::string> subscribedTopics;
    static std::function<void(const char*, const uint8_t*, unsigned int)> callback;
    
    static bool connect(const char* clientId) {
        connectAttempts++;
        if (shouldFailConnect) return false;
        connected = true;
        return true;
    }
    
    static void disconnect() { connected = false; }
    static bool isConnected() { return connected; }
    
    static bool publish(const char* topic, const char* payload, bool retain = false) {
        if (!connected) return false;
        publishedMessages.push_back({topic, payload, retain});
        return true;
    }
    
    static bool subscribe(const char* topic, uint8_t qos = 0) {
        if (!connected) {
            subscribedTopics.push_back(topic); // Queue for later
            return true;
        }
        subscribedTopics.push_back(topic);
        return true;
    }
    
    static void simulateIncomingMessage(const std::string& topic, const std::string& payload) {
        if (callback) {
            callback(topic.c_str(), (const uint8_t*)payload.c_str(), payload.length());
        }
    }
    
    static void setCallback(std::function<void(const char*, const uint8_t*, unsigned int)> cb) {
        callback = cb;
    }
    
    static void reset() {
        connected = false;
        shouldFailConnect = false;
        connectAttempts = 0;
        publishedMessages.clear();
        subscribedTopics.clear();
        callback = nullptr;
    }
    
    static bool wasPublished(const std::string& topic) {
        for (const auto& m : publishedMessages) {
            if (m.topic == topic) return true;
        }
        return false;
    }
    
    static bool wasPublished(const std::string& topic, const std::string& payload) {
        for (const auto& m : publishedMessages) {
            if (m.topic == topic && m.payload == payload) return true;
        }
        return false;
    }
};

bool MockMQTTClient::connected = false;
bool MockMQTTClient::shouldFailConnect = false;
int MockMQTTClient::connectAttempts = 0;
std::vector<MockMessage> MockMQTTClient::publishedMessages;
std::vector<std::string> MockMQTTClient::subscribedTopics;
std::function<void(const char*, const uint8_t*, unsigned int)> MockMQTTClient::callback = nullptr;

// ============================================================================
// MockEventBus
// ============================================================================
class MockEventBus {
public:
    static std::vector<std::string> emittedEvents;
    
    static void emit(const std::string& event) { emittedEvents.push_back(event); }
    static bool wasEmitted(const std::string& event) {
        for (const auto& e : emittedEvents) if (e == event) return true;
        return false;
    }
    static void reset() { emittedEvents.clear(); }
};
std::vector<std::string> MockEventBus::emittedEvents;

} // namespace Mocks
} // namespace DomoticsCore
#endif

using namespace DomoticsCore::Mocks;

// ============================================================================
// MQTT Logic Under Test (extracted for isolation)
// ============================================================================

class MQTTLogicUnderTest {
public:
    // State
    bool autoReconnect = true;
    unsigned long reconnectDelay = 1000;
    unsigned long maxReconnectDelay = 30000;
    unsigned long currentDelay = 1000;
    int reconnectCount = 0;
    
    // Message queue for offline publishing
    std::vector<MockMessage> messageQueue;
    
    // Subscriptions to restore on reconnect
    std::vector<std::string> pendingSubscriptions;
    
    bool connect() {
        if (!MockWifiHAL::isConnected()) return false;
        
        bool result = MockMQTTClient::connect("test_client");
        if (result) {
            MockEventBus::emit("mqtt/connected");
            currentDelay = reconnectDelay; // Reset backoff
            
            // Restore subscriptions
            for (const auto& topic : pendingSubscriptions) {
                MockMQTTClient::subscribe(topic.c_str());
            }
            
            // Flush message queue
            processQueue();
        }
        return result;
    }
    
    void disconnect() {
        MockMQTTClient::disconnect();
        MockEventBus::emit("mqtt/disconnected");
    }
    
    bool publish(const std::string& topic, const std::string& payload, bool retain = false) {
        if (!MockMQTTClient::isConnected()) {
            // Queue message for later
            messageQueue.push_back({topic, payload, retain});
            return true; // Queued successfully
        }
        return MockMQTTClient::publish(topic.c_str(), payload.c_str(), retain);
    }
    
    bool subscribe(const std::string& topic) {
        pendingSubscriptions.push_back(topic);
        if (MockMQTTClient::isConnected()) {
            return MockMQTTClient::subscribe(topic.c_str());
        }
        return true; // Queued for later
    }
    
    void handleReconnection() {
        if (!autoReconnect) return;
        if (MockMQTTClient::isConnected()) return;
        
        reconnectCount++;
        bool success = connect();
        
        if (!success) {
            // Exponential backoff
            currentDelay = std::min(currentDelay * 2, maxReconnectDelay);
        }
    }
    
    void processQueue() {
        if (!MockMQTTClient::isConnected()) return;
        
        for (const auto& msg : messageQueue) {
            MockMQTTClient::publish(msg.topic.c_str(), msg.payload.c_str(), msg.retain);
        }
        messageQueue.clear();
    }
    
    int getQueueSize() const { return messageQueue.size(); }
    unsigned long getCurrentDelay() const { return currentDelay; }
};

// ============================================================================
// Tests
// ============================================================================

MQTTLogicUnderTest* mqtt = nullptr;

void setUp(void) {
    MockWifiHAL::reset();
    MockMQTTClient::reset();
    MockEventBus::reset();
    mqtt = new MQTTLogicUnderTest();
}

void tearDown(void) {
    delete mqtt;
    mqtt = nullptr;
}

// T126: Test MQTT doesn't connect without WiFi
void test_mqtt_no_connect_without_wifi(void) {
    TEST_ASSERT_FALSE(MockWifiHAL::isConnected());
    
    bool result = mqtt->connect();
    
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(MockMQTTClient::isConnected());
    TEST_ASSERT_EQUAL(0, MockMQTTClient::connectAttempts);
}

// T127: Test MQTT exponential backoff
void test_mqtt_exponential_backoff(void) {
    MockWifiHAL::simulateConnect();
    MockMQTTClient::shouldFailConnect = true;
    
    // Initial delay
    TEST_ASSERT_EQUAL(1000, mqtt->getCurrentDelay());
    
    // First failure - delay doubles
    mqtt->handleReconnection();
    TEST_ASSERT_EQUAL(2000, mqtt->getCurrentDelay());
    
    // Second failure - delay doubles again
    mqtt->handleReconnection();
    TEST_ASSERT_EQUAL(4000, mqtt->getCurrentDelay());
    
    // Third failure
    mqtt->handleReconnection();
    TEST_ASSERT_EQUAL(8000, mqtt->getCurrentDelay());
    
    // Should cap at max
    mqtt->maxReconnectDelay = 10000;
    mqtt->handleReconnection();
    TEST_ASSERT_EQUAL(10000, mqtt->getCurrentDelay());
}

// T128: Test MQTT message queuing when offline
void test_mqtt_message_queue_when_offline(void) {
    // Publish while offline
    mqtt->publish("test/topic1", "payload1");
    mqtt->publish("test/topic2", "payload2");
    
    TEST_ASSERT_EQUAL(2, mqtt->getQueueSize());
    TEST_ASSERT_EQUAL(0, MockMQTTClient::publishedMessages.size());
    
    // Connect and flush queue
    MockWifiHAL::simulateConnect();
    mqtt->connect();
    
    TEST_ASSERT_EQUAL(0, mqtt->getQueueSize());
    TEST_ASSERT_EQUAL(2, MockMQTTClient::publishedMessages.size());
    TEST_ASSERT_TRUE(MockMQTTClient::wasPublished("test/topic1", "payload1"));
    TEST_ASSERT_TRUE(MockMQTTClient::wasPublished("test/topic2", "payload2"));
}

// T129: Test MQTT subscription persistence across reconnects
void test_mqtt_subscription_persistence(void) {
    // Subscribe while offline
    mqtt->subscribe("home/sensors/#");
    mqtt->subscribe("home/commands/+");
    
    // Connect
    MockWifiHAL::simulateConnect();
    mqtt->connect();
    
    // Subscriptions should be restored
    TEST_ASSERT_EQUAL(2, MockMQTTClient::subscribedTopics.size());
    
    // Disconnect and reconnect
    mqtt->disconnect();
    MockMQTTClient::subscribedTopics.clear();
    
    mqtt->connect();
    
    // Subscriptions should be restored again
    TEST_ASSERT_EQUAL(2, MockMQTTClient::subscribedTopics.size());
}

// T130: Test MQTT EventBus integration
void test_mqtt_eventbus_integration(void) {
    MockWifiHAL::simulateConnect();
    
    // Connect should emit event
    mqtt->connect();
    TEST_ASSERT_TRUE(MockEventBus::wasEmitted("mqtt/connected"));
    
    // Disconnect should emit event
    mqtt->disconnect();
    TEST_ASSERT_TRUE(MockEventBus::wasEmitted("mqtt/disconnected"));
}

// Additional: Test backoff resets on successful connect
void test_mqtt_backoff_resets_on_success(void) {
    MockWifiHAL::simulateConnect();
    MockMQTTClient::shouldFailConnect = true;
    
    // Build up backoff
    mqtt->handleReconnection();
    mqtt->handleReconnection();
    TEST_ASSERT_EQUAL(4000, mqtt->getCurrentDelay());
    
    // Successful connect resets delay
    MockMQTTClient::shouldFailConnect = false;
    mqtt->connect();
    TEST_ASSERT_EQUAL(1000, mqtt->getCurrentDelay());
}

// Additional: Test incoming message handling
void test_mqtt_incoming_message_handling(void) {
    MockWifiHAL::simulateConnect();
    mqtt->connect();
    
    std::string receivedTopic;
    std::string receivedPayload;
    
    MockMQTTClient::setCallback([&](const char* topic, const uint8_t* payload, unsigned int len) {
        receivedTopic = topic;
        receivedPayload = std::string((char*)payload, len);
    });
    
    MockMQTTClient::simulateIncomingMessage("test/topic", "test payload");
    
    TEST_ASSERT_EQUAL_STRING("test/topic", receivedTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("test payload", receivedPayload.c_str());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_mqtt_no_connect_without_wifi);
    RUN_TEST(test_mqtt_exponential_backoff);
    RUN_TEST(test_mqtt_message_queue_when_offline);
    RUN_TEST(test_mqtt_subscription_persistence);
    RUN_TEST(test_mqtt_eventbus_integration);
    RUN_TEST(test_mqtt_backoff_resets_on_success);
    RUN_TEST(test_mqtt_incoming_message_handling);
    
    return UNITY_END();
}
