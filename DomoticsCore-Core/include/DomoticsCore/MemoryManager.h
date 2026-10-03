#pragma once

/**
 * @file MemoryManager.h
 * @brief Dynamic memory adaptation system for DomoticsCore
 *
 * Provides runtime memory profiling and adaptive configuration based on
 * available heap after boot. This allows the same code to run optimally
 * on devices with different memory constraints (ESP32, ESP8266, future devices).
 *
 * Usage:
 * @code
 * // In setup(), after all components initialized:
 * MemoryManager::instance().detectProfile();
 *
 * // Query profile anywhere:
 * if (MemoryManager::instance().getProfile() == MemoryProfile::MINIMAL) {
 *     // Use reduced features
 * }
 *
 * // Check the live heap before an expensive step:
 * if (MemoryManager::instance().isLowMemory()) { ... }
 * @endcode
 */

#include "Platform_HAL.h"

namespace DomoticsCore {

/**
 * @brief Memory profile levels based on available heap
 */
enum class MemoryProfile {
    FULL,       ///< > 30KB free: All features enabled, max buffers
    STANDARD,   ///< 15-30KB free: Moderate reductions
    MINIMAL,    ///< 8-15KB free: Economy mode, reduced features
    CRITICAL    ///< < 8KB free: Emergency mode, minimal operation
};

/**
 * @brief Memory profile thresholds (in bytes)
 *
 * These can be adjusted based on real-world testing.
 * Default values are conservative starting points.
 */
struct MemoryThresholds {
    uint32_t fullMin      = 30 * 1024;  ///< Minimum for FULL profile (30KB)
    uint32_t standardMin  = 15 * 1024;  ///< Minimum for STANDARD profile (15KB)
    uint32_t minimalMin   = 8 * 1024;   ///< Minimum for MINIMAL profile (8KB)
    // Below minimalMin = CRITICAL
};

/**
 * @brief Limits for each profile
 */
struct ProfileLimits {
    uint8_t maxWsClients;       ///< Max WebSocket clients
};

/**
 * @brief MemoryManager - Singleton for runtime memory adaptation
 *
 * This class detects available memory at boot and provides adaptive
 * configuration values that components can query at runtime.
 */
class MemoryManager {
public:
    // Accepted deviation from Constitution XIII (no singleton abuse):
    // MemoryManager requires global access for buffer sizing decisions across all components.
    // No viable alternative without passing MemoryManager& through every component constructor.
    /**
     * @brief Get singleton instance
     */
    static MemoryManager& instance() {
        static MemoryManager mgr;
        return mgr;
    }

    /**
     * @brief Detect memory profile based on current free heap
     *
     * Call this once in setup() before components are initialized,
     * to get an accurate picture of available runtime memory.
     * Can also be called implicitly via getProfile().
     *
     * @return Detected profile
     */
    MemoryProfile detectProfile() const {
        uint32_t freeHeap = HAL::getFreeHeap();
        heapAtBoot_ = freeHeap;

        if (freeHeap >= thresholds_.fullMin) {
            profile_ = MemoryProfile::FULL;
        } else if (freeHeap >= thresholds_.standardMin) {
            profile_ = MemoryProfile::STANDARD;
        } else if (freeHeap >= thresholds_.minimalMin) {
            profile_ = MemoryProfile::MINIMAL;
        } else {
            profile_ = MemoryProfile::CRITICAL;
        }

        detected_ = true;
        return profile_;
    }

    /**
     * @brief Get current memory profile
     *
     * If detectProfile() hasn't been called, auto-detects on first call.
     * This ensures components always get accurate profile information.
     */
    MemoryProfile getProfile() const {
        if (!detected_) {
            detectProfile();
        }
        return profile_;
    }

    /**
     * @brief Get profile name as string
     */
    const char* getProfileName() const {
        switch (getProfile()) {
            case MemoryProfile::FULL:     return "FULL";
            case MemoryProfile::STANDARD: return "STANDARD";
            case MemoryProfile::MINIMAL:  return "MINIMAL";
            case MemoryProfile::CRITICAL: return "CRITICAL";
            default:                      return "UNKNOWN";
        }
    }

    /**
     * @brief Get max WebSocket clients for current profile
     */
    uint8_t getMaxWsClients() const {
        return getLimits().maxWsClients;
    }

    /**
     * @brief Get heap at boot (after detectProfile was called)
     */
    uint32_t getHeapAtBoot() const {
        return heapAtBoot_;
    }

    /**
     * @brief Check if we're in a low memory situation right now
     *
     * This is a runtime check, not the boot-time profile.
     * Use this for emergency throttling.
     */
    bool isLowMemory() const {
        return HAL::getFreeHeap() < thresholds_.minimalMin;
    }

    /**
     * @brief Set custom thresholds (call before detectProfile)
     */
    void setThresholds(const MemoryThresholds& t) {
        thresholds_ = t;
    }

    /**
     * @brief Get current thresholds
     */
    const MemoryThresholds& getThresholds() const {
        return thresholds_;
    }

private:
    MemoryManager() = default;

    // Non-copyable
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    /**
     * @brief Get limits for current profile
     */
    const ProfileLimits& getLimits() const {
        static const ProfileLimits full     = { 8 };
        static const ProfileLimits standard = { 4 };
        static const ProfileLimits minimal  = { 2 };
        static const ProfileLimits critical = { 1 };

        switch (getProfile()) {
            case MemoryProfile::FULL:     return full;
            case MemoryProfile::STANDARD: return standard;
            case MemoryProfile::MINIMAL:  return minimal;
            case MemoryProfile::CRITICAL: return critical;
            default:                      return standard;
        }
    }

    mutable MemoryProfile profile_ = MemoryProfile::STANDARD;
    MemoryThresholds thresholds_;
    mutable uint32_t heapAtBoot_ = 0;
    mutable bool detected_ = false;
};

} // namespace DomoticsCore
