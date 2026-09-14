#ifndef DOMOTICS_CORE_SYSTEM_TELEMETRY_SETUP_H
#define DOMOTICS_CORE_SYSTEM_TELEMETRY_SETUP_H

/**
 * @file SystemTelemetrySetup.h
 * @brief What System does to feed the telemetry publisher: read the last
 * death, sample the device, wire the MQTT sink and the connect-time crash
 * publish, register the Home Assistant entities.
 *
 * Free functions over the members System hands them, like SystemPersistence.h
 * and SystemWebUISetup.h, so System.h keeps one line per hook.
 */

#include <DomoticsCore/Core.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/FlightRecorder.h>
#include <DomoticsCore/Wifi.h>
#include "SystemConfig.h"
#include "SystemTelemetry.h"
#include "SystemPersistence.h"

#if __has_include(<DomoticsCore/MQTT.h>)
#include <DomoticsCore/MQTT.h>
#include <DomoticsCore/MQTTEvents.h>
#endif
#if __has_include(<DomoticsCore/HomeAssistant.h>)
#include <DomoticsCore/HomeAssistant.h>
#endif

namespace DomoticsCore {
namespace SystemHelpers {

static const char* const LOG_TELEMETRY = "SYSTEM";

/** @brief The core dump partition's state into the summary: read at boot and again at every connect, since an erase does not reboot. */
inline void refreshCoreDumpIn(CrashSummary& d) {
    const HAL::Platform::CoreDumpStatus cd = HAL::Platform::getCoreDumpStatus();
    d.coreDumpSupported = cd.supported;
    d.coreDumpWaiting = cd.dumpPresent;
    d.coreDumpSize = cd.size;
}

/**
 * @brief The crash topic's content: the persisted record when this boot
 * persisted one, RTC's promoted record otherwise, nothing when neither holds
 * a death. `persistedThisBoot` is false when the persistence step was skipped
 * (low heap, no Storage), so a fresh death in RTC is not shadowed by an older
 * blob.
 */
inline void captureLastDeath(Core& core, const SystemConfig& config, bool persistedThisBoot, CrashSummary& out) {
    out = CrashSummary{};
    refreshCoreDumpIn(out);
    const FlightRecorder& fr = FlightRecorder::instance();
#if __has_include(<DomoticsCore/Storage.h>) && __has_include(<DomoticsCore/SystemInfo.h>)
    if (persistedThisBoot && config.enableStorage && config.enableSystemInfo) {
        auto* storage = core.getComponent<Components::StorageComponent>("Storage");
        BootDiagRecord r;
        if (storage && readBootDiagRecord(*storage, r) && r.promotion != 0
            && r.promotion <= static_cast<uint32_t>(FlightRecorder::Promotion::UnownedSoftwareReset)) {
            out.source = CrashSummary::Persisted;
            out.promotion = FlightRecorder::promotionName(static_cast<FlightRecorder::Promotion>(r.promotion));
            out.torn = (r.flags & 1u) != 0;
            out.phase = r.phase; out.buildId = r.buildId; out.uptimeMs = r.uptimeMs;
            out.reason = r.reason; out.epc1 = r.epc1; out.failSize = r.failSize;
            out.failCaller = r.failCaller; out.minFree = r.minFree;
            out.dedupKey = r.dedupKey; out.sameCount = r.sameCount;
            // On the death boot the blob describes the record RTC still holds,
            // and RTC knows the reset reason the promotion saw.
            if (fr.hasPromotedRecord()
                && fr.promoted().dedupKey(static_cast<uint32_t>(fr.resetReason())) == r.dedupKey) {
                String reset = HAL::Platform::getResetReasonString(fr.resetReason());
                snprintf(out.reset, sizeof(out.reset), "%s", reset.c_str());
            }
            return;
        }
    }
#else
    (void)core; (void)config; (void)persistedThisBoot;
#endif
    if (!fr.hasPromotedRecord()) return;
    const FlightRecord& d = fr.promoted();
    out.source = CrashSummary::Rtc;
    out.promotion = FlightRecorder::promotionName(fr.promotion());
    String reset = HAL::Platform::getResetReasonString(fr.resetReason());
    snprintf(out.reset, sizeof(out.reset), "%s", reset.c_str());
    out.torn = fr.promotedIsTorn();
    out.phase = d.phase(); out.buildId = d.w[FlightRecord::W_BUILD];
    out.uptimeMs = d.lastUptimeMs(); out.reason = d.cbReason(); out.epc1 = d.epc1();
    out.failSize = d.failSize(); out.failCaller = d.failCaller();
    out.minFree = d.minFreeBytes();
    out.dedupKey = d.dedupKey(static_cast<uint32_t>(fr.resetReason()));
}

#if __has_include(<DomoticsCore/MQTT.h>)
/** @brief Every System::loop(): note the clock, and when a tick is due, sample the device and publish. */
inline void telemetryTick(SystemTelemetry& t, Components::WifiComponent* wifi, uint32_t bootCount) {
    const uint32_t now = static_cast<uint32_t>(HAL::Platform::getMillis());
    const uint64_t uptimeMs = t.noteMillis(now);
    if (!t.due(now)) return;
    const FlightRecorder& fr = FlightRecorder::instance();
    TelemetrySample s;
    s.heap = HAL::Platform::getAllocatableFreeHeap();
    s.largest = HAL::Platform::getLargestFreeBlock();
    s.trough = fr.current().minFreeBytes();
    s.uptimeS = static_cast<uint32_t>(uptimeMs / 1000u);
    s.bootCount = bootCount;
    s.hasRssi = wifi && wifi->isSTAConnected();
    s.rssi = s.hasRssi ? wifi->getRSSI() : 0;
    uint32_t count = 0, last = 0;
    fr.failedAllocSnapshot(count, last);
    s.fails = count; s.failLast = last;
    s.buildId = fr.current().w[FlightRecord::W_BUILD];
    const uint32_t skippedBefore = t.skipped();
    t.tick(now, s);
    if (t.skipped() != skippedBefore) {
        DLOG_W(LOG_TELEMETRY, "Telemetry tick skipped: %lu B allocatable, under the floor", (unsigned long)s.heap);
    }
}

/** @brief Configure the publisher from the MQTT client id, give it its sink, and publish the crash record at every connect. */
inline void setupTelemetry(Core& core, const SystemConfig& config, SystemTelemetry& t,
                           CrashSummary& lastDeath, Components::MQTTComponent* mqttComp) {
    if (config.telemetryIntervalSec == 0) {
        DLOG_I(LOG_TELEMETRY, "Telemetry off (telemetryIntervalSec = 0)");
        return;
    }
    if (!t.configure(mqttComp->getConfig().clientId.c_str(), config.telemetryIntervalSec, config.telemetryHeapFloor)) {
        DLOG_W(LOG_TELEMETRY, "Telemetry off: MQTT client id longer than %u characters or carrying a wildcard",
               (unsigned)SystemTelemetry::CLIENT_ID_MAX);
        return;
    }
    // The tick never queues (a deferred sample is a stale one); the crash
    // publish at connect may, it is retained and lands after HA's burst.
    t.setSink([mqttComp](const char* topic, const char* payload, size_t len, bool retain, bool mayQueue) {
        if (mayQueue) return mqttComp->publish(String(topic), String(payload), 0, retain);
        return mqttComp->publishNow(topic, payload, len, retain);
    });
    // mqttComp->connect() may already have run in this same setup: the
    // connect event is only enqueued by it, dispatched on a later loop(), so
    // a subscription made here still sees it.
    core.getEventBus().subscribe(MQTTEvents::EVENT_CONNECTED, [&t, &lastDeath](const void* payload) {
        if (!payload || !*static_cast<const bool*>(payload)) return;
        refreshCoreDumpIn(lastDeath);
        if (!t.publishCrash(lastDeath)) {
            DLOG_W(LOG_TELEMETRY, "Crash record not published on %s", t.crashTopic());
        }
    });
    DLOG_I(LOG_TELEMETRY, "Telemetry every %u s on %s; last death on %s",
           (unsigned)config.telemetryIntervalSec, t.telemetryTopic(), t.crashTopic());
}
#endif

#if __has_include(<DomoticsCore/MQTT.h>) && __has_include(<DomoticsCore/HomeAssistant.h>)
/**
 * @brief The diagnostic entities, reading the two topics through value_template
 * so one publish per tick feeds them all. Three ids are the ones FullStack
 * used to declare, so an upgraded device keeps its entities rather than
 * gaining frozen duplicates. The last-death sensor opts out of availability:
 * it must read while the device is down. An id the application already
 * declared is left to the application.
 */
inline void registerTelemetryEntities(Components::HomeAssistant::HomeAssistantComponent* ha, const SystemTelemetry& t) {
    using namespace Components::HomeAssistant;
    size_t n = 0;
    const SystemTelemetry::EntityDef* defs = SystemTelemetry::entityDefs(n);
    int registered = 0;
    for (size_t i = 0; i < n; i++) {
        const SystemTelemetry::EntityDef& d = defs[i];
        if (ha->entity(d.id)) {
            DLOG_W(LOG_TELEMETRY, "Entity '%s' already declared by the application; the system one is not registered", d.id);
            continue;
        }
        ha->addSensor(d.id, d.name, d.unit, d.deviceClass, d.icon, d.stateClass);
        if (HAEntity* e = ha->entity(d.id)) {
            e->entityCategory = "diagnostic";
            e->stateTopicOverride = t.telemetryTopic();
            e->valueTemplate = String("{{ value_json.") + d.key + " }}";
            registered++;
        }
    }
    if (!ha->entity(SystemTelemetry::LAST_DEATH_ID)) {
        ha->addSensor(SystemTelemetry::LAST_DEATH_ID, "Last Death", "", "", "mdi:skull-outline");
        if (HAEntity* e = ha->entity(SystemTelemetry::LAST_DEATH_ID)) {
            e->entityCategory = "diagnostic";
            e->stateTopicOverride = t.crashTopic();
            e->valueTemplate = "{{ value_json.promotion }}";
            e->jsonAttributesTopic = t.crashTopic();
            e->useAvailability = false;
            registered++;
        }
    }
    DLOG_I(LOG_TELEMETRY, "%d diagnostic entities registered for Home Assistant", registered);
}
#endif

} // namespace SystemHelpers
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_SYSTEM_TELEMETRY_SETUP_H
