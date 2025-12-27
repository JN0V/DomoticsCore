#pragma once

/**
 * @file WebUI_ESP8266.h
 * @brief ESP8266-specific WebUI configuration
 */

// ESP8266-specific WebUI buffer sizes
// ESP8266 has only ~80KB RAM, must use smaller buffers

/**
 * @brief WebSocket buffer size for ESP8266
 * Smaller buffer to fit in limited RAM - will truncate if too many contexts
 * ArduinoJson 7 manages JsonDocument sizes dynamically
 */
#ifndef WEBUI_WS_BUFFER_SIZE
#define WEBUI_WS_BUFFER_SIZE 2048
#endif
