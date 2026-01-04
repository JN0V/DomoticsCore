#pragma once

/**
 * @file HeapTracker_ESP8266.h
 * @brief ESP8266-specific HeapTracker implementation using ESP SDK
 * 
 * This file is only included via HeapTracker_HAL.h when ESP8266 is defined,
 * so no additional platform checks are needed.
 */

#include <Esp.h>

namespace DomoticsCore {
namespace Testing {

/**
 * @brief ESP8266 implementation of takeHeapSnapshot()
 * 
 * Uses ESP8266 SDK functions for heap information.
 * Note: ESP8266 has limited heap introspection compared to ESP32.
 */
inline HeapSnapshot takeHeapSnapshot() {
    HeapSnapshot snapshot;
    
    snapshot.freeHeap = ESP.getFreeHeap();
    snapshot.largestFreeBlock = ESP.getMaxFreeBlockSize();
    snapshot.totalHeap = 81920; // ESP8266 has ~80KB RAM
    snapshot.timestamp = DomoticsCore::HAL::Platform::getMillis();
    
    return snapshot;
}

} // namespace Testing
} // namespace DomoticsCore
