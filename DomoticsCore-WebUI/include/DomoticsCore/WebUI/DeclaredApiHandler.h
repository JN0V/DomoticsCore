#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief Answers a GET on a path that a context declares with withAPI().
 *
 * One handler serves every declared path, so a path costs no route of its own.
 * Writes stay on /api/ui/action, which checks the CSRF token. Owned by the
 * server once added.
 */
class DeclaredApiHandler : public AsyncWebHandler {
public:
    using Accepts = std::function<bool(const String& path)>;
    using Answer = std::function<void(AsyncWebServerRequest*)>;

    DeclaredApiHandler(Accepts accepts, Answer answer)
        : accepts_(std::move(accepts)), answer_(std::move(answer)) {}

    bool canHandle(AsyncWebServerRequest* request) const override {
        return request->method() == HTTP_GET && accepts_(request->url());
    }

    void handleRequest(AsyncWebServerRequest* request) override { answer_(request); }

private:
    Accepts accepts_;
    Answer answer_;
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
