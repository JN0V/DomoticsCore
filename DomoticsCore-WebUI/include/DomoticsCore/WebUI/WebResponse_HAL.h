#pragma once

/**
 * @file WebResponse_HAL.h
 * @brief HAL for ESPAsyncWebServer response creation.
 * 
 * Abstracts API differences between ESP32 and ESP8266 versions of ESPAsyncWebServer.
 * Constitution IX: Platform detection only in HAL files.
 */

#include <ESPAsyncWebServer.h>
#include "DomoticsCore/Platform_HAL.h"

namespace DomoticsCore {
namespace HAL {
namespace WebResponse {

/**
 * @brief Create a response with PROGMEM/flash data (gzipped assets)
 * @param request The web request
 * @param code HTTP status code
 * @param contentType MIME type
 * @param data Pointer to data (PROGMEM on ESP8266, flash on ESP32)
 * @param len Length of data
 * @return AsyncWebServerResponse pointer
 */
inline AsyncWebServerResponse* createProgmemResponse(
    AsyncWebServerRequest* request,
    int code,
    const String& contentType,
    const uint8_t* data,
    size_t len
) {
#if DOMOTICS_PLATFORM_ESP32
    // ESP32: Direct pointer to flash works
    return request->beginResponse(code, contentType, data, len);
#elif DOMOTICS_PLATFORM_ESP8266
    // ESP8266: Use beginResponse_P for PROGMEM data
    return request->beginResponse_P(code, contentType, data, len);
#else
    // Stub: Return simple response
    return request->beginResponse(code, contentType, "Not supported");
#endif
}

/**
 * @brief Send a PROGMEM response with gzip encoding
 * @param request The web request
 * @param contentType MIME type
 * @param data Pointer to gzipped data
 * @param len Length of data
 * @param cacheSeconds Cache duration (0 = no cache, default = 1 hour for static assets)
 */
inline void sendGzipResponse(
    AsyncWebServerRequest* request,
    const String& contentType,
    const uint8_t* data,
    size_t len,
    int cacheSeconds = 3600
) {
    auto* resp = createProgmemResponse(request, 200, contentType, data, len);
    resp->addHeader("Content-Encoding", "gzip");
    if (cacheSeconds > 0) {
        // Cache static assets to reduce concurrent requests on ESP8266
        char cacheHeader[64];
        snprintf(cacheHeader, sizeof(cacheHeader), "public, max-age=%d", cacheSeconds);
        resp->addHeader("Cache-Control", cacheHeader);
    } else {
        resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    }
    request->send(resp);
}

} // namespace WebResponse
} // namespace HAL
} // namespace DomoticsCore
