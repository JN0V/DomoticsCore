#pragma once

/**
 * @file WebUI_ESP32.h
 * @brief ESP32-specific WebUI configuration
 */

// ESP32-specific WebUI buffer sizes
// ESP32 has ~520KB RAM, so we can afford larger buffers

/**
 * @brief WebSocket buffer size for ESP32
 * Used for broadcasting WebSocket updates with all context data
 * ArduinoJson 7 manages JsonDocument sizes dynamically
 */
#ifndef WEBUI_WS_BUFFER_SIZE
#define WEBUI_WS_BUFFER_SIZE 8192
#endif
