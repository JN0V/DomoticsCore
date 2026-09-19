#pragma once

/**
 * @file ESPAsyncWebServer.h
 * @brief Mock ESPAsyncWebServer, found by its real name.
 *
 * The board library is absent from the host toolchain, so every header that
 * includes it is uncompilable natively and no test can reach the handlers
 * registered through it. This reproduces the surface those headers use, records
 * what a handler does, and lets a test drive a registered route directly.
 *
 * It is a mock, not an implementation: it parses no HTTP and runs no socket.
 * A test through it proves handler logic, never the server's own behaviour.
 */

#include <DomoticsCore/Platform_HAL.h>

#include <FS.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Mirrors the board library's request-method bitmask.
enum WebRequestMethod : uint8_t {
    HTTP_GET     = 0b00000001,
    HTTP_POST    = 0b00000010,
    HTTP_DELETE  = 0b00000100,
    HTTP_PUT     = 0b00001000,
    HTTP_PATCH   = 0b00010000,
    HTTP_HEAD    = 0b00100000,
    HTTP_OPTIONS = 0b01000000,
    HTTP_ANY     = 0b01111111
};

/// Chunked-filler sentinel: ask the server to call back later.
#define RESPONSE_TRY_AGAIN 0xFFFFFFFF

class AsyncWebServerRequest;
class AsyncWebServerResponse;
class AsyncResponseStream;

using ArRequestHandlerFunction = std::function<void(AsyncWebServerRequest*)>;
using ArUploadHandlerFunction =
        std::function<void(AsyncWebServerRequest*, const String&, size_t, uint8_t*, size_t, bool)>;
using ArMiddlewareNext = std::function<void()>;
using AwsResponseFiller = std::function<size_t(uint8_t*, size_t, size_t)>;

class AsyncWebHeader {
public:
    AsyncWebHeader(const String& name, const String& value) : name_(name), value_(value) {}
    const String& name() const { return name_; }
    const String& value() const { return value_; }

private:
    String name_;
    String value_;
};

class AsyncWebParameter {
public:
    AsyncWebParameter(const String& name, const String& value, bool post = false)
        : name_(name), value_(value), post_(post) {}
    const String& name() const { return name_; }
    const String& value() const { return value_; }
    bool isPost() const { return post_; }

private:
    String name_;
    String value_;
    bool post_;
};

/// The board's TCP client. Only the receive-idle timeout is reached from here.
class AsyncClient {
public:
    void setRxTimeout(uint32_t seconds) { rxTimeoutSeconds = seconds; }
    uint32_t getRxTimeout() const { return rxTimeoutSeconds; }

    uint32_t rxTimeoutSeconds = 0;
};

class AsyncWebServerResponse {
public:
    AsyncWebServerResponse(int code, const String& contentType, const String& body = String(""))
        : code_(code), contentType_(contentType), body_(body) {}
    virtual ~AsyncWebServerResponse() = default;

    void addHeader(const String& name, const String& value) { headers_.emplace_back(name, value); }

    int code() const { return code_; }
    const String& contentType() const { return contentType_; }
    virtual String body() const { return body_; }
    const std::vector<std::pair<String, String>>& headers() const { return headers_; }

    /// Empty when the response carries a body rather than a filler.
    AwsResponseFiller filler;

protected:
    int code_;
    String contentType_;
    String body_;
    std::vector<std::pair<String, String>> headers_;
};

/// Accumulates what a handler prints; satisfies ArduinoJson's writer surface.
class AsyncResponseStream : public AsyncWebServerResponse {
public:
    explicit AsyncResponseStream(const String& contentType)
        : AsyncWebServerResponse(200, contentType) {}

    size_t write(uint8_t c) {
        char s[2] = {static_cast<char>(c), '\0'};
        body_ += s;
        return 1;
    }

    size_t write(const uint8_t* buffer, size_t size) {
        for (size_t i = 0; i < size; ++i) write(buffer[i]);
        return size;
    }

    size_t print(const String& s) {
        body_ += s;
        return s.length();
    }

    size_t print(const char* s) { return print(String(s)); }

    size_t printf(const char* format, ...) {
        char buffer[512];
        va_list args;
        va_start(args, format);
        const int written = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        if (written > 0) body_ += buffer;
        return written > 0 ? static_cast<size_t>(written) : 0;
    }
};

/**
 * @brief A request a test builds by hand and hands to a registered handler.
 *
 * Headers, parameters, credentials and the announced content length are what
 * the handlers read; the send() calls and the disconnect callback are what a
 * test asserts on afterwards.
 */
class AsyncWebServerRequest {
public:
    AsyncWebServerRequest() = default;

    // --- what a handler reads -------------------------------------------------

    bool authenticate(const char* username, const char* password) const {
        if (!hasCredentials) return false;
        return credentialUser == String(username) && credentialPass == String(password);
    }

    void requestAuthentication(const char* realm = nullptr) {
        (void)realm;
        authenticationRequested = true;
        sentCode = 401;
    }

    size_t contentLength() const { return contentLength_; }

    const AsyncWebHeader* getHeader(const String& name) const {
        for (const auto& h : headers_) {
            if (h.name() == name) return &h;
        }
        return nullptr;
    }

    bool hasParam(const String& name, bool post = false) const {
        return getParam(name, post) != nullptr;
    }

    const AsyncWebParameter* getParam(const String& name, bool post = false) const {
        for (const auto& p : params_) {
            if (p.name() == name && p.isPost() == post) return &p;
        }
        return nullptr;
    }

    AsyncClient* client() { return &client_; }

    void onDisconnect(std::function<void()> callback) { disconnectCallback = std::move(callback); }

    // --- responses the handler produces ---------------------------------------

    void send(int code, const String& contentType = String(""), const String& body = String("")) {
        sentCode = code;
        sentContentType = contentType;
        sentBody = body;
        ++sendCount;
    }

    /// Static-file send: the mock records the path rather than reading it.
    void send(fs::FS& fs, const String& path, const String& contentType = String("")) {
        (void)fs;
        sentCode = 200;
        sentContentType = contentType;
        sentFilePath = path;
        ++sendCount;
    }

    void send(AsyncWebServerResponse* response) {
        if (!response) return;
        sentCode = response->code();
        sentContentType = response->contentType();
        sentBody = response->body();
        sentFiller = response->filler;
        sentHeaders = response->headers();
        ++sendCount;
        delete response;
    }

    AsyncWebServerResponse* beginResponse(int code, const String& contentType,
                                          const String& body = String("")) {
        return new AsyncWebServerResponse(code, contentType, body);
    }

    AsyncWebServerResponse* beginResponse(int code, const String& contentType,
                                          const uint8_t* data, size_t len) {
        String body;
        for (size_t i = 0; i < len; ++i) body += static_cast<char>(data[i]);
        return new AsyncWebServerResponse(code, contentType, body);
    }

    AsyncWebServerResponse* beginResponse_P(int code, const String& contentType,
                                            const uint8_t* data, size_t len) {
        return beginResponse(code, contentType, data, len);
    }

    AsyncResponseStream* beginResponseStream(const String& contentType) {
        return new AsyncResponseStream(contentType);
    }

    AsyncWebServerResponse* beginChunkedResponse(const String& contentType, AwsResponseFiller filler) {
        auto* response = new AsyncWebServerResponse(200, contentType);
        response->filler = std::move(filler);
        return response;
    }

    // --- what a test sets before the call, and reads after ---------------------

    void setContentLength(size_t length) { contentLength_ = length; }
    void addHeader(const String& name, const String& value) { headers_.emplace_back(name, value); }
    void addParam(const String& name, const String& value, bool post = false) {
        params_.emplace_back(name, value, post);
    }
    void setCredentials(const String& user, const String& pass) {
        credentialUser = user;
        credentialPass = pass;
        hasCredentials = true;
    }

    /// Runs the callback the handler registered, as a dropped connection would.
    void fireDisconnect() {
        if (disconnectCallback) disconnectCallback();
    }

    /// Drains a chunked response into a string, as the server would.
    String drainChunked(size_t chunkSize = 256) const {
        String out;
        if (!sentFiller) return out;
        std::vector<uint8_t> buffer(chunkSize);
        size_t index = 0;
        while (true) {
            const size_t written = sentFiller(buffer.data(), chunkSize, index);
            if (written == 0) break;
            for (size_t i = 0; i < written; ++i) out += static_cast<char>(buffer[i]);
            index += written;
        }
        return out;
    }

    int sentCode = 0;
    String sentContentType;
    String sentBody;
    String sentFilePath;
    AwsResponseFiller sentFiller;
    std::vector<std::pair<String, String>> sentHeaders;
    int sendCount = 0;
    bool authenticationRequested = false;
    std::function<void()> disconnectCallback;

private:
    size_t contentLength_ = 0;
    std::vector<AsyncWebHeader> headers_;
    std::vector<AsyncWebParameter> params_;
    String credentialUser;
    String credentialPass;
    bool hasCredentials = false;
    AsyncClient client_;
};

/// A route as the server recorded it, so a test can find and run one.
struct RecordedRoute {
    String uri;
    uint8_t method = HTTP_ANY;
    ArRequestHandlerFunction handler;
    ArUploadHandlerFunction uploadHandler;
    bool isUpload = false;
};

class AsyncWebHandler {
public:
    virtual ~AsyncWebHandler() = default;
};

class AsyncWebServer {
public:
    explicit AsyncWebServer(uint16_t port) : port_(port) { instances().push_back(this); }

    ~AsyncWebServer() {
        auto& all = instances();
        for (auto it = all.begin(); it != all.end(); ++it) {
            if (*it == this) { all.erase(it); break; }
        }
    }

    /// Every live server, in construction order. The component owns its server
    /// privately, so this is how a test reaches the routes it registered.
    static std::vector<AsyncWebServer*>& instances() {
        static std::vector<AsyncWebServer*> all;
        return all;
    }

    static AsyncWebServer* last() { return instances().empty() ? nullptr : instances().back(); }

    /// Searches every live server; nullptr when no route matches.
    static RecordedRoute* findRouteAnywhere(const String& uri) {
        for (auto* server : instances()) {
            if (RecordedRoute* route = server->findRoute(uri)) return route;
        }
        return nullptr;
    }

    void on(const char* uri, WebRequestMethod method, ArRequestHandlerFunction handler) {
        routes.push_back(RecordedRoute{String(uri), static_cast<uint8_t>(method), std::move(handler),
                                       nullptr, false});
    }

    void on(const char* uri, WebRequestMethod method, ArRequestHandlerFunction handler,
            ArUploadHandlerFunction uploadHandler) {
        routes.push_back(RecordedRoute{String(uri), static_cast<uint8_t>(method), std::move(handler),
                                       std::move(uploadHandler), true});
    }

    void addHandler(AsyncWebHandler* handler) { handlers.push_back(handler); }
    void begin() { running = true; }
    void end() { running = false; }
    uint16_t port() const { return port_; }

    /// nullptr when no route was registered under that uri.
    RecordedRoute* findRoute(const String& uri) {
        for (auto& route : routes) {
            if (route.uri == uri) return &route;
        }
        return nullptr;
    }

    std::vector<RecordedRoute> routes;
    std::vector<AsyncWebHandler*> handlers;
    bool running = false;

private:
    uint16_t port_;
};
