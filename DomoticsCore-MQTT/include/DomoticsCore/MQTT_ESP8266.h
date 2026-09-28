#pragma once

/**
 * @file MQTT_ESP8266.h
 * @brief ESP8266 implementation of MQTT HAL using PubSubClient
 */

// ESP8266-specific MQTT buffer size
// ESP8266 has only ~80KB RAM, must use smaller buffers
// 768 bytes allows HA discovery payloads (~600 bytes) with headroom
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 768
#endif

#include <lwip/opt.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

namespace DomoticsCore {
namespace HAL {
namespace MQTT {

// Bytes the publish queue may hold, topic, payload and entry included: an ESP8266 runs a full System on about 27 KB of free heap.
#ifndef DOMOTICS_MQTT_QUEUE_BYTES
#define DOMOTICS_MQTT_QUEUE_BYTES 8192
#endif
constexpr size_t kQueueByteBudget = DOMOTICS_MQTT_QUEUE_BYTES;

/**
 * @brief ESP8266 MQTT client implementation
 *
 * Wraps PubSubClient for ESP8266 platform
 */
class MQTTClientImpl : public MQTTClient {
private:
    PubSubClient client;
    WiFiClient wifiClient;
    WiFiClientSecure wifiClientSecure;
    bool useTLS;

public:
    /**
     * @brief Construct MQTT client for ESP8266
     * @param useTLS_ Use TLS/SSL connection
     */
    explicit MQTTClientImpl(bool useTLS_ = false)
        : client(useTLS_ ? (Client&)wifiClientSecure : (Client&)wifiClient)
        , useTLS(useTLS_) {}

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

    // WiFiClient::write() waits up to its 5 s timeout for send-buffer space. A packet
    // larger than the whole buffer goes once the buffer is empty, or it would never go.
    bool canWrite(size_t packetLength) override {
        if (useTLS) return true;
        const size_t need = packetLength < TCP_SND_BUF ? packetLength : TCP_SND_BUF;
        return static_cast<size_t>(wifiClient.availableForWrite()) >= need;
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
