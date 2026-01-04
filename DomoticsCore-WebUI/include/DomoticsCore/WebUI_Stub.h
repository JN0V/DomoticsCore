#pragma once

/**
 * @file WebUI_Stub.h
 * @brief Stub/native platform WebUI configuration
 *
 * Constitution Principle IX: All platform-specific code in HAL files
 */

// Native/stub platform WebUI buffer sizes
// Moderate sizes for testing

/**
 * @brief WebSocket buffer size for stub/native platforms
 */
#ifndef WEBUI_WS_BUFFER_SIZE
#define WEBUI_WS_BUFFER_SIZE 4096
#endif

/**
 * @brief Maximum number of WebUI providers
 */
#ifndef WEBUI_MAX_PROVIDERS
#define WEBUI_MAX_PROVIDERS 32
#endif

/**
 * @brief Maximum WebSocket clients
 */
#ifndef WEBUI_MAX_WS_CLIENTS
#define WEBUI_MAX_WS_CLIENTS 8
#endif
