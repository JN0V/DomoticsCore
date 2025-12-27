#pragma once

/**
 * @file WebUI_HAL.h
 * @brief Hardware Abstraction Layer for WebUI component
 *
 * Provides platform-specific configuration constants for WebUI,
 * following Constitution Principle IX (HAL).
 *
 * Platform-specific implementations:
 * - WebUI_ESP32.h: ESP32 with larger buffers
 * - WebUI_ESP8266.h: ESP8266 with constrained buffers
 * - WebUI_Stub.h: Native/test platform
 */

#include <DomoticsCore/Platform_HAL.h>

// Platform-specific implementation routing
#if defined(ARDUINO_ARCH_ESP32)
    #include "WebUI_ESP32.h"
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
    #include "WebUI_ESP8266.h"
#else
    #include "WebUI_Stub.h"
#endif
