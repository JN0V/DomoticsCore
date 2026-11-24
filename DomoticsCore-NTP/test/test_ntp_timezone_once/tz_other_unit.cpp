/**
 * @file tz_other_unit.cpp
 * @brief The second source file of test_ntp_timezone_once: it includes the NTP
 *        web page header and reports where it sees the timezone table.
 *
 * WebUI.h defines two static members of WebUIComponent in the header itself,
 * which a second file including it would define twice at link time. They are
 * renamed here, for this file only, so that the link reaches what the suite
 * is about: the timezone table.
 */

#define wsBuffer_ wsBufferOtherUnit_
#define bufferLock_ bufferLockOtherUnit_

#include <DomoticsCore/NTP.h>
#include <DomoticsCore/NTPWebUI.h>

const void* timezoneTableSeenByTheOtherUnit() {
    return &DomoticsCore::Components::WebUI::TIMEZONE_LOOKUP[0];
}
