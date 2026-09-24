#ifndef DOMOTICS_CORE_HAL_CORELOG_STUB_H
#define DOMOTICS_CORE_HAL_CORELOG_STUB_H

/**
 * @file CoreLog_Stub.h
 * @brief Host stand-in for the platform's own log capture.
 *
 * A host build has no core to capture, so the mechanism is the seam a native
 * test drives: install, feed the lines a board's logger would have produced, and
 * assert what the component does with them. Feeding is refused while nothing is
 * installed, so a test cannot measure a capture it never asked for.
 */

namespace DomoticsCore {
namespace HAL {
namespace CoreLog {

inline bool installCapture() {
    if (captureInstalled()) return true;
    reset();
    captureInstalled() = 1;
    return true;
}

inline void removeCapture() {
    captureInstalled() = 0;
}

// There is no SDK on a host, so the switch answers "nothing to switch" — and a
// test can make it answer the other way, to reach the branch only ESP8266 takes.
inline bool& sdkSwitchSupportedForTest() {
    static bool value = false;
    return value;
}
inline bool& sdkOutputForTest() {
    static bool value = false;
    return value;
}

inline bool supportsSdkOutputSwitch() { return sdkSwitchSupportedForTest(); }
inline bool setSdkOutput(bool enabled) {
    if (!sdkSwitchSupportedForTest()) return false;
    sdkOutputForTest() = enabled;
    return true;
}

/** @brief Deliver one line the way a platform sink would. */
inline void feedLineForTest(const char* line) {
    if (captureInstalled() && line) pushLine(line, strlen(line));
}

/** @brief Deliver a line one character at a time, the way a character sink does. */
inline void feedCharsForTest(const char* text) {
    if (!captureInstalled() || !text) return;
    for (const char* p = text; *p; ++p) pushChar(*p);
}

} // namespace CoreLog
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_CORE_HAL_CORELOG_STUB_H
