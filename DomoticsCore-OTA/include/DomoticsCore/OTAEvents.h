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
inline constexpr const char* EVENT_START = "ota/start";

/// Published periodically during OTA with progress percentage
inline constexpr const char* EVENT_PROGRESS = "ota/progress";

/// Published when OTA transfer ends (before verification)
inline constexpr const char* EVENT_END = "ota/end";

/// Published when OTA encounters an error
inline constexpr const char* EVENT_ERROR = "ota/error";

/// Published for OTA informational messages
inline constexpr const char* EVENT_INFO = "ota/info";

/// Published when OTA is complete (intermediate, before reboot decision)
inline constexpr const char* EVENT_COMPLETE = "ota/complete";

/// Published when OTA is fully completed with reboot status
inline constexpr const char* EVENT_COMPLETED = "ota/completed";

} // namespace OTAEvents
} // namespace DomoticsCore
