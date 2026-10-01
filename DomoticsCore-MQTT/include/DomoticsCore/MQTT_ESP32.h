#pragma once

/**
 * @file MQTT_ESP32.h
 * @brief ESP32 implementation of MQTT HAL using PubSubClient
 */

// ESP32-specific MQTT buffer size
// ESP32 has ~520KB RAM, so we can afford larger buffers
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 2048
#endif

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <lwip/sockets.h>

namespace DomoticsCore {
namespace HAL {
namespace MQTT {

// Bytes the publish queue may hold, topic, payload and entry included: an ESP32 has room to ride out a slow broker.
#ifndef DOMOTICS_MQTT_QUEUE_BYTES
#define DOMOTICS_MQTT_QUEUE_BYTES 32768
#endif
constexpr size_t kQueueByteBudget = DOMOTICS_MQTT_QUEUE_BYTES;

/**
 * @brief ESP32 MQTT client implementation
 *
 * Wraps PubSubClient for ESP32 platform
 */
class MQTTClientImpl : public MQTTClient {
private:
    PubSubClient client;
    WiFiClient wifiClient;
    WiFiClientSecure wifiClientSecure;
    bool useTLS;

public:
    /**
     * @brief Construct MQTT client for ESP32
     * @param useTLS_ Use TLS/SSL connection
     * @param caCert PEM root CA; the secure client keeps the pointer, not a copy
     */
    explicit MQTTClientImpl(bool useTLS_ = false, const char* caCert = nullptr)
        : client(useTLS_ ? (Client&)wifiClientSecure : (Client&)wifiClient)
        , useTLS(useTLS_) {
        if (useTLS_ && caCert) wifiClientSecure.setCACert(caCert);
    }

    bool connect(const char* id,
                const char* user = nullptr,
                const char* pass = nullptr,
                const char* willTopic = nullptr,
                uint8_t willQoS = 0,
                bool willRetain = false,
                const char* willMessage = nullptr) override {
        if (willTopic && willMessage) {
            return client.connect(id, user, pass, willTopic, willQoS, willRetain, willMessage);
        } else if (user && pass) {
            return client.connect(id, user, pass);
        } else {
            return client.connect(id);
        }
    }

    int lastTlsError(char* buf, size_t size) override {
        if (!useTLS) return MQTTClient::lastTlsError(buf, size);
        // -1 is the TCP connection failing, before any TLS took place.
        const int err = wifiClientSecure.lastError(buf, size);
        if (err == -1) return MQTTClient::lastTlsError(buf, size);
        return err;
    }

    // WiFiClient::write() waits in select() up to 1 s per retry while lwIP holds
    // too much unacknowledged data; asking first, with no wait, keeps loop() free.
    bool canWrite(size_t packetLength) override {
        (void)packetLength;
        if (useTLS) return true;
        const int fd = wifiClient.fd();
        if (fd < 0) return false;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);
        struct timeval tv = {0, 0};
        return select(fd + 1, nullptr, &set, nullptr, &tv) > 0;
    }

    void disconnect() override {
        client.disconnect();
    }

    bool loop() override {
        return client.loop();
    }

    bool publish(const char* topic,
                const uint8_t* payload,
                unsigned int length,
                bool retained = false) override {
        return client.publish(topic, payload, length, retained);
    }

    bool subscribe(const char* topic, uint8_t qos = 0) override {
        return client.subscribe(topic, qos);
    }

    bool unsubscribe(const char* topic) override {
        return client.unsubscribe(topic);
    }

    void setServer(const char* domain, uint16_t port) override {
        client.setServer(domain, port);
    }

    void setCallback(void (*callback)(char*, uint8_t*, unsigned int)) override {
        client.setCallback(callback);
    }

    void setKeepAlive(uint16_t keepAlive) override {
        client.setKeepAlive(keepAlive);
    }

    bool setBufferSize(uint16_t size) override {
        return client.setBufferSize(size);
    }

    uint16_t getBufferSize() override {
        return client.getBufferSize();
    }

    int state() override {
        return client.state();
    }

    bool connected() override {
        return client.connected();
    }
};

} // namespace MQTT
} // namespace HAL
} // namespace DomoticsCore
