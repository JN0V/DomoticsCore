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
 * // Get adaptive buffer size:
 * size_t bufSize = MemoryManager::instance().getBufferSize(BufferType::WebSocket);
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
 * @brief Buffer types that can be sized adaptively
 */
enum class BufferType {
    WebSocket,      ///< WebSocket message buffer
    HttpResponse,   ///< HTTP response buffer
    JsonDocument,   ///< ArduinoJson document size
    LogBuffer       ///< Logging buffer
};

/**
 * @brief Feature flags that can be enabled/disabled based on profile
 */
enum class Feature {
    WebSocketUpdates,   ///< Real-time WebSocket push updates
    ChartHistory,       ///< Store chart data history
    SettingsLazyLoad,   ///< Lazy-load settings contexts
    SchemaCompression,  ///< Compress schema responses
    FullDashboard       ///< Show all dashboard contexts
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
 * @brief Buffer sizes for each profile (in bytes)
 */
struct ProfileBufferSizes {
    size_t webSocket;
    size_t httpResponse;
    size_t jsonDocument;
    size_t logBuffer;
};

/**
 * @brief Timing intervals for each profile (in milliseconds)
 */
struct ProfileIntervals {
    uint32_t wsUpdateInterval;      ///< WebSocket update interval
    uint32_t heapCheckInterval;     ///< How often to recheck heap
};

/**
 * @brief Limits for each profile
 */
struct ProfileLimits {
    uint8_t maxWsClients;       ///< Max WebSocket clients
    uint8_t maxProviders;       ///< Max WebUI providers
    uint8_t chartHistoryPoints; ///< Chart history depth
};

/**
 * @brief MemoryManager - Singleton for runtime memory adaptation
 *
 * This class detects available memory at boot and provides adaptive
 * configuration values that components can query at runtime.
 */
class MemoryManager {
public:
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
     * @brief Get adaptive buffer size for specified type
     */
    size_t getBufferSize(BufferType type) const {
        const auto& sizes = getBufferSizes();
        switch (type) {
            case BufferType::WebSocket:    return sizes.webSocket;
            case BufferType::HttpResponse: return sizes.httpResponse;
            case BufferType::JsonDocument: return sizes.jsonDocument;
            case BufferType::LogBuffer:    return sizes.logBuffer;
            default:                       return 1024;
        }
    }

    /**
     * @brief Check if a feature should be enabled for current profile
     */
    bool shouldEnable(Feature feature) const {
        MemoryProfile p = getProfile();

        switch (feature) {
            case Feature::WebSocketUpdates:
                return p != MemoryProfile::CRITICAL;

            case Feature::ChartHistory:
                return p == MemoryProfile::FULL || p == MemoryProfile::STANDARD;

            case Feature::SettingsLazyLoad:
                return p == MemoryProfile::MINIMAL || p == MemoryProfile::CRITICAL;

            case Feature::SchemaCompression:
                return true;  // Always beneficial

            case Feature::FullDashboard:
                return p == MemoryProfile::FULL || p == MemoryProfile::STANDARD;

            default:
                return true;
        }
    }

    /**
     * @brief Get WebSocket update interval for current profile
     */
    uint32_t getWsUpdateInterval() const {
        return getIntervals().wsUpdateInterval;
    }

    /**
     * @brief Get max WebSocket clients for current profile
     */
    uint8_t getMaxWsClients() const {
        return getLimits().maxWsClients;
    }

    /**
     * @brief Get chart history points for current profile
     */
    uint8_t getChartHistoryPoints() const {
        return getLimits().chartHistoryPoints;
    }

    /**
     * @brief Get heap at boot (after detectProfile was called)
     */
    uint32_t getHeapAtBoot() const {
        return heapAtBoot_;
    }

    /**
     * @brief Get current free heap
     */
    uint32_t getCurrentFreeHeap() const {
        return HAL::getFreeHeap();
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
     * @brief Check if we're in critical memory situation
     */
    bool isCriticalMemory() const {
        return HAL::getFreeHeap() < (thresholds_.minimalMin / 2);
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
     * @brief Get buffer sizes for current profile
     */
    const ProfileBufferSizes& getBufferSizes() const {
        static const ProfileBufferSizes full     = { 8192, 4096, 8192, 200 };
        static const ProfileBufferSizes standard = { 4096, 2048, 4096, 100 };
        static const ProfileBufferSizes minimal  = { 2048, 1024, 2048, 50 };
        static const ProfileBufferSizes critical = { 1024, 512,  1024, 20 };

        switch (getProfile()) {
            case MemoryProfile::FULL:     return full;
            case MemoryProfile::STANDARD: return standard;
            case MemoryProfile::MINIMAL:  return minimal;
            case MemoryProfile::CRITICAL: return critical;
            default:                      return standard;
        }
    }

    /**
     * @brief Get timing intervals for current profile
     */
    const ProfileIntervals& getIntervals() const {
        static const ProfileIntervals full     = { 2000, 60000 };   // 2s updates, 1min heap check
        static const ProfileIntervals standard = { 5000, 30000 };   // 5s updates
        static const ProfileIntervals minimal  = { 10000, 15000 };  // 10s updates
        static const ProfileIntervals critical = { 0, 10000 };      // No updates (0 = disabled)

        switch (getProfile()) {
            case MemoryProfile::FULL:     return full;
            case MemoryProfile::STANDARD: return standard;
            case MemoryProfile::MINIMAL:  return minimal;
            case MemoryProfile::CRITICAL: return critical;
            default:                      return standard;
        }
    }

    /**
     * @brief Get limits for current profile
     */
    const ProfileLimits& getLimits() const {
        static const ProfileLimits full     = { 8, 32, 60 };   // 8 clients, 32 providers, 60 chart points
        static const ProfileLimits standard = { 4, 16, 30 };
        static const ProfileLimits minimal  = { 2, 8, 10 };
        static const ProfileLimits critical = { 1, 4, 0 };     // No chart history

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
