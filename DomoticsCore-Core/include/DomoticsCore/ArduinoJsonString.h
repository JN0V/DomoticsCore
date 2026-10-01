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
#include <string>

// ArduinoJson 7 converter functions for stub String class
// These use ADL (Argument Dependent Lookup) and must be in global namespace
// since String is in global namespace

/**
 * @brief Check if JsonVariant can be converted to String
 *
 * Only a string, as ArduinoJson's own Arduino String support: a missing key
 * must fall back to the default of `doc[k] | String(dflt)` on the host too.
 */
inline bool canConvertFromJson(ArduinoJson::JsonVariantConst src, const String&) {
    return src.is<ArduinoJson::JsonString>();
}

/**
 * @brief Convert JsonVariant to String
 *
 * A non-string value is serialized, as on the boards: null reads "null".
 */
inline void convertFromJson(ArduinoJson::JsonVariantConst src, String& dst) {
    ArduinoJson::JsonString str = src.as<ArduinoJson::JsonString>();
    if (str) {
        dst = String(str.c_str());
        return;
    }
    std::string text;
    ArduinoJson::serializeJson(src, text);
    dst = String(text.c_str());
}

/**
 * @brief Convert String to JsonVariant
 */
inline bool convertToJson(const String& src, ArduinoJson::JsonVariant dst) {
    return dst.set(src.c_str());
}

#endif // !defined(ARDUINO)
