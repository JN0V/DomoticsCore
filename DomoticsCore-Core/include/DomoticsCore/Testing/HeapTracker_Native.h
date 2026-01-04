#pragma once

/**
 * @file HeapTracker_Native.h
 * @brief Native (desktop) HeapTracker implementation with REAL heap tracking
 * 
 * Uses system APIs to get actual memory usage:
 * - Linux: mallinfo2() or mallinfo()
 * - Other: Falls back to process memory info
 * 
 * This allows detecting REAL memory leaks on native platform.
 */

#include <cstdlib>
#include <cstdint>
#include <vector>

// Linux-specific: use mallinfo for real heap stats
#if defined(__linux__)
#include <malloc.h>
#endif

// For cross-platform process memory (fallback)
#if defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace DomoticsCore {
namespace Testing {

/**
 * @brief Get real heap usage on native platform
 * 
 * Returns actual bytes currently allocated on the heap.
 * This is the key function for detecting memory leaks.
 */
inline size_t getRealHeapUsage() {
#if defined(__linux__)
    // Linux: use mallinfo2 (or mallinfo on older systems)
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
    // mallinfo2 available in glibc 2.33+
    struct mallinfo2 mi = mallinfo2();
    return mi.uordblks;  // Total allocated space (bytes)
    #else
    // Older glibc: use mallinfo (may overflow on 32-bit for >2GB)
    struct mallinfo mi = mallinfo();
    return static_cast<size_t>(mi.uordblks);  // Total allocated space
    #endif
#elif defined(__APPLE__)
    // macOS: use mach API
    struct mach_task_basic_info info;
    mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS) {
        return info.resident_size;
    }
    return 0;
#else
    // Fallback: no real tracking available
    return 0;
#endif
}

/**
 * @brief Get total heap arena size on native platform
 */
inline size_t getRealHeapTotal() {
#if defined(__linux__)
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
    struct mallinfo2 mi = mallinfo2();
    return mi.arena;  // Total arena size
    #else
    struct mallinfo mi = mallinfo();
    return static_cast<size_t>(mi.arena);
    #endif
#else
    // Fallback: assume 1GB virtual heap
    return 1024 * 1024 * 1024;
#endif
}

/**
 * @brief Get largest free block (approximation on native)
 */
inline size_t getRealLargestFreeBlock() {
#if defined(__linux__)
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
    struct mallinfo2 mi = mallinfo2();
    return mi.fordblks;  // Total free space
    #else
    struct mallinfo mi = mallinfo();
    return static_cast<size_t>(mi.fordblks);
    #endif
#else
    return getRealHeapTotal() - getRealHeapUsage();
#endif
}

/**
 * @brief Native implementation of takeHeapSnapshot()
 * 
 * Uses REAL system heap metrics - not simulated values.
 * This allows detecting actual memory leaks on native platform.
 */
inline HeapSnapshot takeHeapSnapshot() {
    HeapSnapshot snapshot;
    
    size_t heapUsed = getRealHeapUsage();
    size_t heapTotal = getRealHeapTotal();
    
    // Free heap = total - used
    snapshot.totalHeap = static_cast<uint32_t>(heapTotal > UINT32_MAX ? UINT32_MAX : heapTotal);
    snapshot.freeHeap = static_cast<uint32_t>(heapTotal > heapUsed ? heapTotal - heapUsed : 0);
    snapshot.largestFreeBlock = static_cast<uint32_t>(getRealLargestFreeBlock());
    snapshot.timestamp = DomoticsCore::HAL::Platform::getMillis();
    
    return snapshot;
}

/**
 * @brief Record of a single allocation (for detailed tracking)
 */
struct AllocationRecord {
    void* ptr = nullptr;
    size_t size = 0;
    const char* file = nullptr;
    int line = 0;
    bool freed = false;
};

/**
 * @brief Detailed allocation tracker (optional, for per-allocation tracking)
 * 
 * This provides more detailed tracking than mallinfo when enabled.
 * Use ScopedAllocTracking for automatic enable/disable.
 */
class NativeAllocTracker {
public:
    static NativeAllocTracker& instance() {
        static NativeAllocTracker tracker;
        return tracker;
    }
    
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    
    void recordAlloc(void* ptr, size_t size, const char* file = nullptr, int line = 0) {
        if (!enabled_) return;
        AllocationRecord rec;
        rec.ptr = ptr;
        rec.size = size;
        rec.file = file;
        rec.line = line;
        rec.freed = false;
        allocations_.push_back(rec);
        totalAllocated_ += size;
    }
    
    void recordFree(void* ptr) {
        if (!enabled_ || ptr == nullptr) return;
        for (auto& rec : allocations_) {
            if (rec.ptr == ptr && !rec.freed) {
                rec.freed = true;
                totalFreed_ += rec.size;
                break;
            }
        }
    }
    
    size_t getTotalAllocated() const { return totalAllocated_; }
    size_t getTotalFreed() const { return totalFreed_; }
    size_t getCurrentUsage() const { return totalAllocated_ - totalFreed_; }
    
    std::vector<AllocationRecord> getUnfreedAllocations() const {
        std::vector<AllocationRecord> unfreed;
        for (const auto& rec : allocations_) {
            if (!rec.freed) {
                unfreed.push_back(rec);
            }
        }
        return unfreed;
    }
    
    size_t getUnfreedCount() const {
        size_t count = 0;
        for (const auto& rec : allocations_) {
            if (!rec.freed) count++;
        }
        return count;
    }
    
    size_t getUnfreedBytes() const {
        size_t bytes = 0;
        for (const auto& rec : allocations_) {
            if (!rec.freed) bytes += rec.size;
        }
        return bytes;
    }
    
    void reset() {
        allocations_.clear();
        totalAllocated_ = 0;
        totalFreed_ = 0;
    }

private:
    NativeAllocTracker() = default;
    bool enabled_ = false;
    std::vector<AllocationRecord> allocations_;
    size_t totalAllocated_ = 0;
    size_t totalFreed_ = 0;
};

/**
 * @brief RAII helper for scoped allocation tracking
 */
class ScopedAllocTracking {
public:
    ScopedAllocTracking() {
        NativeAllocTracker::instance().setEnabled(true);
        NativeAllocTracker::instance().reset();
    }
    
    ~ScopedAllocTracking() {
        NativeAllocTracker::instance().setEnabled(false);
    }
    
    size_t getUnfreedCount() const {
        return NativeAllocTracker::instance().getUnfreedCount();
    }
    
    size_t getUnfreedBytes() const {
        return NativeAllocTracker::instance().getUnfreedBytes();
    }
};

} // namespace Testing
} // namespace DomoticsCore
