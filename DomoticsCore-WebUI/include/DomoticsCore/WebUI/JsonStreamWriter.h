#pragma once

/**
 * @file JsonStreamWriter.h
 * @brief Resumable JSON streaming primitives shared by the context and field
 *        state machines of StreamingContextSerializer.
 *
 * Extracted from StreamingContextSerializer.h (SIZE-2) as a
 * privately-inherited base so that no call site in either state machine
 * changes: the switches keep calling writeLiteral()/writeJsonString()/
 * isLiteralComplete() unqualified and reading stringOffset/numBuf directly.
 *
 * Everything here supports partial writes: a call that cannot fit the rest
 * of its output records where it stopped (currentLiteral/literalOffset for
 * literals, stringOffset for JSON strings) and continues from there on the
 * next call. Both state machines share this state deliberately — only one
 * literal or string streams at any moment, and splitting the state would
 * break resumability across the machines' boundary.
 *
 * Two contracts worth knowing before touching anything:
 *
 * - writeLiteral() resumes on POINTER identity. A resumed call must pass the
 *   same pointer it was interrupted with; every state machine case satisfies
 *   this by re-executing the same call site with the same expression. Do not
 *   pass a freshly-built temporary's c_str() — its address may move between
 *   resumes. (The multiselect path did exactly that until SIZE-2's lot.)
 *
 * - Escape sequences are atomic within one write. A \u00XX escape needs six
 *   free bytes and a two-character escape needs two; when they do not fit,
 *   the resume position does not advance, so a buffer smaller than the
 *   longest escape in the content can make no progress at all. Callers hand
 *   this writer HTTP-chunk-sized buffers, far above that floor.
 */

#include <DomoticsCore/Platform_HAL.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

class JsonStreamWriter {
protected:
    // Literal streaming state (writeLiteral)
    const char* currentLiteral = nullptr;
    size_t literalOffset = 0;

    // JSON-string streaming state (writeJsonString): 0 = not started,
    // otherwise 1 + position within the source string.
    size_t stringOffset = 0;

    // Number scratch buffer. The state machines snprintf into it and then
    // stream it via writeLiteral; it needs no reset, every reader fills it
    // first, and its address is stable so the pointer-identity resume holds.
    char numBuf[16];

    /**
     * @brief Reset the streaming state (the three offset/pointer members).
     *
     * Called from begin(): production reuses one serializer across contexts,
     * so an abandoned run's mid-literal or mid-string state must not leak
     * into the next context.
     */
    void resetWriter() {
        currentLiteral = nullptr;
        literalOffset = 0;
        stringOffset = 0;
    }

    /**
     * @brief Write a literal string to buffer, supporting partial writes
     * @return Bytes written (may be partial if buffer is smaller than remaining literal)
     *
     * Uses currentLiteral and literalOffset to track position for streaming.
     */
    size_t writeLiteral(uint8_t* buffer, size_t maxLen, const char* str) {
        // Start new literal or continue existing one
        if (currentLiteral != str) {
            currentLiteral = str;
            literalOffset = 0;
        }

        size_t totalLen = strlen(str);
        size_t remaining = totalLen - literalOffset;

        if (remaining == 0) {
            currentLiteral = nullptr;
            literalOffset = 0;
            return 0;
        }

        size_t toWrite = (remaining < maxLen) ? remaining : maxLen;
        memcpy(buffer, str + literalOffset, toWrite);
        literalOffset += toWrite;

        // Check if complete
        if (literalOffset >= totalLen) {
            currentLiteral = nullptr;
            literalOffset = 0;
        }

        return toWrite;
    }

    /**
     * @brief Check if current literal is complete (or no literal in progress)
     */
    bool isLiteralComplete() const {
        return currentLiteral == nullptr;
    }

    /**
     * @brief Write a JSON-escaped string, streaming across multiple calls
     *
     * The String and const char* overloads were two 70-line near-copies until
     * SIZE-2; both now delegate to the (data, len) core. The String overload
     * passes length() — an embedded NUL would be escaped, as before — and the
     * C-string overload measures with strlen, stopping at a NUL, as before.
     */
    size_t writeJsonString(uint8_t* buffer, size_t maxLen, const String& str) {
        return writeJsonStringCore(buffer, maxLen, str.c_str(),
                                   static_cast<size_t>(str.length()));
    }

    /**
     * @brief Write a JSON-escaped C-string (const char*), null-safe
     */
    size_t writeJsonString(uint8_t* buffer, size_t maxLen, const char* str) {
        const char* p = (str != nullptr) ? str : "";
        return writeJsonStringCore(buffer, maxLen, p, strlen(p));
    }

private:
    size_t writeJsonStringCore(uint8_t* buffer, size_t maxLen,
                               const char* data, size_t len) {
        size_t written = 0;

        // Opening quote
        if (stringOffset == 0) {
            if (maxLen < 1) return 0;
            buffer[written++] = '"';
            stringOffset = 1;  // Mark that we've written opening quote
        }

        // String content with escaping
        // stringOffset-1 is the position in the actual string
        size_t strPos = stringOffset - 1;

        while (written < maxLen && strPos < len) {
            char c = data[strPos];
            const char* escaped = nullptr;

            // Check if character needs escaping
            switch (c) {
                case '"':  escaped = "\\\""; break;
                case '\\': escaped = "\\\\"; break;
                case '\n': escaped = "\\n"; break;
                case '\r': escaped = "\\r"; break;
                case '\t': escaped = "\\t"; break;
                default:
                    // BUG-33: unsigned comparison — with plain char the native
                    // host (signed char) sign-extends UTF-8 bytes negative and
                    // mangles them into \u00XX; every shipped target defines
                    // __CHAR_UNSIGNED__ and never did. Match the boards.
                    if (static_cast<unsigned char>(c) < 0x20) {
                        // Control character - write as \u00XX
                        if (written + 6 > maxLen) {
                            stringOffset = strPos + 1;
                            return written;
                        }
                        static const char hex[] = "0123456789abcdef";
                        buffer[written++] = '\\';
                        buffer[written++] = 'u';
                        buffer[written++] = '0';
                        buffer[written++] = '0';
                        buffer[written++] = hex[(c >> 4) & 0xF];
                        buffer[written++] = hex[c & 0xF];
                        strPos++;
                        continue;
                    }
                    break;
            }

            if (escaped) {
                size_t escLen = strlen(escaped);
                if (written + escLen > maxLen) {
                    stringOffset = strPos + 1;
                    return written;
                }
                memcpy(buffer + written, escaped, escLen);
                written += escLen;
            } else {
                buffer[written++] = c;
            }
            strPos++;
        }

        // Closing quote
        if (strPos >= len) {
            if (written < maxLen) {
                buffer[written++] = '"';
                stringOffset = 0;  // Reset for next string
                return written;
            }
        }

        stringOffset = strPos + 1;
        return written;
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
