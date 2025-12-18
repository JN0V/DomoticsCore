#pragma once

/**
 * @file MockMQTTClient.h
 * @brief Mock MQTT client for isolated unit testing without real broker
 * 
 * This mock replaces PubSubClient to allow testing HomeAssistant and other
 * MQTT-dependent components without requiring a real MQTT broker.
 */

#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>

namespace DomoticsCore {
namespace Mocks {

/**
 * @brief Recorded MQTT message for test verification
 */
struct MockMQTTMessage {
    String topic;
    String payload;
    bool retain;
    uint8_t qos;
};

/**
 * @brief Mock MQTT client that records publish/subscribe calls
 */
class MockMQTTClient {
public:
    // Simulated connection state
    static bool connected;
    static String broker;
    static uint16_t port;
    
    // Recorded operations for test verification
    static std::vector<MockMQTTMessage> publishedMessages;
    static std::vector<String> subscribedTopics;
    
    // Incoming message simulation
    static std::function<void(const char*, const uint8_t*, unsigned int)> messageCallback;
    
    // Connection simulation
    static bool connect(const char* clientId) {
        connected = true;
        return true;
    }
    
    static bool connect(const char* clientId, const char* user, const char* pass) {
        connected = true;
        return true;
    }
    
    static void disconnect() {
        connected = false;
    }
    
    static bool isConnected() { return connected; }
    
    // Publishing
    static bool publish(const char* topic, const char* payload, bool retain = false) {
        if (!connected) return false;
        publishedMessages.push_back({String(topic), String(payload), retain, 0});
        return true;
    }
    
    static bool publish(const char* topic, const uint8_t* payload, unsigned int length, bool retain = false) {
        if (!connected) return false;
        String payloadStr;
        payloadStr.reserve(length);
        for (unsigned int i = 0; i < length; i++) {
            payloadStr += (char)payload[i];
        }
        publishedMessages.push_back({String(topic), payloadStr, retain, 0});
        return true;
    }
    
    // Subscribing
    static bool subscribe(const char* topic, uint8_t qos = 0) {
        if (!connected) return false;
        subscribedTopics.push_back(String(topic));
        return true;
    }
    
    static bool unsubscribe(const char* topic) {
        auto it = std::find(subscribedTopics.begin(), subscribedTopics.end(), String(topic));
        if (it != subscribedTopics.end()) {
            subscribedTopics.erase(it);
            return true;
        }
        return false;
    }
    
    // Test control methods
    static void simulateIncomingMessage(const String& topic, const String& payload) {
        if (messageCallback) {
            messageCallback(topic.c_str(), (const uint8_t*)payload.c_str(), payload.length());
        }
    }
    
    static void simulateConnect() {
        connected = true;
    }
    
    static void simulateDisconnect() {
        connected = false;
    }
    
    static void reset() {
        connected = false;
        broker = "";
        port = 1883;
        publishedMessages.clear();
        subscribedTopics.clear();
        messageCallback = nullptr;
    }
    
    // Verification helpers
    static bool wasPublished(const String& topic) {
        for (const auto& msg : publishedMessages) {
            if (msg.topic == topic) return true;
        }
        return false;
    }
    
    static bool wasPublished(const String& topic, const String& payload) {
        for (const auto& msg : publishedMessages) {
            if (msg.topic == topic && msg.payload == payload) return true;
        }
        return false;
    }
    
    static bool wasSubscribed(const String& topic) {
        return std::find(subscribedTopics.begin(), subscribedTopics.end(), topic) != subscribedTopics.end();
    }
    
    static int getPublishCount() { return publishedMessages.size(); }
    static int getSubscribeCount() { return subscribedTopics.size(); }
};

// Static member initialization
bool MockMQTTClient::connected = false;
String MockMQTTClient::broker = "";
uint16_t MockMQTTClient::port = 1883;
std::vector<MockMQTTMessage> MockMQTTClient::publishedMessages;
std::vector<String> MockMQTTClient::subscribedTopics;
std::function<void(const char*, const uint8_t*, unsigned int)> MockMQTTClient::messageCallback = nullptr;

} // namespace Mocks
} // namespace DomoticsCore
