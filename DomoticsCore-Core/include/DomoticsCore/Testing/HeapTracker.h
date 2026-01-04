#pragma once

/**
 * @file HeapTracker.h
 * @brief Memory leak detection and heap monitoring for DomoticsCore
 * 
 * Provides platform-agnostic heap tracking with checkpoints and assertions.
 * Uses HAL pattern for cross-platform support (Native, ESP32, ESP8266).
 * 
 * Usage:
 * @code
 * HeapTracker tracker;
 * tracker.checkpoint("before");
 * // ... allocate memory ...
 * tracker.checkpoint("after");
 * 
 * // Assert no leak (with tolerance)
 * HEAP_ASSERT_STABLE(tracker, "before", "after", 100);
 * @endcode
 */

#include <DomoticsCore/Platform_HAL.h>
#include <map>
#include <vector>

namespace DomoticsCore {
namespace Testing {

/**
 * @brief Snapshot of heap state at a point in time
 */
struct HeapSnapshot {
    uint32_t freeHeap = 0;          // Free heap in bytes
    uint32_t largestFreeBlock = 0;  // Largest contiguous free block
    uint32_t totalHeap = 0;         // Total heap size (if available)
    uint32_t timestamp = 0;         // Timestamp in milliseconds
    
    /**
     * @brief Calculate fragmentation percentage
     * @return Fragmentation % (0 = no fragmentation, 100 = fully fragmented)
     */
    float getFragmentation() const {
        if (freeHeap == 0) return 0.0f;
        return 100.0f * (1.0f - (float)largestFreeBlock / (float)freeHeap);
    }
};

/**
 * @brief Named checkpoint with heap snapshot
 */
struct HeapCheckpoint {
    String name;
    HeapSnapshot snapshot;
};

/**
 * @brief Result of memory test assertion
 */
struct MemoryTestResult {
    bool passed = false;
    int32_t delta = 0;              // Bytes difference (positive = leak)
    int32_t tolerance = 0;          // Allowed tolerance
    String startCheckpoint;
    String endCheckpoint;
    String message;
    
    operator bool() const { return passed; }
};

// Forward declaration for platform-specific snapshot implementation
HeapSnapshot takeHeapSnapshot();

/**
 * @brief HeapTracker - Core heap monitoring class
 * 
 * Platform-specific implementations provide actual heap metrics.
 * This base class provides checkpoint management and comparison logic.
 */
class HeapTracker {
public:
    HeapTracker() = default;
    virtual ~HeapTracker() = default;
    
    /**
     * @brief Take a heap snapshot at current point
     * @return Current heap state
     */
    HeapSnapshot takeSnapshot() const {
        return takeHeapSnapshot();
    }
    
    /**
     * @brief Create a named checkpoint
     * @param name Unique checkpoint name
     */
    void checkpoint(const String& name) {
        HeapCheckpoint cp;
        cp.name = name;
        cp.snapshot = takeSnapshot();
        checkpoints_[name] = cp;
    }
    
    /**
     * @brief Get snapshot at named checkpoint
     * @param name Checkpoint name
     * @return HeapSnapshot (zero-initialized if not found)
     */
    HeapSnapshot getCheckpoint(const String& name) const {
        auto it = checkpoints_.find(name);
        if (it != checkpoints_.end()) {
            return it->second.snapshot;
        }
        return HeapSnapshot{};
    }
    
    /**
     * @brief Check if checkpoint exists
     * @param name Checkpoint name
     * @return true if checkpoint exists
     */
    bool hasCheckpoint(const String& name) const {
        return checkpoints_.find(name) != checkpoints_.end();
    }
    
    /**
     * @brief Calculate heap delta between two checkpoints
     * @param startName Start checkpoint name
     * @param endName End checkpoint name
     * @return Bytes difference (positive = memory increased/leaked)
     */
    int32_t getDelta(const String& startName, const String& endName) const {
        HeapSnapshot start = getCheckpoint(startName);
        HeapSnapshot end = getCheckpoint(endName);
        // Note: freeHeap decrease = memory used/leaked
        return (int32_t)start.freeHeap - (int32_t)end.freeHeap;
    }
    
    /**
     * @brief Calculate leak rate between checkpoints
     * @param startName Start checkpoint name
     * @param endName End checkpoint name
     * @return Leak rate in bytes per minute (positive = leaking)
     */
    float getLeakRate(const String& startName, const String& endName) const {
        HeapSnapshot start = getCheckpoint(startName);
        HeapSnapshot end = getCheckpoint(endName);
        
        int32_t delta = getDelta(startName, endName);
        uint32_t durationMs = end.timestamp - start.timestamp;
        
        if (durationMs == 0) return 0.0f;
        
        // Convert to bytes per minute
        return (float)delta / ((float)durationMs / 60000.0f);
    }
    
    /**
     * @brief Assert heap is stable between checkpoints
     * @param startName Start checkpoint name
     * @param endName End checkpoint name
     * @param toleranceBytes Allowed variation in bytes
     * @return MemoryTestResult with pass/fail and details
     */
    MemoryTestResult assertStable(const String& startName, const String& endName, 
                                   int32_t toleranceBytes = 0) const {
        MemoryTestResult result;
        result.startCheckpoint = startName;
        result.endCheckpoint = endName;
        result.tolerance = toleranceBytes;
        result.delta = getDelta(startName, endName);
        
        if (!hasCheckpoint(startName)) {
            result.passed = false;
            result.message = "Start checkpoint '" + startName + "' not found";
            return result;
        }
        
        if (!hasCheckpoint(endName)) {
            result.passed = false;
            result.message = "End checkpoint '" + endName + "' not found";
            return result;
        }
        
        result.passed = (result.delta >= -toleranceBytes && result.delta <= toleranceBytes);
        
        if (result.passed) {
            result.message = "Heap stable: delta=" + String(result.delta) + 
                           " bytes (tolerance=" + String(toleranceBytes) + ")";
        } else {
            result.message = "HEAP LEAK DETECTED: delta=" + String(result.delta) + 
                           " bytes (tolerance=" + String(toleranceBytes) + ")";
        }
        
        return result;
    }
    
    /**
     * @brief Assert no heap growth since checkpoint
     * @param checkpointName Checkpoint to compare against current heap
     * @param toleranceBytes Allowed variation
     * @return MemoryTestResult
     */
    MemoryTestResult assertNoGrowth(const String& checkpointName, 
                                     int32_t toleranceBytes = 0) const {
        // Create temporary "now" checkpoint
        HeapSnapshot now = takeSnapshot();
        HeapSnapshot start = getCheckpoint(checkpointName);
        
        MemoryTestResult result;
        result.startCheckpoint = checkpointName;
        result.endCheckpoint = "current";
        result.tolerance = toleranceBytes;
        result.delta = (int32_t)start.freeHeap - (int32_t)now.freeHeap;
        
        if (!hasCheckpoint(checkpointName)) {
            result.passed = false;
            result.message = "Checkpoint '" + checkpointName + "' not found";
            return result;
        }
        
        result.passed = (result.delta >= -toleranceBytes && result.delta <= toleranceBytes);
        
        if (result.passed) {
            result.message = "No growth: delta=" + String(result.delta) + " bytes";
        } else {
            result.message = "HEAP GROWTH: delta=" + String(result.delta) + " bytes";
        }
        
        return result;
    }
    
    /**
     * @brief Clear all checkpoints
     */
    void clear() {
        checkpoints_.clear();
    }
    
    /**
     * @brief Get number of checkpoints
     */
    size_t getCheckpointCount() const {
        return checkpoints_.size();
    }
    
    /**
     * @brief Get current free heap (convenience)
     */
    uint32_t getFreeHeap() const {
        return takeSnapshot().freeHeap;
    }
    
    /**
     * @brief Generate JSON report of all checkpoints
     * @return JSON string with checkpoint data
     */
    String toJson() const {
        String json = "{\"checkpoints\":[";
        bool first = true;
        for (const auto& pair : checkpoints_) {
            if (!first) json += ",";
            first = false;
            json += "{\"name\":\"" + pair.first + "\",";
            json += "\"freeHeap\":" + String(pair.second.snapshot.freeHeap) + ",";
            json += "\"largestBlock\":" + String(pair.second.snapshot.largestFreeBlock) + ",";
            json += "\"fragmentation\":" + String(pair.second.snapshot.getFragmentation(), 1) + ",";
            json += "\"timestamp\":" + String(pair.second.snapshot.timestamp) + "}";
        }
        json += "]}";
        return json;
    }

protected:
    std::map<String, HeapCheckpoint> checkpoints_;
};

} // namespace Testing
} // namespace DomoticsCore

// Platform-specific implementation is included via HAL (outside namespace)
#include <DomoticsCore/Testing/HeapTracker_HAL.h>

namespace DomoticsCore {
namespace Testing {

/**
 * @brief Convenience macro to create checkpoint
 * @param tracker HeapTracker instance
 * @param name Checkpoint name (string literal)
 */
#define HEAP_CHECKPOINT(tracker, name) (tracker).checkpoint(name)

/**
 * @brief Assert heap stability between checkpoints
 * @param tracker HeapTracker instance
 * @param start Start checkpoint name
 * @param end End checkpoint name
 * @param tolerance Allowed bytes variation
 */
#define HEAP_ASSERT_STABLE(tracker, start, end, tolerance) \
    do { \
        auto _result = (tracker).assertStable(start, end, tolerance); \
        TEST_ASSERT_TRUE_MESSAGE(_result.passed, _result.message.c_str()); \
    } while(0)

/**
 * @brief Assert no heap growth since checkpoint
 * @param tracker HeapTracker instance
 * @param checkpoint Checkpoint name to compare against
 * @param tolerance Allowed bytes variation
 */
#define HEAP_ASSERT_NO_GROWTH(tracker, checkpoint, tolerance) \
    do { \
        auto _result = (tracker).assertNoGrowth(checkpoint, tolerance); \
        TEST_ASSERT_TRUE_MESSAGE(_result.passed, _result.message.c_str()); \
    } while(0)

} // namespace Testing
} // namespace DomoticsCore
