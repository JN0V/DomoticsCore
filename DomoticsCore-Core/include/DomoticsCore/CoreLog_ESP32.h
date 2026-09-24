#ifndef DOMOTICS_CORE_HAL_CORELOG_ESP32_H
#define DOMOTICS_CORE_HAL_CORELOG_ESP32_H

/**
 * @file CoreLog_ESP32.h
 * @brief ESP32 capture of the log lines the platform writes itself.
 *
 * Two sinks, and they carry different lines. The precompiled ESP-IDF libraries
 * write through esp_log_write, which esp_log_set_vprintf intercepts — that is
 * where esp_ota_ops names which OTA error it hit. The Arduino core's log_e and
 * friends go through log_printf to ets_printf, which only a character sink sees;
 * in an Arduino build ESP_LOGx is remapped onto the same path, so the vprintf
 * hook alone captures nothing the application itself writes.
 *
 * Both sinks keep the serial port: the vprintf hook chains to the handler it
 * replaced, and the character sink writes the character on before counting it.
 */

#if DOMOTICS_PLATFORM_ESP32

#include <esp_log.h>
#include <stdarg.h>
#include <stdio.h>

extern "C" {
#include <rom/ets_sys.h>
#include <rom/uart.h>
int uartGetDebug();   // esp32-hal-uart.c: which UART the core prints its own lines on
// esp32-hal-uart.c defines this and no header declares it. It puts back the
// character sink the core had installed for the current debug UART.
void uart_install_putc();
}

namespace DomoticsCore {
namespace HAL {
namespace CoreLog {

inline vprintf_like_t& previousVprintf() {
    static vprintf_like_t previous = nullptr;
    return previous;
}

inline int vprintfHook(const char* format, va_list args) {
    char line[MAX_LINE];
    va_list copy;
    va_copy(copy, args);
    const int written = vsnprintf(line, sizeof(line), format, copy);
    va_end(copy);
    if (written > 0) {
        const size_t len = static_cast<size_t>(written) < sizeof(line)
                               ? static_cast<size_t>(written) : sizeof(line) - 1;
        pushLine(line, len);
    }
    return previousVprintf() ? previousVprintf()(format, args) : written;
}

// Flash-resident, like the core's own sink (uart0_write_char, which this build
// does not put in IRAM either): forcing the chain into IRAM makes the xtensa
// linker refuse the literal pool.
inline void putcHook(char c) {
    uart_tx_one_char(static_cast<uint8_t>(c));   // the serial port keeps every character
    pushChar(c);
}

inline bool installCapture() {
    if (captureInstalled()) return true;   // a second install would chain the hook to itself
    reset();
    previousVprintf() = esp_log_set_vprintf(&vprintfHook);
    // The character sink is the core's, and it writes to the UART the core chose.
    // A build whose console is USB CDC has none — `uartGetDebug()` says so — and
    // taking the sink there would send the core's lines to pins nobody reads.
    if (uartGetDebug() == 0) {
        ets_install_putc1(reinterpret_cast<void (*)(char)>(&putcHook));
    }
    captureInstalled() = 1;
    return true;
}

inline void removeCapture() {
    if (!captureInstalled()) return;
    if (previousVprintf()) {
        esp_log_set_vprintf(previousVprintf());
        previousVprintf() = nullptr;
    }
    uart_install_putc();
    captureInstalled() = 0;
}

/** @brief Nothing to switch on: both sinks print whatever the build level allows. */
inline constexpr bool supportsSdkOutputSwitch() { return false; }
inline bool setSdkOutput(bool /*enabled*/) { return false; }

} // namespace CoreLog
} // namespace HAL
} // namespace DomoticsCore

#endif // DOMOTICS_PLATFORM_ESP32
#endif // DOMOTICS_CORE_HAL_CORELOG_ESP32_H
