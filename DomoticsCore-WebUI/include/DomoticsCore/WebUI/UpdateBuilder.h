#pragma once

/**
 * @file UpdateBuilder.h
 * @brief The full WebUI update assembly, extracted from
 *        `WebUIComponent::buildUpdateJson` (SIZE-1) beside the
 *        `buildSystemHeader` slice BUG-32 extracted before it.
 *
 * This is the function BUG-32's crowding lives in: contexts are appended
 * until the buffer runs short and are DROPPED SILENTLY past that point, and
 * device-name escaping can consume up to ~155 bytes of the budget. Extracting
 * it gives the crowding a native home at the level where it happens — the
 * previous tests could only reach the header step.
 */

#include <cstddef>
#include <cstdint>
#include <map>
#include "DomoticsCore/Platform_HAL.h"  // String, DSNPRINTF_P
#include "DomoticsCore/IWebUIProvider.h"
#include "DomoticsCore/WebUI/SystemHeader.h"

namespace DomoticsCore {
namespace Components {
namespace WebUI {

/**
 * @brief Build one `{"system":{...},"contexts":{...}}` update into `buf`.
 *
 * Walks `contextProviders` in map order. A provider whose data is empty or
 * `"{}"` is skipped; when `forceFull` is false a provider is also skipped
 * unless `forceNext` is set or its `hasDataChanged(contextId)` reports a
 * change (the delta path used by the periodic broadcast; polling passes
 * `forceFull=true`). Contexts that no longer fit are dropped, never
 * truncated: the 512-byte early break and the per-context `needed` bound
 * both guard the closing braces, so the output is complete JSON even when
 * crowded.
 *
 * @return length written excluding the NUL, or 0 on failure (header did not
 *         fit, or the closing braces did not).
 */
inline int buildUpdateJson(char* buf, size_t bufSize,
                           const std::map<String, IWebUIProvider*>& contextProviders,
                           const char* deviceName,
                           uint32_t millisNow, uint32_t freeHeap, int wsClients,
                           bool forceFull, bool forceNext) {
    int pos = buildSystemHeader(buf, bufSize, millisNow, freeHeap, wsClients, deviceName);

    if (pos < 0 || pos >= (int)bufSize) return 0;

    int contextCount = 0;

    for (const auto& pair : contextProviders) {
        if (pos > (int)bufSize - 512) break;

        const String& contextId = pair.first;
        IWebUIProvider* provider = pair.second;

        // Delta check - skip unchanged data (only for SSE broadcast, not polling)
        if (!forceFull && !forceNext && !provider->hasDataChanged(contextId)) continue;

        String contextData = provider->getWebUIData(contextId);
        if (contextData.isEmpty() || contextData == "{}") continue;

        int needed = contextId.length() + contextData.length() + 5;
        if (pos + needed >= (int)bufSize - 10) break;

        if (contextCount > 0) buf[pos++] = ',';

        int written = DSNPRINTF_P(buf + pos, bufSize - pos,
            "\"%s\":%s", contextId.c_str(), contextData.c_str());

        if (written > 0 && pos + written < (int)bufSize) {
            pos += written;
            contextCount++;
        }
    }

    if (pos < (int)bufSize - 3) {
        buf[pos++] = '}';
        buf[pos++] = '}';
        buf[pos] = '\0';
        return pos;
    }
    return 0;
}

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
