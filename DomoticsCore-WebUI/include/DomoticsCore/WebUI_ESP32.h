#pragma once

/**
 * @file WebUI_ESP32.h
 * @brief ESP32-specific WebUI configuration
 *
 * Constitution Principle IX: All platform-specific code in HAL files
 */

// ESP32-specific WebUI buffer sizes
// ESP32 has ~520KB RAM, so we can afford larger buffers

/**
 * @brief WebSocket buffer size for ESP32
 * Used for broadcasting WebSocket updates with all context data
 */
#ifndef WEBUI_WS_BUFFER_SIZE
#define WEBUI_WS_BUFFER_SIZE 8192
#endif

/**
 * @brief Maximum number of WebUI providers on ESP32
 */
#ifndef WEBUI_MAX_PROVIDERS
#define WEBUI_MAX_PROVIDERS 32
#endif

/**
 * @brief Maximum WebSocket clients on ESP32
 */
#ifndef WEBUI_MAX_WS_CLIENTS
#define WEBUI_MAX_WS_CLIENTS 8
#endif
