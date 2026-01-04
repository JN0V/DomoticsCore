#pragma once

/**
 * @file WebUI_ESP8266.h
 * @brief ESP8266-specific WebUI configuration
 *
 * Constitution Principle IX: All platform-specific code in HAL files
 */

// ESP8266-specific WebUI buffer sizes
// ESP8266 has only ~80KB RAM, must use smaller buffers

/**
 * @brief WebSocket buffer size for ESP8266
 * Smaller buffer to fit in limited RAM - will truncate if too many contexts
 */
#ifndef WEBUI_WS_BUFFER_SIZE
#define WEBUI_WS_BUFFER_SIZE 2048
#endif

/**
 * @brief Maximum number of WebUI providers on ESP8266
 * Limits memory consumption from provider registry
 */
#ifndef WEBUI_MAX_PROVIDERS
#define WEBUI_MAX_PROVIDERS 8
#endif

/**
 * @brief Maximum WebSocket clients on ESP8266
 */
#ifndef WEBUI_MAX_WS_CLIENTS
#define WEBUI_MAX_WS_CLIENTS 2
#endif
