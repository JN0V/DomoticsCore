#pragma once

/**
 * @file JsonEscape.h
 * @brief One stateless JSON-string escaper for `char*` sinks (BUG-32).
 */

#include <cstddef>
#include <cstdio>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief Escape `in` as a JSON string body (no surrounding quotes) into `out`.
 *
 * Writes at most `cap - 1` bytes plus a terminating NUL. **Never emits a partial
 * escape sequence**: if the next escaped character would not fit whole, it stops
 * before it rather than writing a truncated `\\` or `\\u00` — a lone trailing
 * backslash would escape the closing quote the caller appends and reintroduce the
 * exact corruption this exists to prevent. Returns the number of bytes written,
 * excluding the NUL.
 *
 * This is the stateless escaper for `char*` sinks. `StreamingContextSerializer`
 * keeps its own resumable escaper for `uint8_t*` chunk boundaries; this does not
 * replace it. It replaces the dead `printJsonEscaped` that `WebUI.h` once carried
 * and nothing called (removed with BUG-32).
 */
inline size_t jsonEscape(char* out, size_t cap, const char* in) {
    if (!out || cap == 0) return 0;
    size_t w = 0;
    if (in) {
        for (const char* p = in; *p; ++p) {
            const unsigned char c = static_cast<unsigned char>(*p);
            const char* esc = nullptr;
            char ubuf[7];
            size_t need;
            switch (c) {
                case '"':  esc = "\\\""; need = 2; break;
                case '\\': esc = "\\\\"; need = 2; break;
                case '\n': esc = "\\n";  need = 2; break;
                case '\r': esc = "\\r";  need = 2; break;
                case '\t': esc = "\\t";  need = 2; break;
                default:
                    if (c < 0x20) {
                        // JSON requires \u00xx for the other control characters.
                        snprintf(ubuf, sizeof(ubuf), "\\u%04x", c);
                        esc = ubuf;
                        need = 6;
                    } else {
                        need = 1;  // pass through
                    }
            }
            // Leave room for the NUL: w + need must stay below cap.
            if (w + need >= cap) break;  // never a partial escape
            if (esc) {
                for (size_t i = 0; i < need; ++i) out[w++] = esc[i];
            } else {
                out[w++] = static_cast<char>(c);
            }
        }
    }
    out[w] = '\0';
    return w;
}

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
