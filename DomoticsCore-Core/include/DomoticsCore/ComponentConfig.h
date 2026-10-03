#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include "Platform_HAL.h"

namespace DomoticsCore {
namespace Components {

/**
 * Component status enumeration for detailed error reporting
 */
enum class ComponentStatus {
    Success = 0,
    ConfigError,
    HardwareError,
    DependencyError,
    NetworkError,
    MemoryError,
    TimeoutError,
    InvalidState,
    NotSupported
};

/**
 * Convert ComponentStatus to human-readable string
 */
inline const char* statusToString(ComponentStatus status) {
    switch (status) {
        case ComponentStatus::Success: return "Success";
        case ComponentStatus::ConfigError: return "Configuration Error";
        case ComponentStatus::HardwareError: return "Hardware Error";
        case ComponentStatus::DependencyError: return "Dependency Error";
        case ComponentStatus::NetworkError: return "Network Error";
        case ComponentStatus::MemoryError: return "Memory Error";
        case ComponentStatus::TimeoutError: return "Timeout Error";
        case ComponentStatus::InvalidState: return "Invalid State";
        case ComponentStatus::NotSupported: return "Not Supported";
        default: return "Unknown Error";
    }
}

/**
 * Component metadata information
 */
struct ComponentMetadata {
    const char* name = "";
    const char* version = "1.0.0";
    const char* author = "";
    const char* description = "";
    const char* category = "";
    std::vector<String> tags;
    
    ComponentMetadata(const char* n = "", const char* v = "1.0.0", 
                     const char* a = "", const char* d = "")
        : name(n), version(v), author(a), description(d) {}
};

} // namespace Components
} // namespace DomoticsCore
