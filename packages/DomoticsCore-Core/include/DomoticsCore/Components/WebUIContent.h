#pragma once

#include <Arduino.h>
// Generated at build time by scripts/embed_webui.py
#include <DomoticsCore/Generated/WebUIAssets.h>

namespace DomoticsCore {
namespace Components {

class WebUIContent {
public:
    // Gzipped PROGMEM asset accessors (generated)
    static const uint8_t* htmlGz() { return WEBUI_HTML_GZ; }
    static size_t         htmlGzLen() { return WEBUI_HTML_GZ_LEN; }
    static const uint8_t* cssGz()  { return WEBUI_CSS_GZ; }
    static size_t         cssGzLen() { return WEBUI_CSS_GZ_LEN; }
    static const uint8_t* jsGz()   { return WEBUI_JS_GZ; }
    static size_t         jsGzLen() { return WEBUI_JS_GZ_LEN; }
};

} // namespace Components
} // namespace DomoticsCore
