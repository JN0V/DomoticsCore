#pragma once

/**
 * @file HAEvents.h
 * @brief Home Assistant component events.
 *
 * Published by HomeAssistantComponent during discovery and entity registration.
 * Subscribe to these events to track HA integration status.
 */

namespace DomoticsCore {
namespace HAEvents {

/// Published when discovery payload is sent to HA
inline constexpr const char* EVENT_DISCOVERY_PUBLISHED = "ha/discovery_published";

/// Published when a new entity is added
inline constexpr const char* EVENT_ENTITY_ADDED = "ha/entity_added";

} // namespace HAEvents
} // namespace DomoticsCore
