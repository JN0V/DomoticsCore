#pragma once

/**
 * @file MQTT_Stub.h
 * @brief Stub implementation of MQTT HAL for native testing
 */

// Native/stub platform MQTT buffer size
// Moderate size for testing
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 1024
#endif

#include <cstdint>
#include <cstring>
#include <string>
#include <functional>

namespace DomoticsCore {
namespace HAL {
namespace MQTT {

/**
 * @brief Mock MQTT client for native tests
 *
 * Simulates MQTT broker interaction without network
 */
class MQTTClientImpl : public MQTTClient {
private:
    bool isConnected = false;
    std::string serverDomain;
    uint16_t serverPort = 0;
    uint16_t bufferSize = 256;
    uint16_t keepAliveSeconds = 15;
    int connectionState = -1;  // -1 = disconnected, 0 = connected
    std::function<void(char*, uint8_t*, unsigned int)> messageCallback;

    // Connection credentials (for testing)
    std::string clientId;
    std::string username;
    std::string password;
    std::string lwtTopic;
    std::string lwtMessage;
    uint8_t lwtQoS = 0;
    bool lwtRetain = false;

    // Statistics (for testing)
    uint32_t publishCount = 0;
    uint32_t subscribeCount = 0;
    uint32_t unsubscribeCount = 0;

public:
    MQTTClientImpl(bool useTLS = false) {
        (void)useTLS;  // Unused in stub
    }

    bool connect(const char* id,
                const char* user = nullptr,
                const char* pass = nullptr,
                const char* willTopic = nullptr,
                uint8_t willQoS_ = 0,
                bool willRetain_ = false,
                const char* willMessage_ = nullptr) override {
        // Simulate connection success if server configured
        if (serverDomain.empty() || serverPort == 0) {
            connectionState = -2;  // Connection refused
            isConnected = false;
            return false;
        }

        // Store connection info
        clientId = id ? id : "";
        username = user ? user : "";
        password = pass ? pass : "";
        lwtTopic = willTopic ? willTopic : "";
        lwtQoS = willQoS_;
        lwtRetain = willRetain_;
        lwtMessage = willMessage_ ? willMessage_ : "";

        // Simulate successful connection
        isConnected = true;
        connectionState = 0;
        return true;
    }

    void disconnect() override {
        isConnected = false;
        connectionState = -1;
    }

    bool loop() override {
        // In stub, loop just returns connection status
        return isConnected;
    }

    bool publish(const char* topic,
                const uint8_t* payload,
                unsigned int length,
                bool retained = false) override {
        (void)topic;
        (void)payload;
        (void)length;
        (void)retained;

        if (!isConnected) {
            return false;
        }

        publishCount++;
        return true;
    }

    bool subscribe(const char* topic, uint8_t qos = 0) override {
        (void)topic;
        (void)qos;

        if (!isConnected) {
            return false;
        }

        subscribeCount++;
        return true;
    }

    bool unsubscribe(const char* topic) override {
        (void)topic;

        if (!isConnected) {
            return false;
        }

        unsubscribeCount++;
        return true;
    }

    void setServer(const char* domain, uint16_t port) override {
        serverDomain = domain ? domain : "";
        serverPort = port;
    }

    void setCallback(void (*callback)(char*, uint8_t*, unsigned int)) override {
        if (callback) {
            messageCallback = callback;
        } else {
            messageCallback = nullptr;
        }
    }

    void setKeepAlive(uint16_t keepAlive) override {
        keepAliveSeconds = keepAlive;
    }

    bool setBufferSize(uint16_t size) override {
        if (size < 128 || size > 65535) {
            return false;
        }
        bufferSize = size;
        return true;
    }

    uint16_t getBufferSize() override {
        return bufferSize;
    }

    int state() override {
        return connectionState;
    }

    bool connected() override {
        return isConnected;
    }

    // Test helper methods (not part of public interface)
    uint32_t getPublishCount() const { return publishCount; }
    uint32_t getSubscribeCount() const { return subscribeCount; }
    uint32_t getUnsubscribeCount() const { return unsubscribeCount; }
    const std::string& getClientId() const { return clientId; }
    const std::string& getUsername() const { return username; }
    const std::string& getLWTTopic() const { return lwtTopic; }
    const std::string& getLWTMessage() const { return lwtMessage; }

    // Simulate receiving a message (for testing callbacks)
    void simulateMessage(const char* topic, const uint8_t* payload, unsigned int length) {
        if (messageCallback && isConnected) {
            char* topicBuf = new char[strlen(topic) + 1];
            strcpy(topicBuf, topic);
            messageCallback(topicBuf, const_cast<uint8_t*>(payload), length);
            delete[] topicBuf;
        }
    }
};

} // namespace MQTT
} // namespace HAL
} // namespace DomoticsCore
