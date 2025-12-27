#pragma once

/**
 * @file ArduinoJsonString.h
 * @brief ArduinoJson 7 compatibility converters for stub String class
 *
 * Include this header AFTER ArduinoJson.h when using the stub String class
 * in native tests. Provides the necessary converter functions for ArduinoJson
 * to work with our String type.
 *
 * Only active when Platform_Stub.h is used (native tests).
 * Arduino platforms (ESP32, ESP8266, AVR, etc.) use Arduino's native String
 * class which ArduinoJson already supports.
 *
 * @note Future platforms (Arduino UNO/Mega/AVR) will use Arduino's String,
 * not our stub, so this header won't interfere with them.
 */

// Only for native/stub platforms - exclude ALL Arduino platforms
// Arduino's String class is already supported by ArduinoJson natively
#if !defined(ARDUINO)

#include <ArduinoJson.h>

// ArduinoJson 7 converter functions for stub String class
// These use ADL (Argument Dependent Lookup) and must be in global namespace
// since String is in global namespace

/**
 * @brief Check if JsonVariant can be converted to String
 */
inline bool canConvertFromJson(ArduinoJson::JsonVariantConst src, const String&) {
    return src.is<const char*>() || src.isNull();
}

/**
 * @brief Convert JsonVariant to String
 */
inline void convertFromJson(ArduinoJson::JsonVariantConst src, String& dst) {
    const char* str = src.as<const char*>();
    dst = str ? String(str) : String();
}

/**
 * @brief Convert String to JsonVariant
 */
inline bool convertToJson(const String& src, ArduinoJson::JsonVariant dst) {
    return dst.set(src.c_str());
}

#endif // !defined(ARDUINO)
