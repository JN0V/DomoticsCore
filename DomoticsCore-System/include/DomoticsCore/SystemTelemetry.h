#ifndef DOMOTICS_CORE_SYSTEM_TELEMETRY_H
#define DOMOTICS_CORE_SYSTEM_TELEMETRY_H

/**
 * @file SystemTelemetry.h
 * @brief What leaves the device about its own health: a telemetry sample on
 * an interval and the last death at every broker connect.
 *
 * Pure: it knows nothing about MQTT, Home Assistant or the HAL. System hands
 * it a sink and fills the samples; a test hands it a scripted sink. Both
 * payloads are built with snprintf into stack buffers, so the publisher
 * allocates nothing on the device's side of the client.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <functional>

namespace DomoticsCore {
namespace SystemHelpers {

/** @brief One tick's reading. `trough` is the flight recorder's, 16-byte granular. */
struct TelemetrySample {
    uint32_t heap = 0;        // allocatable free heap, bytes
    uint32_t largest = 0;     // largest free block, bytes
    uint32_t trough = 0;      // the lowest heap inside the recorder's last 10 s sampling window
    uint32_t uptimeS = 0;     // wrap-aware: from noteMillis(), not a bare millis()/1000
    uint32_t bootCount = 0;   // 0 when nothing persists it
    bool     hasRssi = false; // false: no station link, `rssi` is published as null
    int32_t  rssi = 0;
    uint32_t fails = 0;       // survived allocation failures this boot
    uint32_t failLast = 0;    // the last one's size, bytes
    uint32_t buildId = 0;
};

/** @brief The last death, from the persisted record or from RTC. */
struct CrashSummary {
    enum Source : uint8_t { None = 0, Persisted = 1, Rtc = 2 };
    Source source = None;
    const char* promotion = "none";   // FlightRecorder::promotionName(): static text
    char reset[24] = {0};             // the reason the promotion saw; RTC-sourced only
    bool torn = false;                // crc failed: phase and promotion only
    uint32_t phase = 0, buildId = 0, uptimeMs = 0, reason = 0, epc1 = 0;
    uint32_t failSize = 0, failCaller = 0, minFree = 0, dedupKey = 0;
    uint32_t sameCount = 0;           // persisted only; 0 leaves it out
    bool coreDumpSupported = false, coreDumpWaiting = false;
    uint32_t coreDumpSize = 0;
};

class SystemTelemetry {
public:
    enum : size_t { TOPIC_MAX = 96, CLIENT_ID_MAX = 80, TELEMETRY_MAX = 256, CRASH_MAX = 512 };

    /** @brief One Home Assistant diagnostic sensor over the telemetry topic: its discovery fields and the payload key it reads. */
    struct EntityDef {
        const char* id; const char* name; const char* unit; const char* deviceClass;
        const char* icon; const char* stateClass; const char* key;
    };
    /** The seven sensors over `{clientId}/telemetry`; the last-death sensor over `{clientId}/crash` is LAST_DEATH_ID. */
    static const EntityDef* entityDefs(size_t& count) {
        static const EntityDef defs[] = {
            { "free_heap",          "Free Heap",           "B",   "data_size",       "mdi:memory",        "measurement",      "heap" },
            { "sys_heap_largest",   "Largest Free Block",  "B",   "data_size",       "mdi:memory",        "measurement",      "largest" },
            { "sys_heap_min",       "Minimum Free Heap",   "B",   "data_size",       "mdi:memory",        "measurement",      "min" },
            { "uptime",             "Uptime",              "s",   "duration",        "mdi:clock-outline", "total_increasing", "uptime" },
            { "sys_boot_count",     "Boot Count",          "",    "",                "mdi:restart",       "total_increasing", "boot" },
            { "wifi_signal",        "WiFi Signal",         "dBm", "signal_strength", "mdi:wifi",          "measurement",      "rssi" },
            { "sys_alloc_failures", "Allocation Failures", "",    "",                "mdi:alert-outline", "total_increasing", "fails" },
        };
        count = sizeof(defs) / sizeof(defs[0]);
        return defs;
    }
    static constexpr const char* LAST_DEATH_ID = "sys_last_death";

    /** The transport. `mayQueue` is true for the connect-time crash publish only: a sample deferred is a sample stale. */
    typedef std::function<bool(const char* topic, const char* payload, size_t len, bool retain, bool mayQueue)> Sink;

    /**
     * Both topics from one id. False (and everything disabled) when the id does
     * not fit or carries an MQTT wildcard; an interval of 0 builds the topics
     * and leaves the publisher disabled, and returns true.
     */
    bool configure(const char* clientId, uint16_t intervalSec, uint32_t heapFloor) {
        enabled_ = false;
        telemetryTopic_[0] = crashTopic_[0] = '\0';
        if (!clientId || strlen(clientId) > CLIENT_ID_MAX || strpbrk(clientId, "+#") != nullptr) return false;
        snprintf(telemetryTopic_, sizeof(telemetryTopic_), "%s/telemetry", clientId);
        snprintf(crashTopic_, sizeof(crashTopic_), "%s/crash", clientId);
        intervalMs_ = static_cast<uint32_t>(intervalSec) * 1000u;
        heapFloor_ = heapFloor;
        enabled_ = intervalSec > 0;
        return true;
    }
    void setSink(Sink s) { sink_ = s; }
    bool enabled() const { return enabled_; }
    const char* telemetryTopic() const { return telemetryTopic_; }
    const char* crashTopic() const { return crashTopic_; }
    bool due(uint32_t nowMs) const { return enabled_ && (nowMs - lastTickMs_) >= intervalMs_; }

    /** Call every loop with millis(): counts the 49.7-day wraps so the uptime keeps climbing. Returns the uptime in ms. */
    uint64_t noteMillis(uint32_t nowMs) {
        if (nowMs < lastMillis_) ++wraps_;
        lastMillis_ = nowMs;
        return (static_cast<uint64_t>(wraps_) << 32) | nowMs;
    }
    uint64_t uptimeMs() const { return (static_cast<uint64_t>(wraps_) << 32) | lastMillis_; }

    /** One publish per interval; under the floor the tick is skipped and counted. Returns true when a payload went out. */
    bool tick(uint32_t nowMs, const TelemetrySample& s) {
        if (!due(nowMs)) return false;
        lastTickMs_ = nowMs;
        // the boot-long minimum: the lowest of every sample and every trough seen
        if (s.heap < minSeen_) minSeen_ = s.heap;
        if (s.trough && s.trough < minSeen_) minSeen_ = s.trough;
        if (s.heap < heapFloor_) { ++skipped_; return false; }
        char buf[TELEMETRY_MAX];
        size_t n = formatTelemetry(buf, sizeof(buf), s, minSeen_, skipped_, failed_);
        if (!sink_ || !sink_(telemetryTopic_, buf, n, false, false)) { ++failed_; return false; }
        ++sent_;
        return true;
    }

    /** Retained, may be queued: what a reconnect says about the last death. */
    bool publishCrash(const CrashSummary& c) {
        if (!sink_ || crashTopic_[0] == '\0') return false;
        char buf[CRASH_MAX];
        size_t n = formatCrash(buf, sizeof(buf), c);
        return sink_(crashTopic_, buf, n, true, true);
    }

    uint32_t skipped() const { return skipped_; }
    uint32_t failed() const { return failed_; }
    uint32_t sent() const { return sent_; }
    /** The boot-long minimum published as `min`; 0xFFFFFFFF before the first tick. */
    uint32_t minSeen() const { return minSeen_; }

    /** `min` is the boot-long minimum (the caller's), `trough` the sample's own window; `rssi` is `null` without a station link. */
    static size_t formatTelemetry(char* buf, size_t len, const TelemetrySample& s, uint32_t minSeen,
                                  uint32_t skipped, uint32_t failed) {
        char rssi[16];
        if (s.hasRssi) snprintf(rssi, sizeof(rssi), "%ld", (long)s.rssi);
        else snprintf(rssi, sizeof(rssi), "null");
        int n = snprintf(buf, len,
            "{\"heap\":%lu,\"largest\":%lu,\"min\":%lu,\"trough\":%lu,\"uptime\":%lu,\"boot\":%lu,\"rssi\":%s,"
            "\"fails\":%lu,\"fail_last\":%lu,\"skipped\":%lu,\"failed\":%lu,\"build\":\"%08lx\"}",
            (unsigned long)s.heap, (unsigned long)s.largest, (unsigned long)minSeen, (unsigned long)s.trough,
            (unsigned long)s.uptimeS, (unsigned long)s.bootCount, rssi,
            (unsigned long)s.fails, (unsigned long)s.failLast,
            (unsigned long)skipped, (unsigned long)failed, (unsigned long)s.buildId);
        return clamp(n, len);
    }

    static size_t formatCrash(char* buf, size_t len, const CrashSummary& c) {
        const char* source = c.source == CrashSummary::Persisted ? "persisted"
                           : c.source == CrashSummary::Rtc ? "rtc" : "none";
        const char* promotion = c.promotion ? c.promotion : "none";
        int n;
        if (c.source == CrashSummary::None) {
            n = snprintf(buf, len, "{\"source\":\"none\",\"promotion\":\"none\"}");
        } else if (c.torn) {
            // the other fields are what the crc could not vouch for
            n = snprintf(buf, len, "{\"source\":\"%s\",\"promotion\":\"%s\",\"phase\":%lu,\"torn\":1}",
                         source, promotion, (unsigned long)c.phase);
        } else {
            n = snprintf(buf, len,
                "{\"source\":\"%s\",\"promotion\":\"%s\",\"phase\":%lu,\"build\":\"%08lx\","
                "\"uptime_ms\":%lu,\"reason\":%lu,\"epc1\":\"0x%08lx\",\"fail_size\":%lu,"
                "\"fail_caller\":\"0x%08lx\",\"min_free\":%lu,\"dedup\":\"%08lx\"",
                source, promotion, (unsigned long)c.phase, (unsigned long)c.buildId,
                (unsigned long)c.uptimeMs, (unsigned long)c.reason, (unsigned long)c.epc1,
                (unsigned long)c.failSize, (unsigned long)c.failCaller, (unsigned long)c.minFree,
                (unsigned long)c.dedupKey);
            size_t pos = clamp(n, len);
            if (c.reset[0]) pos += clamp(snprintf(buf + pos, len - pos, ",\"reset\":\"%s\"", c.reset), len - pos);
            if (c.sameCount) pos += clamp(snprintf(buf + pos, len - pos, ",\"same_count\":%lu", (unsigned long)c.sameCount), len - pos);
            if (c.coreDumpSupported) {
                pos += clamp(snprintf(buf + pos, len - pos, ",\"coredump\":{\"waiting\":%s,\"size\":%lu}",
                                      c.coreDumpWaiting ? "true" : "false", (unsigned long)c.coreDumpSize), len - pos);
            }
            pos += clamp(snprintf(buf + pos, len - pos, "}"), len - pos);
            return pos;
        }
        return clamp(n, len);
    }

private:
    static size_t clamp(int n, size_t len) {
        if (n < 0) return 0;
        return static_cast<size_t>(n) >= len ? (len ? len - 1 : 0) : static_cast<size_t>(n);
    }

    char telemetryTopic_[TOPIC_MAX] = {0};
    char crashTopic_[TOPIC_MAX] = {0};
    uint32_t intervalMs_ = 0, heapFloor_ = 0, lastTickMs_ = 0;
    uint32_t skipped_ = 0, failed_ = 0, sent_ = 0;
    uint32_t minSeen_ = 0xFFFFFFFFu;
    uint32_t lastMillis_ = 0, wraps_ = 0;
    bool enabled_ = false;
    Sink sink_;
};

} // namespace SystemHelpers
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_SYSTEM_TELEMETRY_H
