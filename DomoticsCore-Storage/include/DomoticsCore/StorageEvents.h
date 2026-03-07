#pragma once

/**
 * @file StorageEvents.h
 * @brief Storage component events.
 *
 * Published by StorageComponent when storage is initialized.
 * Subscribe to these events to perform actions after storage is ready.
 */

namespace DomoticsCore {
namespace StorageEvents {

/// Published when storage backend is initialized and ready
static constexpr const char* EVENT_READY = "storage/ready";

/// Published when a storage value is changed, removed, or cleared
static constexpr const char* EVENT_CHANGED = "storage/changed";

/// POD payload for EVENT_CHANGED (EventBus byte-copies — no String members)
struct StorageChangedEvent {
    char key[64];
};

} // namespace StorageEvents
} // namespace DomoticsCore
