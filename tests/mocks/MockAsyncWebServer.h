#pragma once

/**
 * @file MockAsyncWebServer.h
 * @brief Mock AsyncWebServer for isolated unit testing without real HTTP
 * 
 * This mock allows testing WebUI components without requiring
 * a real network stack or HTTP server.
 */

#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>

namespace DomoticsCore {
namespace Mocks {

/**
 * @brief Recorded HTTP request for test verification
 */
struct MockHttpRequest {
    String method;
    String path;
    std::map<String, String> params;
    String body;
};

/**
 * @brief Recorded HTTP response for test verification
 */
struct MockHttpResponse {
    int statusCode;
    String contentType;
    String body;
};

/**
 * @brief Mock route handler
 */
struct MockRoute {
    String method;
    String path;
    std::function<MockHttpResponse(const MockHttpRequest&)> handler;
};

/**
 * @brief Mock AsyncWebServer that records requests and responses
 */
class MockAsyncWebServer {
public:
    // Recorded operations
    static std::vector<MockHttpRequest> receivedRequests;
    static std::vector<MockHttpResponse> sentResponses;
    static std::vector<MockRoute> routes;
    
    // Server state
    static bool running;
    static uint16_t port;
    
    // Server operations
    static void begin(uint16_t p = 80) {
        port = p;
        running = true;
    }
    
    static void end() {
        running = false;
    }
    
    static bool isRunning() { return running; }
    
    // Route registration
    static void on(const String& path, const String& method, 
                   std::function<MockHttpResponse(const MockHttpRequest&)> handler) {
        routes.push_back({method, path, handler});
    }
    
    static void onGet(const String& path, 
                      std::function<MockHttpResponse(const MockHttpRequest&)> handler) {
        on(path, "GET", handler);
    }
    
    static void onPost(const String& path, 
                       std::function<MockHttpResponse(const MockHttpRequest&)> handler) {
        on(path, "POST", handler);
    }
    
    // Test simulation - send a request and get response
    static MockHttpResponse simulateRequest(const MockHttpRequest& req) {
        receivedRequests.push_back(req);
        
        // Find matching route
        for (const auto& route : routes) {
            if (route.method == req.method && route.path == req.path) {
                MockHttpResponse resp = route.handler(req);
                sentResponses.push_back(resp);
                return resp;
            }
        }
        
        // 404 if no route found
        MockHttpResponse notFound = {404, "text/plain", "Not Found"};
        sentResponses.push_back(notFound);
        return notFound;
    }
    
    // Convenience methods for common request types
    static MockHttpResponse simulateGet(const String& path, 
                                        const std::map<String, String>& params = {}) {
        MockHttpRequest req = {"GET", path, params, ""};
        return simulateRequest(req);
    }
    
    static MockHttpResponse simulatePost(const String& path, const String& body,
                                         const std::map<String, String>& params = {}) {
        MockHttpRequest req = {"POST", path, params, body};
        return simulateRequest(req);
    }
    
    // Test control methods
    static void reset() {
        receivedRequests.clear();
        sentResponses.clear();
        routes.clear();
        running = false;
        port = 80;
    }
    
    // Verification helpers
    static bool wasRequested(const String& path) {
        for (const auto& req : receivedRequests) {
            if (req.path == path) return true;
        }
        return false;
    }
    
    static bool wasRequested(const String& method, const String& path) {
        for (const auto& req : receivedRequests) {
            if (req.method == method && req.path == path) return true;
        }
        return false;
    }
    
    static int getRequestCount() { return receivedRequests.size(); }
    static int getRouteCount() { return routes.size(); }
    
    static const MockHttpRequest* getLastRequest() {
        return receivedRequests.empty() ? nullptr : &receivedRequests.back();
    }
    
    static const MockHttpResponse* getLastResponse() {
        return sentResponses.empty() ? nullptr : &sentResponses.back();
    }
};

// Static member initialization
std::vector<MockHttpRequest> MockAsyncWebServer::receivedRequests;
std::vector<MockHttpResponse> MockAsyncWebServer::sentResponses;
std::vector<MockRoute> MockAsyncWebServer::routes;
bool MockAsyncWebServer::running = false;
uint16_t MockAsyncWebServer::port = 80;

} // namespace Mocks
} // namespace DomoticsCore
