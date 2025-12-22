#pragma once

/**
 * @file OTAEvents.h
 * @brief OTA component events.
 *
 * Published by OTAComponent during firmware update process.
 * Subscribe to these events to track OTA progress or react to updates.
 */

namespace DomoticsCore {
namespace OTAEvents {

/// Published when OTA update starts
static constexpr const char* EVENT_START = "ota/start";

/// Published periodically during OTA with progress percentage
static constexpr const char* EVENT_PROGRESS = "ota/progress";

/// Published when OTA transfer ends (before verification)
static constexpr const char* EVENT_END = "ota/end";

/// Published when OTA encounters an error
static constexpr const char* EVENT_ERROR = "ota/error";

/// Published for OTA informational messages
static constexpr const char* EVENT_INFO = "ota/info";

/// Published when OTA is complete (intermediate, before reboot decision)
static constexpr const char* EVENT_COMPLETE = "ota/complete";

/// Published when OTA is fully completed with reboot status
static constexpr const char* EVENT_COMPLETED = "ota/completed";

} // namespace OTAEvents
} // namespace DomoticsCore
