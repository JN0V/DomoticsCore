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

} // namespace StorageEvents
} // namespace DomoticsCore
