#pragma once

/**
 * @file WebUI_Stub.h
 * @brief Stub/native platform WebUI configuration
 */

// Native/stub platform WebUI buffer sizes
// Moderate sizes for testing

/**
 * @brief WebSocket buffer size for stub/native platforms
 * ArduinoJson 7 manages JsonDocument sizes dynamically
 */
#ifndef WEBUI_WS_BUFFER_SIZE
#define WEBUI_WS_BUFFER_SIZE 4096
#endif
