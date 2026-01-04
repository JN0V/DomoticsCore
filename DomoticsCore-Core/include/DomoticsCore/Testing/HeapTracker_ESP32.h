#pragma once

/**
 * @file HeapTracker_ESP32.h
 * @brief ESP32-specific HeapTracker implementation using heap_caps API
 * 
 * This file is only included via HeapTracker_HAL.h when ESP32 is defined,
 * so no additional platform checks are needed.
 */

#include <esp_heap_caps.h>

namespace DomoticsCore {
namespace Testing {

/**
 * @brief ESP32 implementation of takeHeapSnapshot()
 * 
 * Uses ESP-IDF heap_caps API for detailed heap information.
 */
inline HeapSnapshot takeHeapSnapshot() {
    HeapSnapshot snapshot;
    
    snapshot.freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    snapshot.largestFreeBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    snapshot.totalHeap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    snapshot.timestamp = DomoticsCore::HAL::Platform::getMillis();
    
    return snapshot;
}

} // namespace Testing
} // namespace DomoticsCore
