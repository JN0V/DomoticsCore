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
static constexpr const char* EVENT_DISCOVERY_PUBLISHED = "ha/discovery_published";

/// Published when a new entity is added
static constexpr const char* EVENT_ENTITY_ADDED = "ha/entity_added";

/// Published when an entity receives a command from Home Assistant
static constexpr const char* EVENT_COMMAND = "ha/command";

/**
 * @brief Event data for HA command received via MQTT
 *
 * Fixed-size POD struct (~256 bytes), zero heap allocation.
 * Published on ha/command topic when an entity processes a command.
 * - entityId: the entity that received the command
 * - component: HA component type (switch, light, button, alarm_control_panel)
 * - command: raw MQTT payload (or parsed command for alarm_control_panel)
 * - code: alarm PIN code (empty for non-alarm entities)
 */
struct HACommandEvent {
    char entityId[64];
    char component[32];
    char command[128];
    char code[32];
};

} // namespace HAEvents
} // namespace DomoticsCore
