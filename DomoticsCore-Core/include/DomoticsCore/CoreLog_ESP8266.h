#ifndef DOMOTICS_CORE_HAL_CORELOG_ESP8266_H
#define DOMOTICS_CORE_HAL_CORELOG_ESP8266_H

/**
 * @file CoreLog_ESP8266.h
 * @brief ESP8266 capture of the log lines the platform writes itself.
 *
 * One sink here: ets_install_putc1, which carries both ets_printf and, when SDK
 * printing is on, the SDK's own narration. The sink forwards every character to
 * UART0 the way the core's own does, so taking it costs the serial port nothing.
 *
 * SDK printing is off by default and not by accident: HardwareSerial::begin()
 * calls end(), which calls uart_set_debug(UART_NO) and system_set_os_print(0).
 * Measured on a nodemcuv2 — a full failed join prints nothing at all — so
 * setSdkOutput(true) is what makes those lines exist, on the serial port as well
 * as on the console. An application that calls Serial.setDebugOutput() later
 * takes the sink back, which is why every enable reinstalls it.
 */

#if DOMOTICS_PLATFORM_ESP8266

extern "C" {
#include <ets_sys.h>
#include <user_interface.h>
#include <uart.h>   // uart_set_debug / uart_get_debug: the core's own sink
}

namespace DomoticsCore {
namespace HAL {
namespace CoreLog {

inline void IRAM_ATTR putcHook(char c) {
    // Forward first, cooked the way the core's sink cooks it: a newline carries
    // its carriage return, and a full FIFO is waited on rather than overrun. The
    // UART is the one the core prints its own debug on, UART0 when it has none —
    // where the SDK's narration went before this took the sink.
    const uint8_t uart = (uart_get_debug() == 1) ? 1 : 0;
    while (((USS(uart) >> USTXC) & 0xff) >= 0x7e) { }
    if (c == '\n') USF(uart) = '\r';
    USF(uart) = c;
    pushChar(c);
}

/** @brief Take the sink without touching what the ring already holds. */
inline void takeSink() {
    ets_install_putc1(reinterpret_cast<void (*)(char)>(&putcHook));
}

inline bool installCapture() {
    if (captureInstalled()) return true;
    reset();
    takeSink();
    captureInstalled() = 1;
    return true;
}

inline void removeCapture() {
    if (!captureInstalled()) return;
    captureInstalled() = 0;
    // Hands the sink back to whatever the core had for the current debug UART,
    // including the one that ignores every character when there is none.
    uart_set_debug(uart_get_debug());
}

/** @brief The SDK's narration exists only while this is on, and costs a line a second while a join fails. */
inline constexpr bool supportsSdkOutputSwitch() { return true; }
inline bool setSdkOutput(bool enabled) {
    system_set_os_print(enabled ? 1 : 0);
    if (enabled) takeSink();   // an application's setDebugOutput() may have taken the sink
    return true;
}

} // namespace CoreLog
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP8266
#endif // DOMOTICS_CORE_HAL_CORELOG_ESP8266_H
