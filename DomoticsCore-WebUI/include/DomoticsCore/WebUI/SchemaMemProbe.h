#pragma once

/**
 * @file SchemaMemProbe.h
 * @brief Heap-staging diagnostics for schema generation (SIZE-1 extraction).
 *
 * Extracted verbatim from WebUIComponent: a fixed pool of probes, each armed
 * when a /api/ui/schema request is queued, sampling free heap and max-alloc
 * before the response, then logging deltas at +500ms, +2s and +10s from the
 * component loop. Diagnostics only — every emission is DLOG_D and nothing
 * branches on any of it; the value is watching allocation behaviour around
 * chunked schema responses on real boards without attaching a debugger.
 */

#include "DomoticsCore/Logger.h"  // DLOG_D and the LOG_WEB tag
#include "DomoticsCore/Platform_HAL.h"

#include <cstdint>

namespace DomoticsCore {
namespace Components {
namespace WebUI {

class SchemaMemProbes {
public:
    /**
     * @brief The values an onDisconnect callback captures by value: the probe
     *        sequence number and the pre-send heap samples.
     */
    struct Armed {
        uint32_t seq = 0;
        uint32_t heapBefore = 0;
        uint32_t maxBefore = 0;
    };

    /**
     * @brief Claim the next probe slot (round-robin) and sample the heap.
     */
    Armed arm() {
        Probe& p = probes_[next_ % SLOTS];
        next_ = static_cast<uint8_t>(next_ + 1);

        p.active = true;
        p.seq = ++seq_;
        p.stage = 0;
        p.t0 = HAL::Platform::getMillis();
        p.heapBefore = HAL::Platform::getFreeHeap();
        p.maxBefore = HAL::Platform::getMaxAllocHeap();

        Armed a;
        a.seq = p.seq;
        a.heapBefore = p.heapBefore;
        a.maxBefore = p.maxBefore;
        return a;
    }

    /**
     * @brief Log the post-queue heap deltas for a just-armed probe.
     */
    void logQueued(const Armed& a) const {
        const uint32_t h = HAL::Platform::getFreeHeap();
        const uint32_t m = HAL::Platform::getMaxAllocHeap();
        DLOG_D(LOG_WEB, "Schema queued #%u: heap=%u (delta=%d), max=%u (delta=%d)",
               (unsigned)a.seq,
               (unsigned)h, (int)h - (int)a.heapBefore,
               (unsigned)m, (int)m - (int)a.maxBefore);
    }

    /**
     * @brief Emit the staged +500ms/+2s/+10s delta logs for active probes.
     *        Called from the component loop.
     */
    void tick() {
        for (uint8_t i = 0; i < SLOTS; i++) {
            Probe& p = probes_[i];
            if (!p.active) continue;

            const unsigned long now = HAL::Platform::getMillis();
            const unsigned long dt = now - p.t0;

            if (p.stage == 0 && dt >= 500) {
                logStage(p, "+500ms");
                p.stage = 1;
            } else if (p.stage == 1 && dt >= 2000) {
                logStage(p, "+2s");
                p.stage = 2;
            } else if (p.stage == 2 && dt >= 10000) {
                logStage(p, "+10s");
                p.active = false;
            }
        }
    }

private:
    struct Probe {
        bool active = false;
        uint32_t seq = 0;
        unsigned long t0 = 0;
        uint32_t heapBefore = 0;
        uint32_t maxBefore = 0;
        uint8_t stage = 0;
    };

    static constexpr uint8_t SLOTS = 6;
    Probe probes_[SLOTS];
    uint32_t seq_ = 0;
    uint8_t next_ = 0;

    static void logStage(const Probe& p, const char* label) {
        const uint32_t h = HAL::Platform::getFreeHeap();
        const uint32_t m = HAL::Platform::getMaxAllocHeap();
        DLOG_D(LOG_WEB, "Schema mem #%u %s: heap=%u (delta=%d), max=%u (delta=%d)",
               (unsigned)p.seq, label,
               (unsigned)h, (int)h - (int)p.heapBefore,
               (unsigned)m, (int)m - (int)p.maxBefore);
    }
};

} // namespace WebUI
} // namespace Components
} // namespace DomoticsCore
