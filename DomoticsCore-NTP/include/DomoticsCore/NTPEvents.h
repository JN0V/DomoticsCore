#pragma once

/**
 * @file NTPEvents.h
 * @brief NTP component events.
 *
 * Published by NTPComponent when time synchronization occurs.
 * Subscribe to these events to react to time sync status.
 */

namespace DomoticsCore {
namespace NTPEvents {

/// Published when time is successfully synchronized
static constexpr const char* EVENT_SYNCED = "ntp/synced";

/// Published when time synchronization fails
static constexpr const char* EVENT_SYNC_FAILED = "ntp/sync_failed";

} // namespace NTPEvents
} // namespace DomoticsCore
