#pragma once

/**
 * @file AsyncEventSource.h
 * @brief Mock for the SSE half of ESPAsyncWebServer, under its real name.
 *
 * Records connected clients and every message sent, so a test can assert what a
 * handler pushed without a socket.
 */

#include "ESPAsyncWebServer.h"

class AsyncEventSourceClient {
public:
    explicit AsyncEventSourceClient(uint32_t lastId = 0) : lastId_(lastId) {}
    uint32_t lastId() const { return lastId_; }

private:
    uint32_t lastId_;
};

/// A sent event, kept in order for assertions.
struct RecordedEvent {
    String message;
    String eventName;
    uint32_t id = 0;
    uint32_t reconnect = 0;
};

class AsyncEventSource : public AsyncWebHandler {
public:
    explicit AsyncEventSource(const String& uri) : uri_(uri) {}

    void onConnect(std::function<void(AsyncEventSourceClient*)> callback) {
        connectCallback = std::move(callback);
    }

    void addMiddleware(std::function<void(AsyncWebServerRequest*, ArMiddlewareNext)> middleware) {
        middlewares.push_back(std::move(middleware));
    }

    void send(const char* message, const char* eventName = nullptr, uint32_t id = 0,
              uint32_t reconnect = 0) {
        events.push_back(RecordedEvent{String(message), String(eventName ? eventName : ""), id,
                                       reconnect});
    }

    size_t count() const { return clientCount; }
    void close() { clientCount = 0; }
    const String& uri() const { return uri_; }

    /// Runs the connect callback, as an arriving client would.
    void connectClient(uint32_t lastId = 0) {
        ++clientCount;
        AsyncEventSourceClient client(lastId);
        if (connectCallback) connectCallback(&client);
    }

    std::vector<RecordedEvent> events;
    std::vector<std::function<void(AsyncWebServerRequest*, ArMiddlewareNext)>> middlewares;
    std::function<void(AsyncEventSourceClient*)> connectCallback;
    size_t clientCount = 0;

private:
    String uri_;
};
