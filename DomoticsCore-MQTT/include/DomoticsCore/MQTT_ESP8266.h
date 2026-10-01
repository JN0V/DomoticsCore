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
#include <memory>

namespace DomoticsCore {
namespace HAL {
namespace MQTT {

// Bytes the publish queue may hold, topic, payload and entry included: an ESP8266 runs a full System on about 27 KB of free heap.
#ifndef DOMOTICS_MQTT_QUEUE_BYTES
#define DOMOTICS_MQTT_QUEUE_BYTES 8192
#endif
constexpr size_t kQueueByteBudget = DOMOTICS_MQTT_QUEUE_BYTES;

// TLS record buffers, each way; the broker must accept the max fragment length extension.
constexpr int kTlsBufferBytes = 1024;

/**
 * @brief ESP8266 MQTT client implementation
 *
 * Wraps PubSubClient for ESP8266 platform
 */
class MQTTClientImpl : public MQTTClient {
private:
    PubSubClient client;
    WiFiClient wifiClient;
    // BearSSL keeps a pointer to its trust anchors: declared first, destroyed last.
    std::unique_ptr<BearSSL::X509List> trustAnchors;
    WiFiClientSecure wifiClientSecure;
    bool useTLS;

public:
    /**
     * @brief Construct MQTT client for ESP8266
     * @param useTLS_ Use TLS/SSL connection
     * @param caCert PEM root CA, parsed into a copy; certificate dates are
     *        checked against the system clock, so NTP must have synced
     */
    explicit MQTTClientImpl(bool useTLS_ = false, const char* caCert = nullptr)
        : client(useTLS_ ? (Client&)wifiClientSecure : (Client&)wifiClient)
        , useTLS(useTLS_) {
        if (useTLS_ && caCert) {
            trustAnchors.reset(new BearSSL::X509List(caCert));
            wifiClientSecure.setTrustAnchors(trustAnchors.get());
            // BearSSL's default 16 KB receive buffer does not fit beside a System;
            // a smaller one makes it ask the broker for short records (MFLN).
            wifiClientSecure.setBufferSizes(kTlsBufferBytes, kTlsBufferBytes);
        }
    }

    bool connect(const char* id,
                const char* user = nullptr,
                const char* pass = nullptr,
                const char* willTopic = nullptr,
                uint8_t willQoS = 0,
                bool willRetain = false,
                const char* willMessage = nullptr) override {
        // A failed handshake keeps its reason until the next one: clear it, so
        // a later refused TCP connection does not report it again.
        if (useTLS) wifiClientSecure.stop();
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
        return wifiClientSecure.getLastSSLError(buf, size);
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
