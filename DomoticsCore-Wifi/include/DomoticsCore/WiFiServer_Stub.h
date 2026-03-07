#pragma once

/**
 * @file WiFiServer_Stub.h
 * @brief Stub WiFiServer/WiFiClient implementation for native tests
 *
 * WiFiClient uses shared state so that copies (as produced by WiFiServer::accept)
 * share the same read/write buffers — enabling protocol-level testing.
 */

#include <DomoticsCore/Platform_HAL.h>
#include "IPAddress_Stub.h"
#include <vector>
#include <memory>

namespace DomoticsCore {
namespace HAL {

using namespace DomoticsCore::HAL::Platform;

/**
 * @class WiFiClient
 * @brief Stub WiFi client for native tests
 *
 * Copies share state (connected flag, buffers) via shared_ptr,
 * so simulateIncomingData() on any copy is visible to all others.
 */
class WiFiClient {
private:
    struct SharedState {
        bool connected = false;
        std::vector<uint8_t> writeBuffer;
        std::vector<uint8_t> readBuffer;
        size_t readPos = 0;
        uint32_t clientId = 0;
    };
    std::shared_ptr<SharedState> _s;

public:
    WiFiClient() : _s(std::make_shared<SharedState>()) {}
    WiFiClient(bool isConnected, uint32_t id = 0)
        : _s(std::make_shared<SharedState>()) {
        _s->connected = isConnected;
        _s->clientId = id;
    }

    operator bool() const { return _s->connected; }
    bool operator==(const WiFiClient& other) const { return _s == other._s; }
    bool operator!=(const WiFiClient& other) const { return _s != other._s; }

    bool connected() const { return _s->connected; }
    void stop() { _s->connected = false; _s->writeBuffer.clear(); _s->readBuffer.clear(); _s->readPos = 0; }

    // IP address methods
    uint32_t remoteIP() const { return _s->clientId; }

    // Print methods for compatibility
    size_t println() {
        size_t n = write((uint8_t)'\r');
        n += write((uint8_t)'\n');
        return n;
    }
    size_t println(const char* str) {
        size_t n = write(str);
        n += write((uint8_t)'\r');
        n += write((uint8_t)'\n');
        return n;
    }
    size_t println(const String& str) { return println(str.c_str()); }
    size_t print(const char* str) { return write(str); }
    size_t print(const String& str) { return write(str.c_str()); }

    size_t write(uint8_t c) { _s->writeBuffer.push_back(c); return 1; }
    size_t write(const uint8_t* buf, size_t size) {
        _s->writeBuffer.insert(_s->writeBuffer.end(), buf, buf + size);
        return size;
    }
    size_t write(const char* str) {
        if (!str) return 0;
        size_t len = strlen(str);
        return write((const uint8_t*)str, len);
    }

    int available() const { return _s->readBuffer.size() - _s->readPos; }
    int read() {
        if (_s->readPos >= _s->readBuffer.size()) return -1;
        return _s->readBuffer[_s->readPos++];
    }
    int read(uint8_t* buf, size_t size) {
        size_t avail = _s->readBuffer.size() - _s->readPos;
        size_t toRead = (size < avail) ? size : avail;
        for (size_t i = 0; i < toRead; i++) {
            buf[i] = _s->readBuffer[_s->readPos++];
        }
        return toRead;
    }
    int peek() const {
        if (_s->readPos >= _s->readBuffer.size()) return -1;
        return _s->readBuffer[_s->readPos];
    }

    void flush() { _s->writeBuffer.clear(); }

    // Test helpers
    const std::vector<uint8_t>& getWriteBuffer() const { return _s->writeBuffer; }
    std::string getWriteBufferAsString() const {
        return std::string(_s->writeBuffer.begin(), _s->writeBuffer.end());
    }
    void clearWriteBuffer() { _s->writeBuffer.clear(); }
    void simulateIncomingData(const uint8_t* data, size_t len) {
        _s->readBuffer.insert(_s->readBuffer.end(), data, data + len);
    }
    void simulateIncomingData(const char* str) {
        if (str) simulateIncomingData((const uint8_t*)str, strlen(str));
    }
    void simulateDisconnect() { _s->connected = false; }
};

/**
 * @class WiFiServer
 * @brief Stub WiFi server for native tests
 */
class WiFiServer {
private:
    uint16_t port = 0;
    bool listening = false;
    std::vector<WiFiClient> pendingClients;

public:
    WiFiServer(uint16_t p) : port(p) {}

    void begin() { listening = true; }
    void end() { listening = false; pendingClients.clear(); }
    void stop() { end(); }

    void setNoDelay(bool /*nodelay*/) {}

    bool hasClient() const { return !pendingClients.empty(); }

    WiFiClient accept() {
        if (pendingClients.empty()) return WiFiClient(false);
        WiFiClient client = pendingClients.front();
        pendingClients.erase(pendingClients.begin());
        return client;
    }

    WiFiClient available() { return accept(); }

    // Test helpers — returns the client so caller can inject data via shared state
    WiFiClient simulateClient(bool connected = true, uint32_t id = 0x0A0B0C0D) {
        pendingClients.push_back(WiFiClient(connected, id));
        return pendingClients.back();  // shares state with the pending copy
    }

    uint16_t getPort() const { return port; }
    bool isListening() const { return listening; }
};

} // namespace HAL
} // namespace DomoticsCore
