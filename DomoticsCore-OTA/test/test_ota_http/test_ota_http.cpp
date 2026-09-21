/**
 * @file test_ota_http.cpp
 * @brief Drives the routes OTAWebUI registers, through the mocked async server.
 *
 * Every other OTA suite calls beginUpload()/acceptUploadChunk()/finalizeUpload()
 * directly, so the upload handler's own gates — the CSRF check, the credentials
 * check, the state reset and their order, the abort on a dropped client and the
 * narrowing of the announced size — have never run under test.
 *
 * The mocked server records; it parses no HTTP. What passes here is the
 * handler's logic, never the real server's behaviour.
 */

#include <unity.h>

#include <DomoticsCore/OTA.h>
#include <DomoticsCore/OTAWebUI.h>
#include <DomoticsCore/Update_HAL.h>
#include <DomoticsCore/WebUI.h>

#include <memory>

using namespace DomoticsCore::Components;

namespace {

/// Announced minus delivered on a real browser upload: boundary, part headers
/// and the trailing boundary. Measured on both boards.
constexpr size_t kMultipartEnvelope = 220;

const uint8_t kFirmware[64] = {
    0xE9, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C,
    0x3D, 0x3E, 0x3F, 0x40};

/// One wired OTA-over-WebUI stack, rebuilt for every test.
struct Harness {
    WebUIConfig webConfig;
    OTAConfig otaConfig;
    std::unique_ptr<WebUIComponent> webui;
    std::unique_ptr<OTAComponent> ota;
    std::unique_ptr<WebUI::OTAWebUI> provider;

    void build() {
        webui.reset(new WebUIComponent(webConfig));
        webui->begin();
        ota.reset(new OTAComponent(otaConfig));
        ota->begin();
        provider.reset(new WebUI::OTAWebUI(ota.get()));
        provider->init(webui.get());
    }

    RecordedRoute* uploadRoute() { return AsyncWebServer::findRouteAnywhere("/api/ota/upload"); }
    RecordedRoute* tokenRoute() { return AsyncWebServer::findRouteAnywhere("/api/ui/token"); }

    /// The token the UI hands a browser, read back through its own route —
    /// which is itself behind the credentials check when auth is on.
    String csrfToken(const char* user = nullptr, const char* pass = nullptr) {
        AsyncWebServerRequest request;
        if (user) request.setCredentials(user, pass);
        tokenRoute()->handler(&request);
        const String body = request.sentBody;
        const int start = body.indexOf("\":\"");
        const int end = body.lastIndexOf('"');
        if (start < 0 || end <= start + 3) return String("");
        return body.substring(start + 3, end);
    }
};

Harness* harness = nullptr;

/// Feeds a body to the upload handler the way the server would: index 0 first,
/// `final` on the last chunk. `announced` is what Content-Length carried.
void deliver(AsyncWebServerRequest& request, const uint8_t* payload, size_t length,
             size_t announced, size_t chunkSize = 0) {
    RecordedRoute* route = harness->uploadRoute();
    request.setContentLength(announced);
    const size_t step = chunkSize == 0 ? length : chunkSize;
    size_t index = 0;
    if (length == 0) {
        route->uploadHandler(&request, String("firmware.bin"), 0, nullptr, 0, true);
        return;
    }
    while (index < length) {
        const size_t take = (index + step > length) ? (length - index) : step;
        const bool final = (index + take) >= length;
        route->uploadHandler(&request, String("firmware.bin"), index,
                             const_cast<uint8_t*>(payload + index), take, final);
        index += take;
    }
}

/// Runs the completion handler, which is what answers the client.
void complete(AsyncWebServerRequest& request) {
    harness->uploadRoute()->handler(&request);
}

}  // namespace

void setUp() {
    harness = new Harness();
}

void tearDown() {
    delete harness;
    harness = nullptr;
}

void test_the_upload_route_is_registered_with_an_upload_handler() {
    harness->build();
    RecordedRoute* route = harness->uploadRoute();
    TEST_ASSERT_NOT_NULL_MESSAGE(route, "/api/ota/upload was never registered");
    TEST_ASSERT_TRUE_MESSAGE(route->isUpload, "route registered without an upload handler");
    TEST_ASSERT_TRUE(static_cast<bool>(route->uploadHandler));
    TEST_ASSERT_TRUE(static_cast<bool>(route->handler));
}

void test_a_body_without_the_csrf_token_never_opens_an_update() {
    harness->build();
    AsyncWebServerRequest request;
    deliver(request, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);

    // beginUpload() sets totalBytes to the announced size; still zero means the
    // gate returned before any flash was opened.
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, harness->ota->getTotalBytes(),
                                     "an update was opened for a request with no token");
    complete(request);
    TEST_ASSERT_EQUAL_INT(403, request.sentCode);
}

void test_the_token_the_ui_hands_out_is_the_one_the_upload_accepts() {
    harness->build();
    const String token = harness->csrfToken();
    TEST_ASSERT_TRUE_MESSAGE(token.length() == 16, "the UI route returned no usable token");

    AsyncWebServerRequest request;
    request.addHeader("X-DC-Token", token);
    deliver(request, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);

    complete(request);
    TEST_ASSERT_EQUAL_INT(200, request.sentCode);
    TEST_ASSERT_TRUE_MESSAGE(request.sentBody.indexOf("\"success\":true") >= 0,
                             "a correctly-tokened upload did not report success");
}

void test_a_refused_upload_does_not_poison_the_next_one() {
    harness->build();
    const String token = harness->csrfToken();

    AsyncWebServerRequest refused;
    deliver(refused, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);
    complete(refused);
    TEST_ASSERT_EQUAL_INT(403, refused.sentCode);

    // The reset at index 0 runs before the gates, so the rejected flag the
    // refusal raised is gone by the time this body starts.
    AsyncWebServerRequest accepted;
    accepted.addHeader("X-DC-Token", token);
    deliver(accepted, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);
    complete(accepted);
    // respondJson answers 200 either way; the verdict is the body. Asserting the
    // status code alone passes with the reset removed.
    TEST_ASSERT_TRUE_MESSAGE(accepted.sentBody.indexOf("\"success\":true") >= 0,
                             "a previous refusal silently rejected the next upload");
}

void test_wrong_credentials_ask_for_authentication_and_open_nothing() {
    harness->webConfig.enableAuth = true;
    strncpy(harness->webConfig.username, "admin", sizeof(harness->webConfig.username) - 1);
    strncpy(harness->webConfig.password, "correct", sizeof(harness->webConfig.password) - 1);
    harness->build();
    const String token = harness->csrfToken("admin", "correct");

    AsyncWebServerRequest request;
    request.addHeader("X-DC-Token", token);
    request.setCredentials("admin", "wrong");
    deliver(request, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, harness->ota->getTotalBytes(),
                                     "an update was opened for a request that failed auth");
    complete(request);
    TEST_ASSERT_TRUE_MESSAGE(request.authenticationRequested,
                             "the handler answered without asking for credentials");
}

void test_right_credentials_let_the_upload_through() {
    harness->webConfig.enableAuth = true;
    strncpy(harness->webConfig.username, "admin", sizeof(harness->webConfig.username) - 1);
    strncpy(harness->webConfig.password, "correct", sizeof(harness->webConfig.password) - 1);
    harness->build();
    const String token = harness->csrfToken("admin", "correct");

    AsyncWebServerRequest request;
    request.addHeader("X-DC-Token", token);
    request.setCredentials("admin", "correct");
    deliver(request, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);

    complete(request);
    TEST_ASSERT_FALSE(request.authenticationRequested);
    TEST_ASSERT_TRUE_MESSAGE(request.sentBody.indexOf("\"success\":true") >= 0,
                             "an authenticated upload did not report success");
}

void test_a_client_that_vanishes_mid_upload_releases_the_update() {
    harness->build();
    const String token = harness->csrfToken();

    AsyncWebServerRequest request;
    request.addHeader("X-DC-Token", token);
    request.setContentLength(sizeof(kFirmware) + kMultipartEnvelope);
    harness->uploadRoute()->uploadHandler(&request, String("firmware.bin"), 0,
                                          const_cast<uint8_t*>(kFirmware), 16, false);
    TEST_ASSERT_TRUE_MESSAGE(harness->ota->isBusy(), "the update never opened");

    request.fireDisconnect();
    TEST_ASSERT_FALSE_MESSAGE(harness->ota->isBusy(),
                              "a vanished client left the update open");

    // The lock this releases is what made every later upload fail until a
    // power-cycle: a second session must be able to open.
    TEST_ASSERT_TRUE_MESSAGE(harness->ota->beginUpload(sizeof(kFirmware)),
                             "the next upload was refused after a disconnect");
}

void test_a_finished_uploads_own_disconnect_aborts_nothing() {
    harness->build();
    const String token = harness->csrfToken();

    AsyncWebServerRequest request;
    request.addHeader("X-DC-Token", token);
    deliver(request, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);
    complete(request);
    TEST_ASSERT_EQUAL_INT(200, request.sentCode);

    // onDisconnect fires after every request, successful ones included. What
    // holds here is abortUpload()'s own inactive-session guard: the handler's
    // parallel check is belt-and-braces and cannot be observed from outside.
    const size_t abortsBefore = DomoticsCore::HAL::OTAUpdate::s_stubAbortCalls;
    request.fireDisconnect();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(abortsBefore,
                                     DomoticsCore::HAL::OTAUpdate::s_stubAbortCalls,
                                     "a completed upload was aborted by its own disconnect");
}

void test_the_receive_idle_timeout_widens_only_once_the_gates_pass() {
    harness->build();
    const String token = harness->csrfToken();

    AsyncWebServerRequest refused;
    deliver(refused, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, refused.client()->getRxTimeout(),
                                     "a request with no token was granted the wider timeout");

    AsyncWebServerRequest accepted;
    accepted.addHeader("X-DC-Token", token);
    deliver(accepted, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(harness->otaConfig.uploadIdleTimeoutSec,
                                     accepted.client()->getRxTimeout(),
                                     "an accepted upload kept the server's 3 s idle limit");
}

void test_the_announced_envelope_is_narrowed_to_what_was_delivered() {
    harness->build();
    const String token = harness->csrfToken();

    AsyncWebServerRequest request;
    request.addHeader("X-DC-Token", token);
    // The shape of every browser upload: Content-Length counts the whole
    // multipart body, the handler is fed only the firmware.
    deliver(request, kFirmware, sizeof(kFirmware), sizeof(kFirmware) + kMultipartEnvelope, 16);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(sizeof(kFirmware), harness->ota->getTotalBytes(),
                                     "the reported total is the envelope, not the firmware");
}

// The provider's action handler had never been called by anything. Its refusals
// are what a person meets: the Start Update button with no URL configured is one
// click on the OTA card.
void test_every_refusal_of_the_action_handler_names_its_reason(void) {
    Harness h;
    h.build();
    auto refused = [](const String& r, const char* reason) {
        TEST_ASSERT_TRUE_MESSAGE(r.indexOf("\"success\":false") >= 0, r.c_str());
        TEST_ASSERT_TRUE_MESSAGE(r.indexOf(reason) >= 0, r.c_str());
    };
    std::map<String, String> params;
    params["field"] = String("start_update");
    params["value"] = String("clicked");

    refused(h.provider->handleWebUIRequest(String("ota_unified"), String("/"), String("POST"), params),
            "No firmware URL configured");

    std::map<String, String> unknown;
    unknown["field"] = String("nope");
    unknown["value"] = String("1");
    refused(h.provider->handleWebUIRequest(String("ota_unified"), String("/"), String("POST"), unknown),
            "Unknown field");
    refused(h.provider->handleWebUIRequest(String("ota_unified"), String("/"), String("PUT"), params),
            "Method not allowed");
    refused(h.provider->handleWebUIRequest(String("ota_unified"), String("/"), String("POST"),
                                           std::map<String, String>()),
            "Invalid request");

    WebUI::OTAWebUI orphan(nullptr);
    refused(orphan.handleWebUIRequest(String("ota_unified"), String("/"), String("POST"), params),
            "Component not available");
}

// A URL the page stored is the one the button uses, so the refusal above is
// about the configuration and not about the button.
void test_a_configured_url_is_stored(void) {
    Harness h;
    h.build();
    std::map<String, String> params;
    params["field"] = String("update_url");
    params["value"] = String("http://example.invalid/fw.bin");
    const String stored = h.provider->handleWebUIRequest(String("ota_unified"), String("/"),
                                                         String("POST"), params);
    TEST_ASSERT_TRUE_MESSAGE(stored.indexOf("\"success\":true") >= 0, stored.c_str());
    TEST_ASSERT_EQUAL_STRING("http://example.invalid/fw.bin", h.ota->getConfig().updateUrl.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_the_upload_route_is_registered_with_an_upload_handler);
    RUN_TEST(test_a_body_without_the_csrf_token_never_opens_an_update);
    RUN_TEST(test_the_token_the_ui_hands_out_is_the_one_the_upload_accepts);
    RUN_TEST(test_a_refused_upload_does_not_poison_the_next_one);
    RUN_TEST(test_wrong_credentials_ask_for_authentication_and_open_nothing);
    RUN_TEST(test_right_credentials_let_the_upload_through);
    RUN_TEST(test_a_client_that_vanishes_mid_upload_releases_the_update);
    RUN_TEST(test_a_finished_uploads_own_disconnect_aborts_nothing);
    RUN_TEST(test_the_receive_idle_timeout_widens_only_once_the_gates_pass);
    RUN_TEST(test_the_announced_envelope_is_narrowed_to_what_was_delivered);
    RUN_TEST(test_every_refusal_of_the_action_handler_names_its_reason);
    RUN_TEST(test_a_configured_url_is_stored);
    return UNITY_END();
}
