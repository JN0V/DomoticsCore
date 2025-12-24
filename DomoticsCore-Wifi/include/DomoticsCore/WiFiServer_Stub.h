#pragma once

/**
 * @file WiFiServer_Stub.h
 * @brief Stub WiFiServer/WiFiClient implementation for native tests
 */

#include <DomoticsCore/Platform_HAL.h>
#include "IPAddress_Stub.h"
#include <vector>

namespace DomoticsCore {
namespace HAL {

using namespace DomoticsCore::HAL::Platform;

/**
 * @class WiFiClient
 * @brief Stub WiFi client for native tests
 */
class WiFiClient {
private:
    bool _connected = false;
    std::vector<uint8_t> writeBuffer;
    std::vector<uint8_t> readBuffer;
    size_t readPos = 0;
    uint32_t clientId = 0;

public:
    WiFiClient() = default;
    WiFiClient(bool isConnected, uint32_t id = 0) : _connected(isConnected), clientId(id) {}

    operator bool() const { return _connected; }
    bool operator==(const WiFiClient& other) const { return _connected == other._connected && clientId == other.clientId; }
    bool operator!=(const WiFiClient& other) const { return !(*this == other); }

    bool connected() const { return _connected; }
    void stop() { _connected = false; writeBuffer.clear(); readBuffer.clear(); readPos = 0; }

    // IP address methods
    uint32_t remoteIP() const { return clientId; }

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

    size_t write(uint8_t c) { writeBuffer.push_back(c); return 1; }
    size_t write(const uint8_t* buf, size_t size) {
        writeBuffer.insert(writeBuffer.end(), buf, buf + size);
        return size;
    }
    size_t write(const char* str) {
        if (!str) return 0;
        size_t len = strlen(str);
        return write((const uint8_t*)str, len);
    }

    int available() const { return readBuffer.size() - readPos; }
    int read() {
        if (readPos >= readBuffer.size()) return -1;
        return readBuffer[readPos++];
    }
    int read(uint8_t* buf, size_t size) {
        size_t available = readBuffer.size() - readPos;
        size_t toRead = (size < available) ? size : available;
        for (size_t i = 0; i < toRead; i++) {
            buf[i] = readBuffer[readPos++];
        }
        return toRead;
    }
    int peek() const {
        if (readPos >= readBuffer.size()) return -1;
        return readBuffer[readPos];
    }

    void flush() { writeBuffer.clear(); }

    // Test helpers
    const std::vector<uint8_t>& getWriteBuffer() const { return writeBuffer; }
    void simulateIncomingData(const uint8_t* data, size_t len) {
        readBuffer.insert(readBuffer.end(), data, data + len);
    }
    void simulateIncomingData(const char* str) {
        if (str) simulateIncomingData((const uint8_t*)str, strlen(str));
    }
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

    // Test helpers
    void simulateClient(bool connected = true, uint32_t id = 0x0A0B0C0D) {
        pendingClients.push_back(WiFiClient(connected, id));
    }

    uint16_t getPort() const { return port; }
    bool isListening() const { return listening; }
};

} // namespace HAL
} // namespace DomoticsCore
