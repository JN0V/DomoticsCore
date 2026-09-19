#pragma once

/**
 * @file WebUIAssets.h
 * @brief Mock of the generated web assets, under the real generated path.
 *
 * The real header is produced by embed_webui.py and gitignored, so it exists on
 * a machine that has built the WebUI and nowhere else — a host suite that
 * depended on it would pass locally and fail on a clean checkout. The bytes are
 * the browser's, never the handler's, so an empty set is the honest stand-in.
 */

#include <DomoticsCore/Platform_HAL.h>

inline const uint8_t WEBUI_HTML_GZ[] = {0};
inline const size_t WEBUI_HTML_GZ_LEN = 0;

inline const uint8_t WEBUI_CSS_GZ[] = {0};
inline const size_t WEBUI_CSS_GZ_LEN = 0;

inline const uint8_t WEBUI_JS_GZ[] = {0};
inline const size_t WEBUI_JS_GZ_LEN = 0;

inline const uint8_t WEBUI_COMBINED_GZ[] = {0};
inline const size_t WEBUI_COMBINED_GZ_LEN = 0;
