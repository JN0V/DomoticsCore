#pragma once

#include <stddef.h>
#include <stdint.h>

namespace DomoticsCore {
namespace Utils {

/**
 * Per-address memory of failed authentication attempts, deciding when the
 * next attempt from that address may be read. It never refuses anything:
 * the wait starts at one second, doubles with each consecutive failure up to
 * a cap, and an address quiet for a minute is forgotten. Fixed table, no
 * allocation; when it is full the address that failed longest ago is evicted.
 */
class AuthDelay {
public:
    static constexpr size_t ENTRIES = 4;
    static constexpr unsigned long FORGET_MS = 60000;

    /** The wait after `failures` consecutive failures, capped at `capMs` (0 = no wait). */
    static unsigned long waitMs(uint8_t failures, uint32_t capMs) {
        if (failures == 0 || capMs == 0) return 0;
        const unsigned shift = failures > 16 ? 16 : failures - 1;
        const unsigned long wait = 1000UL << shift;
        return wait < capMs ? wait : capMs;
    }

    /** When the next attempt from `ip` may be read; `now` when it is not waiting. */
    unsigned long notBefore(uint32_t ip, unsigned long now, uint32_t capMs) const {
        const Entry* e = find(ip);
        if (!e || now - e->lastFailureAt >= FORGET_MS) return now;
        return e->lastFailureAt + waitMs(e->failures, capMs);
    }

    /** True while `ip` is inside the wait its last failure set. */
    bool isWaiting(uint32_t ip, unsigned long now, uint32_t capMs) const {
        return (long)(now - notBefore(ip, now, capMs)) < 0;
    }

    /** Records a failure from `ip` and returns its count of consecutive failures. */
    uint8_t noteFailure(uint32_t ip, unsigned long now) {
        Entry* e = find(ip);
        if (e && now - e->lastFailureAt >= FORGET_MS) e->failures = 0;
        if (!e) {
            e = &entries[0];   // a free slot, else the one that failed longest ago
            for (auto& c : entries) {
                if (c.failures == 0) { e = &c; break; }
                if (now - c.lastFailureAt > now - e->lastFailureAt) e = &c;
            }
            e->ip = ip;
            e->failures = 0;
        }
        if (e->failures < 255) e->failures++;
        e->lastFailureAt = now;
        return e->failures;
    }

    /** A success clears the address. */
    void forget(uint32_t ip) {
        if (Entry* e = find(ip)) e->failures = 0;
    }

private:
    struct Entry { uint32_t ip = 0; uint8_t failures = 0; unsigned long lastFailureAt = 0; };
    Entry entries[ENTRIES];

    Entry* find(uint32_t ip) {
        for (auto& e : entries) if (e.failures != 0 && e.ip == ip) return &e;
        return nullptr;
    }
    const Entry* find(uint32_t ip) const {
        for (const auto& e : entries) if (e.failures != 0 && e.ip == ip) return &e;
        return nullptr;
    }
};

} // namespace Utils
} // namespace DomoticsCore
