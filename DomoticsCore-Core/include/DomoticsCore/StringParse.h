#pragma once

#include "Platform_HAL.h"

namespace DomoticsCore {
namespace Utils {

// Digits only, at least one: the rule for any decimal a user types. toInt() is
// atol(), which reads "4x" as 4 and "abc" as 0.
inline bool digitsOnly(const String& s) {
    const unsigned n = static_cast<unsigned>(s.length());
    if (n == 0) return false;
    for (unsigned i = 0; i < n; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}

} // namespace Utils
} // namespace DomoticsCore
